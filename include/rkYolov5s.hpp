/*
 * rkYolov5s.hpp - YOLOv5s 模型推理类的定义
 *
 * 本文件定义了 rkYolov5s 类，封装了 YOLOv5s 模型加载、初始化和推理的完整生命周期，
 * 包括 RKNN 模型上下文管理、张量属性配置、多线程安全推理等功能。
 */

#ifndef RKYOLOV5S_H
#define RKYOLOV5S_H

#include "rknn_api.h"

#include "opencv2/core/core.hpp"

/* 辅助函数：打印张量属性信息（用于调试） */
static void dump_tensor_attr(rknn_tensor_attr *attr);
/* 辅助函数：从文件中读取指定偏移和大小的原始数据 */
static unsigned char *load_data(FILE *fp, size_t ofst, size_t sz);
/* 辅助函数：从指定路径加载模型文件到内存，并返回模型数据指针及其大小 */
static unsigned char *load_model(const char *filename, int *model_size);
/* 辅助函数：将浮点型输出数据保存到文件（用于调试） */
static int saveFloat(const char *file_name, float *output, int element_size);

/*
 * rkYolov5s - YOLOv5s 目标检测推理类
 *
 * 封装了 RKNN YOLOv5s 模型从加载、初始化到推理的完整流程，
 * 内部使用互斥锁保证多线程推理时的安全性。
 */
class rkYolov5s
{
private:
    int ret;                  // 通用返回值，用于记录各操作的返回状态
    std::mutex mtx;           // 互斥锁，保证多线程推理安全
    std::string model_path;   // RKNN 模型文件路径
    unsigned char *model_data; // 模型二进制数据（加载到内存中）

    rknn_context ctx;                   // RKNN 推理上下文
    rknn_input_output_num io_num;       // 模型输入输出张量数量
    rknn_tensor_attr *input_attrs;      // 输入张量属性数组
    rknn_tensor_attr *output_attrs;     // 输出张量属性数组
    rknn_input inputs[1];               // 模型输入数据（单输入）

    int channel, width, height;   // 模型输入张量的通道数、宽度和高度
    int img_width, img_height;    // 当前推理图像的实际宽度和高度

    float nms_threshold, box_conf_threshold; // NMS 阈值和边界框置信度阈值

public:
    /* 构造函数：指定模型文件路径，创建推理类实例 */
    rkYolov5s(const std::string &model_path);

    /*
     * 初始化模型：加载模型数据、创建 RKNN 上下文、配置输入输出张量属性
     *
     * @param ctx_in   外部传入的 RKNN 上下文指针（用于共享上下文场景）
     * @param isChild  是否为子线程实例（子线程复用父线程的上下文）
     * @return         成功返回 0，失败返回非零值
     */
    int init(rknn_context *ctx_in, bool isChild);

    /* 获取当前 RKNN 推理上下文的指针（用于子线程共享上下文） */
    rknn_context *get_pctx();

    /*
     * 执行一次目标检测推理
     *
     * @param ori_img  输入原始图像（OpenCV BGR 格式）
     * @return         标注了检测框和类别信息的结果图像
     */
    cv::Mat infer(cv::Mat &ori_img);

    /* 析构函数：释放模型数据、张量属性及 RKNN 上下文等资源 */
    ~rkYolov5s();
};

#endif
