#include <stdio.h>
#include <memory>
#include <sys/time.h>

#include "opencv2/core/core.hpp"
#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgproc/imgproc.hpp"
#include "rkYolov5s.hpp"
#include "rknnPool.hpp"

int main(int argc, char **argv)
{
    char *model_name = NULL;

    // 检查命令行参数: 程序名 + 模型路径 + 视频/摄像头
    if (argc != 3)
    {
        printf("Usage: %s <rknn model> <video/camera> \n", argv[0]);
        return -1;
    }

    // 参数1: 模型所在路径
    model_name = (char *)argv[1];
    // 参数2: 视频文件路径或摄像头序号(0, 1, 2...)
    char *vedio_name = argv[2];

    // 初始化RKNN线程池，默认3个线程
    // 每个线程会加载一个独立的模型实例，充分利用RK3588的3个NPU核心
    int threadNum = 3;
    rknnPool<rkYolov5s, cv::Mat, cv::Mat> testPool(model_name, threadNum);
    if (testPool.init() != 0)
    {
        printf("rknnPool init fail!\n");
        return -1;
    }

    // 创建OpenCV显示窗口
    cv::namedWindow("Camera FPS");

    // 打开视频文件或摄像头
    cv::VideoCapture capture;
    if (strlen(vedio_name) == 1)
        // 如果参数长度为1，认为是摄像头序号(如"0")
        capture.open((int)(vedio_name[0] - '0'));
    else
        // 否则认为是视频文件路径
        capture.open(vedio_name);

    // 记录开始时间，用于计算总平均帧率
    struct timeval time;
    gettimeofday(&time, nullptr);
    auto startTime = time.tv_sec * 1000 + time.tv_usec / 1000;

    int frames = 0;           // 已处理帧数
    auto beforeTime = startTime; // 上一轮帧率统计的起始时间
    double currentFps = 0.0;  // 当前实时帧率

    // 主循环: 读取视频帧并进行目标检测
    while (capture.isOpened())
    {
        cv::Mat img;
        // 读取一帧图像
        if (capture.read(img) == false)
            break;

        // 将图像放入线程池进行异步推理
        // put()是非阻塞的，会立即返回，推理在后台线程中进行
        if (testPool.put(img) != 0)
            break;

        // 等待推理结果
        // 前threadNum帧不需要等待，因为线程池还在填充中
        // get()是阻塞的，会等待直到对应的推理完成
        if (frames >= threadNum && testPool.get(img) != 0)
            break;

        // 每30帧计算一次实时帧率
        if (frames % 30 == 0 && frames > 0)
        {
            gettimeofday(&time, nullptr);
            auto currentTime = time.tv_sec * 1000 + time.tv_usec / 1000;
            currentFps = 30.0 / float(currentTime - beforeTime) * 1000.0;
            beforeTime = currentTime;
        }

        // 在画面左上角显示帧率(绿色文字)
        char fpsText[32];
        snprintf(fpsText, sizeof(fpsText), "FPS: %.2f", currentFps);
        cv::putText(img, fpsText, cv::Point(10, 30), cv::FONT_HERSHEY_SIMPLEX, 1.0, cv::Scalar(0, 255, 0), 2);

        // 显示处理后的图像
        cv::imshow("Camera FPS", img);
        if (cv::waitKey(1) == 'q') // 延时1毫秒，按q键退出
            break;

        frames++;
    }

    // 清空线程池: 获取所有剩余的推理结果
    while (true)
    {
        cv::Mat img;
        if (testPool.get(img) != 0)
            break;
        cv::imshow("Camera FPS", img);
        if (cv::waitKey(1) == 'q')
            break;
        frames++;
    }

    // 计算并打印总平均帧率
    gettimeofday(&time, nullptr);
    auto endTime = time.tv_sec * 1000 + time.tv_usec / 1000;
    printf("Average:\t %f fps/s\n", float(frames) / float(endTime - startTime) * 1000.0);

    return 0;
}
