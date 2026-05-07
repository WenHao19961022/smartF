#include "../include/inference_engine.h"
#include <fstream>
#include <iostream>

InferenceEngine::InferenceEngine(const std::string& enginePath) {
    if (!LoadEngine(enginePath)) {
        throw std::runtime_error("Failed to load TensorRT engine: " + enginePath);
    }
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

bool InferenceEngine::Infer(const cv::Mat& frame, std::vector<float>& outputData) {
    if (frame.empty()) return false;

    cv::Mat blob = cv::dnn::blobFromImage(frame, 1.0 / 255.0,
        cv::Size(mInputDims[2], mInputDims[3]),
        cv::Scalar(0, 0, 0), true, false);

    cudaMemcpyAsync(mGpuBuffers[0], blob.data, mInputSizeBytes, cudaMemcpyHostToDevice, mStream);
    mContext->enqueueV3(mStream);

    outputData.resize(mOutputSizeBytes / sizeof(float));
    cudaMemcpyAsync(outputData.data(), mGpuBuffers[1], mOutputSizeBytes, cudaMemcpyDeviceToHost, mStream);
    cudaStreamSynchronize(mStream);

    return true;
}
