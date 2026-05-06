#include "../include/inference_engine.h"
#include <fstream>
#include <iostream>

InferenceEngine::InferenceEngine(const std::string& enginePath) {
    if (!loadEngine(enginePath)) {
        throw std::runtime_error("Failed to load TensorRT engine: " + enginePath);
    }
}

InferenceEngine::~InferenceEngine() {
    if (m_stream) cudaStreamDestroy(m_stream);
    for (void* buf : m_gpuBuffers) if (buf) cudaFree(buf);
    if (m_context) delete m_context;
    if (m_engine) delete m_engine;
    if (m_runtime) delete m_runtime;
}

bool InferenceEngine::loadEngine(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.good()) return false;

    file.seekg(0, file.end);
    size_t size = file.tellg();
    file.seekg(0, file.beg);
    std::vector<char> data(size);
    file.read(data.data(), size);

    m_runtime = nvinfer1::createInferRuntime(m_logger);
    m_engine = m_runtime->deserializeCudaEngine(data.data(), size);
    m_context = m_engine->createExecutionContext();
    cudaStreamCreate(&m_stream);

    // 自动获取绑定信息
    for (int i = 0; i < 2; ++i) {
        nvinfer1::Dims dims = m_engine->getBindingDimensions(i);
        size_t vol = 1;
        for (int j = 0; j < dims.nbDims; ++j) vol *= dims.d[j];
        
        size_t bytes = vol * sizeof(float);
        cudaMalloc(&m_gpuBuffers[i], bytes);
        
        if (m_engine->bindingIsInput(i)) {
            m_inputSizeBytes = bytes;
            for(int j=0; j<dims.nbDims; ++j) m_inputDims.push_back(dims.d[j]);
        } else {
            m_outputSizeBytes = bytes;
            for(int j=0; j<dims.nbDims; ++j) m_outputDims.push_back(dims.d[j]);
        }
    }
    return true;
}

bool InferenceEngine::infer(const cv::Mat& frame, std::vector<float>& outputData) {
    // 1. 预处理 (Resize to 640x640, BGR to RGB, Normalize)
    cv::Mat blob = cv::dnn::blobFromImage(frame, 1.0/255.0, cv::Size(m_inputDims[2], m_inputDims[3]), cv::Scalar(0,0,0), true, false);

    // 2. 数据拷贝 (H2D)
    cudaMemcpyAsync(m_gpuBuffers[0], blob.data, m_inputSizeBytes, cudaMemcpyHostToDevice, m_stream);

    // 3. 执行推理
    m_context->enqueueV2(m_gpuBuffers, m_stream, nullptr);

    // 4. 数据拷贝 (D2H)
    outputData.resize(m_outputSizeBytes / sizeof(float));
    cudaMemcpyAsync(outputData.data(), m_gpuBuffers[1], m_outputSizeBytes, cudaMemcpyDeviceToHost, m_stream);

    // 5. 同步流
    cudaStreamSynchronize(m_stream);
    return true;
}