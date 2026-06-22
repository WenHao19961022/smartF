#include "../include/static_scene_analyzer.h"
#include "../../common/include/config_manager.h"
#include "../../common/include/logger.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <sstream>

namespace {

std::vector<std::string> SplitPaths(const std::string& value) {
    std::vector<std::string> paths;
    std::stringstream stream(value);
    std::string path;
    while (std::getline(stream, path, ',')) {
        size_t first = path.find_first_not_of(" \t");
        size_t last = path.find_last_not_of(" \t");
        if (first != std::string::npos) paths.push_back(path.substr(first, last - first + 1));
    }
    return paths;
}

float ChannelMedian(const cv::Mat& channel, const cv::Mat& mask) {
    int histogramSize = 256;
    float range[] = {0.0f, 256.0f};
    const float* ranges[] = {range};
    cv::Mat histogram;
    int channelIndex = 0;
    cv::calcHist(&channel, 1, &channelIndex, mask, histogram, 1, &histogramSize, ranges);
    float total = static_cast<float>(cv::countNonZero(mask));
    float cumulative = 0.0f;
    for (int i = 0; i < histogramSize; ++i) {
        cumulative += histogram.at<float>(i);
        if (cumulative >= total * 0.5f) return static_cast<float>(i);
    }
    return 0.0f;
}

float MaskedMean(const cv::Mat& image, const cv::Mat& mask) {
    return static_cast<float>(cv::mean(image, mask)[0]);
}

} // namespace

float SceneAnalysis::ForegroundCoverage(const cv::Rect& rawRect) const {
    cv::Rect bounds(0, 0, foregroundMask.cols, foregroundMask.rows);
    cv::Rect rect = rawRect & bounds;
    if (rect.area() <= 0 || foregroundMask.empty()) return 0.0f;
    return static_cast<float>(cv::countNonZero(foregroundMask(rect))) / rect.area();
}

int SceneAnalysis::ComponentForRect(const cv::Rect& rect, float minOverlap) const {
    int bestId = -1;
    float bestOverlap = 0.0f;
    for (const auto& component : components) {
        int intersection = (rect & component.rect).area();
        float overlap = rect.area() > 0 ? static_cast<float>(intersection) / rect.area() : 0.0f;
        if (overlap >= minOverlap && overlap > bestOverlap) {
            bestOverlap = overlap;
            bestId = component.id;
        }
    }
    return bestId;
}

StaticSceneAnalyzer::StaticSceneAnalyzer() {
    Reload();
}

bool StaticSceneAnalyzer::Reload() {
    mBackgrounds.clear();
    mBackgroundPaths.clear();
    auto& config = ConfigManager::GetInstance();
    std::string pathsValue = config.GetString("cv.background_paths", "");
    if (pathsValue.empty()) pathsValue = config.GetString("cv.bag_background_path", "");
    mBackgroundPaths = SplitPaths(pathsValue);
    for (const auto& path : mBackgroundPaths) {
        cv::Mat image = cv::imread(path);
        if (image.empty()) {
            LOG_PRINT("[CvModel]", "Static background unavailable: " << path);
            continue;
        }
        cv::resize(image, image, cv::Size(640, 640));
        mBackgrounds.push_back(image);
        LOG_PRINT("[CvModel]", "Loaded static background: " << path);
    }
    return !mBackgrounds.empty();
}

cv::Mat StaticSceneAnalyzer::BuildBoardMask(const cv::Size& size) const {
    auto& config = ConfigManager::GetInstance();
    std::vector<cv::Point> polygon = {
        {static_cast<int>(config.GetFloat("cv.location_tl_x", 0.0f) * size.width),
         static_cast<int>(config.GetFloat("cv.location_tl_y", 0.0f) * size.height)},
        {static_cast<int>(config.GetFloat("cv.location_tr_x", 1.0f) * size.width),
         static_cast<int>(config.GetFloat("cv.location_tr_y", 0.0f) * size.height)},
        {static_cast<int>(config.GetFloat("cv.location_br_x", 1.0f) * size.width),
         static_cast<int>(config.GetFloat("cv.location_br_y", 1.0f) * size.height)},
        {static_cast<int>(config.GetFloat("cv.location_bl_x", 0.0f) * size.width),
         static_cast<int>(config.GetFloat("cv.location_bl_y", 1.0f) * size.height)}
    };
    cv::Mat mask(size, CV_8UC1, cv::Scalar(0));
    cv::fillConvexPoly(mask, polygon, cv::Scalar(255));
    return mask;
}

