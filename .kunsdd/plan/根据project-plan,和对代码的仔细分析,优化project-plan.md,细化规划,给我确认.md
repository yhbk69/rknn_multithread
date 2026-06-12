---

## 当前状态总结

**已交付**：Windows 端完整可用的 YOLO 检测 Qt 应用

| 功能 | 状态 |
|------|------|
| 加载 ONNX 模型 | ✅ |
| USB 摄像头实时检测 | ✅ |
| 视频文件检测 | ✅ 限 12fps 防崩溃 |
| 图片单张检测 | ✅ |
| 工具栏：打开图片/视频/摄像头 | ✅ 使用 .ui 自带 action |
| 菜单栏：File → 打开/加载/退出 | ✅ |
| 置信度/NMS 滑块 | ✅ |
| 彩色日志 | ✅ |
| FPS/耗时显示 | ✅ |

**下一步：Phase 3 — WebSocket + MJPEG 推流**

5 个模块需要移植，每个改动很小（改 include 路径 + 去掉 TensorRT 引用），全部可以在 Windows 上测试：
- 浏览器打开 `http://localhost:9093` 看实时 MJPEG 画面
- WebSocket 连接 `ws://localhost:9090` 远程控制

要开始吗？