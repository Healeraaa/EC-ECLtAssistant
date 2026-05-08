#include "SerialPortAssistant.h"
#include <QMessageBox>
#include <QTimerEvent>
#include <QDebug>
#include <QTimer>
#include <cmath> 

SerialPortAssistant::SerialPortAssistant(QWidget* parent) : QMainWindow(parent) {
    this->setWindowTitle(QString::fromUtf8("EC-ECL 电化学工作站"));
    this->setMinimumSize(1280, 900);
    serialPort = new QSerialPort(this);

    initUI();
    setupConnections();
    
    // 激进优化：用定时器定期处理缓冲区，而不是在 readyRead 中处理
    QTimer* processTimer = new QTimer(this);
    connect(processTimer, &QTimer::timeout, this, &SerialPortAssistant::processBinaryBuffer);
    processTimer->start(1);  // 改为 1ms，提高处理频率
    
    this->startTimer(1000);
    updatePortList();
}

void SerialPortAssistant::initUI() {
    QWidget* centralWidget = new QWidget(this);
    this->setCentralWidget(centralWidget);
    QHBoxLayout* mainLayout = new QHBoxLayout(centralWidget);
    QVBoxLayout* leftLayout = new QVBoxLayout();

    // 1. 接收区与图表 (左侧)
    SerialPort_ReceiveAear = new QPlainTextEdit();
    SerialPort_ReceiveAear->setReadOnly(true);
    leftLayout->addWidget(new QLabel(QString::fromUtf8("接收数据:")), 0);
    leftLayout->addWidget(SerialPort_ReceiveAear, 2);

    QChart* chart = new QChart();
    axisX = new QValueAxis(); axisX->setRange(0, 100);
    axisY = new QValueAxis(); axisY->setRange(-2, 2);
    chart->addAxis(axisX, Qt::AlignBottom);
    chart->addAxis(axisY, Qt::AlignLeft);
    chartView = new QChartView(chart);
    leftLayout->addWidget(chartView, 5);

    ScrollBar_X = new QScrollBar(Qt::Horizontal);
    ScrollBar_X->setVisible(false);
    leftLayout->addWidget(ScrollBar_X);

    // 2. 参数配置区 (右侧)
    QVBoxLayout* rightLayout = new QVBoxLayout();
    QWidget* configWidget = new QWidget();
    configWidget->setFixedWidth(350);
    QGridLayout* grid = new QGridLayout(configWidget);

    int row = 0;
    SerialPort_Number = new QComboBox();
    SerialPort_BaudRate = new QComboBox();
    SerialPort_BaudRate->addItems({ "115200", "921600", "2000000", "3000000","600000" });
    SerialPort_BaudRate->setCurrentText("600000");
    grid->addWidget(new QLabel(QString::fromUtf8("端口:")), row, 0);
    grid->addWidget(SerialPort_Number, row++, 1);

    grid->addWidget(new QLabel(QString::fromUtf8("--- 硬件配置 ---")), row++, 0, 1, 2);

    Combo_Mode = new QComboBox();
    Combo_Mode->addItems({ QString::fromUtf8("CV (循环伏安)"), QString::fromUtf8("DPV (差分脉冲)") });
    grid->addWidget(new QLabel(QString::fromUtf8("测试模式:")), row, 0);
    grid->addWidget(Combo_Mode, row++, 1);

    // 初始化硬件参数下拉框[cite: 16]
    QStringList configLabels = { QString::fromUtf8("通道:"), QString::fromUtf8("IV增益:"),
                                 QString::fromUtf8("电压增益1:"), QString::fromUtf8("电压增益2:") };
    for (int i = 0; i < 4; ++i) {
        Combo_Configs[i] = new QComboBox();
        grid->addWidget(new QLabel(configLabels[i]), row, 0);
        grid->addWidget(Combo_Configs[i], row++, 1);
    }

    // 设置下拉框具体内容与对应数值[cite: 16]
    Combo_Configs[0]->addItem("Channel1", 2);
    Combo_Configs[0]->addItem("Channel2", 3);
    Combo_Configs[0]->addItem("Channel3", 1);
    Combo_Configs[0]->addItem("Channel4", 0);

    Combo_Configs[1]->addItem("Gain1K", 0);
    Combo_Configs[1]->addItem("Gain100K", 1);
    Combo_Configs[1]->addItem("Gain1M", 2);
    Combo_Configs[1]->addItem("Gain100M", 3);

    Combo_Configs[2]->addItem("Gain5X", 0);
    Combo_Configs[2]->addItem("Gain20X", 1);

    Combo_Configs[3]->addItem("Gain1X", 0);
    Combo_Configs[3]->addItem("Gain5X", 1);
    Combo_Configs[3]->addItem("Gain20X", 2);
    Combo_Configs[3]->addItem("Gain50X", 3);

    grid->addWidget(new QLabel(QString::fromUtf8("--- 参数设置 ---")), row++, 0, 1, 2);
    for (int i = 0; i < 6; ++i) {
        Label_Floats[i] = new QLabel();
        Spin_Floats[i] = new QDoubleSpinBox();
        Spin_Floats[i]->setRange(-10.0, 10.0);
        Spin_Floats[i]->setDecimals(4);
        grid->addWidget(Label_Floats[i], row, 0);
        grid->addWidget(Spin_Floats[i], row++, 1);
    }

    Edit_XRange = new QLineEdit("50000");
    grid->addWidget(new QLabel(QString::fromUtf8("波形显示范围:")), row, 0);
    grid->addWidget(Edit_XRange, row++, 1);

    Edit_XMin = new QLineEdit("0");
    Edit_XMax = new QLineEdit("100");
    grid->addWidget(new QLabel(QString::fromUtf8("X Min Range:")), row, 0);
    grid->addWidget(Edit_XMin, row++, 1);
    grid->addWidget(new QLabel(QString::fromUtf8("X Max Range:")), row, 0);
    grid->addWidget(Edit_XMax, row++, 1);

    Edit_YMin = new QLineEdit("-2");
    Edit_YMax = new QLineEdit("2");
    grid->addWidget(new QLabel(QString::fromUtf8("Y Min Range:")), row, 0);
    grid->addWidget(Edit_YMin, row++, 1);
    grid->addWidget(new QLabel(QString::fromUtf8("Y Max Range:")), row, 0);
    grid->addWidget(Edit_YMax, row++, 1);

    CheckBox_EnablePlot = new QCheckBox(QString::fromUtf8("启用波形"));
    CheckBox_EnablePlot->setChecked(true);
    CheckBox_SaveCSV = new QCheckBox(QString::fromUtf8("保存CSV"));
    grid->addWidget(CheckBox_EnablePlot, row++, 0);
    grid->addWidget(CheckBox_SaveCSV, row++, 0);

    rightLayout->addWidget(configWidget);
    rightLayout->addStretch();

    SerialPort_Connect = new QPushButton(QString::fromUtf8("打开设备"));
    SerialPort_Disonnect = new QPushButton(QString::fromUtf8("关闭设备"));
    SerialPort_Send = new QPushButton(QString::fromUtf8("下发配置"));
    Btn_ResetPlot = new QPushButton(QString::fromUtf8("重置图表"));
    QPushButton* Btn_ApplyXAxis = new QPushButton("Apply X Range");
    QPushButton* Btn_ApplyYAxis = new QPushButton("Apply Y Range");

    rightLayout->addWidget(SerialPort_Connect);
    rightLayout->addWidget(SerialPort_Disonnect);
    rightLayout->addWidget(SerialPort_Send);
    rightLayout->addWidget(Btn_ResetPlot);
    rightLayout->addWidget(Btn_ApplyXAxis);
    rightLayout->addWidget(Btn_ApplyYAxis);
    
    connect(Btn_ApplyXAxis, &QPushButton::clicked, [this]() {
        double xMin = Edit_XMin->text().toDouble();
        double xMax = Edit_XMax->text().toDouble();
        if (xMin < xMax) {
            axisX->setRange(xMin, xMax);
        }
    });
    
    connect(Btn_ApplyYAxis, &QPushButton::clicked, [this]() {
        double yMin = Edit_YMin->text().toDouble();
        double yMax = Edit_YMax->text().toDouble();
        if (yMin < yMax) {
            axisY->setRange(yMin, yMax);
        }
    });

    mainLayout->addLayout(leftLayout, 1);
    mainLayout->addLayout(rightLayout, 0);
    updateChemLabels(0);
}