cv::Mat StaticSceneAnalyzer::NormalizeToBackground(
    const cv::Mat& frame, const cv::Mat& background, const cv::Mat& sampleMask,
    bool& illuminationValid) const
{
    std::vector<cv::Mat> frameChannels;
    std::vector<cv::Mat> backgroundChannels;
    cv::split(frame, frameChannels);
    cv::split(background, backgroundChannels);
    std::vector<cv::Mat> normalizedChannels(3);
    illuminationValid = true;
    float gainMin = ConfigManager::GetInstance().GetFloat("cv.background_gain_min", 0.60f);
    float gainMax = ConfigManager::GetInstance().GetFloat("cv.background_gain_max", 1.70f);
    for (int channel = 0; channel < 3; ++channel) {
        float currentMedian = std::max(1.0f, ChannelMedian(frameChannels[channel], sampleMask));
        float referenceMedian = ChannelMedian(backgroundChannels[channel], sampleMask);
        float gain = referenceMedian / currentMedian;
        if (gain < gainMin || gain > gainMax) illuminationValid = false;
        gain = std::max(gainMin, std::min(gainMax, gain));
        frameChannels[channel].convertTo(normalizedChannels[channel], CV_8UC1, gain);
    }
    cv::Mat normalized;
    cv::merge(normalizedChannels, normalized);
    return normalized;
}

