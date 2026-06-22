#include "../include/static_scene_analyzer.h"
#include "../../common/include/config_manager.h"
#include <opencv2/opencv.hpp>
#include <cstdlib>
#include <iostream>

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "usage: static_scene_replay BACKGROUND IMAGE...\n";
        return 2;
    }
    bool expectEmpty = argc > 2 && std::string(argv[2]) == "--empty";
    int firstImage = expectEmpty ? 3 : 2;
    if (argc <= firstImage) return 2;
    ConfigManager::GetInstance().SetString("cv.background_paths", argv[1]);
    ConfigManager::GetInstance().SetFloat("cv.location_tl_x", 0.164063f);
    ConfigManager::GetInstance().SetFloat("cv.location_tl_y", 0.179167f);
    ConfigManager::GetInstance().SetFloat("cv.location_tr_x", 0.792188f);
    ConfigManager::GetInstance().SetFloat("cv.location_tr_y", 0.179167f);
    ConfigManager::GetInstance().SetFloat("cv.location_br_x", 1.0f);
    ConfigManager::GetInstance().SetFloat("cv.location_br_y", 0.802083f);
    ConfigManager::GetInstance().SetFloat("cv.location_bl_x", 0.0f);
    ConfigManager::GetInstance().SetFloat("cv.location_bl_y", 0.802083f);
    StaticSceneAnalyzer analyzer;
    if (!analyzer.HasBackground()) return 3;

    int invalid = 0;
    for (int i = firstImage; i < argc; ++i) {
        cv::Mat image = cv::imread(argv[i]);
        SceneAnalysis analysis = analyzer.Analyze(image);
        std::cout << argv[i] << " status=" << static_cast<int>(analysis.status)
                  << " components=" << analysis.components.size()
                  << " residual=" << analysis.backgroundResidual
                  << " blur=" << analysis.blurScore << "\n";
        if (!analysis.IsValid() || (expectEmpty && !analysis.components.empty())) invalid++;
    }
    return invalid == 0 ? 0 : 1;
}