void SerialPortAssistant::updateChemLabels(int index) {
    QStringList labels;
    if (index == 0) { // CV (循环伏安) 模式
        labels << "初始电位(mV)" << "终止电位(mV)" << "扫描极限1(mV)" << "扫描极限2(mV)" << "扫描速率(mV/s)" << "循环次数";

        // 为 CV 模式设置不同的上下限 (示例值)
        Spin_Floats[0]->setRange(-5000.0, 5000.0); // 初始电位
        Spin_Floats[1]->setRange(-5000.0, 5000.0); // 终止电位
        Spin_Floats[2]->setRange(-5000.0, 5000.0); // 扫描极限1
        Spin_Floats[3]->setRange(-5000.0, 5000.0); // 扫描极限2
        Spin_Floats[4]->setRange(0, 2000.0);     // 扫描速率
        Spin_Floats[5]->setRange(1.0, 1000.0);     // 循环次数

        // 设置 CV 模式的默认参数
        Spin_Floats[0]->setValue(0.0);      // 初始电位 = 0
        Spin_Floats[1]->setValue(0.0);      // 终止电位 = 0
        Spin_Floats[2]->setValue(500.0);    // 扫描极限1 = 500
        Spin_Floats[3]->setValue(-500.0);   // 扫描极限2 = -500
        Spin_Floats[4]->setValue(30.0);     // 扫描速率 = 30
        Spin_Floats[5]->setValue(3.0);      // 循环次数 = 3
    }
    else { // DPV (差分脉冲) 模式
        labels << "初始电位(mV)" << "终止电位(mV)" << "步进电位(mV)" << "脉冲幅度(mV)" << "脉冲宽度(ms)" << "脉冲周期(ms)";

        // 为 DPV 模式设置不同的上下限 (示例值)
        Spin_Floats[0]->setRange(-5000.0, 5000.0); // 初始电位
        Spin_Floats[1]->setRange(-5000.0, 5000.0); // 终止电位
        Spin_Floats[2]->setRange(1.0, 1000.0);      // 步进电位
        Spin_Floats[3]->setRange(1.0, 5000.0);      // 脉冲幅度
        Spin_Floats[4]->setRange(1.0, 10000.0);     // 脉冲宽度
        Spin_Floats[5]->setRange(10.0, 10000.0);    // 脉冲周期

        // 设置 DPV 模式的默认参数
        Spin_Floats[0]->setValue(Spin_Floats[0]->minimum());
        Spin_Floats[1]->setValue(Spin_Floats[1]->minimum());
        Spin_Floats[2]->setValue(Spin_Floats[2]->minimum());
        Spin_Floats[3]->setValue(Spin_Floats[3]->minimum());
        Spin_Floats[4]->setValue(Spin_Floats[4]->minimum());
        Spin_Floats[5]->setValue(Spin_Floats[5]->minimum());
    }

    for (int i = 0; i < 6; ++i) {
        Label_Floats[i]->setText(labels[i]);
    }
}