SceneAnalysis StaticSceneAnalyzer::Analyze(const cv::Mat& frame) const {
    SceneAnalysis result;
    if (frame.empty() || mBackgrounds.empty()) {
        result.status = RecognitionStatus::BackgroundMissing;
        return result;
    }

    cv::Mat resized;
    cv::resize(frame, resized, cv::Size(640, 640));
    cv::Mat boardMask = BuildBoardMask(resized.size());
    cv::Mat staticMask;
    cv::bitwise_not(boardMask, staticMask);
    // Include a narrow board border so geometry checks still work when the polygon fills most of the image.
    if (cv::countNonZero(staticMask) < resized.total() / 10) staticMask.setTo(255);

    double bestResidual = std::numeric_limits<double>::max();
    cv::Mat bestNormalized;
    const cv::Mat* bestBackground = nullptr;
    bool anyIlluminationValid = false;
    for (const auto& background : mBackgrounds) {
        bool illuminationValid = true;
        cv::Mat normalized = NormalizeToBackground(resized, background, staticMask, illuminationValid);
        cv::Mat difference;
        cv::absdiff(normalized, background, difference);
        cv::cvtColor(difference, difference, cv::COLOR_BGR2GRAY);
        double residual = MaskedMean(difference, staticMask);
        if (residual < bestResidual) {
            bestResidual = residual;
            bestNormalized = normalized;
            bestBackground = &background;
        }
        anyIlluminationValid = anyIlluminationValid || illuminationValid;
    }

    result.normalizedFrame = bestNormalized;
    result.backgroundResidual = static_cast<float>(bestResidual);
    if (!anyIlluminationValid || bestBackground == nullptr) {
        result.status = RecognitionStatus::IlluminationOutOfRange;
        return result;
    }

    cv::Mat gray;
    cv::cvtColor(bestNormalized, gray, cv::COLOR_BGR2GRAY);
    cv::Mat laplacian;
    cv::Laplacian(gray, laplacian, CV_32F);
    cv::Scalar lapMean, lapStd;
    cv::meanStdDev(laplacian, lapMean, lapStd, boardMask);
    result.blurScore = static_cast<float>(lapStd[0] * lapStd[0]);
    float blurMin = ConfigManager::GetInstance().GetFloat("cv.static_blur_score_min", 20.0f);
    if (result.blurScore < blurMin) {
        result.status = RecognitionStatus::InsufficientStableFrames;
        return result;
    }

    cv::Mat currentEdges, backgroundGray, backgroundEdges, edgeDifference;
    cv::Canny(gray, currentEdges, 60, 140);
    cv::cvtColor(*bestBackground, backgroundGray, cv::COLOR_BGR2GRAY);
    cv::Canny(backgroundGray, backgroundEdges, 60, 140);
    cv::Mat currentEdgesFloat, backgroundEdgesFloat, staticMaskFloat;
    currentEdges.convertTo(currentEdgesFloat, CV_32F);
    backgroundEdges.convertTo(backgroundEdgesFloat, CV_32F);
    staticMask.convertTo(staticMaskFloat, CV_32F, 1.0 / 255.0);
    cv::multiply(currentEdgesFloat, staticMaskFloat, currentEdgesFloat);
    cv::multiply(backgroundEdgesFloat, staticMaskFloat, backgroundEdgesFloat);
    double phaseResponse = 0.0;
    cv::Point2d shift = cv::phaseCorrelate(backgroundEdgesFloat, currentEdgesFloat,
                                            cv::noArray(), &phaseResponse);
    float shiftMax = ConfigManager::GetInstance().GetFloat("cv.background_camera_shift_max", 4.0f);
    if (phaseResponse > 0.20
        && std::sqrt(shift.x * shift.x + shift.y * shift.y) > shiftMax) {
        result.status = RecognitionStatus::CameraMoved;
        return result;
    }
    cv::absdiff(currentEdges, backgroundEdges, edgeDifference);
    float geometryResidual = MaskedMean(edgeDifference, staticMask);
    float geometryMax = ConfigManager::GetInstance().GetFloat("cv.background_geometry_residual_max", 38.0f);
    if (geometryResidual > geometryMax) {
        result.status = RecognitionStatus::CameraMoved;
        return result;
    }

    float residualMax = ConfigManager::GetInstance().GetFloat("cv.background_residual_max", 45.0f);
    if (bestResidual > residualMax) {
        result.status = RecognitionStatus::IlluminationOutOfRange;
        return result;
    }

    cv::Mat colorDifference;
    cv::absdiff(bestNormalized, *bestBackground, colorDifference);
    cv::cvtColor(colorDifference, colorDifference, cv::COLOR_BGR2GRAY);
    cv::threshold(colorDifference, colorDifference,
                  ConfigManager::GetInstance().GetInt("cv.foreground_diff_threshold", 28),
                  255, cv::THRESH_BINARY);
    cv::threshold(edgeDifference, edgeDifference,
                  ConfigManager::GetInstance().GetInt("cv.foreground_edge_threshold", 30),
                  255, cv::THRESH_BINARY);
    cv::bitwise_or(colorDifference, edgeDifference, result.foregroundMask);
    cv::bitwise_and(result.foregroundMask, boardMask, result.foregroundMask);
    cv::morphologyEx(result.foregroundMask, result.foregroundMask, cv::MORPH_OPEN,
                     cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(3, 3)));
    cv::morphologyEx(result.foregroundMask, result.foregroundMask, cv::MORPH_CLOSE,
                     cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(11, 11)));

    cv::Mat labels, stats, centroids;
    int count = cv::connectedComponentsWithStats(result.foregroundMask, labels, stats, centroids, 8);
    int minArea = ConfigManager::GetInstance().GetInt("cv.foreground_component_min_area", 700);
    cv::Mat filtered(result.foregroundMask.size(), CV_8UC1, cv::Scalar(0));
    int nextId = 0;
    for (int label = 1; label < count; ++label) {
        int area = stats.at<int>(label, cv::CC_STAT_AREA);
        if (area < minArea) continue;
        cv::Rect rect(stats.at<int>(label, cv::CC_STAT_LEFT), stats.at<int>(label, cv::CC_STAT_TOP),
                      stats.at<int>(label, cv::CC_STAT_WIDTH), stats.at<int>(label, cv::CC_STAT_HEIGHT));
        filtered.setTo(255, labels == label);
        result.components.push_back({nextId++, rect, area});
    }
    result.foregroundMask = filtered;
    result.status = RecognitionStatus::Valid;
    return result;
}
