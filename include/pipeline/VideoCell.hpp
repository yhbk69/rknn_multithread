/*
 * VideoCell.hpp - 视频显示单元
 *
 * 封装四路视频网格中的单个通道：
 *   - QGraphicsView + QGraphicsScene 用于显示帧
 *   - QLabel 覆盖层显示 FPS
 *   - QPushButton 放大/缩小
 *   - QGraphicsRectItem ROI 围栏叠加层
 */

#ifndef VIDEO_CELL_HPP
#define VIDEO_CELL_HPP

#include <QWidget>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsPixmapItem>
#include <QGraphicsRectItem>
#include <QLabel>
#include <QPushButton>
#include <QImage>
#include <QSize>

struct VideoCell {
    QWidget* container = nullptr;
    QGraphicsView* view = nullptr;
    QGraphicsScene* scene = nullptr;
    QGraphicsPixmapItem* pixmap_item = nullptr;
    QGraphicsRectItem* roi_item = nullptr;       // ROI 围栏叠加层
    QLabel* overlay = nullptr;
    QPushButton* zoom_in_btn = nullptr;
    QPushButton* zoom_out_btn = nullptr;
    QLabel* channel_label = nullptr;
    QImage last_frame;
    double last_fps = 0.0;
    QSize last_pix_size;
    long long last_fps_text_update = 0;
};

#endif // VIDEO_CELL_HPP
