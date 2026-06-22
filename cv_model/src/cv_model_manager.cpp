#include "../include/cv_model_manager.h"
#include "../include/camera_model.h"
#include "../../common/include/config_manager.h"
#include "../../common/include/logger.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <cmath>
#include <map>
#include <set>
#include <tuple>
#include <algorithm>
#include <numeric>

static uint32_t NowEpochMs32() {
    return static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count());
}

struct FruitClassMapping {
    const char* label;
    FruitType fruitType;
    FreshnessLevel freshness;
};

struct YoloDetection {
    float cx, cy, w, h;
    float confidence;
    int classId;
    std::vector<float> classScores;
};

struct ResolvedDetection {
    FruitType fruitType;
    FreshnessLevel freshness;
    bool orangeReclass;
    float orangeScore;
    float rottenOrangeScore;
    float orangeRatio;
};

struct BagCandidate {
    cv::Rect rect;
    float score;
    const char* color;
};

struct CalibratedLocation {
    uint8_t x;
    uint8_t y;
};

static cv::Rect DetectionRect(const YoloDetection& det, const cv::Size& bounds) {
    int x1 = std::max(0, static_cast<int>(std::round(det.cx - det.w * 0.5f)));
    int y1 = std::max(0, static_cast<int>(std::round(det.cy - det.h * 0.5f)));
    int x2 = std::min(bounds.width, static_cast<int>(std::round(det.cx + det.w * 0.5f)));
    int y2 = std::min(bounds.height, static_cast<int>(std::round(det.cy + det.h * 0.5f)));
    if (x2 <= x1 || y2 <= y1) {
        return cv::Rect();
    }
    return cv::Rect(x1, y1, x2 - x1, y2 - y1);
}

static float EstimateDarkSpotRatio(const cv::Mat& modelFrame, const YoloDetection& det, FruitType fruitType) {
    if (modelFrame.empty()) {
        return 0.0f;
    }

    cv::Rect roi = DetectionRect(det, modelFrame.size());
    if (roi.width < 12 || roi.height < 12) {
        return 0.0f;
    }

    cv::Mat crop = modelFrame(roi);
    cv::Mat hsv;
    cv::cvtColor(crop, hsv, cv::COLOR_BGR2HSV);

    int darkV = ConfigManager::GetInstance().GetInt("cv.rotten_dark_v_threshold", 80);
    int darkS = ConfigManager::GetInstance().GetInt("cv.rotten_dark_s_threshold", 30);
    if (fruitType == FruitType::Orange) {
        darkV = ConfigManager::GetInstance().GetInt("cv.rotten_dark_v_threshold_orange", darkV);
        darkS = ConfigManager::GetInstance().GetInt("cv.rotten_dark_s_threshold_orange", darkS);
    }

    cv::Mat darkMask;
    cv::inRange(hsv, cv::Scalar(0, darkS, 0), cv::Scalar(180, 255, darkV), darkMask);

    cv::Mat fruitColorMask;
    cv::inRange(hsv, cv::Scalar(0, 35, 40), cv::Scalar(180, 255, 255), fruitColorMask);

    cv::Mat fruitMask;
    cv::bitwise_or(fruitColorMask, darkMask, fruitMask);

    cv::Mat kernel = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(3, 3));
    cv::morphologyEx(darkMask, darkMask, cv::MORPH_OPEN, kernel);
    cv::morphologyEx(fruitMask, fruitMask, cv::MORPH_CLOSE, kernel);

    int fruitPixels = cv::countNonZero(fruitMask);
    if (fruitPixels <= 0) {
        return 0.0f;
    }

    int darkPixels = cv::countNonZero(darkMask);
    return static_cast<float>(darkPixels) / static_cast<float>(fruitPixels);
}

static float EstimateOrangePeelRatio(const cv::Mat& modelFrame, const YoloDetection& det) {
    if (modelFrame.empty()) {
        return 0.0f;
    }

    cv::Rect roi = DetectionRect(det, modelFrame.size());
    if (roi.width < 12 || roi.height < 12) {
        return 0.0f;
    }

    cv::Mat crop = modelFrame(roi);
    cv::Mat hsv;
    cv::cvtColor(crop, hsv, cv::COLOR_BGR2HSV);

    cv::Mat orangeMask;
    cv::inRange(hsv, cv::Scalar(5, 60, 50), cv::Scalar(35, 255, 255), orangeMask);

    cv::Mat fruitMask;
    cv::inRange(hsv, cv::Scalar(0, 35, 40), cv::Scalar(180, 255, 255), fruitMask);

    cv::Mat darkMask;
    cv::inRange(hsv, cv::Scalar(0, 30, 0), cv::Scalar(180, 255, 80), darkMask);
    cv::bitwise_or(fruitMask, darkMask, fruitMask);

    cv::Mat kernel = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(3, 3));
    cv::morphologyEx(orangeMask, orangeMask, cv::MORPH_OPEN, kernel);
    cv::morphologyEx(fruitMask, fruitMask, cv::MORPH_CLOSE, kernel);

    int fruitPixels = cv::countNonZero(fruitMask);
    if (fruitPixels <= 0) {
        return 0.0f;
    }
    return static_cast<float>(cv::countNonZero(orangeMask)) / static_cast<float>(fruitPixels);
}

static float RectIoU(const cv::Rect& a, const cv::Rect& b) {
    int interArea = (a & b).area();
    int unionArea = a.area() + b.area() - interArea;
    return unionArea > 0 ? static_cast<float>(interArea) / static_cast<float>(unionArea) : 0.0f;
}

static CalibratedLocation ToCalibratedLocation(float modelX, float modelY, float modelSize = 640.0f) {
    if (!std::isfinite(modelX) || !std::isfinite(modelY) || modelSize <= 0.0f) {
        return {0, 0};
    }

    float x = modelX / modelSize;
    float y = modelY / modelSize;
    auto& config = ConfigManager::GetInstance();
    if (config.GetBool("cv.location_calibration_enable", false)) {
        float xMin = config.GetFloat("cv.location_x_min", 0.0f);
        float xMax = config.GetFloat("cv.location_x_max", 1.0f);
        float yMin = config.GetFloat("cv.location_y_min", 0.0f);
        float yMax = config.GetFloat("cv.location_y_max", 1.0f);

        // Invalid field measurements must not collapse every object onto an edge.
        if (xMax - xMin >= 0.01f) {
            x = (x - xMin) / (xMax - xMin);
        }
        if (yMax - yMin >= 0.01f) {
            y = (y - yMin) / (yMax - yMin);
        }
        if (config.GetBool("cv.location_flip_x", false)) {
            x = 1.0f - x;
        }
        if (config.GetBool("cv.location_flip_y", false)) {
            y = 1.0f - y;
        }
    }

    auto toByte = [](float value) -> uint8_t {
        value = std::max(0.0f, std::min(1.0f, value));
        return static_cast<uint8_t>(std::round(value * 255.0f));
    };
    return {toByte(x), toByte(y)};
}

static bool AcceptBagContour(
    const std::vector<cv::Point>& contour,
    const cv::Mat& mask,
    float minAreaRatio,
    float maxAreaRatio,
    cv::Rect& rect,
    float& fillRatio)
{
    double area = cv::contourArea(contour);
    double frameArea = static_cast<double>(mask.rows * mask.cols);
    if (frameArea <= 0.0) {
        return false;
    }

    double areaRatio = area / frameArea;
    if (areaRatio < minAreaRatio || areaRatio > maxAreaRatio) {
        return false;
    }

    rect = cv::boundingRect(contour);
    if (rect.width < 30 || rect.height < 30) {
        return false;
    }

    double aspect = static_cast<double>(rect.width) / static_cast<double>(rect.height);
    if (aspect < 0.25 || aspect > 4.0) {
        return false;
    }

    fillRatio = static_cast<float>(area / static_cast<double>(rect.area()));
    return fillRatio >= 0.12f;
}

static bool ShouldMergeBagRects(const cv::Rect& a, const cv::Rect& b) {
    if (a.area() <= 0 || b.area() <= 0) {
        return false;
    }

    int interArea = (a & b).area();
    int minArea = std::min(a.area(), b.area());
    float smallerOverlap = static_cast<float>(interArea) / static_cast<float>(minArea);
    float iou = RectIoU(a, b);
    float iouThreshold = ConfigManager::GetInstance().GetFloat("cv.bag_merge_iou_threshold", 0.25f);
    float overlapThreshold = ConfigManager::GetInstance().GetFloat("cv.bag_merge_smaller_overlap", 0.55f);
    if (iou >= iouThreshold || smallerOverlap >= overlapThreshold) {
        return true;
    }

    // Specular highlights can split one bag contour into two non-overlapping pieces.
    // Only join close pieces whose projections still strongly align on one axis.
    int gapX = std::max(0, std::max(a.x, b.x) - std::min(a.x + a.width, b.x + b.width));
    int gapY = std::max(0, std::max(a.y, b.y) - std::min(a.y + a.height, b.y + b.height));
    int overlapX = std::max(0, std::min(a.x + a.width, b.x + b.width) - std::max(a.x, b.x));
    int overlapY = std::max(0, std::min(a.y + a.height, b.y + b.height) - std::max(a.y, b.y));
    float xAlignment = static_cast<float>(overlapX) / static_cast<float>(std::min(a.width, b.width));
    float yAlignment = static_cast<float>(overlapY) / static_cast<float>(std::min(a.height, b.height));
    int maxGap = ConfigManager::GetInstance().GetInt("cv.bag_merge_max_gap_px", 20);
    float minAlignment = ConfigManager::GetInstance().GetFloat("cv.bag_merge_min_axis_overlap", 0.45f);
    bool verticallySplit = gapY <= maxGap && xAlignment >= minAlignment;
    bool horizontallySplit = gapX <= maxGap && yAlignment >= minAlignment;
    if (!verticallySplit && !horizontallySplit) {
        return false;
    }

    cv::Rect united = a | b;
    float maxUnionAreaRatio = ConfigManager::GetInstance().GetFloat("cv.bag_max_area_ratio", 0.45f);
    return static_cast<float>(united.area()) / (640.0f * 640.0f) <= maxUnionAreaRatio;
}

