#include "SerialPortAssistant.h"
#include <QtWidgets/QApplication>

int main(int argc, char* argv[])
{
    // 启用高DPI缩放支持，防止在高分辨率屏幕上界面缩成一团
#if (QT_VERSION >= QT_VERSION_CHECK(5, 6, 0))
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
#endif

    QApplication a(argc, argv);

    // 设置应用程序的全局字体（可选，防止中文显示为乱码或方块）
    QFont font = a.font();
    font.setFamily("Microsoft YaHei"); // 优先使用微软雅黑
    a.setFont(font);

    SerialPortAssistant w;
    w.show(); // 显示主窗口

    return a.exec(); // 进入 Qt 事件循环
}