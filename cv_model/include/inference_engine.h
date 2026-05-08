#ifndef INFERENCE_ENGINE_H
#define INFERENCE_ENGINE_H

#include <NvInfer.h>
#include <cuda_runtime_api.h>
#include <opencv2/opencv.hpp>
#include <string>
#include <vector>
#include <memory>

// ==================== TensorRT日志类 ====================
class TrtLogger : public nvinfer1::ILogger {
    void log(Severity severity, const char* msg) noexcept override {
        if (severity <= Severity::kWARNING) printf("[TRT] %s\n", msg);
    }
};

// ==================== 推理引擎类 ====================
class InferenceEngine {
public:
    InferenceEngine(const std::string& enginePath);
    ~InferenceEngine();

    bool Infer(const cv::Mat& frame, std::vector<float>& outputData);

    std::vector<int> GetInputDims() const { return mInputDims; }
    std::vector<int> GetOutputDims() const { return mOutputDims; }
    bool IsCpuMode() const { return mUseCpu; }

private:
    bool LoadEngine(const std::string& path);
    bool LoadCpuOnnx(const std::string& enginePath);
    bool InferCpu(const cv::Mat& frame, std::vector<float>& outputData);
    void Preprocess(const cv::Mat& frame, float* gpuInput);

    TrtLogger mLogger;
    nvinfer1::IRuntime* mRuntime = nullptr;
    nvinfer1::ICudaEngine* mEngine = nullptr;
    nvinfer1::IExecutionContext* mContext = nullptr;
    cudaStream_t mStream = nullptr;

    void* mGpuBuffers[2] = {nullptr, nullptr};
    size_t mInputSizeBytes = 0;
    size_t mOutputSizeBytes = 0;

    // CPU 模式
    bool mUseCpu = false;
    cv::dnn::Net mCpuNet;
    std::vector<int> mInputDims;
    std::vector<int> mOutputDims;
};

#endif // INFERENCE_ENGINE_H