static void AddBagCandidate(std::vector<BagCandidate>& bags, const BagCandidate& candidate) {
    BagCandidate merged = candidate;
    for (size_t i = 0; i < bags.size();) {
        auto& bag = bags[i];
        if (ShouldMergeBagRects(bag.rect, merged.rect)) {
            merged.rect = bag.rect | merged.rect;
            if (bag.score > merged.score) {
                merged.color = bag.color;
            }
            merged.score = std::max(bag.score, merged.score);
            bags.erase(bags.begin() + static_cast<std::ptrdiff_t>(i));
            i = 0; // The enlarged box may now connect another fragment.
            continue;
        }

        int interArea = (bag.rect & merged.rect).area();
        int minArea = std::min(bag.rect.area(), merged.rect.area());
        float smallerOverlap = minArea > 0 ? static_cast<float>(interArea) / static_cast<float>(minArea) : 0.0f;
        float iou = RectIoU(bag.rect, merged.rect);
        if (iou > 0.35f || smallerOverlap > 0.65f) {
            bool preferCandidate = iou > 0.35f
                ? merged.score > bag.score
                : merged.rect.area() > bag.rect.area();
            if (preferCandidate) {
                bag = merged;
            }
            return;
        }
        ++i;
    }
    bags.push_back(merged);
}

static bool OverlapsKnownFruit(const cv::Rect& candidate, const std::vector<cv::Rect>& fruitRects) {
    int candidateArea = candidate.area();
    if (candidateArea <= 0) {
        return true;
    }

    int combinedFruitOverlap = 0;
    for (const auto& fruitRect : fruitRects) {
        int interArea = (candidate & fruitRect).area();
        if (interArea <= 0) {
            continue;
        }
        combinedFruitOverlap += interArea;
        int fruitArea = fruitRect.area();
        if (fruitArea <= 0) {
            continue;
        }
        float candidateOverlap = static_cast<float>(interArea) / static_cast<float>(candidateArea);
        bool similarSizeToFruit = candidateArea <= static_cast<int>(fruitArea * 1.25f);
        if (candidateOverlap > 0.55f || (similarSizeToFruit && RectIoU(candidate, fruitRect) > 0.25f)) {
            return true;
        }
    }

    float maxCombinedOverlap = ConfigManager::GetInstance().GetFloat("cv.bag_fruit_overlap_max", 0.35f);
    float combinedOverlapRatio = static_cast<float>(std::min(combinedFruitOverlap, candidateArea))
        / static_cast<float>(candidateArea);
    if (combinedOverlapRatio > maxCombinedOverlap) {
        return true;
    }

    return false;
}

static cv::Rect ClampRect(const cv::Rect& rect, const cv::Size& bounds) {
    int x1 = std::max(0, rect.x);
    int y1 = std::max(0, rect.y);
    int x2 = std::min(bounds.width, rect.x + rect.width);
    int y2 = std::min(bounds.height, rect.y + rect.height);
    if (x2 <= x1 || y2 <= y1) {
        return cv::Rect();
    }
    return cv::Rect(x1, y1, x2 - x1, y2 - y1);
}

static cv::Rect ExpandRect(const cv::Rect& rect, int margin, const cv::Size& bounds) {
    return ClampRect(cv::Rect(rect.x - margin, rect.y - margin,
                              rect.width + margin * 2, rect.height + margin * 2), bounds);
}

static bool TouchesImageBorder(const cv::Rect& rect, const cv::Size& bounds, float marginRatio) {
    int borderMarginX = static_cast<int>(bounds.width * marginRatio);
    int borderMarginY = static_cast<int>(bounds.height * marginRatio);
    return rect.x <= borderMarginX
        || rect.y <= borderMarginY
        || rect.x + rect.width >= bounds.width - borderMarginX
        || rect.y + rect.height >= bounds.height - borderMarginY;
}

static float WarmFruitCoverage(const cv::Mat& hsv, const cv::Rect& rect) {
    if (hsv.empty() || rect.area() <= 0) {
        return 0.0f;
    }

    cv::Mat hsvRoi = hsv(rect);
    cv::Mat warmMask;
    cv::Mat redMask;
    cv::inRange(hsvRoi, cv::Scalar(0, 50, 50), cv::Scalar(45, 255, 255), warmMask);
    cv::inRange(hsvRoi, cv::Scalar(160, 50, 50), cv::Scalar(180, 255, 255), redMask);
    cv::bitwise_or(warmMask, redMask, warmMask);
    return static_cast<float>(cv::countNonZero(warmMask)) / static_cast<float>(rect.area());
}

static bool RejectWarmFruitAsWhiteBag(const cv::Rect& rect, const cv::Mat& hsv, float whiteCoverage) {
    float maxWarmCoverage = ConfigManager::GetInstance().GetFloat("cv.bag_white_warm_fruit_ratio_max", 0.45f);
    float warmCoverage = WarmFruitCoverage(hsv, rect);
    return warmCoverage > maxWarmCoverage && warmCoverage > whiteCoverage * 1.2f;
}

static cv::Mat GetBagBackgroundFrame() {
    static std::string loadedPath;
    static bool loadAttempted = false;
    static cv::Mat backgroundFrame;

    std::string path = ConfigManager::GetInstance().GetString("cv.bag_background_path", "");
    if (path.empty()) {
        return cv::Mat();
    }

    if (!loadAttempted || path != loadedPath) {
        loadedPath = path;
        loadAttempted = true;
        backgroundFrame.release();

        cv::Mat loaded = cv::imread(path);
        if (!loaded.empty()) {
            cv::resize(loaded, backgroundFrame, cv::Size(640, 640));
            LOG_PRINT("[CvModel]", "Loaded bag background: " << path);
        } else {
            LOG_PRINT("[CvModel]", "Bag background not available: " << path);
        }
    }

    return backgroundFrame;
}

static bool AcceptWhiteBagRect(
    const cv::Rect& rawRect,
    const cv::Mat& whiteMask,
    const cv::Mat& whiteSupportMask,
    const cv::Mat& edges,
    const cv::Mat& gray,
    const cv::Mat& hsv,
    float minAreaRatio,
    float maxAreaRatio,
    float& score)
{
    cv::Rect rect = ClampRect(rawRect, whiteMask.size());
    if (rect.width < 30 || rect.height < 30) {
        return false;
    }

    double frameArea = static_cast<double>(whiteMask.rows * whiteMask.cols);
    double rectArea = static_cast<double>(rect.area());
    double areaRatio = rectArea / frameArea;
    if (areaRatio < minAreaRatio || areaRatio > maxAreaRatio) {
        return false;
    }

    double aspect = static_cast<double>(rect.width) / static_cast<double>(rect.height);
    if (aspect < 0.25 || aspect > 4.0) {
        return false;
    }

    float whiteCoverageMin = ConfigManager::GetInstance().GetFloat("cv.bag_white_coverage_min", 0.18f);
    float edgeRatioMin = ConfigManager::GetInstance().GetFloat("cv.bag_white_edge_ratio_min", 0.015f);
    float innerEdgeRatioMin = ConfigManager::GetInstance().GetFloat("cv.bag_white_inner_edge_ratio_min", 0.004f);
    float stddevMin = ConfigManager::GetInstance().GetFloat("cv.bag_white_stddev_min", 8.0f);
    float borderContrastMin = ConfigManager::GetInstance().GetFloat("cv.bag_white_border_contrast_min", 6.0f);
    float glareRatioMax = ConfigManager::GetInstance().GetFloat("cv.bag_white_glare_ratio_max", 0.55f);
    float ignoreTopRatio = ConfigManager::GetInstance().GetFloat("cv.bag_ignore_top_ratio", 0.08f);
    float borderMarginRatio = ConfigManager::GetInstance().GetFloat("cv.bag_white_border_margin_ratio", 0.03f);

    if (ConfigManager::GetInstance().GetBool("cv.bag_white_reject_border_touch", true)
        && TouchesImageBorder(rect, whiteMask.size(), borderMarginRatio)) {
        return false;
    }

    cv::Mat whiteRoi = whiteMask(rect);
    float whiteCoverage = static_cast<float>(cv::countNonZero(whiteRoi)) / static_cast<float>(rect.area());
    if (whiteCoverage < whiteCoverageMin) {
        return false;
    }
    if (RejectWarmFruitAsWhiteBag(rect, hsv, whiteCoverage)) {
        return false;
    }

    cv::Mat edgeSupportRoi;
    cv::bitwise_and(edges(rect), whiteSupportMask(rect), edgeSupportRoi);
    float edgeRatio = static_cast<float>(cv::countNonZero(edgeSupportRoi)) / static_cast<float>(rect.area());
    if (edgeRatio < edgeRatioMin) {
        return false;
    }

    cv::Rect innerRect = ClampRect(cv::Rect(rect.x + 8, rect.y + 8, rect.width - 16, rect.height - 16), whiteMask.size());
    float innerEdgeRatio = 0.0f;
    if (innerRect.area() > 0) {
        cv::Mat innerEdges;
        cv::bitwise_and(edges(innerRect), whiteSupportMask(innerRect), innerEdges);
        innerEdgeRatio = static_cast<float>(cv::countNonZero(innerEdges)) / static_cast<float>(innerRect.area());
    }

    cv::Scalar meanGray;
    cv::Scalar stdGray;
    cv::meanStdDev(gray(rect), meanGray, stdGray, whiteRoi);
    float grayStddev = static_cast<float>(stdGray[0]);

    cv::Rect outerRect = ExpandRect(rect, 14, whiteMask.size());
    float borderContrast = 0.0f;
    if (outerRect.area() > rect.area()) {
        cv::Mat ringMask(outerRect.size(), CV_8UC1, cv::Scalar(255));
        cv::Rect innerInOuter(rect.x - outerRect.x, rect.y - outerRect.y, rect.width, rect.height);
        cv::rectangle(ringMask, innerInOuter, cv::Scalar(0), cv::FILLED);
        cv::Scalar outerMean = cv::mean(gray(outerRect), ringMask);
        borderContrast = std::abs(static_cast<float>(meanGray[0] - outerMean[0]));
    }

    cv::Mat hsvRoi = hsv(rect);
    cv::Mat sChannel;
    cv::Mat vChannel;
    cv::extractChannel(hsvRoi, sChannel, 1);
    cv::extractChannel(hsvRoi, vChannel, 2);
    cv::Mat glareMask;
    cv::inRange(vChannel, cv::Scalar(245), cv::Scalar(255), glareMask);
    cv::Mat lowSaturationMask;
    cv::inRange(sChannel, cv::Scalar(0), cv::Scalar(35), lowSaturationMask);
    cv::bitwise_and(glareMask, lowSaturationMask, glareMask);
    float glareRatio = static_cast<float>(cv::countNonZero(glareMask)) / static_cast<float>(rect.area());

    bool topLightingArea = rect.y < static_cast<int>(whiteMask.rows * ignoreTopRatio);
    bool hasInternalTexture = innerEdgeRatio >= innerEdgeRatioMin || grayStddev >= stddevMin;
    bool hasObjectBoundary = borderContrast >= borderContrastMin || edgeRatio >= edgeRatioMin * 1.6f;

    if (topLightingArea && !hasInternalTexture) {
        return false;
    }
    if (glareRatio > glareRatioMax && innerEdgeRatio < innerEdgeRatioMin && grayStddev < stddevMin * 1.5f) {
        return false;
    }
    if (!hasInternalTexture && !hasObjectBoundary) {
        return false;
    }

    score = edgeRatio + innerEdgeRatio * 1.5f + grayStddev * 0.002f
        + borderContrast * 0.001f + whiteCoverage * 0.05f;
    return true;
}