void SerialPortAssistant::setupConnections() {
    connect(SerialPort_Connect, &QPushButton::clicked, [=]() { togglePort(true); });
    connect(SerialPort_Disonnect, &QPushButton::clicked, [=]() { togglePort(false); });
    connect(Combo_Mode, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SerialPortAssistant::updateChemLabels);
    connect(SerialPort_Send, &QPushButton::clicked, this, &SerialPortAssistant::sendConfig);
    connect(Btn_ResetPlot, &QPushButton::clicked, this, &SerialPortAssistant::clearAllData);

    // 激进优化：不在 readyRead 中处理，只是追加数据
    connect(serialPort, &QSerialPort::readyRead, [=]() {
        buffer.append(serialPort->readAll());
    });
}

void SerialPortAssistant::sendConfig() {
    if (!serialPort->isOpen()) return;

    // 修改处：使用 currentData().toInt() 获取下拉框对应的实际数值[cite: 16]
    QString dLine = QString("ceiod:%1,%2,%3,%4,%5\n")
        .arg(Combo_Mode->currentIndex())
        .arg(Combo_Configs[0]->currentData().toInt())
        .arg(Combo_Configs[1]->currentData().toInt())
        .arg(Combo_Configs[2]->currentData().toInt())
        .arg(Combo_Configs[3]->currentData().toInt());

    QString fLine = "ceiof:";
    for (int i = 0; i < 6; ++i) {
        fLine += QString::number(Spin_Floats[i]->value(), 'f', 6);
        if (i < 5) fLine += ",";
    }
    fLine += "\n";

    QByteArray packet = (dLine + fLine).toLocal8Bit();
    serialPort->write(packet);
    SerialPort_ReceiveAear->appendPlainText(QString::fromUtf8("[发送配置] -> ") + dLine.trimmed() + " | " + fLine.trimmed());
}

