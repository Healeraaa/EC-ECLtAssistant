#include "SerialPortAssistant.h"
#include <QMessageBox>
#include <QTimerEvent>
#include <QDebug> 

SerialPortAssistant::SerialPortAssistant(QWidget* parent) : QMainWindow(parent) {
    this->setWindowTitle(QString::fromUtf8("EC-ECL 电化学工作站"));
    this->setMinimumSize(1280, 900);
    serialPort = new QSerialPort(this);

    initUI();
    setupConnections();
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
    SerialPort_BaudRate->addItems({ "115200", "921600", "2000000", "3000000" });
    SerialPort_BaudRate->setCurrentText("3000000");
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

    Edit_XRange = new QLineEdit("100");
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
    }

    for (int i = 0; i < 6; ++i) {
        Label_Floats[i]->setText(labels[i]);
        // 建议切换模式时重置为最小值或默认值，防止旧数据超出新范围被截断
        Spin_Floats[i]->setValue(Spin_Floats[i]->minimum());
    }
}

void SerialPortAssistant::setupConnections() {
    connect(SerialPort_Connect, &QPushButton::clicked, [=]() { togglePort(true); });
    connect(SerialPort_Disonnect, &QPushButton::clicked, [=]() { togglePort(false); });
    connect(Combo_Mode, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SerialPortAssistant::updateChemLabels);
    connect(SerialPort_Send, &QPushButton::clicked, this, &SerialPortAssistant::sendConfig);
    connect(Btn_ResetPlot, &QPushButton::clicked, this, &SerialPortAssistant::clearAllData);

    connect(serialPort, &QSerialPort::readyRead, [=]() {
        buffer.append(serialPort->readAll());
        processBinaryBuffer();
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
    while (buffer.size() >= 19) {
        // 查找第一个有效的二进制帧头
        int headerPos = -1;
        for (int i = 0; i < buffer.size() - 1; ++i) {
            if (static_cast<uint8_t>(buffer[i]) == 0x55 && 
                static_cast<uint8_t>(buffer[i+1]) == 0xAA) {
                headerPos = i;
                break;
            }
        }
        
        if (headerPos == -1) {
            // 找不到帧头，但只清除明显的垃圾字符
            // 保留最后两个字节以防帧头被分片
            if (buffer.size() > 2) {
                buffer.remove(0, buffer.size() - 2);
            }
            break;
        }
        
        if (headerPos > 0) {
            buffer.remove(0, headerPos);
        }
        
        // 读取帧长
        if (buffer.size() < 4) break;  // 需要至少4字节来读取帧长
        
        uint16_t frameLen = static_cast<uint8_t>(buffer[2]) | (static_cast<uint8_t>(buffer[3]) << 8);
        
        // 帧长合理性检查：最小19字节，最大2048字节
        if (frameLen < 19 ) {
            // 帧长异常，跳过第一个字节重新寻找
            buffer.remove(0, 1);
            continue;
        }
        
        if (buffer.size() < frameLen) {
            // 帧数据未接收完整，等待下一次readyRead
            break;
        }
        
        // 验证帧尾
        if (static_cast<uint8_t>(buffer[frameLen - 2]) != 0x0D || static_cast<uint8_t>(buffer[frameLen - 1]) != 0x0A) {
            // 帧尾错误，说明这不是一个有效的帧头，跳过并继续搜索
            buffer.remove(0, 2);
            continue;
        }
        
        // 验证CRC
        uint16_t calcCRC = calculateCRC16(buffer.left(frameLen - 4));
        uint16_t recvCRC = static_cast<uint8_t>(buffer[frameLen - 4]) | (static_cast<uint8_t>(buffer[frameLen - 3]) << 8);
        if (calcCRC != recvCRC) {
            // CRC错误，跳过并继续搜索
            buffer.remove(0, 2);
            continue;
        }

        // ✅ 数据包完全有效，开始处理
        uint8_t id; uint32_t freq, ts; uint16_t count;
        
        // 手动读取元数据
        id = static_cast<uint8_t>(buffer[4]);
        freq = static_cast<uint8_t>(buffer[5]) | 
               (static_cast<uint8_t>(buffer[6]) << 8) |
               (static_cast<uint8_t>(buffer[7]) << 16) |
               (static_cast<uint8_t>(buffer[8]) << 24);
        ts = static_cast<uint8_t>(buffer[9]) | 
             (static_cast<uint8_t>(buffer[10]) << 8) |
             (static_cast<uint8_t>(buffer[11]) << 16) |
             (static_cast<uint8_t>(buffer[12]) << 24);
        count = static_cast<uint8_t>(buffer[13]) | (static_cast<uint8_t>(buffer[14]) << 8);

        if (CheckBox_EnablePlot->isChecked()) {
            int sIdx = (id == 0x02) ? 1 : 0;
            while (seriesList.size() <= sIdx) {
                QLineSeries* s = new QLineSeries();
                chartView->chart()->addSeries(s);
                s->attachAxis(axisX); s->attachAxis(axisY);
                seriesList.append(s);
            }
            
            // 读取数据点
            int dataPos = 15;
            
            for (int i = 0; i < count; ++i) {
                if (dataPos + 4 > frameLen - 4) {
                    break;
                }
                
                uint32_t rawVal = 0;
                rawVal |= (uint8_t)buffer[dataPos + 0];
                rawVal |= ((uint8_t)buffer[dataPos + 1]) << 8;
                rawVal |= ((uint8_t)buffer[dataPos + 2]) << 16;
                rawVal |= ((uint8_t)buffer[dataPos + 3]) << 24;
                
                float val = *(float*)&rawVal;
                dataPos += 4;
                
                seriesList[sIdx]->append(plotCount++, val);
            }
            
            int xr = Edit_XRange->text().toInt();
            axisX->setRange(qMax(0.0, plotCount - xr), plotCount);
            
            // 应用自定义X轴范围（如果设置了）
            double xMin = Edit_XMin->text().toDouble();
            double xMax = Edit_XMax->text().toDouble();
            if (xMin < xMax) {
                axisX->setRange(xMin, xMax);
            }
            
            // 更新Y轴范围
            double yMin = Edit_YMin->text().toDouble();
            double yMax = Edit_YMax->text().toDouble();
            axisY->setRange(yMin, yMax);
        }
        
        // ✅ 成功处理一帧，移除它
        buffer.remove(0, frameLen);
    }
}

void SerialPortAssistant::togglePort(bool open) {
    if (open) {
        serialPort->setPortName(SerialPort_Number->currentText());
        serialPort->setBaudRate(SerialPort_BaudRate->currentText().toInt());
        if (serialPort->open(QIODevice::ReadWrite)) {
            SerialPort_Connect->setEnabled(false);
            SerialPort_Disonnect->setEnabled(true);
        }
    }
    else {
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