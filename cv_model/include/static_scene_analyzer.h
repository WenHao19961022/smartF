#ifndef STATIC_SCENE_ANALYZER_H
#define STATIC_SCENE_ANALYZER_H

#include "../api/cv_model_api.h"
#include <opencv2/opencv.hpp>
#include <string>
#include <vector>

struct ForegroundComponent {
    int id = -1;
    cv::Rect rect;
    int area = 0;
};

struct SceneAnalysis {
    RecognitionStatus status = RecognitionStatus::BackgroundMissing;
    cv::Mat normalizedFrame;
    cv::Mat foregroundMask;
    std::vector<ForegroundComponent> components;
    float backgroundResidual = 0.0f;
    float blurScore = 0.0f;

    bool IsValid() const { return status == RecognitionStatus::Valid; }
    float ForegroundCoverage(const cv::Rect& rect) const;
    int ComponentForRect(const cv::Rect& rect, float minOverlap = 0.15f) const;
};

class StaticSceneAnalyzer {
public:
    StaticSceneAnalyzer();
    bool Reload();
    SceneAnalysis Analyze(const cv::Mat& frame) const;
    bool HasBackground() const { return !mBackgrounds.empty(); }

private:
    cv::Mat BuildBoardMask(const cv::Size& size) const;
    cv::Mat NormalizeToBackground(const cv::Mat& frame, const cv::Mat& background,
                                  const cv::Mat& sampleMask, bool& illuminationValid) const;

    std::vector<cv::Mat> mBackgrounds;
    std::vector<std::string> mBackgroundPaths;
};

#endif