uint16_t SerialPortAssistant::calculateCRC16(const QByteArray& data) {
    uint16_t crc = 0xFFFF;
    for (int i = 0; i < data.size(); i++) {
        crc ^= (uint16_t)((uint8_t)data[i]) << 8;
        for (int j = 0; j < 8; j++) {
            if (crc & 0x8000) crc = (crc << 1) ^ 0x1021;
            else crc = crc << 1;
        }
    }
    return crc;
}

void SerialPortAssistant::processBinaryBuffer() {
    // 保留原版本供兼容性
    processBufferOptimized();
}

// 激进优化版本：批量处理、减少 GUI 更新
void SerialPortAssistant::processBufferOptimized() {
    if (buffer.size() < 19 || !CheckBox_EnablePlot->isChecked()) return;
    
    int pos = 0;
    static int frameCount = 0;
    static bool firstFrameLogged = false;
    int currentFrames = 0;
    while (pos + 19 <= buffer.size()) {
        int headerPos = buffer.indexOf(QByteArray("\x55\xAA"), pos);
        if (headerPos == -1) break;
        
        pos = headerPos;
        
        // 🔥 诊断：第一个帧头位置
        if (!firstFrameLogged) {
            qDebug() << ">>> FIRST FRAME HEADER at pos:" << pos << "bufferSize:" << buffer.size();
            firstFrameLogged = true;
        }
        
        // 🔥 直接从数据包中读取frameLen（小端法）
        if (pos + 4 > buffer.size()) break;  // 确保能读到frameLen字段
        
        uint16_t frameLen = static_cast<uint8_t>(buffer[pos + 2]) | 
                            (static_cast<uint8_t>(buffer[pos + 3]) << 8);
        
        // 验证frameLen的合理性
        if (frameLen < 19 || frameLen > 8192) {
            qDebug() << "Invalid frameLen:" << frameLen << "at pos" << pos;
            pos += 2;
            continue;
        }
        
        // 检查缓冲区是否有完整的一帧
        if (pos + frameLen > buffer.size()) {
            qDebug() << "Incomplete frame: need" << frameLen << "bytes, but only" << (buffer.size() - pos) << "available";
            break;
        }
        
        uint8_t tail1 = static_cast<uint8_t>(buffer[pos + frameLen - 2]);
        uint8_t tail2 = static_cast<uint8_t>(buffer[pos + frameLen - 1]);
        // 验证帧尾（应该是0x0D 0x0A）
        if (tail1 != 0x0D || tail2 != 0x0A) {
            qDebug() << "Bad frame tail at pos" << pos << ":" << QString::number(tail1, 16) << QString::number(tail2, 16);
            pos += 2;
            continue;
        }
        
        // 🔥 CRC校验：计算从帧头到数据负载的所有数据（frameLen - 4 字节）
        QByteArray frameData = buffer.mid(pos, frameLen - 4);
        uint16_t calculatedCRC = calculateCRC16(frameData);
        // 读取帧中存储的CRC（小端序：低字节在前）
        uint16_t frameCRC = static_cast<uint8_t>(buffer[pos + frameLen - 4]) | 
                            (static_cast<uint8_t>(buffer[pos + frameLen - 3]) << 8);
        if (calculatedCRC != frameCRC) {
            // 诊断：输出详细帧结构信息
            QString headerHex = QString::number((uint8_t)buffer[pos], 16).rightJustified(2, '0') + " " +
                               QString::number((uint8_t)buffer[pos+1], 16).rightJustified(2, '0') + " " +
                               QString::number((uint8_t)buffer[pos+2], 16).rightJustified(2, '0') + " " +
                               QString::number((uint8_t)buffer[pos+3], 16).rightJustified(2, '0');
            QString crcBytesHex = QString::number((uint8_t)buffer[pos + frameLen - 4], 16).rightJustified(2, '0') + " " +
                                 QString::number((uint8_t)buffer[pos + frameLen - 3], 16).rightJustified(2, '0');
            
            qDebug() << "\n🔴 CRC check FAILED at pos" << pos 
                     << "\n  frameLen=" << frameLen
                     << "\n  calculated CRC=" << QString::number(calculatedCRC, 16)
                     << "stored CRC=" << QString::number(frameCRC, 16)
                     << "\n  Header bytes:" << headerHex
                     << "\n  CRC bytes:" << crcBytesHex
                     << "\n  Frame data size for CRC calc:" << frameData.size();
            pos += 2;
            continue;
        }
        
        // 🔥 根据新协议读取字段
        uint8_t id = static_cast<uint8_t>(buffer[pos + 4]);
        
        // 读取采样频率 (Fs) - 位于 pos+5~8，小端序 uint32_t
        uint32_t fs = static_cast<uint8_t>(buffer[pos + 5]) |
                      (static_cast<uint8_t>(buffer[pos + 6]) << 8) |
                      (static_cast<uint8_t>(buffer[pos + 7]) << 16) |
                      (static_cast<uint8_t>(buffer[pos + 8]) << 24);
        
        // 读取数据量 (N) - 位于 pos+13~14，小端序 uint16_t
        uint16_t count = static_cast<uint8_t>(buffer[pos + 13]) | 
                         (static_cast<uint8_t>(buffer[pos + 14]) << 8);
        
        if (count > 2048 || count < 1 || fs == 0) {
            qDebug() << "Invalid frame: count=" << count << "fs=" << fs;
            pos += 2;
            continue;
        }
        
        // 🔥 初始化基础采样率（从第一个有效帧获取）
        if (baseSamplingRate == 0) {
            baseSamplingRate = fs;
            qDebug() << "Base sampling rate set to:" << baseSamplingRate << "Hz";
        }
        
        int dataStartPos = pos + 15;
        int dataEndPos = pos + frameLen - 4;  // CRC之前
        int floatsCanRead = (dataEndPos - dataStartPos) / 4;
        
        if (floatsCanRead < 1) {
            pos += 2;
            continue;
        }
        
        // 根据ID确定需要多少条曲线
        int seriesCount = (id == 0x01) ? 2 : (id == 0x02 ? 3 : 1);
        
        // 确保有足够的曲线
        while (seriesList.size() < seriesCount) {
            QLineSeries* s = new QLineSeries();
            chartView->chart()->addSeries(s);
            s->attachAxis(axisX); s->attachAxis(axisY);
            seriesList.append(s);
            
            QPen pen;
            if (seriesList.size() == 1) pen.setColor(Qt::blue);      // 第一条曲线 - 蓝色
            else if (seriesList.size() == 2) pen.setColor(Qt::red);  // 第二条曲线 - 红色
            else if (seriesList.size() == 3) pen.setColor(Qt::green); // 第三条曲线 - 绿色
            pen.setWidth(2);
            s->setPen(pen);
        }
        
        int dataPos = pos + 15;
        int floatsToRead = qMin((int)count, floatsCanRead);
        
        // 🔥 收集新点，根据ID类型处理不同的数据
        QVector<QPointF> newPoints[3];  // 最多3条曲线
        
        if (id == 0x01) {
            // ID=0x01: IV数据 - 分离电压和电流
            // 数据格式: 电压、电流、电压、电流...
            // 每对(电压+电流)对应一个采样点，使用全局基础采样率计算时间
            int pairsInThisFrame = count / 2;
            for (int pairIdx = 0; pairIdx < pairsInThisFrame; ++pairIdx) {
                // 🔥 使用全局计数器计算时间，确保连续性
                uint64_t globalPairIdx = globalSamplePairCount + pairIdx;
                double timeX = (double)globalPairIdx / baseSamplingRate;
                
                // 读取电压数据（偶数索引）
                if (dataPos + 4 <= dataEndPos) {
                    uint32_t rawVal = 0;
                    rawVal |= (uint8_t)buffer[dataPos + 0];
                    rawVal |= ((uint8_t)buffer[dataPos + 1]) << 8;
                    rawVal |= ((uint8_t)buffer[dataPos + 2]) << 16;
                    rawVal |= ((uint8_t)buffer[dataPos + 3]) << 24;
                    
                    float voltageVal = *(float*)&rawVal;
                    if (std::isfinite(voltageVal)) {
                        newPoints[0].append(QPointF(timeX, voltageVal));
                    }
                    dataPos += 4;
                }
                
                // 读取电流数据（奇数索引）
                if (dataPos + 4 <= dataEndPos) {
                    uint32_t rawVal = 0;
                    rawVal |= (uint8_t)buffer[dataPos + 0];
                    rawVal |= ((uint8_t)buffer[dataPos + 1]) << 8;
                    rawVal |= ((uint8_t)buffer[dataPos + 2]) << 16;
                    rawVal |= ((uint8_t)buffer[dataPos + 3]) << 24;
                    
                    float currentVal = *(float*)&rawVal;
                    if (std::isfinite(currentVal)) {
                        newPoints[1].append(QPointF(timeX, currentVal));
                    }
                    dataPos += 4;
                }
            }
            // 🔥 更新全局对计数器
            globalSamplePairCount += pairsInThisFrame;
        } else if (id == 0x02) {
            // ID=0x02: 光数据 - 单条曲线，X轴基于全局基础采样率
            for (int i = 0; i < floatsToRead; ++i) {
                if (dataPos + 4 > dataEndPos) break;
                
                uint32_t rawVal = 0;
                rawVal |= (uint8_t)buffer[dataPos + 0];
                rawVal |= ((uint8_t)buffer[dataPos + 1]) << 8;
                rawVal |= ((uint8_t)buffer[dataPos + 2]) << 16;
                rawVal |= ((uint8_t)buffer[dataPos + 3]) << 24;
                
                float val = *(float*)&rawVal;
                
                if (std::isfinite(val)) {
                    // 🔥 使用全局计数器和基础采样率计算时间，确保与0x01在同一时间轴
                    uint64_t globalSampleIdx = globalOpticalSampleCount + i;
                    double timeX = (double)globalSampleIdx / baseSamplingRate;
                    newPoints[2].append(QPointF(timeX, val));
                }
                
                dataPos += 4;
            }
            // 🔥 更新全局光采样计数器
            globalOpticalSampleCount += floatsToRead;
        } else {
            // 其他ID：默认处理为单条曲线
            for (int i = 0; i < floatsToRead; ++i) {
                if (dataPos + 4 > dataEndPos) break;
                
                uint32_t rawVal = 0;
                rawVal |= (uint8_t)buffer[dataPos + 0];
                rawVal |= ((uint8_t)buffer[dataPos + 1]) << 8;
                rawVal |= ((uint8_t)buffer[dataPos + 2]) << 16;
                rawVal |= ((uint8_t)buffer[dataPos + 3]) << 24;
                
                float val = *(float*)&rawVal;
                
                if (std::isfinite(val)) {
                    // 使用全局基础采样率计算时间
                    uint64_t globalSampleIdx = globalOpticalSampleCount + i;
                    double timeX = (double)globalSampleIdx / baseSamplingRate;
                    newPoints[0].append(QPointF(timeX, val));
                }
                
                dataPos += 4;
            }
            // 更新全局计数器
            globalOpticalSampleCount += floatsToRead;
        }
        
        // 🔥 关键：限制series的点数，防止无限增长导致卡顿
        int xr = Edit_XRange->text().toInt();
        if (xr <= 0) xr = 100; 
        if (xr > 50000) xr = 50000;
        
        // 更新所有活跃的曲线
        for (int sIdx = 0; sIdx < seriesCount && sIdx < 3; ++sIdx) {
            if (newPoints[sIdx].isEmpty()) continue;
            
            // 获取现有点数
            QList<QPointF> allPoints = seriesList[sIdx]->points();
            // 追加新点
            for (const auto& p : newPoints[sIdx]) {
                allPoints.append(p);
            }
            // 只保留最后 xr 个点
            int startIdx = qMax(0, (int)allPoints.size() - xr);
            QList<QPointF> displayPoints = allPoints.mid(startIdx);
            
            // 一次性更新series
            seriesList[sIdx]->clear();
            seriesList[sIdx]->replace(displayPoints);
        }
        
        currentFrames++;
        pos += frameLen;
    }
    
    // 设置轴范围
    double xMin = Edit_XMin->text().toDouble();
    double xMax = Edit_XMax->text().toDouble();
    if (xMin < xMax) {
        axisX->setRange(xMin, xMax);
    }
    
    double yMin = Edit_YMin->text().toDouble();
    double yMax = Edit_YMax->text().toDouble();
    if (yMin < yMax) {
        axisY->setRange(yMin, yMax);
    }
    
    // 🔥 强制刷新图表
    chartView->chart()->update();
    chartView->update();
    
    // 诊断：打印性能信息
    frameCount += currentFrames;
    static int diagCount = 0;
    if (diagCount++ % 10 == 0) {
        int xr = Edit_XRange->text().toInt();
        if (xr <= 0) xr = 100;
        
        int usedPoints = (seriesList.size() > 0) ? seriesList[0]->count() : 0;
        int usedPercent = (xr > 0) ? (usedPoints * 100 / xr) : 0;
        
        qDebug() << "Frames processed:" << currentFrames 
                 << "| Series points:" << usedPoints
                 << "| X-Range capacity:" << xr
                 << "| Used:" << usedPercent << "%"
                 << "| Total frames:" << frameCount
                 << "| Buffer size:" << buffer.size();
        
        // 🔥 在接收区显示波形使用情况（每500帧显示一次）
        static int statusCount = 0;
        if (statusCount++ % 5 == 0) {
            // 使用英文显示，避免中文乱码问题
            SerialPort_ReceiveAear->appendPlainText(
                QString("[Waveform Status] Used: %1/%2 points (%3%%) | Frames: %4 | 0x01 Pairs: %5 | 0x02 Samples: %6")
                    .arg(usedPoints)
                    .arg(xr)
                    .arg(usedPercent)
                    .arg(frameCount)
                    .arg(globalSamplePairCount)
                    .arg(globalOpticalSampleCount)
            );
        }
    }
    
    if (pos > 0) {
        buffer.remove(0, pos);
    } else if (buffer.size() > 0) {
        // 🔥 如果缓冲区前 2 字节不是帧头，说明前面有垃圾数据，清除它
        int firstValidHeader = buffer.indexOf(QByteArray("\x55\xAA"));
        if (firstValidHeader > 0) {
            qDebug() << "Removing garbage data:" << firstValidHeader << "bytes before first valid header";
            buffer.remove(0, firstValidHeader);
        }
    }
}

