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

struct BagCandidate {
    cv::Rect rect;
    float score;
    const char* color;
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

static float EstimateDarkSpotRatio(const cv::Mat& modelFrame, const YoloDetection& det) {
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

static float RectIoU(const cv::Rect& a, const cv::Rect& b) {
    int interArea = (a & b).area();
    int unionArea = a.area() + b.area() - interArea;
    return unionArea > 0 ? static_cast<float>(interArea) / static_cast<float>(unionArea) : 0.0f;
}

static uint8_t ToLocationByte(float modelCoord, float modelSize = 640.0f) {
    if (!std::isfinite(modelCoord) || modelSize <= 0.0f) {
        return 0;
    }
    float clamped = std::max(0.0f, std::min(modelSize, modelCoord));
    int scaled = static_cast<int>(std::round(clamped * 255.0f / modelSize));
    return static_cast<uint8_t>(std::max(0, std::min(255, scaled)));
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

static void AddBagCandidate(std::vector<BagCandidate>& bags, const BagCandidate& candidate) {
    for (auto& bag : bags) {
        if (RectIoU(bag.rect, candidate.rect) > 0.35f) {
            if (candidate.score > bag.score) {
                bag = candidate;
            }
            return;
        }
    }
    bags.push_back(candidate);
}

static bool OverlapsKnownFruit(const cv::Rect& candidate, const std::vector<cv::Rect>& fruitRects) {
    int candidateArea = candidate.area();
    if (candidateArea <= 0) {
        return true;
    }

    for (const auto& fruitRect : fruitRects) {
        int interArea = (candidate & fruitRect).area();
        if (interArea <= 0) {
            continue;
        }
        float candidateOverlap = static_cast<float>(interArea) / static_cast<float>(candidateArea);
        if (RectIoU(candidate, fruitRect) > 0.25f || candidateOverlap > 0.55f) {
            return true;
        }
    }
    return false;
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
    for (const auto& contour : contours) {
        cv::Rect rect;
        float fillRatio = 0.0f;
        if (AcceptBagContour(contour, redMask, redMinArea, maxAreaRatio, rect, fillRatio)) {
            if (OverlapsKnownFruit(rect, fruitRects)) {
                continue;
            }
            AddBagCandidate(candidates, {rect, fillRatio, "red"});
        }
    }

    int whiteSMax = ConfigManager::GetInstance().GetInt("cv.bag_white_s_max", 55);
    int whiteVMin = ConfigManager::GetInstance().GetInt("cv.bag_white_v_min", 135);
    float whiteMinArea = ConfigManager::GetInstance().GetFloat("cv.bag_white_min_area_ratio", 0.02f);
    float whiteEdgeRatioMin = ConfigManager::GetInstance().GetFloat("cv.bag_white_edge_ratio_min", 0.015f);

    cv::Mat whiteMask;
    cv::inRange(hsv, cv::Scalar(0, 0, whiteVMin), cv::Scalar(180, whiteSMax, 255), whiteMask);

    cv::Mat whiteKernel = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(5, 5));
    cv::morphologyEx(whiteMask, whiteMask, cv::MORPH_OPEN, whiteKernel);
    cv::morphologyEx(whiteMask, whiteMask, cv::MORPH_CLOSE, whiteKernel);

    cv::Mat gray;
    cv::Mat edges;
    cv::cvtColor(modelFrame, gray, cv::COLOR_BGR2GRAY);
    cv::Canny(gray, edges, 60, 140);

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

        cv::Mat contourMask = cv::Mat::zeros(whiteMask.size(), CV_8UC1);
        std::vector<std::vector<cv::Point>> oneContour{contour};
        cv::drawContours(contourMask, oneContour, 0, cv::Scalar(255), cv::FILLED);

        cv::Mat contourEdges;
        cv::bitwise_and(edges, contourMask, contourEdges);
        int areaPixels = cv::countNonZero(contourMask);
        float edgeRatio = areaPixels > 0
            ? static_cast<float>(cv::countNonZero(contourEdges)) / static_cast<float>(areaPixels)
            : 0.0f;
        if (edgeRatio < whiteEdgeRatioMin) {
            continue;
        }

        AddBagCandidate(candidates, {rect, edgeRatio + fillRatio * 0.1f, "white"});
    }

    for (const auto& candidate : candidates) {
        FruitInfo info;
        info.fruitType = FruitType::PlasticBag;
        info.locationX = ToLocationByte(candidate.rect.x + candidate.rect.width * 0.5f);
        info.locationY = ToLocationByte(candidate.rect.y + candidate.rect.height * 0.5f);
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
    float confThreshold = 0.5f,
    float nmsThreshold = 0.4f)
{
    std::vector<FruitInfo> results;

    if (output.empty() || outputDims.size() < 3) {
        return results;
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

        if (maxConf > confThreshold) {
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
        std::cout << "[" << det.classId << "] " << label_name
                  << " conf=" << det.confidence
                  << " apple_scores(fresh=" << freshAppleScore
                  << ", rotten=" << rottenAppleScore << ")"
                  << std::endl;

        if (det.classId < 0 || det.classId >= static_cast<int>(classMappings.size())) {
            continue;
        }

        FruitInfo info;
        info.fruitType = classMappings[det.classId].fruitType;
        info.locationX = ToLocationByte(det.cx);
        info.locationY = ToLocationByte(det.cy);
        info.freshness = classMappings[det.classId].freshness;
        if (info.fruitType != FruitType::PlasticBag) {
            cv::Rect fruitRect = DetectionRect(det, cv::Size(640, 640));
            if (fruitRect.area() > 0) {
                acceptedFruitRects.push_back(fruitRect);
            }
        }

        if (ConfigManager::GetInstance().GetBool("cv.rotten_spot_enable", true)
            && info.fruitType != FruitType::PlasticBag) {
            cv::Mat modelFrame;
            if (!frame.empty()) {
                cv::resize(frame, modelFrame, cv::Size(640, 640));
            }
            float darkRatio = EstimateDarkSpotRatio(modelFrame, det);
            float rottenThreshold = ConfigManager::GetInstance().GetFloat("cv.rotten_dark_ratio_threshold", 0.15f);
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
            if (classMappings[detections[j].classId].fruitType != classMappings[det.classId].fruitType) {
                continue;
            }

            if (calcIoU(det, detections[j]) > nmsThreshold) {
                suppressed[j] = true;
            }
        }
    }

    auto bagDetections = DetectPlasticBagsOpenCV(frame, acceptedFruitRects);
    for (const auto& bag : bagDetections) {
        bool duplicate = false;
        for (const auto& existing : results) {
            if (existing.fruitType != FruitType::PlasticBag) {
                continue;
            }
            int dx = static_cast<int>(existing.locationX) - static_cast<int>(bag.locationX);
            int dy = static_cast<int>(existing.locationY) - static_cast<int>(bag.locationY);
            if (std::sqrt(static_cast<float>(dx * dx + dy * dy)) < kDynamicPositionTolerance) {
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
        FreshnessLevel freshness;
        float sumX;
        float sumY;
        int count;
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
                        if (cand.type != det.fruitType || cand.freshness != det.freshness || used[j]) {
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
                            used[j] = true;
                            matched = true;
                            break;
                        }
                    }
                    if (!matched) {
                        fruitCandidates.push_back({det.fruitType, det.freshness,
                                                   static_cast<float>(det.locationX),
                                                   static_cast<float>(det.locationY), 1});
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
        if (cand.count >= kStaticConfirmThreshold) {
            FruitInfo info;
            info.fruitType = cand.type;
            info.freshness = cand.freshness;
            info.locationX = static_cast<uint8_t>(std::min(255, static_cast<int>(cand.sumX / cand.count)));
            info.locationY = static_cast<uint8_t>(std::min(255, static_cast<int>(cand.sumY / cand.count)));
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
