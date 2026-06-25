#ifndef VIDEO_CELL_HPP
#define VIDEO_CELL_HPP

#include <QWidget>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsPixmapItem>
#include <QLabel>
#include <QPushButton>
#include <QImage>
#include <QSize>

struct VideoCell {
    QWidget* container = nullptr;
    QGraphicsView* view = nullptr;
    QGraphicsScene* scene = nullptr;
    QGraphicsPixmapItem* pixmap_item = nullptr;
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
