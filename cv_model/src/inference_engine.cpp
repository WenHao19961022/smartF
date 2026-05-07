#include "../include/inference_engine.h"
#include <fstream>
#include <iostream>

InferenceEngine::InferenceEngine(const std::string& enginePath) {
    if (!LoadEngine(enginePath)) {
        throw std::runtime_error("Failed to load TensorRT engine: " + enginePath);
    }
}

InferenceEngine::~InferenceEngine() {
    // 释放 CUDA 流
    if (m_stream) cudaStreamDestroy(m_stream);
    
    // 释放 GPU 显存
    for (void* buf : m_gpuBuffers) {
        if (buf) cudaFree(buf);
    }

    // TensorRT 8.x+ 弃用了 .destroy()，直接使用 delete 释放接口指针
    if (m_context) delete m_context;
    if (m_engine) delete m_engine;
    if (m_runtime) delete m_runtime;
}

bool InferenceEngine::LoadEngine(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.good()) return false;

    file.seekg(0, file.end);
    size_t size = file.tellg();
    file.seekg(0, file.beg);
    std::vector<char> data(size);
    file.read(data.data(), size);

    m_runtime = nvinfer1::createInferRuntime(m_logger);
    if (!m_runtime) return false;

    m_engine = m_runtime->deserializeCudaEngine(data.data(), size);
    if (!m_engine) return false;

    m_context = m_engine->createExecutionContext();
    if (!m_context) return false;

    cudaStreamCreate(&m_stream);

    // 获取输入输出绑定信息
    // 使用 getNbIOTensors (TRT 8.5+) 替代 getNbBindings (已弃用)
    int nbIO = m_engine->getNbIOTensors();
    
    for (int i = 0; i < nbIO; ++i) {
        const char* name = m_engine->getIOTensorName(i);
        nvinfer1::TensorIOMode mode = m_engine->getTensorIOMode(name);
        nvinfer1::Dims dims = m_engine->getTensorShape(name);
        
        size_t vol = 1;
        for (int j = 0; j < dims.nbDims; ++j) vol *= dims.d[j];
        size_t bytes = vol * sizeof(float);
        
        cudaMalloc(&m_gpuBuffers[i], bytes);
        
        if (mode == nvinfer1::TensorIOMode::kINPUT) {
            m_inputSizeBytes = bytes;
            m_inputDims.clear();
            for(int j = 0; j < dims.nbDims; ++j) m_inputDims.push_back(dims.d[j]);
            // 设置输入张量地址 (V3 模式必需)
            m_context->setTensorAddress(name, m_gpuBuffers[i]);
        } else {
            m_outputSizeBytes = bytes;
            m_outputDims.clear();
            for(int j = 0; j < dims.nbDims; ++j) m_outputDims.push_back(dims.d[j]);
            // 设置输出张量地址 (V3 模式必需)
            m_context->setTensorAddress(name, m_gpuBuffers[i]);
        }
    }
    return true;
}

bool InferenceEngine::Infer(const cv::Mat& frame, std::vector<float>& outputData) {
    if (frame.empty()) return false;

    // 1. 预处理 (Resize to m_inputDims[2]x[3], BGR to RGB, Normalize)
    cv::Mat blob = cv::dnn::blobFromImage(frame, 1.0/255.0, 
                                          cv::Size(m_inputDims[2], m_inputDims[3]), 
                                          cv::Scalar(0,0,0), true, false);

    // 2. 数据拷贝 (H2D)
    cudaMemcpyAsync(m_gpuBuffers[0], blob.data, m_inputSizeBytes, cudaMemcpyHostToDevice, m_stream);

    // 3. 执行推理 (使用 enqueueV3 替代已弃用的 enqueueV2)
    // 注意：enqueueV3 依赖前面 LoadEngine 中 setTensorAddress 的设置
    m_context->enqueueV3(m_stream);

    // 4. 数据拷贝 (D2H)
    outputData.resize(m_outputSizeBytes / sizeof(float));
    cudaMemcpyAsync(outputData.data(), m_gpuBuffers[1], m_outputSizeBytes, cudaMemcpyDeviceToHost, m_stream);

    // 5. 同步流
    cudaStreamSynchronize(m_stream);
    
    return true;
}