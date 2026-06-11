/*
 * preprocess.h - YOLOv5 图像预处理模块的头文件
 *
 * 本文件定义了 YOLOv5 模型推理前的图像预处理相关函数接口，
 * 包括 Letterbox 等比缩放填充和 RGA 硬件加速缩放等操作。
 */

#ifndef _RKNN_YOLOV5_DEMO_PREPROCESS_H_
#define _RKNN_YOLOV5_DEMO_PREPROCESS_H_

#include <stdio.h>
#include "im2d.h"
#include "rga.h"
#include "opencv2/core/core.hpp"
#include "opencv2/imgcodecs.hpp"
#include "opencv2/imgproc.hpp"
#include "postprocess.h"

/*
 * Letterbox 图像预处理：保持宽高比的等比缩放并填充灰色边框，避免图像变形。
 *
 * @param image        输入原始图像
 * @param padded_image 输出填充后的图像
 * @param pads         输出填充边距信息（左、右、上、下各填充了多少像素）
 * @param scale        等比缩放的比例因子
 * @param target_size  目标尺寸（宽 x 高）
 * @param pad_color    填充区域的颜色，默认为灰色 (128, 128, 128)
 */
void letterbox(const cv::Mat &image, cv::Mat &padded_image, BOX_RECT &pads, const float scale, const cv::Size &target_size, const cv::Scalar &pad_color = cv::Scalar(128, 128, 128));

/*
 * 使用 RK3588 RGA 硬件加速进行图像缩放，比 CPU 软缩放快很多。
 *
 * @param src           RGA 源缓冲区
 * @param dst           RGA 目标缓冲区
 * @param image         输入原始图像（OpenCV Mat 格式）
 * @param resized_image 输出缩放后的图像（OpenCV Mat 格式）
 * @param target_size   目标尺寸（宽 x 高）
 * @return              成功返回 0，失败返回非零值
 */
int resize_rga(rga_buffer_t &src, rga_buffer_t &dst, const cv::Mat &image, cv::Mat &resized_image, const cv::Size &target_size);

#endif //_RKNN_YOLOV5_DEMO_PREPROCESS_H_
