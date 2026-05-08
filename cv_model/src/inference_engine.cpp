#include "../include/inference_engine.h"
#include "../../common/include/logger.h"
#include <fstream>
#include <iostream>
#include <chrono>

InferenceEngine::InferenceEngine(const std::string& enginePath) {
    // 检查 CUDA 是否可用
    int cudaDeviceCount = 0;
    cudaError_t err = cudaGetDeviceCount(&cudaDeviceCount);

    if (err == cudaSuccess && cudaDeviceCount > 0) {
        LOG_PRINT("[TRT]", "CUDA available, attempting GPU inference (device count: " << cudaDeviceCount << ")");
        if (LoadEngine(enginePath)) {
            LOG_PRINT("[TRT]", "TensorRT engine loaded successfully on GPU.");
            return;  // GPU 模式加载成功
        }
        LOG_PRINT("[TRT]", "TensorRT GPU load failed, falling back to CPU mode.");
    } else {
        LOG_PRINT("[TRT]", "CUDA error " << err << ", falling back to CPU mode.");
    }

    // CPU 模式：尝试加载 ONNX 模型（从 enginePath 推断 onnx 路径）
    // 例如 /path/to/yolo12n.engine -> /path/to/yolo12n.onnx
    std::string onnxPath = enginePath;
    size_t pos = onnxPath.rfind(".engine");
    if (pos != std::string::npos) {
        onnxPath = onnxPath.substr(0, pos) + ".onnx";
    }

    LOG_PRINT("[TRT]", "Loading CPU ONNX model: " << onnxPath);
    if (LoadCpuOnnx(onnxPath)) {
        mUseCpu = true;
        LOG_PRINT("[TRT]", "CPU ONNX model loaded successfully.");
        return;
    }

    // 最终失败：抛异常让上层捕获
    throw std::runtime_error("Failed to load model (no GPU and no ONNX found): " + onnxPath);
}

InferenceEngine::~InferenceEngine() {
    if (mStream) cudaStreamDestroy(mStream);
    for (void* buf : mGpuBuffers) {
        if (buf) cudaFree(buf);
    }
    if (mContext) delete mContext;
    if (mEngine) delete mEngine;
    if (mRuntime) delete mRuntime;
}

bool InferenceEngine::LoadEngine(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.good()) return false;

    file.seekg(0, file.end);
    size_t size = file.tellg();
    file.seekg(0, file.beg);
    std::vector<char> data(size);
    file.read(data.data(), size);

    mRuntime = nvinfer1::createInferRuntime(mLogger);
    if (!mRuntime) return false;

    mEngine = mRuntime->deserializeCudaEngine(data.data(), size);
    if (!mEngine) return false;

    mContext = mEngine->createExecutionContext();
    if (!mContext) return false;

    cudaStreamCreate(&mStream);

    int nbIO = mEngine->getNbIOTensors();
    for (int i = 0; i < nbIO; ++i) {
        const char* name = mEngine->getIOTensorName(i);
        nvinfer1::TensorIOMode mode = mEngine->getTensorIOMode(name);
        nvinfer1::Dims dims = mEngine->getTensorShape(name);

        size_t vol = 1;
        for (int j = 0; j < dims.nbDims; ++j) vol *= dims.d[j];
        size_t bytes = vol * sizeof(float);

        cudaMalloc(&mGpuBuffers[i], bytes);

        if (mode == nvinfer1::TensorIOMode::kINPUT) {
            mInputSizeBytes = bytes;
            mInputDims.clear();
            for (int j = 0; j < dims.nbDims; ++j) mInputDims.push_back(dims.d[j]);
            mContext->setTensorAddress(name, mGpuBuffers[i]);
        } else {
            mOutputSizeBytes = bytes;
            mOutputDims.clear();
            for (int j = 0; j < dims.nbDims; ++j) mOutputDims.push_back(dims.d[j]);
            mContext->setTensorAddress(name, mGpuBuffers[i]);
        }
    }
    return true;
}

bool InferenceEngine::LoadCpuOnnx(const std::string& path) {
    try {
        mCpuNet = cv::dnn::readNet(path);
        if (mCpuNet.empty()) return false;
        mCpuNet.setPreferableBackend(cv::dnn::DNN_BACKEND_OPENCV);
        mCpuNet.setPreferableTarget(cv::dnn::DNN_TARGET_CPU);
        return true;
    } catch (const std::exception& e) {
        LOG_PRINT("[TRT]", "LoadCpuOnnx failed: " << e.what());
        return false;
    }
}

bool InferenceEngine::Infer(const cv::Mat& frame, std::vector<float>& outputData) {
    if (frame.empty()) {
        LOG_PRINT("[TRT]", "Infer() called with empty frame");
        return false;
    }

    auto inferStart = std::chrono::steady_clock::now();

    if (mUseCpu) {
        bool ret = InferCpu(frame, outputData);
        auto inferEnd = std::chrono::steady_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(inferEnd - inferStart).count();
        LOG_PRINT("[TRT]", "[CPU] Infer done in " << ms << "ms | Frame: " << frame.cols << "x" << frame.rows
                  << " | Output size: " << outputData.size());
        return ret;
    }

    // GPU 模式
    cv::Mat blob = cv::dnn::blobFromImage(frame, 1.0 / 255.0,
        cv::Size(mInputDims[2], mInputDims[3]),
        cv::Scalar(0, 0, 0), true, false);

    LOG_PRINT("[TRT]", "[GPU] Infer start | Frame: " << frame.cols << "x" << frame.rows
              << " -> Blob: " << mInputDims[2] << "x" << mInputDims[3]
              << " | InputBytes: " << mInputSizeBytes << " OutputBytes: " << mOutputSizeBytes);

    cudaMemcpyAsync(mGpuBuffers[0], blob.data, mInputSizeBytes, cudaMemcpyHostToDevice, mStream);
    mContext->enqueueV3(mStream);

    outputData.resize(mOutputSizeBytes / sizeof(float));
    cudaMemcpyAsync(outputData.data(), mGpuBuffers[1], mOutputSizeBytes, cudaMemcpyDeviceToHost, mStream);
    cudaStreamSynchronize(mStream);

    auto inferEnd = std::chrono::steady_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(inferEnd - inferStart).count();
    LOG_PRINT("[TRT]", "[GPU] Infer done in " << ms << "ms | Output values: " << outputData.size()
              << " OutputDims: [" << (mOutputDims.size() > 0 ? mOutputDims[0] : 0)
              << "," << (mOutputDims.size() > 1 ? mOutputDims[1] : 0)
              << "," << (mOutputDims.size() > 2 ? mOutputDims[2] : 0) << "]");

    return true;
}

bool InferenceEngine::InferCpu(const cv::Mat& frame, std::vector<float>& outputData) {
    try {
        cv::Mat blob = cv::dnn::blobFromImage(frame, 1.0 / 255.0,
            cv::Size(640, 640),
            cv::Scalar(0, 0, 0), true, false);

        mCpuNet.setInput(blob);
        cv::Mat prob = mCpuNet.forward();

        outputData.resize(prob.total());
        memcpy(outputData.data(), prob.data, prob.total() * sizeof(float));
        return true;
    } catch (const std::exception& e) {
        LOG_PRINT("[TRT]", "InferCpu failed: " << e.what());
        return false;
    }
}