static void AddForegroundBagCandidates(
    const cv::Mat& modelFrame,
    const cv::Mat& redMask,
    const cv::Mat& whiteMask,
    const cv::Mat& edges,
    const cv::Mat& hsv,
    const std::vector<cv::Rect>& fruitRects,
    std::vector<BagCandidate>& candidates)
{
    cv::Mat backgroundFrame = GetBagBackgroundFrame();
    if (backgroundFrame.empty()) {
        return;
    }

    cv::Mat diff;
    cv::absdiff(modelFrame, backgroundFrame, diff);
    cv::Mat grayDiff;
    cv::cvtColor(diff, grayDiff, cv::COLOR_BGR2GRAY);

    int diffThreshold = ConfigManager::GetInstance().GetInt("cv.bag_bg_diff_threshold", 40);
    float minAreaRatio = ConfigManager::GetInstance().GetFloat("cv.bag_bg_min_area_ratio", 0.01f);
    float maxAreaRatio = ConfigManager::GetInstance().GetFloat("cv.bag_bg_max_area_ratio", 0.45f);
    float borderMarginRatio = ConfigManager::GetInstance().GetFloat("cv.bag_bg_border_margin_ratio", 0.03f);
    float minWhiteCoverage = ConfigManager::GetInstance().GetFloat("cv.bag_bg_white_coverage_min", 0.15f);
    float minRedCoverage = ConfigManager::GetInstance().GetFloat("cv.bag_bg_red_coverage_min", 0.04f);
    float minEdgeRatio = ConfigManager::GetInstance().GetFloat("cv.bag_bg_edge_ratio_min", 0.012f);
    int closeKernelSize = ConfigManager::GetInstance().GetInt("cv.bag_bg_close_kernel", 5);
    closeKernelSize = std::max(3, closeKernelSize | 1);

    cv::Mat fgMask;
    cv::threshold(grayDiff, fgMask, diffThreshold, 255, cv::THRESH_BINARY);
    cv::morphologyEx(fgMask, fgMask, cv::MORPH_OPEN,
                     cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(5, 5)));
    cv::morphologyEx(fgMask, fgMask, cv::MORPH_CLOSE,
                     cv::getStructuringElement(cv::MORPH_RECT, cv::Size(closeKernelSize, closeKernelSize)));

    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(fgMask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
    double frameArea = static_cast<double>(fgMask.rows * fgMask.cols);

    for (const auto& contour : contours) {
        double contourArea = cv::contourArea(contour);
        double areaRatio = contourArea / frameArea;
        if (areaRatio < minAreaRatio || areaRatio > maxAreaRatio) {
            continue;
        }

        cv::Rect rect = cv::boundingRect(contour);
        if (rect.width < 30 || rect.height < 30) {
            continue;
        }

        double aspect = static_cast<double>(rect.width) / static_cast<double>(rect.height);
        if (aspect < 0.25 || aspect > 4.0) {
            continue;
        }
        if (TouchesImageBorder(rect, fgMask.size(), borderMarginRatio)) {
            continue;
        }
        if (OverlapsKnownFruit(rect, fruitRects)) {
            continue;
        }

        float rectArea = static_cast<float>(rect.area());
        float whiteCoverage = static_cast<float>(cv::countNonZero(whiteMask(rect))) / rectArea;
        float redCoverage = static_cast<float>(cv::countNonZero(redMask(rect))) / rectArea;
        float edgeRatio = static_cast<float>(cv::countNonZero(edges(rect))) / rectArea;
        if (whiteCoverage < minWhiteCoverage && redCoverage < minRedCoverage && edgeRatio < minEdgeRatio) {
            continue;
        }

        bool isWhiteCandidate = redCoverage <= whiteCoverage;
        if (isWhiteCandidate && RejectWarmFruitAsWhiteBag(rect, hsv, whiteCoverage)) {
            continue;
        }
        const char* color = isWhiteCandidate ? "white" : "red";
        float score = static_cast<float>(areaRatio) + whiteCoverage * 0.2f + redCoverage * 0.4f + edgeRatio;
        AddBagCandidate(candidates, {rect, score, color});
    }
}

static void AddLowContrastWhiteBagCandidates(
    const cv::Mat& modelFrame,
    const cv::Mat& whiteMask,
    const cv::Mat& edges,
    const cv::Mat& hsv,
    const std::vector<cv::Rect>& fruitRects,
    std::vector<BagCandidate>& candidates)
{
    if (!ConfigManager::GetInstance().GetBool("cv.bag_white_low_contrast_enable", true)) {
        return;
    }

    cv::Mat gray;
    cv::cvtColor(modelFrame, gray, cv::COLOR_BGR2GRAY);

    int supportVMin = ConfigManager::GetInstance().GetInt("cv.bag_white_low_contrast_v_min", 180);
    int supportSMax = ConfigManager::GetInstance().GetInt("cv.bag_white_low_contrast_s_max", 90);
    cv::Mat supportMask;
    cv::inRange(hsv, cv::Scalar(0, 0, supportVMin), cv::Scalar(180, supportSMax, 255), supportMask);

    cv::Mat candidateMask = supportMask.clone();

    float topIgnoreRatio = ConfigManager::GetInstance().GetFloat("cv.bag_white_low_contrast_top_ignore_ratio", 0.12f);
    float bottomLimitRatio = ConfigManager::GetInstance().GetFloat("cv.bag_white_low_contrast_bottom_limit_ratio", 0.55f);
    float sideMarginRatio = ConfigManager::GetInstance().GetFloat("cv.bag_white_low_contrast_side_margin_ratio", 0.12f);
    int topIgnore = static_cast<int>(candidateMask.rows * topIgnoreRatio);
    int bottomLimit = static_cast<int>(candidateMask.rows * bottomLimitRatio);
    int sideMargin = static_cast<int>(candidateMask.cols * sideMarginRatio);
    if (topIgnore > 0) {
        candidateMask(cv::Rect(0, 0, candidateMask.cols, std::min(topIgnore, candidateMask.rows))).setTo(0);
    }
    if (bottomLimit < candidateMask.rows) {
        candidateMask(cv::Rect(0, std::max(0, bottomLimit), candidateMask.cols,
                               candidateMask.rows - std::max(0, bottomLimit))).setTo(0);
    }
    if (sideMargin > 0 && sideMargin * 2 < candidateMask.cols) {
        candidateMask(cv::Rect(0, 0, sideMargin, candidateMask.rows)).setTo(0);
        candidateMask(cv::Rect(candidateMask.cols - sideMargin, 0, sideMargin, candidateMask.rows)).setTo(0);
    }

    cv::morphologyEx(candidateMask, candidateMask, cv::MORPH_OPEN,
                     cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(7, 7)));
    cv::morphologyEx(candidateMask, candidateMask, cv::MORPH_CLOSE,
                     cv::getStructuringElement(cv::MORPH_RECT, cv::Size(31, 31)));

    float minAreaRatio = ConfigManager::GetInstance().GetFloat("cv.bag_white_low_contrast_min_area_ratio", 0.04f);
    float maxAreaRatio = ConfigManager::GetInstance().GetFloat("cv.bag_white_low_contrast_max_area_ratio", 0.22f);
    int minWidth = ConfigManager::GetInstance().GetInt("cv.bag_white_low_contrast_min_width", 120);
    int minHeight = ConfigManager::GetInstance().GetInt("cv.bag_white_low_contrast_min_height", 110);
    float maxBottomRatio = ConfigManager::GetInstance().GetFloat("cv.bag_white_low_contrast_max_bottom_ratio", 0.58f);
    float centerXMin = ConfigManager::GetInstance().GetFloat("cv.bag_white_low_contrast_center_x_min", 0.35f);
    float centerXMax = ConfigManager::GetInstance().GetFloat("cv.bag_white_low_contrast_center_x_max", 0.68f);
    float minAspect = ConfigManager::GetInstance().GetFloat("cv.bag_white_low_contrast_min_aspect", 0.75f);
    float maxAspect = ConfigManager::GetInstance().GetFloat("cv.bag_white_low_contrast_max_aspect", 2.2f);
    float minCoverage = ConfigManager::GetInstance().GetFloat("cv.bag_white_low_contrast_min_coverage", 0.45f);
    float meanSMax = ConfigManager::GetInstance().GetFloat("cv.bag_white_low_contrast_mean_s_max", 55.0f);
    float stddevMin = ConfigManager::GetInstance().GetFloat("cv.bag_white_low_contrast_stddev_min", 18.0f);
    float scoreBias = ConfigManager::GetInstance().GetFloat("cv.bag_white_low_contrast_score_bias", 0.25f);

    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(candidateMask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
    double frameArea = static_cast<double>(candidateMask.rows * candidateMask.cols);
    for (const auto& contour : contours) {
        double contourArea = cv::contourArea(contour);
        float areaRatio = static_cast<float>(contourArea / frameArea);
        if (areaRatio < minAreaRatio || areaRatio > maxAreaRatio) {
            continue;
        }

        cv::Rect rect = cv::boundingRect(contour);
        if (rect.width < minWidth || rect.height < minHeight) {
            continue;
        }
        if (static_cast<float>(rect.y + rect.height) / candidateMask.rows > maxBottomRatio) {
            continue;
        }
        float centerX = static_cast<float>(rect.x + rect.width * 0.5f) / candidateMask.cols;
        if (centerX < centerXMin || centerX > centerXMax) {
            continue;
        }
        float aspect = static_cast<float>(rect.width) / static_cast<float>(rect.height);
        if (aspect < minAspect || aspect > maxAspect) {
            continue;
        }
        if (OverlapsKnownFruit(rect, fruitRects)) {
            continue;
        }

        float rectArea = static_cast<float>(rect.area());
        float whiteCoverage = static_cast<float>(cv::countNonZero(supportMask(rect))) / rectArea;
        if (whiteCoverage < minCoverage) {
            continue;
        }

        cv::Mat hsvRoi = hsv(rect);
        std::vector<cv::Mat> hsvChannels;
        cv::split(hsvRoi, hsvChannels);
        float meanSaturation = static_cast<float>(cv::mean(hsvChannels[1])[0]);
        if (meanSaturation > meanSMax) {
            continue;
        }

        cv::Scalar meanGray;
        cv::Scalar stdGray;
        cv::meanStdDev(gray(rect), meanGray, stdGray);
        float grayStddev = static_cast<float>(stdGray[0]);
        if (grayStddev < stddevMin) {
            continue;
        }

        float edgeRatio = static_cast<float>(cv::countNonZero(edges(rect))) / rectArea;
        float score = scoreBias + areaRatio + whiteCoverage * 0.15f + edgeRatio
            + grayStddev * 0.001f + std::max(0.0f, meanSMax - meanSaturation) * 0.001f;
        score = std::min(0.99f, score);
        AddBagCandidate(candidates, {rect, score, "white"});
    }
}

