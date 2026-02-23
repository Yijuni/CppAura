#include "ImageRecognizer.h"

ImageRecognizer::ImageRecognizer(const std::string& model_path,
    const std::string& label_path)
    : env(ORT_LOGGING_LEVEL_WARNING, "ImageRecognizer")
{
    // 创建会话选项对象，用于配置推理行为。
    Ort::SessionOptions session_options;
    // 设置 单个算子内部并行线程数为 1（适合轻量级或嵌入式场景，避免多线程开销）。
    session_options.SetIntraOpNumThreads(1);
    // 启用 扩展图优化（如算子融合、常量折叠），提升推理速度。
    session_options.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_EXTENDED);

    // 创建 ONNX 模型会话（Ort::Session），加载 .onnx 文件。
    session = std::make_unique<Ort::Session>(env, model_path.c_str(), session_options);

    // 创建默认内存分配器，用于获取输入/输出节点名称等字符串
    allocator = std::make_unique<Ort::AllocatorWithDefaultOptions>();

    // 获取 第 0 个输入/输出节点的名称（大多数图像分类模型只有一个输入和一个输出）
    input_name = session->GetInputNameAllocated(0, *allocator).get();
    output_name = session->GetOutputNameAllocated(0, *allocator).get();

    // 获取输入张量的形状（如 {1, 3, 224, 224}）
    input_shape = session->GetInputTypeInfo(0).GetTensorTypeAndShapeInfo().GetShape();
    // 假设输入格式为 NCHW（Batch, Channel, Height, Width），提取高宽。
    input_height = static_cast<int>(input_shape[2]);
    input_width = static_cast<int>(input_shape[3]);

    // LoadLabels：读取标签文件
    LoadLabels(label_path);
}

//加载标签：就是图片的分类，图片代表了什么
// labels 是你从 .txt 文件加载的类别名列表，比如：cat dog car airplane
void ImageRecognizer::LoadLabels(const std::string& label_path) {
    std::ifstream infile(label_path);
    if (!infile.is_open()) {
        throw std::runtime_error("Failed to open label file: " + label_path);
    }

    std::string line;
    while (std::getline(infile, line)) {
        if (!line.empty()) {
            labels.push_back(line);
        }
    }
    infile.close();

    if (labels.empty()) {
        throw std::runtime_error("No labels loaded from file: " + label_path);
    }
}

std::string ImageRecognizer::PredictFromFile(const std::string& image_path) {
    // 用 OpenCV 读图，失败则抛异常。
    cv::Mat img = cv::imread(image_path);
    if (img.empty()) {
        throw std::runtime_error("Failed to load image: " + image_path);
    }
    // 转交给 PredictFromMat 处理。
    return PredictFromMat(img);
}

// PredictFromBuffer：从内存 buffer（如 HTTP 请求体）预测
std::string ImageRecognizer::PredictFromBuffer(const std::vector<unsigned char>& image_data) {
    // cv::imdecode 从 JPEG/PNG 等编码数据解码为 cv::Mat。
    // 常用于 Web 服务接收图片上传。
    cv::Mat img = cv::imdecode(image_data, cv::IMREAD_COLOR);
    if (img.empty()) {
        throw std::runtime_error("Failed to decode image from buffer");
    }
    return PredictFromMat(img);
}

// 核心推理逻辑
std::string ImageRecognizer::PredictFromMat(const cv::Mat& img_raw) {
    if (img_raw.empty()) {
        throw std::runtime_error("Input image is empty");
    }

    // 缩放图像到模型期望尺寸（如 224x224）。
    cv::Mat img;
    cv::resize(img_raw, img, cv::Size(input_width, input_height));
    // 归一化
    img.convertTo(img, CV_32F, 1.0 / 255.0);

    // NHWC -> NCHW
    // OpenCV 默认图像是 HWC（高×宽×通道），但 ONNX 模型通常期望 CHW（通道×高×宽）
    cv::dnn::blobFromImage(img, img);

    //  输出 img 现在是 CV_32F，尺寸 (1, 3, H, W)，连续内存
    std::vector<int64_t> dims = { 1, 3, input_height, input_width };
    // 显式定义张量维度和元素总数。
    size_t input_tensor_size = 1 * 3 * input_height * input_width;

    // 创建 CPU 内存描述符，告诉 ONNX Runtime 数据在 CPU 上。
    Ort::MemoryInfo memory_info = Ort::MemoryInfo::CreateCpu(
        OrtAllocatorType::OrtArenaAllocator, OrtMemType::OrtMemTypeDefault);
    
    // img.ptr<float>() 直接传递 OpenCV Mat 的内存指针给 ONNX Runtime。
    Ort::Value input_tensor = Ort::Value::CreateTensor<float>(
        memory_info, img.ptr<float>(), input_tensor_size, dims.data(), dims.size());

    // 准备输入/输出节点名称数组（C 风格）。
    const char* input_names[] = { input_name.c_str() };
    const char* output_names[] = { output_name.c_str() };

    // 返回 std::vector<Ort::Value>，包含所有输出张量。
    // 返回 std::vector<Ort::Value>，包含所有输出张量
    auto output_tensors = session->Run(
        Ort::RunOptions{ nullptr },
        input_names, &input_tensor, 1,
        output_names, 1
    );

    // 获取输出 logits 的原始指针（假设输出是 float[N]）。
    float* output_data = output_tensors.front().GetTensorMutableData<float>();

    // 手动实现 argmax：找最大值索引。
    // 如果没加载标签，默认 1000 类（ImageNet）。
    int num_classes = labels.empty() ? 1000 : (int)labels.size();
    // 这个范围内的最大值（概率）的下标（指针）
    int pred_class = std::max_element(output_data, output_data + num_classes) - output_data;

    if (pred_class >= 0 && pred_class < (int)labels.size()) {
        return labels[pred_class];
    }
    else {
        return "Unknown";
    }
}
