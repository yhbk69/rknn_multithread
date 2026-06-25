#ifndef FRAME_DETECTIONS_HPP
#define FRAME_DETECTIONS_HPP

#include <QString>
#include <QVector>

struct FrameDetections {
    int frame_id = 0;
    struct Det {
        QString class_name;
        float confidence = 0;
        int left = 0, top = 0, right = 0, bottom = 0;
    };
    QVector<Det> detections;
};

#endif // FRAME_DETECTIONS_HPP