static std::vector<FruitInfo> DetectOrangesOpenCV(
    const cv::Mat& frame,
    std::vector<cv::Rect>& fruitRects)
{
    std::vector<FruitInfo> results;
    if (frame.empty() || !ConfigManager::GetInstance().GetBool("cv.orange_opencv_detect_enable", true)) {
        return results;
    }

    cv::Mat modelFrame;
    cv::resize(frame, modelFrame, cv::Size(640, 640));

    cv::Mat hsv;
    cv::cvtColor(modelFrame, hsv, cv::COLOR_BGR2HSV);

    cv::Mat orangeMask;
    cv::inRange(hsv, cv::Scalar(5, 60, 50), cv::Scalar(35, 255, 255), orangeMask);
    cv::morphologyEx(orangeMask, orangeMask, cv::MORPH_OPEN,
                     cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(5, 5)));
    cv::morphologyEx(orangeMask, orangeMask, cv::MORPH_CLOSE,
                     cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(9, 9)));

    float minAreaRatio = ConfigManager::GetInstance().GetFloat("cv.orange_opencv_min_area_ratio", 0.008f);
    float maxAreaRatio = ConfigManager::GetInstance().GetFloat("cv.orange_opencv_max_area_ratio", 0.18f);
    float minCoverage = ConfigManager::GetInstance().GetFloat("cv.orange_opencv_min_coverage", 0.55f);
    float minCircularity = ConfigManager::GetInstance().GetFloat("cv.orange_opencv_min_circularity", 0.35f);
    float minAspect = ConfigManager::GetInstance().GetFloat("cv.orange_opencv_min_aspect", 0.55f);
    float maxAspect = ConfigManager::GetInstance().GetFloat("cv.orange_opencv_max_aspect", 1.35f);
    int minFruitBoxSide = ConfigManager::GetInstance().GetInt("cv.min_fruit_box_side", 36);

    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(orangeMask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
    double frameArea = static_cast<double>(orangeMask.rows * orangeMask.cols);

    for (const auto& contour : contours) {
        double area = cv::contourArea(contour);
        double areaRatio = area / frameArea;
        if (areaRatio < minAreaRatio || areaRatio > maxAreaRatio) {
            continue;
        }

        cv::Rect rect = cv::boundingRect(contour);
        if (rect.width < minFruitBoxSide || rect.height < minFruitBoxSide) {
            continue;
        }

        double aspect = static_cast<double>(rect.width) / static_cast<double>(rect.height);
        if (aspect < minAspect || aspect > maxAspect) {
            continue;
        }

        double perimeter = cv::arcLength(contour, true);
        double circularity = perimeter > 0.0
            ? 4.0 * 3.14159265358979323846 * area / (perimeter * perimeter)
            : 0.0;
        if (circularity < minCircularity) {
            continue;
        }

        if (OverlapsKnownFruit(rect, fruitRects)) {
            continue;
        }

        float orangeCoverage = static_cast<float>(cv::countNonZero(orangeMask(rect)))
            / static_cast<float>(rect.area());
        if (orangeCoverage < minCoverage) {
            continue;
        }

        YoloDetection pseudoDet{};
        pseudoDet.cx = rect.x + rect.width * 0.5f;
        pseudoDet.cy = rect.y + rect.height * 0.5f;
        pseudoDet.w = static_cast<float>(rect.width);
        pseudoDet.h = static_cast<float>(rect.height);
        pseudoDet.confidence = std::min(0.99f, std::max(0.25f, orangeCoverage * static_cast<float>(circularity)));
        pseudoDet.classId = 2;

        float darkRatio = EstimateDarkSpotRatio(modelFrame, pseudoDet, FruitType::Orange);
        float rottenThreshold = ConfigManager::GetInstance().GetFloat("cv.rotten_dark_ratio_threshold_orange", 0.18f);

        FruitInfo info;
        info.fruitType = FruitType::Orange;
        CalibratedLocation location = ToCalibratedLocation(pseudoDet.cx, pseudoDet.cy);
        info.locationX = location.x;
        info.locationY = location.y;
        info.freshness = darkRatio >= rottenThreshold ? FreshnessLevel::Rotten : FreshnessLevel::Fresh;

        results.push_back(info);
        fruitRects.push_back(rect);
        LOG_PRINT("[CvModel]", "  OrangeFallback score=" << pseudoDet.confidence
                  << " coverage=" << orangeCoverage
                  << " circularity=" << circularity
                  << " darkRatio=" << darkRatio
                  << " freshness=" << (int)info.freshness
                  << " rect=(" << rect.x << "," << rect.y << ","
                  << rect.width << "," << rect.height << ")");
    }

    return results;
}

