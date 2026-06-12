/*
 * config.h - 编译时常量配置
 * 
 * 存放编译时确定的固定常量（缓冲区大小、数组维度等）
 * 运行时参数（阈值、路径、线程数等）请使用 config.json
 */

#ifndef CONFIG_H
#define CONFIG_H

/* 后处理相关常量 */
#define OBJ_NAME_MAX_SIZE 16        // 目标类别名称的最大长度（字节）
#define OBJ_NUMB_MAX_SIZE 64        // 单帧最多可检测到的目标数量上限
#define OBJ_CLASS_NUM 80            // 目标类别总数（COCO 数据集共 80 类）
#define PROP_BOX_SIZE (5 + OBJ_CLASS_NUM)  // 每个检测框的属性数量

/* 默认配置路径 */
#define DEFAULT_CONFIG_PATH "config.json"

/* 显示相关 */
#define FPS_TEXT_MAX_SIZE 32        // FPS 文字缓冲区大小
#define FPS_UPDATE_INTERVAL 30      // 每 N 帧更新一次 FPS

#endif /* CONFIG_H */
