/*
 * mainwindow_style.cpp - Qt 样式表定义
 *
 * 定义整个应用的深色主题样式：
 *   - 背景色：#0a0a0a（纯黑）
 *   - 文字色：#e8e8e8（浅灰）
 *   - 强调色：#3498db（蓝色）
 *   - 按钮：深色背景 + 圆角
 *   - 输入框：深色背景 + 蓝色边框聚焦
 */

#include "rk/mainwindow.hpp"
#include <QApplication>

/**
 * 设置应用全局样式
 * 使用 Qt StyleSheet（类似 CSS）定义所有控件的外观
 */
void MainWindow::setupStyle() {
    qApp->setStyleSheet(R"(
        QMainWindow, QWidget {
            font-family: "Segoe UI", "Noto Sans CJK SC", "WenQuanYi Micro Hei", sans-serif;
            font-size: 13px;
            color: #e8e8e8;
            background-color: #0a0a0a;
        }
        QGroupBox {
            font-weight: bold;
            border: 1px solid #3a3a3a;
            border-radius: 6px;
            margin-top: 14px;
            padding: 14px 10px 10px 10px;
            background-color: #1a1a1a;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            left: 12px;
            padding: 0 8px;
            color: #5dade2;
        }
        QLineEdit {
            border: 1px solid #3a3a3a;
            border-radius: 4px;
            padding: 6px 10px;
            background-color: #141414;
            color: #f5f5f5;
            selection-background-color: #1a5276;
            font-size: 13px;
            min-height: 20px;
        }
        QLineEdit:focus {
            border: 1px solid #3498db;
            background-color: #1a1a1a;
        }
        QLineEdit:disabled {
            background-color: #1a1a1a;
            color: #555555;
        }
        QPushButton {
            border: 1px solid #3a3a3a;
            border-radius: 4px;
            padding: 7px 16px;
            background-color: #252525;
            color: #f0f0f0;
            min-height: 26px;
            font-size: 13px;
        }
        QPushButton:hover {
            background-color: #333333;
            border: 1px solid #4a4a4a;
            color: #ffffff;
        }
        QPushButton:pressed {
            background-color: #1a1a1a;
        }
        QPushButton:disabled {
            background-color: #1a1a1a;
            color: #444444;
        }
        QPushButton#startBtn {
            background-color: #1a5276;
            border-color: #2980b9;
            color: #ffffff;
            font-weight: bold;
        }
        QPushButton#startBtn:hover {
            background-color: #2980b9;
        }
        QPushButton#startBtn:disabled {
            background-color: #1a1a1a;
            color: #444444;
        }
        QPushButton#stopBtn {
            background-color: #922b21;
            border-color: #c0392b;
            color: #ffffff;
            font-weight: bold;
        }
        QPushButton#stopBtn:hover {
            background-color: #c0392b;
        }
        QPushButton#stopBtn:disabled {
            background-color: #1a1a1a;
            color: #444444;
        }
        QSlider::groove:horizontal {
            border: none;
            height: 6px;
            background: #333333;
            border-radius: 3px;
        }
        QSlider::handle:horizontal {
            background: #3498db;
            border: none;
            width: 16px;
            height: 16px;
            margin: -5px 0;
            border-radius: 8px;
        }
        QSlider::handle:horizontal:hover {
            background: #5dade2;
        }
        QSlider::sub-page:horizontal {
            background: #3498db;
            border-radius: 3px;
        }
        QSpinBox {
            border: 1px solid #3a3a3a;
            border-radius: 4px;
            padding: 5px 8px;
            background-color: #141414;
            color: #f5f5f5;
            min-height: 26px;
        }
        QTextEdit {
            border: 1px solid #3a3a3a;
            border-radius: 4px;
            background-color: #0f0f0f;
            color: #e0e0e0;
            font-family: "Cascadia Code", "Consolas", monospace;
            font-size: 12px;
            padding: 6px;
        }
        QProgressBar {
            border: 1px solid #3a3a3a;
            border-radius: 4px;
            background-color: #141414;
            text-align: center;
            color: #e8e8e8;
            min-height: 20px;
            font-size: 12px;
        }
        QProgressBar::chunk {
            background-color: #1a5276;
            border-radius: 3px;
        }
        QStatusBar {
            background-color: #1a1a1a;
            color: #e8e8e8;
            font-size: 12px;
            border-top: 1px solid #3a3a3a;
        }
        QSplitter::handle {
            background-color: #2a2a2a;
        }
        QSplitter::handle:horizontal {
            width: 3px;
        }
        QLabel {
            color: #e0e0e0;
        }
        QLabel#statusValue {
            color: #2ecc71;
            font-weight: bold;
            font-size: 13px;
        }
        QLabel#statusHeader {
            color: #95a5a6;
            font-size: 13px;
        }
        QScrollBar:vertical {
            background-color: #141414;
            width: 10px;
            margin: 0;
        }
        QScrollBar::handle:vertical {
            background-color: #444444;
            min-height: 30px;
            border-radius: 5px;
        }
        QScrollBar::handle:vertical:hover {
            background-color: #555555;
        }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
            height: 0px;
        }
    )");
}
