#ifndef INFERENCE_ENGINE_H
#define INFERENCE_ENGINE_H

#include <NvInfer.h>
#include <cuda_runtime_api.h>
#include <opencv2/opencv.hpp>
#include <string>
#include <vector>
#include <memory>

class TRTLogger : public nvinfer1::ILogger {
    void log(Severity severity, const char* msg) noexcept override {
        if (severity <= Severity::kWARNING) printf("[TRT] %s\n", msg);
    }
};

class InferenceEngine {
public:
    InferenceEngine(const std::string& enginePath);
    ~InferenceEngine();

    // 执行异步推理
    bool Infer(const cv::Mat& frame, std::vector<float>& outputData);

    // 获取模型输入输出尺寸，供外部后处理参考
    std::vector<int> GetInputDims() const { return m_inputDims; }
    std::vector<int> GetOutputDims() const { return m_outputDims; }

private:
    bool LoadEngine(const std::string& path);
    void Preprocess(const cv::Mat& frame, float* gpuInput);

    TRTLogger m_logger;
    nvinfer1::IRuntime* m_runtime = nullptr;
    nvinfer1::ICudaEngine* m_engine = nullptr;
    nvinfer1::IExecutionContext* m_context = nullptr;
    cudaStream_t m_stream = nullptr;

    void* m_gpuBuffers[2] = {nullptr, nullptr};
    size_t m_inputSizeBytes = 0;
    size_t m_outputSizeBytes = 0;

    std::vector<int> m_inputDims;
    std::vector<int> m_outputDims;
};

#endif