static std::vector<FruitInfo> DetectPlasticBagsOpenCV(
    const cv::Mat& frame,
    const std::vector<cv::Rect>& fruitRects)
{
    std::vector<FruitInfo> results;
    if (frame.empty() || !ConfigManager::GetInstance().GetBool("cv.bag_detect_enable", true)) {
        return results;
    }

    cv::Mat modelFrame;
    cv::resize(frame, modelFrame, cv::Size(640, 640));

    cv::Mat hsv;
    cv::cvtColor(modelFrame, hsv, cv::COLOR_BGR2HSV);

    float maxAreaRatio = ConfigManager::GetInstance().GetFloat("cv.bag_max_area_ratio", 0.45f);
    std::vector<BagCandidate> candidates;

    cv::Mat redLow;
    cv::Mat redHigh;
    cv::Mat redMask;
    int redSMin = ConfigManager::GetInstance().GetInt("cv.bag_red_s_min", 70);
    int redVMin = ConfigManager::GetInstance().GetInt("cv.bag_red_v_min", 50);
    cv::inRange(hsv, cv::Scalar(0, redSMin, redVMin), cv::Scalar(12, 255, 255), redLow);
    cv::inRange(hsv, cv::Scalar(168, redSMin, redVMin), cv::Scalar(180, 255, 255), redHigh);
    cv::bitwise_or(redLow, redHigh, redMask);

    cv::Mat redKernel = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(5, 5));
    cv::morphologyEx(redMask, redMask, cv::MORPH_OPEN, redKernel);
    cv::morphologyEx(redMask, redMask, cv::MORPH_CLOSE, redKernel);

    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(redMask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
    float redMinArea = ConfigManager::GetInstance().GetFloat("cv.bag_red_min_area_ratio", 0.01f);
    float redBorderMargin = ConfigManager::GetInstance().GetFloat("cv.bag_red_border_margin_ratio", 0.03f);
    for (const auto& contour : contours) {
        cv::Rect rect;
        float fillRatio = 0.0f;
        if (AcceptBagContour(contour, redMask, redMinArea, maxAreaRatio, rect, fillRatio)) {
            if (ConfigManager::GetInstance().GetBool("cv.bag_red_reject_border_touch", true)
                && TouchesImageBorder(rect, redMask.size(), redBorderMargin)) {
                continue;
            }
            if (OverlapsKnownFruit(rect, fruitRects)) {
                continue;
            }
            AddBagCandidate(candidates, {rect, fillRatio, "red"});
        }
    }

    int whiteSMax = ConfigManager::GetInstance().GetInt("cv.bag_white_s_max", 90);
    int whiteVMin = ConfigManager::GetInstance().GetInt("cv.bag_white_v_min", 105);
    float whiteMinArea = ConfigManager::GetInstance().GetFloat("cv.bag_white_min_area_ratio", 0.02f);

    cv::Mat whiteMask;
    cv::inRange(hsv, cv::Scalar(0, 0, whiteVMin), cv::Scalar(180, whiteSMax, 255), whiteMask);

    cv::Mat whiteKernel = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(5, 5));
    cv::morphologyEx(whiteMask, whiteMask, cv::MORPH_OPEN, whiteKernel);
    cv::morphologyEx(whiteMask, whiteMask, cv::MORPH_CLOSE, whiteKernel);

    cv::Mat gray;
    cv::Mat edges;
    cv::cvtColor(modelFrame, gray, cv::COLOR_BGR2GRAY);
    cv::Canny(gray, edges, 60, 140);

    cv::Mat whiteSupportMask;
    cv::dilate(whiteMask, whiteSupportMask, cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(7, 7)));

    AddForegroundBagCandidates(modelFrame, redMask, whiteMask, edges, hsv, fruitRects, candidates);
    AddLowContrastWhiteBagCandidates(modelFrame, whiteMask, edges, hsv, fruitRects, candidates);

    contours.clear();
    cv::findContours(whiteMask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
    for (const auto& contour : contours) {
        cv::Rect rect;
        float fillRatio = 0.0f;
        if (!AcceptBagContour(contour, whiteMask, whiteMinArea, maxAreaRatio, rect, fillRatio)) {
            continue;
        }
        if (OverlapsKnownFruit(rect, fruitRects)) {
            continue;
        }
        float score = 0.0f;
        if (!AcceptWhiteBagRect(rect, whiteMask, whiteSupportMask, edges, gray, hsv,
                                whiteMinArea, maxAreaRatio, score)) {
            continue;
        }
        AddBagCandidate(candidates, {rect, score + fillRatio * 0.05f, "white"});
    }

    cv::Mat whiteEdges;
    cv::bitwise_and(edges, whiteSupportMask, whiteEdges);
    cv::dilate(whiteEdges, whiteEdges, cv::getStructuringElement(cv::MORPH_RECT, cv::Size(5, 5)));
    cv::morphologyEx(whiteEdges, whiteEdges, cv::MORPH_CLOSE,
                     cv::getStructuringElement(cv::MORPH_RECT, cv::Size(11, 11)));

    contours.clear();
    cv::findContours(whiteEdges, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
    for (const auto& contour : contours) {
        cv::Rect rect = cv::boundingRect(contour);
        if (OverlapsKnownFruit(rect, fruitRects)) {
            continue;
        }
        float score = 0.0f;
        if (!AcceptWhiteBagRect(rect, whiteMask, whiteSupportMask, edges, gray, hsv,
                                whiteMinArea, maxAreaRatio, score)) {
            continue;
        }
        AddBagCandidate(candidates, {rect, score, "white"});
    }

    for (const auto& candidate : candidates) {
        FruitInfo info;
        info.fruitType = FruitType::PlasticBag;
        CalibratedLocation location = ToCalibratedLocation(
            candidate.rect.x + candidate.rect.width * 0.5f,
            candidate.rect.y + candidate.rect.height * 0.5f);
        info.locationX = location.x;
        info.locationY = location.y;
        info.freshness = FreshnessLevel::Fresh;
        results.push_back(info);
        LOG_PRINT("[CvModel]", "  BagDetect color=" << candidate.color
                  << " score=" << candidate.score
                  << " rect=(" << candidate.rect.x << "," << candidate.rect.y
                  << "," << candidate.rect.width << "," << candidate.rect.height << ")"
                  << " pos=(" << (int)info.locationX << "," << (int)info.locationY << ")");
    }

    return results;
}

CvModelManager& CvModelManager::GetInstance() {
    static CvModelManager instance;
    return instance;
}

CvModelManager::CvModelManager()
    : mBaselineFrameCount(0)
    , mBaselineEstablished(false) {
}

CvModelManager::~CvModelManager() {
}

bool CvModelManager::CvModelInit() {
    mInitStatus.store(kInitUnfinished);

    mStaticRecognitionSwitch.store(kRecognitionSwitchOff);
    mDynamicRecognitionSwitch.store(kRecognitionSwitchOff);
    mStaticRecognitionStatus.store(kRecognitionIdle);
    mDynamicRecognitionStatus.store(kRecognitionIdle);

    mDataMutex.lock();
    mStaticRecognitionResult = {};
    mDynamicRecognitionResult = {};
    mDataMutex.unlock();

    {
        std::lock_guard<std::mutex> lock(mDynamicMutex);
        mTrackedFruits.clear();
        mWindowFrames.clear();
        mBaselineFrameCount = 0;
        mBaselineEstablished = false;
    }
    {
        std::lock_guard<std::mutex> lock(mDataMutex);
        mDynamicRecognitionResult = {};
    }

    std::string enginePath = ConfigManager::GetInstance().GetString("cv.model_path", "cv_model/yolov8/yolov8s.engine");
    std::string onnxPath = ConfigManager::GetInstance().GetString("cv.model_onnx_path", "cv_model/yolov8/yolov8s.onnx");

    try {
        mInferenceEngine = std::make_unique<InferenceEngine>(enginePath);
        if (mInferenceEngine->IsCpuMode()) {
            LOG_PRINT("[CvModel]", "Running in CPU mode (ONNX: " << onnxPath << ")");
        } else {
            LOG_PRINT("[CvModel]", "TensorRT Engine Initialized Successfully.");
        }
        SetReady();
        return true;
    } catch (const std::exception& e) {
        LOG_PRINT("[CvModel]", "CV Model init failed: " << e.what());
        return false;
    }
}

bool CvModelManager::IsCameraReady() const {
    return CameraModule::GetInstance().IsOpened();
}

// ==================== MainLoop ====================
void CvModelManager::MainLoop() {
    LOG_PRINT("[CvModel]", "MainLoop started");
    while (true) {
        if (!IsCvModelReady()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            continue;
        }

        // --- 静态识别触发 ---
        if (IsStaticRecognitionSwitchOn() && IsStaticRecognitionIdle()) {
            LOG_PRINT("[CvModel]", "MainLoop: StaticRecognition triggered");
            StaticRecognitionInternal();
        }

        // --- 动态识别：开关ON & 动态空闲 → 进入动态识别流程 ---
        if (IsDynamicRecognitionSwitchOn() && IsDynamicRecognitionIdle()) {
            LOG_PRINT("[CvModel]", "MainLoop: DynamicRecognition triggered (switch=ON, status=IDLE)");
            DynamicRecognitionLoop();
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

// ==================== YOLO后处理 ====================
// ==================== YOLO Post-Processing ====================
static std::vector<FruitInfo> PostProcessYOLO(
    const cv::Mat& frame,
    const std::vector<float>& output,
    const std::vector<int>& outputDims,
    float confThreshold = -1.0f,
    float nmsThreshold = -1.0f)
{
    std::vector<FruitInfo> results;

    if (output.empty() || outputDims.size() < 3) {
        return results;
    }
    if (confThreshold < 0.0f) {
        confThreshold = ConfigManager::GetInstance().GetFloat("cv.conf_threshold", 0.25f);
    }
    if (nmsThreshold < 0.0f) {
        nmsThreshold = ConfigManager::GetInstance().GetFloat("cv.nms_threshold", 0.4f);
    }

    const int dim1 = outputDims[1];
    const int dim2 = outputDims[2];
    const bool channelsFirst = dim1 < dim2;
    const int channels = channelsFirst ? dim1 : dim2;
    const int numProposals = channelsFirst ? dim2 : dim1;
    const int numClasses = channels - 4;

    if (numClasses <= 0) {
        LOG_PRINT("[CvModel]", "Invalid YOLO output shape: ["
                  << outputDims[0] << "," << dim1 << "," << dim2 << "]");
        return results;
    }

    const std::vector<FruitClassMapping> classMappings = {
        {"fresh_apple", FruitType::Apple, FreshnessLevel::Fresh},
        {"fresh_banana", FruitType::Banana, FreshnessLevel::Fresh},
        {"fresh_orange", FruitType::Orange, FreshnessLevel::Fresh},
        {"rotten_apple", FruitType::Apple, FreshnessLevel::Rotten},
        {"rotten_banana", FruitType::Banana, FreshnessLevel::Rotten},
        {"rotten_orange", FruitType::Orange, FreshnessLevel::Rotten},
        {"plastic_bag", FruitType::PlasticBag, FreshnessLevel::Fresh}
    };

    std::vector<YoloDetection> detections;
    cv::Mat modelFrame;
    if (!frame.empty()) {
        cv::resize(frame, modelFrame, cv::Size(640, 640));
    }

    auto resolveDetection = [&](const YoloDetection& det) -> ResolvedDetection {
        ResolvedDetection resolved{};
        resolved.fruitType = classMappings[det.classId].fruitType;
        resolved.freshness = classMappings[det.classId].freshness;
        resolved.orangeReclass = false;
        resolved.orangeScore = det.classScores.size() > 2 ? det.classScores[2] : 0.0f;
        resolved.rottenOrangeScore = det.classScores.size() > 5 ? det.classScores[5] : 0.0f;
        resolved.orangeRatio = 0.0f;

        if (resolved.fruitType == FruitType::Apple
            && ConfigManager::GetInstance().GetBool("cv.orange_color_reclass_enable", true)) {
            resolved.orangeRatio = EstimateOrangePeelRatio(modelFrame, det);
            float orangeScoreMin = ConfigManager::GetInstance().GetFloat("cv.orange_reclass_score_min", 0.03f);
            float orangeRatioMin = ConfigManager::GetInstance().GetFloat("cv.orange_reclass_ratio_min", 0.45f);
            float orangeStrongRatioMin = ConfigManager::GetInstance().GetFloat("cv.orange_reclass_strong_ratio_min", 0.80f);
            bool scoreBackedOrange = std::max(resolved.orangeScore, resolved.rottenOrangeScore) >= orangeScoreMin
                && resolved.orangeRatio >= orangeRatioMin;
            bool strongColorOrange = resolved.orangeRatio >= orangeStrongRatioMin;
            if (scoreBackedOrange || strongColorOrange) {
                resolved.fruitType = FruitType::Orange;
                resolved.freshness = resolved.rottenOrangeScore > resolved.orangeScore
                    ? FreshnessLevel::Rotten
                    : FreshnessLevel::Fresh;
                resolved.orangeReclass = true;
            }
        }

        return resolved;
    };

    auto readOutput = [&](int proposal, int channel) -> float {
        if (channelsFirst) {
            return output[channel * numProposals + proposal];
        }
        return output[proposal * channels + channel];
    };

    auto calcIoU = [](const YoloDetection& a, const YoloDetection& b) -> float {
        float ax1 = a.cx - a.w * 0.5f;
        float ay1 = a.cy - a.h * 0.5f;
        float ax2 = a.cx + a.w * 0.5f;
        float ay2 = a.cy + a.h * 0.5f;
        float bx1 = b.cx - b.w * 0.5f;
        float by1 = b.cy - b.h * 0.5f;
        float bx2 = b.cx + b.w * 0.5f;
        float by2 = b.cy + b.h * 0.5f;

        float interX1 = std::max(ax1, bx1);
        float interY1 = std::max(ay1, by1);
        float interX2 = std::min(ax2, bx2);
        float interY2 = std::min(ay2, by2);
        float interW = std::max(0.0f, interX2 - interX1);
        float interH = std::max(0.0f, interY2 - interY1);
        float interArea = interW * interH;
        float areaA = std::max(0.0f, ax2 - ax1) * std::max(0.0f, ay2 - ay1);
        float areaB = std::max(0.0f, bx2 - bx1) * std::max(0.0f, by2 - by1);
        float unionArea = areaA + areaB - interArea;
        return unionArea > 0.0f ? interArea / unionArea : 0.0f;
    };

    for (int i = 0; i < numProposals; ++i) {
        float cx = readOutput(i, 0);
        float cy = readOutput(i, 1);
        float bw = readOutput(i, 2);
        float bh = readOutput(i, 3);

        float maxConf = 0.0f;
        int maxClassId = 0;
        std::vector<float> classScores(std::min(numClasses, static_cast<int>(classMappings.size())), 0.0f);

        for (int c = 0; c < static_cast<int>(classScores.size()); ++c) {
            float conf = readOutput(i, 4 + c);
            classScores[c] = conf;
            if (conf > maxConf) {
                maxConf = conf;
                maxClassId = c;
            }
        }

        if (maxConf >= confThreshold) {
            detections.push_back({cx, cy, bw, bh, maxConf, maxClassId, classScores});
        }
    }

    std::sort(detections.begin(), detections.end(),
              [](const YoloDetection& a, const YoloDetection& b) {
                  return a.confidence > b.confidence;
              });

    std::vector<bool> suppressed(detections.size(), false);
    std::vector<cv::Rect> acceptedFruitRects;

    // If there are detections, print a separator for this frame
    if (!detections.empty()) {
        std::cout << "============ 检测到目标 ==========" << std::endl;
    }

    for (size_t i = 0; i < detections.size(); ++i) {
        if (suppressed[i]) continue;

        const auto& det = detections[i];
        
        // --- ADDED: Print [ID] Label directly to terminal ---
        std::string label_name = (det.classId < static_cast<int>(classMappings.size())) ? classMappings[det.classId].label : "Unknown";
        float freshAppleScore = det.classScores.size() > 0 ? det.classScores[0] : 0.0f;
        float rottenAppleScore = det.classScores.size() > 3 ? det.classScores[3] : 0.0f;
        float freshOrangeScore = det.classScores.size() > 2 ? det.classScores[2] : 0.0f;
        float rottenOrangeScore = det.classScores.size() > 5 ? det.classScores[5] : 0.0f;
        std::cout << "[" << det.classId << "] " << label_name
                  << " conf=" << det.confidence
                  << " modelPos=(" << det.cx << "," << det.cy << ")"
                  << " apple_scores(fresh=" << freshAppleScore
                  << ", rotten=" << rottenAppleScore << ")"
                  << " orange_scores(fresh=" << freshOrangeScore
                  << ", rotten=" << rottenOrangeScore << ")"
                  << std::endl;

        if (det.classId < 0 || det.classId >= static_cast<int>(classMappings.size())) {
            continue;
        }

        ResolvedDetection resolved = resolveDetection(det);
        FruitInfo info;
        info.fruitType = resolved.fruitType;
        CalibratedLocation location = ToCalibratedLocation(det.cx, det.cy);
        info.locationX = location.x;
        info.locationY = location.y;
        info.freshness = resolved.freshness;
        if (resolved.orangeReclass) {
            LOG_PRINT("[CvModel]", "  Reclass apple->orange"
                      << " orangeScore=" << resolved.orangeScore
                      << " rottenOrangeScore=" << resolved.rottenOrangeScore
                      << " orangeRatio=" << resolved.orangeRatio);
        }
        if (info.fruitType != FruitType::PlasticBag) {
            float minFruitAreaRatio = ConfigManager::GetInstance().GetFloat("cv.min_fruit_box_area_ratio", 0.003f);
            int minFruitBoxSide = ConfigManager::GetInstance().GetInt("cv.min_fruit_box_side", 36);
            float boxAreaRatio = (det.w * det.h) / (640.0f * 640.0f);
            if (boxAreaRatio < minFruitAreaRatio || det.w < minFruitBoxSide || det.h < minFruitBoxSide) {
                LOG_PRINT("[CvModel]", "  Skip tiny fruit candidate " << label_name
                          << " box=(" << det.w << "x" << det.h << ")"
                          << " areaRatio=" << boxAreaRatio);
                continue;
            }
        }
        if (info.fruitType != FruitType::PlasticBag) {
            cv::Rect fruitRect = DetectionRect(det, cv::Size(640, 640));
            if (fruitRect.area() > 0) {
                acceptedFruitRects.push_back(fruitRect);
            }
        }

        if (ConfigManager::GetInstance().GetBool("cv.rotten_spot_enable", true)
            && info.fruitType != FruitType::PlasticBag) {
            float darkRatio = EstimateDarkSpotRatio(modelFrame, det, info.fruitType);
            float rottenThreshold = ConfigManager::GetInstance().GetFloat("cv.rotten_dark_ratio_threshold", 0.15f);
            if (info.fruitType == FruitType::Orange) {
                rottenThreshold = ConfigManager::GetInstance().GetFloat("cv.rotten_dark_ratio_threshold_orange", 0.18f);
            } else if (info.fruitType == FruitType::Banana) {
                rottenThreshold = ConfigManager::GetInstance().GetFloat("cv.rotten_dark_ratio_threshold_banana", rottenThreshold);
            }
            if (darkRatio >= rottenThreshold) {
                info.freshness = FreshnessLevel::Rotten;
            }
            LOG_PRINT("[CvModel]", "  SpotCheck " << label_name
                      << " darkRatio=" << darkRatio
                      << " threshold=" << rottenThreshold
                      << " freshness=" << (int)info.freshness);
        }

        results.push_back(info);

        // NMS logic
        for (size_t j = i + 1; j < detections.size(); ++j) {
            if (suppressed[j]) continue;
            if (detections[j].classId < 0 || detections[j].classId >= static_cast<int>(classMappings.size())) {
                continue;
            }
            if (resolveDetection(detections[j]).fruitType != info.fruitType) {
                continue;
            }

            bool duplicateBag = info.fruitType == FruitType::PlasticBag
                && ShouldMergeBagRects(DetectionRect(det, cv::Size(640, 640)),
                                       DetectionRect(detections[j], cv::Size(640, 640)));
            if (duplicateBag || calcIoU(det, detections[j]) > nmsThreshold) {
                suppressed[j] = true;
            }
        }
    }

    auto orangeFallbackDetections = DetectOrangesOpenCV(frame, acceptedFruitRects);
    results.insert(results.end(), orangeFallbackDetections.begin(), orangeFallbackDetections.end());

    auto bagDetections = DetectPlasticBagsOpenCV(frame, acceptedFruitRects);
    for (const auto& bag : bagDetections) {
        bool duplicate = false;
        for (const auto& existing : results) {
            if (existing.fruitType != FruitType::PlasticBag) {
                continue;
            }
            int dx = static_cast<int>(existing.locationX) - static_cast<int>(bag.locationX);
            int dy = static_cast<int>(existing.locationY) - static_cast<int>(bag.locationY);
            int mergeDistance = ConfigManager::GetInstance().GetInt("cv.bag_cross_source_merge_distance", 30);
            if (std::sqrt(static_cast<float>(dx * dx + dy * dy)) < mergeDistance) {
                duplicate = true;
                break;
            }
        }
        if (!duplicate) {
            results.push_back(bag);
        }
    }

    return results;
}
// ==================== 静态识别 ====================
void CvModelManager::StaticRecognitionInternal() {
    LOG_PRINT("[CvModel]", "=== StaticRecognitionInternal START ===");
    SetStaticRecognitionStatus(kRecognitionBusy);

    struct StaticCandidate {
        FruitType type;
        float sumX;
        float sumY;
        int count;
        int freshVotes;
        int staleVotes;
        int rottenVotes;
    };
    std::vector<StaticCandidate> fruitCandidates;
    StaticRecognitionResult result = {};
    result.timestamp = static_cast<uint32_t>(
        std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());

    int successfulDetections = 0;
    auto startTime = std::chrono::steady_clock::now();

    for (int detectionIndex = 0; detectionIndex < kStaticDetectionCount; ++detectionIndex) {
        if (!IsStaticRecognitionSwitchOn()) {
            LOG_PRINT("[CvModel]", "Static recognition cancelled at detection " << detectionIndex);
            break;
        }

        cv::Mat frame = CameraModule::GetInstance().GetLatestFrame();

        if (!frame.empty() && mInferenceEngine) {
            std::vector<float> rawOutput;
            if (mInferenceEngine->Infer(frame, rawOutput)) {
                auto detections = PostProcessYOLO(frame, rawOutput, mInferenceEngine->GetOutputDims());
                std::vector<bool> used(fruitCandidates.size(), false);
                for (const auto& det : detections) {
                    bool matched = false;
                    for (size_t j = 0; j < fruitCandidates.size(); ++j) {
                        auto& cand = fruitCandidates[j];
                        if (j >= used.size()) {
                            used.resize(fruitCandidates.size(), false);
                        }
                        if (cand.type != det.fruitType || used[j]) {
                            continue;
                        }

                        float avgX = cand.sumX / cand.count;
                        float avgY = cand.sumY / cand.count;
                        float dx = det.locationX - avgX;
                        float dy = det.locationY - avgY;
                        if (std::sqrt(dx * dx + dy * dy) < kDynamicPositionTolerance) {
                            cand.sumX += det.locationX;
                            cand.sumY += det.locationY;
                            cand.count++;
                            if (det.freshness == FreshnessLevel::Rotten) {
                                cand.rottenVotes++;
                            } else if (det.freshness == FreshnessLevel::Stale) {
                                cand.staleVotes++;
                            } else {
                                cand.freshVotes++;
                            }
                            used[j] = true;
                            matched = true;
                            break;
                        }
                    }
                    if (!matched) {
                        StaticCandidate candidate{};
                        candidate.type = det.fruitType;
                        candidate.sumX = static_cast<float>(det.locationX);
                        candidate.sumY = static_cast<float>(det.locationY);
                        candidate.count = 1;
                        candidate.freshVotes = det.freshness == FreshnessLevel::Fresh ? 1 : 0;
                        candidate.staleVotes = det.freshness == FreshnessLevel::Stale ? 1 : 0;
                        candidate.rottenVotes = det.freshness == FreshnessLevel::Rotten ? 1 : 0;
                        fruitCandidates.push_back(candidate);
                        used.push_back(true);
                    }
                    LOG_PRINT("[CvModel]", "  Detection " << detectionIndex << ": type=" << (int)det.fruitType
                              << " pos=(" << (int)det.locationX << "," << (int)det.locationY << ")"
                              << " freshness=" << (int)det.freshness);
                }
                successfulDetections++;
            }
        } else if (frame.empty()) {
            continue;
        }
    }

    auto endTime = std::chrono::steady_clock::now();
    auto durationMs = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
    LOG_PRINT("[CvModel]", "Static recognition completed " << successfulDetections << "/" << kStaticDetectionCount
              << " detections in " << durationMs << "ms");

    // 多数确认
    std::vector<FruitInfo> finalDetections;
    for (const auto& cand : fruitCandidates) {
        int confirmThreshold = kStaticConfirmThreshold;
        if (cand.type == FruitType::PlasticBag) {
            confirmThreshold = ConfigManager::GetInstance().GetInt("cv.static_bag_confirm_threshold", 2);
        }
        if (cand.count >= confirmThreshold) {
            FruitInfo info;
            info.fruitType = cand.type;
            if (cand.rottenVotes * 2 >= cand.count) {
                info.freshness = FreshnessLevel::Rotten;
            } else if (cand.staleVotes * 2 >= cand.count) {
                info.freshness = FreshnessLevel::Stale;
            } else {
                info.freshness = FreshnessLevel::Fresh;
            }
            info.locationX = static_cast<uint8_t>(std::min(255, static_cast<int>(cand.sumX / cand.count)));
            info.locationY = static_cast<uint8_t>(std::min(255, static_cast<int>(cand.sumY / cand.count)));
            LOG_PRINT("[CvModel]", "  StaticCandidate final type=" << (int)info.fruitType
                      << " count=" << cand.count
                      << " votes(fresh=" << cand.freshVotes
                      << ", stale=" << cand.staleVotes
                      << ", rotten=" << cand.rottenVotes << ")"
                      << " freshness=" << (int)info.freshness);
            finalDetections.push_back(info);
        }
    }

    result.fruitCount = std::min(static_cast<uint8_t>(finalDetections.size()), kMaxStaticFruitCount);
    for (uint8_t i = 0; i < result.fruitCount; ++i) {
        result.fruits[i] = finalDetections[i];
    }

    {
        std::lock_guard<std::mutex> lock(mDataMutex);
        mStaticRecognitionResult = result;
    }

    LOG_PRINT("[CvModel]", "=== StaticRecognitionInternal END === fruitCount=" << (int)result.fruitCount);

    SetStaticRecognitionSwitch(kRecognitionSwitchOff);
    SetStaticRecognitionStatus(kRecognitionIdle);
}

// ==================== 动态识别主循环（新版） ====================
void CvModelManager::DynamicRecognitionLoop() {
    LOG_PRINT("[CvModel]", "=== DynamicRecognitionLoop START ===");
    SetDynamicRecognitionStatus(kRecognitionBusy);

    // 初始化动态识别状态
    {
        std::lock_guard<std::mutex> lock(mDynamicMutex);
        mTrackedFruits.clear();
        mWindowFrames.clear();
        mBaselineFrameCount = 0;
        mBaselineEstablished = false;
    }
    {
        std::lock_guard<std::mutex> lock(mDataMutex);
        mDynamicRecognitionResult = {};
    }

    LOG_PRINT("[CvModel]", "DynamicRecognitionLoop: entering baseline + monitoring loop");

    // 主循环：持续运行直到开关关闭
    while (IsDynamicRecognitionSwitchOn()) {
        // 获取一帧并推理
        cv::Mat frame = CameraModule::GetInstance().GetLatestFrame();
        if (frame.empty() || !mInferenceEngine) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }

        std::vector<float> rawOutput;
        if (!mInferenceEngine->Infer(frame, rawOutput)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }

        auto detections = PostProcessYOLO(frame, rawOutput, mInferenceEngine->GetOutputDims());

        {
            std::lock_guard<std::mutex> lock(mDynamicMutex);

            if (!mBaselineEstablished) {
                // ===== 基准建立阶段 =====
                mWindowFrames.push_back(detections);
                mBaselineFrameCount++;

                if (mBaselineFrameCount >= kDynamicBaselineFrames) {
                    DynamicEstablishBaseline();
                    mBaselineEstablished = true;
                    LOG_PRINT("[CvModel]", "Baseline established, switching to monitoring mode");
                }
            } else {
                // ===== 监测阶段 =====
                // 加入滑动窗口
                mWindowFrames.push_back(detections);
                if (static_cast<int>(mWindowFrames.size()) > kDynamicWindowSize) {
                    mWindowFrames.erase(mWindowFrames.begin());
                }

                // 更新追踪水果状态
                DynamicMonitoringStep();
                // 检测变化
                DynamicCheckChanges();
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // 开关被关闭，构建最终结果
    LOG_PRINT("[CvModel]", "DynamicRecognitionLoop: switch OFF, building final result");

    DynamicRecognitionResult result = {};
    {
        std::lock_guard<std::mutex> dynLock(mDynamicMutex);

        // 如果基准还没建立就结束了，结果为空
        if (mBaselineEstablished) {
            // 做最后一轮变化检测（处理还在pending中的变化）
            DynamicCheckChanges();
        }

        // 从 trackedFruits 中收集所有 confirmed 变化事件
        // 同时也从动态结果中读取之前记录的事件
        {
            std::lock_guard<std::mutex> dataLock(mDataMutex);
            result = mDynamicRecognitionResult;
        }
    }

    LOG_PRINT("[CvModel]", "DynamicRecognitionLoop: eventCount=" << (int)result.eventCount);
    for (uint8_t i = 0; i < result.eventCount; ++i) {
        LOG_PRINT("[CvModel]", "  Event[" << (int)i << "]: type=" << (int)result.events[i].fruitType
                  << " action=" << (result.events[i].action == FruitChangeAction::PUT_IN ? "PUT_IN" : "TAKE_OUT")
                  << " ts=" << result.events[i].timestamp);
    }
    LOG_PRINT("[CvModel]", "=== DynamicRecognitionLoop END ===");

    // 清理动态识别状态
    {
        std::lock_guard<std::mutex> lock(mDynamicMutex);
        mTrackedFruits.clear();
        mWindowFrames.clear();
        mBaselineFrameCount = 0;
        mBaselineEstablished = false;
    }

    SetDynamicRecognitionStatus(kRecognitionIdle);
}

// ==================== 确立基准 ====================
void CvModelManager::DynamicEstablishBaseline() {
    // 统计每种水果在基准帧中的出现次数
    // 使用位置容差合并同类水果
    struct Candidate {
        FruitType type;
        float sumX, sumY;
        int count;
    };
    std::vector<Candidate> candidates;

    for (const auto& frameDetections : mWindowFrames) {
        std::vector<bool> candidateUsed(candidates.size(), false);

        for (const auto& fruit : frameDetections) {
            bool matched = false;
            for (size_t j = 0; j < candidates.size(); ++j) {
                if (j >= candidateUsed.size()) {
                    candidateUsed.resize(candidates.size(), false);
                }
                if (candidateUsed[j]) continue;
                if (candidates[j].type != fruit.fruitType) continue;

                float dx = fruit.locationX - (candidates[j].sumX / candidates[j].count);
                float dy = fruit.locationY - (candidates[j].sumY / candidates[j].count);
                float dist = std::sqrt(dx * dx + dy * dy);

                if (dist < kDynamicPositionTolerance) {
                    candidates[j].sumX += fruit.locationX;
                    candidates[j].sumY += fruit.locationY;
                    candidates[j].count++;
                    candidateUsed[j] = true;
                    matched = true;
                    break;
                }
            }

            if (!matched) {
                candidates.push_back({fruit.fruitType,
                                      static_cast<float>(fruit.locationX),
                                      static_cast<float>(fruit.locationY), 1});
                candidateUsed.push_back(true);
            }
        }
    }

    // 在 >= 50% 的基准帧中出现的水果视为基准水果
    int threshold = static_cast<int>(std::ceil(kDynamicBaselineFrames * 0.5f));
    LOG_PRINT("[CvModel]", "EstablishBaseline: " << candidates.size() << " candidates, threshold=" << threshold);

    for (const auto& cand : candidates) {
        if (cand.count >= threshold) {
            TrackedFruitState state;
            state.fruitType = cand.type;
            state.locationX = static_cast<uint8_t>(std::min(255, static_cast<int>(cand.sumX / cand.count)));
            state.locationY = static_cast<uint8_t>(std::min(255, static_cast<int>(cand.sumY / cand.count)));
            state.isBaseline = true;
            state.alreadyConfirmed = false;
            state.continuousCount = 0;
            state.continuousAbsentCount = 0;
            state.totalAppearCount = kDynamicWindowSize;  // 初始认为全窗口都存在
            state.cooldownFrames = 0;

            mTrackedFruits.push_back(state);
            LOG_PRINT("[CvModel]", "  Baseline fruit: type=" << (int)state.fruitType
                      << " pos=(" << (int)state.locationX << "," << (int)state.locationY << ")"
                      << " appeared=" << cand.count << "/" << kDynamicBaselineFrames);
        }
    }

    LOG_PRINT("[CvModel]", "Baseline established: " << mTrackedFruits.size() << " fruits");
}

// ==================== 监测步骤（更新追踪状态） ====================
void CvModelManager::DynamicMonitoringStep() {
    if (mWindowFrames.empty()) return;

    // 取最新一帧的检测结果
    const auto& latestDetections = mWindowFrames.back();

    // 标记每个追踪水果在本帧是否被检测到
    for (auto& tracked : mTrackedFruits) {
        if (tracked.cooldownFrames > 0) {
            tracked.cooldownFrames--;
        }

        bool found = false;
        for (const auto& det : latestDetections) {
            if (det.fruitType == tracked.fruitType && IsFruitSimilar(det, {tracked.fruitType, tracked.locationX, tracked.locationY, FreshnessLevel::Fresh})) {
                found = true;
                // 更新位置（平滑）
                tracked.locationX = static_cast<uint8_t>((tracked.locationX * 7 + det.locationX * 3) / 10);
                tracked.locationY = static_cast<uint8_t>((tracked.locationY * 7 + det.locationY * 3) / 10);
                break;
            }
        }

        if (found) {
            tracked.continuousCount++;
            tracked.continuousAbsentCount = 0;
            tracked.totalAppearCount++;
        } else {
            tracked.continuousAbsentCount++;
            tracked.continuousCount = 0;
        }
    }

    // 检测是否有新水果出现（不在追踪列表中的）
    for (const auto& det : latestDetections) {
        bool existing = HasTrackedFruit(det.fruitType, det.locationX, det.locationY);
        if (!existing) {
            // 新水果，加入追踪列表
            TrackedFruitState state;
            state.fruitType = det.fruitType;
            state.locationX = det.locationX;
            state.locationY = det.locationY;
            state.isBaseline = false;
            state.alreadyConfirmed = false;
            state.continuousCount = 1;
            state.continuousAbsentCount = 0;
            state.totalAppearCount = 1;
            state.cooldownFrames = 0;
            mTrackedFruits.push_back(state);
            LOG_PRINT("[CvModel]", "New fruit detected: type=" << (int)det.fruitType
                      << " pos=(" << (int)det.locationX << "," << (int)det.locationY << ")");
        }
    }
}

// ==================== 检测变化 ====================
void CvModelManager::DynamicCheckChanges() {
    int windowSize = static_cast<int>(mWindowFrames.size());
    if (windowSize < kDynamicWindowSize) return;  // 窗口未满，不检测

    for (auto& tracked : mTrackedFruits) {
        if (tracked.cooldownFrames > 0) continue;

        int appearCount = CountWindowAppearances(tracked);
        float ratio = static_cast<float>(appearCount) / windowSize;

        // 基准水果消失 → TAKE_OUT
        if (tracked.isBaseline && ratio < kDynamicDisappearRatio) {
            RecordChangeEvent(tracked.fruitType, FruitChangeAction::TAKE_OUT,
                              tracked.locationX, tracked.locationY);
            LOG_PRINT("[CvModel]", "CHANGE: TAKE_OUT type=" << (int)tracked.fruitType
                      << " ratio=" << ratio << " (<" << kDynamicDisappearRatio << ")");

            // 确认后翻转状态：该水果现在不在冰箱中，开始监测是否会被重新放入
            tracked.isBaseline = false;
            tracked.alreadyConfirmed = false;
            tracked.continuousCount = 0;
            tracked.continuousAbsentCount = 0;
            tracked.totalAppearCount = 0;
            tracked.cooldownFrames = kDynamicEventCooldownFrames;
        }

        // 非基准水果出现 → PUT_IN
        if (!tracked.isBaseline && ratio >= kDynamicAppearRatio) {
            RecordChangeEvent(tracked.fruitType, FruitChangeAction::PUT_IN,
                              tracked.locationX, tracked.locationY);
            LOG_PRINT("[CvModel]", "CHANGE: PUT_IN type=" << (int)tracked.fruitType
                      << " ratio=" << ratio << " (>=" << kDynamicAppearRatio << ")");

            // 确认后翻转状态：该水果现在在冰箱中，开始监测是否会被取走
            tracked.isBaseline = true;
            tracked.alreadyConfirmed = false;
            tracked.continuousCount = 0;
            tracked.continuousAbsentCount = 0;
            tracked.totalAppearCount = 0;
            tracked.cooldownFrames = kDynamicEventCooldownFrames;
        }
    }

    // 清理：完全消失的非基准水果（误检测）移除追踪列表
    mTrackedFruits.erase(
        std::remove_if(mTrackedFruits.begin(), mTrackedFruits.end(),
            [](const TrackedFruitState& tf) {
                return !tf.isBaseline && tf.continuousAbsentCount > kDynamicWindowSize * 2;
            }),
        mTrackedFruits.end());
}

// ==================== 记录变化事件 ====================
void CvModelManager::RecordChangeEvent(FruitType type, FruitChangeAction action, uint8_t x, uint8_t y) {
    uint32_t now = NowEpochMs32();

    std::lock_guard<std::mutex> dataLock(mDataMutex);

    if (mDynamicRecognitionResult.eventCount >= kMaxDynamicEventCount) {
        LOG_PRINT("[CvModel]", "WARNING: event list full, cannot record more events");
        return;
    }

    uint8_t idx = mDynamicRecognitionResult.eventCount;
    mDynamicRecognitionResult.events[idx].fruitType = type;
    mDynamicRecognitionResult.events[idx].action = action;
    mDynamicRecognitionResult.events[idx].timestamp = now;
    mDynamicRecognitionResult.events[idx].locationX = x;
    mDynamicRecognitionResult.events[idx].locationY = y;
    mDynamicRecognitionResult.eventCount++;

    LOG_PRINT("[CvModel]", "Event recorded [" << (int)idx << "]: type=" << (int)type
              << " action=" << (action == FruitChangeAction::PUT_IN ? "PUT_IN" : "TAKE_OUT")
              << " ts=" << now << " pos=(" << (int)x << "," << (int)y << ")");
}

// ==================== 工具方法 ====================
int CvModelManager::CountWindowAppearances(const TrackedFruitState& tracked) const {
    int count = 0;
    FruitInfo trackedInfo{tracked.fruitType, tracked.locationX, tracked.locationY, FreshnessLevel::Fresh};
    for (const auto& frame : mWindowFrames) {
        bool found = false;
        for (const auto& det : frame) {
            if (IsFruitSimilar(det, trackedInfo)) {
                found = true;
                break;
            }
        }
        if (found) count++;
    }
    return count;
}

bool CvModelManager::IsFruitSimilar(const FruitInfo& a, const FruitInfo& b) const {
    if (a.fruitType != b.fruitType) return false;
    int dx = static_cast<int>(a.locationX) - static_cast<int>(b.locationX);
    int dy = static_cast<int>(a.locationY) - static_cast<int>(b.locationY);
    int dist = static_cast<int>(std::sqrt(static_cast<float>(dx * dx + dy * dy)));
    return dist < kDynamicPositionTolerance;
}

bool CvModelManager::HasTrackedFruit(FruitType type, uint8_t x, uint8_t y) {
    FruitInfo target{type, x, y, FreshnessLevel::Fresh};
    for (auto& tracked : mTrackedFruits) {
        FruitInfo trackedInfo{tracked.fruitType, tracked.locationX, tracked.locationY, FreshnessLevel::Fresh};
        if (IsFruitSimilar(target, trackedInfo)) {
            return true;
        }
    }
    return false;
}

// ==================== 结果获取 ====================
StaticRecognitionResult CvModelManager::GetStaticResult() {
    std::lock_guard<std::mutex> lock(mDataMutex);
    return mStaticRecognitionResult;
}

DynamicRecognitionResult CvModelManager::GetDynamicResult() {
    std::lock_guard<std::mutex> lock(mDataMutex);
    return mDynamicRecognitionResult;
}
