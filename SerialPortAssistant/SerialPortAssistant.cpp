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
    int currentFrames = 0;
    while (pos + 19 <= buffer.size()) {
        int headerPos = buffer.indexOf(QByteArray("\x55\xAA"), pos);
        if (headerPos == -1) break;
        
        pos = headerPos;
        
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
        
        uint8_t id = static_cast<uint8_t>(buffer[pos + 4]);
        uint16_t count = static_cast<uint8_t>(buffer[pos + 13]) | 
                         (static_cast<uint8_t>(buffer[pos + 14]) << 8);
        
        if (count > 2048 || count < 100) {
            pos += 2;
            continue;
        }
        
        int dataStartPos = pos + 15;
        int dataEndPos = pos + frameLen - 2;  // 帧尾前
        int floatsCanRead = (dataEndPos - dataStartPos) / 4;
        
        if (floatsCanRead < 1) {
            pos += 2;
            continue;
        }
        
        int sIdx = (id == 0x02) ? 1 : 0;
        while (seriesList.size() <= sIdx) {
            QLineSeries* s = new QLineSeries();
            chartView->chart()->addSeries(s);
            s->attachAxis(axisX); s->attachAxis(axisY);
            seriesList.append(s);
            
            QPen pen;
            pen.setColor(sIdx == 0 ? Qt::blue : Qt::red);
            pen.setWidth(2);
            s->setPen(pen);
        }
        
        int dataPos = pos + 15;
        int floatsToRead = qMin((int)count, floatsCanRead);
        
        // 🔥 优化：先收集新点，避免无限append导致卡顿
        QVector<QPointF> newPoints;
        for (int i = 0; i < floatsToRead; ++i) {
            if (dataPos + 4 > pos + frameLen - 2) break;
             
            uint32_t rawVal = 0;
            rawVal |= (uint8_t)buffer[dataPos + 0];
            rawVal |= ((uint8_t)buffer[dataPos + 1]) << 8;
            rawVal |= ((uint8_t)buffer[dataPos + 2]) << 16;
            rawVal |= ((uint8_t)buffer[dataPos + 3]) << 24;
            
            float val = *(float*)&rawVal;
            
            if (std::isfinite(val) && std::abs(val) <= 100.0) {
                newPoints.append(QPointF(plotCount++, val));
            }
            
            dataPos += 4;
        }
        
        // 🔥 关键：限制series的点数，防止无限增长导致卡顿（最多3000个点）
        int xr = Edit_XRange->text().toInt();
        if (xr <= 0) xr = 100; 
        if (xr > 50000) xr = 50000;  // 最多保留3000个点
        
        // 获取现有点数
        QList<QPointF> allPoints = seriesList[sIdx]->points();
        // 追加新点
        for (const auto& p : newPoints) {
            allPoints.append(p);
        }
        // 只保留最后 xr 个点
        int startIdx = qMax(0, (int)allPoints.size() - xr);
        QList<QPointF> displayPoints = allPoints.mid(startIdx);
        
        // 一次性更新series
        seriesList[sIdx]->clear();
        seriesList[sIdx]->replace(displayPoints);
        
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
        qDebug() << "Frames processed:" << currentFrames 
                 << "| Series points:" << (seriesList.size() > 0 ? seriesList[0]->count() : 0)
                 << "| Total frames:" << frameCount
                 << "| Buffer size:" << buffer.size();
    }
    
    if (pos > 0) {
        buffer.remove(0, pos);
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
            plotCount = 0;        // 重置计数器
            
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
    plotCount = 0;
    for (auto s : seriesList) s->clear();
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