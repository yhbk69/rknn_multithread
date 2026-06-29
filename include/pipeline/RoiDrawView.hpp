/*
 * RoiDrawView.hpp - 支持围栏绘制的视频视图
 *
 * 在 QGraphicsView 基础上增加鼠标交互：
 *   - 按住左键拖拽绘制矩形围栏
 *   - 释放时通过回调通知
 *   - 右键点击清除围栏
 */

#ifndef ROI_DRAW_VIEW_HPP
#define ROI_DRAW_VIEW_HPP

#include <QGraphicsView>
#include <QGraphicsRectItem>
#include <QMouseEvent>
#include <QPen>
#include <QBrush>
#include <functional>

class RoiDrawView : public QGraphicsView {
public:
    using RoiCallback = std::function<void(int x, int y, int w, int h)>;
    using ClearCallback = std::function<void()>;

    explicit RoiDrawView(QWidget* parent = nullptr)
        : QGraphicsView(parent) {
        setRenderHint(QPainter::Antialiasing);
        setMouseTracking(true);
        roi_rect_ = new QGraphicsRectItem();
        roi_rect_->setPen(QPen(Qt::red, 2, Qt::DashLine));
        roi_rect_->setBrush(QBrush(QColor(255, 0, 0, 40)));
        roi_rect_->setVisible(false);
        roi_rect_->setZValue(10);
    }

    void setRoiDrawnCallback(RoiCallback cb) { roi_cb_ = std::move(cb); }
    void setRoiClearedCallback(ClearCallback cb) { clear_cb_ = std::move(cb); }

    void clearRoiVisual() {
        roi_rect_->setVisible(false);
    }

protected:
    void mousePressEvent(QMouseEvent* e) override {
        if (e->button() == Qt::LeftButton) {
            drawing_ = true;
            start_pos_ = mapToScene(e->pos()).toPoint();
            roi_rect_->setRect(QRectF(start_pos_, QSize(0, 0)));
            roi_rect_->setVisible(true);
        } else if (e->button() == Qt::RightButton) {
            roi_rect_->setVisible(false);
            if (clear_cb_) clear_cb_();
        }
        QGraphicsView::mousePressEvent(e);
    }

    void mouseMoveEvent(QMouseEvent* e) override {
        if (drawing_) {
            QPoint cur = mapToScene(e->pos()).toPoint();
            QRectF rect(start_pos_, cur);
            roi_rect_->setRect(rect.normalized());
        }
        QGraphicsView::mouseMoveEvent(e);
    }

    void mouseReleaseEvent(QMouseEvent* e) override {
        if (e->button() == Qt::LeftButton && drawing_) {
            drawing_ = false;
            QRectF rect = roi_rect_->rect();
            if (rect.width() > 10 && rect.height() > 10) {
                if (roi_cb_) roi_cb_((int)rect.x(), (int)rect.y(),
                                     (int)rect.width(), (int)rect.height());
            } else {
                roi_rect_->setVisible(false);
                if (clear_cb_) clear_cb_();
            }
        }
        QGraphicsView::mouseReleaseEvent(e);
    }

private:
    QGraphicsRectItem* roi_rect_;
    bool drawing_ = false;
    QPoint start_pos_;
    RoiCallback roi_cb_;
    ClearCallback clear_cb_;
};

#endif // ROI_DRAW_VIEW_HPP