void SerialPortAssistant::togglePort(bool open) {
    if (open) {
        serialPort->setPortName(SerialPort_Number->currentText());
        serialPort->setBaudRate(SerialPort_BaudRate->currentText().toInt());
        
        // 🔥 激进优化：增加接收缓冲区到 4 MB（3000000 baud 下有 ~10秒 缓冲）
        serialPort->setReadBufferSize(4 * 1024 * 1024);
        
        if (serialPort->open(QIODevice::ReadWrite)) {
            // 清空所有旧数据
            serialPort->clear();  // 清空串口缓冲区
            buffer.clear();       // 清空本地缓冲区
            
            // 清空所有曲线数据
            for (auto s : seriesList) {
                s->clear();
            }
            
            // 🔥 重置全局时间跟踪
            baseSamplingRate = 0;
            globalSamplePairCount = 0;
            globalOpticalSampleCount = 0;
            
            SerialPort_Connect->setEnabled(false);
            SerialPort_Disonnect->setEnabled(true);
        }
    }
    else {
        // 🔥 关闭前强制处理缓冲区中的最后一帧
        if (buffer.size() > 19) {
            qDebug() << "Flushing buffer before close: size=" << buffer.size() << "bytes";
            
            // 临时启用绘图，确保能处理最后的数据
            bool wasEnabled = CheckBox_EnablePlot->isChecked();
            if (!wasEnabled) {
                CheckBox_EnablePlot->setChecked(true);
            }
            
            // 再处理一次，尝试处理最后的数据
            processBinaryBuffer();
            
            // 恢复原状
            if (!wasEnabled) {
                CheckBox_EnablePlot->setChecked(false);
            }
            
            // 清空剩余数据
            if (buffer.size() > 0) {
                qDebug() << "Remaining in buffer after flush:" << buffer.size() << "bytes (discarded)";
            }
            buffer.clear();
        }
        
        serialPort->close();
        SerialPort_Connect->setEnabled(true);
        SerialPort_Disonnect->setEnabled(false);
    }
}

void SerialPortAssistant::clearAllData() {
    for (auto s : seriesList) s->clear();
    
    // 🔥 重置全局时间跟踪，使下次数据从x=0开始
    baseSamplingRate = 0;
    globalSamplePairCount = 0;
    globalOpticalSampleCount = 0;
    
    SerialPort_ReceiveAear->appendPlainText(QString::fromUtf8("[系统] 图表已重置，全局计数器已清零"));
}

void SerialPortAssistant::updatePortList() {
    QList<QSerialPortInfo> infos = QSerialPortInfo::availablePorts();
    if (infos.size() != lastPortList.size()) {
        SerialPort_Number->clear();
        for (const auto& info : infos) SerialPort_Number->addItem(info.portName());
        lastPortList.clear();
        for (const auto& info : infos) lastPortList << info.portName();
    }
}

void SerialPortAssistant::timerEvent(QTimerEvent*) { updatePortList(); }
SerialPortAssistant::~SerialPortAssistant() {}