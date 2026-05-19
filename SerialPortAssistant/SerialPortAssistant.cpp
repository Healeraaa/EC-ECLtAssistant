#include "SerialPortAssistant.h"
#include <QMessageBox>
#include <QTimerEvent>
#include <cmath>

SerialPortAssistant::SerialPortAssistant(QWidget* parent) : QMainWindow(parent) {
    this->setWindowTitle(QString::fromUtf8("EC-ECL Recorder"));
    this->setMinimumSize(1280, 900);
    serialPort = new QSerialPort(this);

    initUI();
    setupConnections();

    QTimer* processTimer = new QTimer(this);
    connect(processTimer, &QTimer::timeout, this, &SerialPortAssistant::processBinaryBuffer);
    processTimer->start(1);

    // CSV 缓冲区定时刷新（每秒写一次文件）
    m_csvFlushTimer = new QTimer(this);
    connect(m_csvFlushTimer, &QTimer::timeout, this, &SerialPortAssistant::flushCSVBuffer);

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
    leftLayout->addWidget(new QLabel(QString::fromUtf8("调试数据:")), 0);
    leftLayout->addWidget(SerialPort_ReceiveAear, 2);

    QChart* chart = new QChart();
    // 开启图例
    chart->legend()->setVisible(true);
    chart->legend()->setAlignment(Qt::AlignBottom);

    axisX = new QValueAxis(); axisX->setRange(0, 100);
    axisY = new QValueAxis(); axisY->setRange(-2, 2);
    axisYRight = new QValueAxis();
    axisYRight->setRange(-2, 2);
    chart->addAxis(axisX, Qt::AlignBottom);
    chart->addAxis(axisY, Qt::AlignLeft);
    chart->addAxis(axisYRight, Qt::AlignRight);
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
    Combo_Mode->addItems({ QString::fromUtf8("CV (循环伏安)"), QString::fromUtf8("DPV (差分脉冲)"), QString::fromUtf8("CA (计时电流)") });
    grid->addWidget(new QLabel(QString::fromUtf8("测试模式:")), row, 0);
    grid->addWidget(Combo_Mode, row++, 1);

    // 通道选择
    Combo_Configs[0] = new QComboBox();
    grid->addWidget(new QLabel(QString::fromUtf8("通道:")), row, 0);
    grid->addWidget(Combo_Configs[0], row++, 1);

    Combo_Configs[0]->addItem("Channel1", 2);
    Combo_Configs[0]->addItem("Channel2", 3);
    Combo_Configs[0]->addItem("Channel3", 1);
    Combo_Configs[0]->addItem("Channel4", 0);

    // 🔥 新的范围选择下拉框，替代原有的 IV增益、电压增益1、电压增益2
    Combo_Range = new QComboBox();
    Combo_Range->addItem("100 nA");
    Combo_Range->addItem("330 nA");
    Combo_Range->addItem("1 uA");
    Combo_Range->addItem("3.3 uA");
    Combo_Range->addItem("10 uA");
    Combo_Range->addItem("33 uA");
    Combo_Range->addItem("100 uA");
    Combo_Range->addItem("330 uA");
    Combo_Range->addItem("1 mA");
    Combo_Range->addItem("3.3 mA");
    Combo_Range->addItem("10 mA");
    Combo_Range->addItem("30.3 mA");
    Combo_Range->addItem("100 mA");
    grid->addWidget(new QLabel(QString::fromUtf8("测量范围:")), row, 0);
    grid->addWidget(Combo_Range, row++, 1);
    
    // 保留原有的 Combo_Configs[1], [2], [3] 但隐藏，用于内部存储
    for (int i = 1; i < 4; ++i) {
        Combo_Configs[i] = new QComboBox();
        Combo_Configs[i]->setVisible(false);
    }
    
    // 初始化 IV增益选项
    Combo_Configs[1]->addItem("Gain33", 0);
    Combo_Configs[1]->addItem("Gain1K", 1);
    Combo_Configs[1]->addItem("Gain10K", 2);
    Combo_Configs[1]->addItem("Gain100K", 3);

    // 初始化 电压增益1 选项
    Combo_Configs[2]->addItem("Gain1X", 0);
    Combo_Configs[2]->addItem("Gain10X", 1);

    // 初始化 电压增益2 选项
    Combo_Configs[3]->addItem("Gain1X", 0);
    Combo_Configs[3]->addItem("Gain3.3X", 1);
    Combo_Configs[3]->addItem("Gain10X", 2);
    Combo_Configs[3]->addItem("Gain33X", 3);
    
    // 设置初始值
    Combo_Configs[1]->setCurrentIndex(3);  // Gain100K
    Combo_Configs[2]->setCurrentIndex(1);  // Gain10X
    Combo_Configs[3]->setCurrentIndex(3);  // Gain33X

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
    grid->addWidget(new QLabel(QString::fromUtf8("Y-Left Min:")), row, 0);
    grid->addWidget(Edit_YMin, row++, 1);
    grid->addWidget(new QLabel(QString::fromUtf8("Y-Left Max:")), row, 0);
    grid->addWidget(Edit_YMax, row++, 1);

    Edit_YRightMin = new QLineEdit("-2");
    Edit_YRightMax = new QLineEdit("2");
    grid->addWidget(new QLabel(QString::fromUtf8("Y-Right Min:")), row, 0);
    grid->addWidget(Edit_YRightMin, row++, 1);
    grid->addWidget(new QLabel(QString::fromUtf8("Y-Right Max:")), row, 0);
    grid->addWidget(Edit_YRightMax, row++, 1);

    CheckBox_EnablePlot = new QCheckBox(QString::fromUtf8("启用波形"));
    CheckBox_EnablePlot->setChecked(true);
    CheckBox_SaveCSV = new QCheckBox(QString::fromUtf8("保存CSV"));
    CheckBox_SaveCSV->setChecked(true);
    grid->addWidget(CheckBox_EnablePlot, row++, 0);
    grid->addWidget(CheckBox_SaveCSV, row++, 0);

    rightLayout->addWidget(configWidget);
    rightLayout->addStretch();

    SerialPort_Connect = new QPushButton(QString::fromUtf8("打开设备"));
    SerialPort_Disonnect = new QPushButton(QString::fromUtf8("关闭设备"));
    SerialPort_Send = new QPushButton(QString::fromUtf8("下发配置"));
    Btn_ResetPlot = new QPushButton(QString::fromUtf8("重置图表"));
    QPushButton* Btn_ApplyXAxis = new QPushButton("Apply X Range");
    QPushButton* Btn_ApplyYAxis = new QPushButton("Apply Y Left");
    QPushButton* Btn_ApplyYRight = new QPushButton("Apply Y Right");

    rightLayout->addWidget(SerialPort_Connect);
    rightLayout->addWidget(SerialPort_Disonnect);
    rightLayout->addWidget(SerialPort_Send);
    rightLayout->addWidget(Btn_ResetPlot);
    rightLayout->addWidget(Btn_ApplyXAxis);
    rightLayout->addWidget(Btn_ApplyYAxis);
    rightLayout->addWidget(Btn_ApplyYRight);

    connect(Btn_ApplyXAxis, &QPushButton::clicked, [this]() {
        double xMin = Edit_XMin->text().toDouble();
        double xMax = Edit_XMax->text().toDouble();
        if (xMin < xMax) axisX->setRange(xMin, xMax);
        });
    connect(Btn_ApplyYAxis, &QPushButton::clicked, [this]() {
        double yMin = Edit_YMin->text().toDouble();
        double yMax = Edit_YMax->text().toDouble();
        if (yMin < yMax) axisY->setRange(yMin, yMax);
        });
    connect(Btn_ApplyYRight, &QPushButton::clicked, [this]() {
        double yMin = Edit_YRightMin->text().toDouble();
        double yMax = Edit_YRightMax->text().toDouble();
        if (yMin < yMax) axisYRight->setRange(yMin, yMax);
        });

    mainLayout->addLayout(leftLayout, 1);
    mainLayout->addLayout(rightLayout, 0);
    updateChemLabels(0);
}

void SerialPortAssistant::updateChemLabels(int index) {
    QStringList labels;
    if (index == 0) {
        labels << QString::fromUtf8("初始电位(mV)") << QString::fromUtf8("终止电位(mV)") << QString::fromUtf8("扫描极限1(mV)") << QString::fromUtf8("扫描极限2(mV)") << QString::fromUtf8("扫描速率(mV/s)") << QString::fromUtf8("循环次数");
        Spin_Floats[0]->setRange(-5000.0, 5000.0);
        Spin_Floats[1]->setRange(-5000.0, 5000.0);
        Spin_Floats[2]->setRange(-5000.0, 5000.0);
        Spin_Floats[3]->setRange(-5000.0, 5000.0);
        Spin_Floats[4]->setRange(0, 2000.0);
        Spin_Floats[5]->setRange(1.0, 1000.0);
        Spin_Floats[0]->setValue(0.0);
        Spin_Floats[1]->setValue(0.0);
        Spin_Floats[2]->setValue(500.0);
        Spin_Floats[3]->setValue(-500.0);
        Spin_Floats[4]->setValue(30.0);
        Spin_Floats[5]->setValue(3.0);
    }
    else if (index == 1) {
        labels << QString::fromUtf8("初始电位(mV)") << QString::fromUtf8("终止电位(mV)") << QString::fromUtf8("步进电位(mV)") << QString::fromUtf8("脉冲幅度(mV)") << QString::fromUtf8("脉冲宽度(ms)") << QString::fromUtf8("脉冲周期(ms)");
        Spin_Floats[0]->setRange(-5000.0, 5000.0);
        Spin_Floats[1]->setRange(-5000.0, 5000.0);
        Spin_Floats[2]->setRange(1.0, 1000.0);
        Spin_Floats[3]->setRange(1.0, 5000.0);
        Spin_Floats[4]->setRange(1.0, 10000.0);
        Spin_Floats[5]->setRange(10.0, 10000.0);
        Spin_Floats[0]->setValue(Spin_Floats[0]->minimum());
        Spin_Floats[1]->setValue(Spin_Floats[1]->minimum());
        Spin_Floats[2]->setValue(Spin_Floats[2]->minimum());
        Spin_Floats[3]->setValue(Spin_Floats[3]->minimum());
        Spin_Floats[4]->setValue(Spin_Floats[4]->minimum());
        Spin_Floats[5]->setValue(Spin_Floats[5]->minimum());
    }
    else if (index == 2) {
        labels << QString::fromUtf8("初始电位(mV)") << QString::fromUtf8("初始电位时间(s)") << QString::fromUtf8("阶跃1电位(mV)") << QString::fromUtf8("阶跃1时间(s)") << QString::fromUtf8("阶跃2电位(mV)") << QString::fromUtf8("阶跃2时间(s)");
        Spin_Floats[0]->setRange(-5000.0, 5000.0);
        Spin_Floats[1]->setRange(0.0, 1000.0);
        Spin_Floats[2]->setRange(-5000.0, 5000.0);
        Spin_Floats[3]->setRange(0.0, 10000.0);
        Spin_Floats[4]->setRange(-5000.0, 5000.0);
        Spin_Floats[5]->setRange(0.0, 10000.0);
        Spin_Floats[0]->setValue(0.0);
        Spin_Floats[1]->setValue(3.0);
        Spin_Floats[2]->setValue(1000.0);
        Spin_Floats[3]->setValue(1.0);
        Spin_Floats[4]->setValue(0.0);
        Spin_Floats[5]->setValue(3.0);
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
    connect(serialPort, &QSerialPort::readyRead, [=]() {
        buffer.append(serialPort->readAll());
        });
    // 动态启停 CSV 记录
    connect(CheckBox_SaveCSV, &QCheckBox::toggled, this, &SerialPortAssistant::onSaveCSVToggled);
}

void SerialPortAssistant::sendConfig() {
    if (!serialPort->isOpen()) return;

    // 🔥 根据 Combo_Range 选择，自动设置对应的 IV增益、电压增益1、电压增益2
    int rangeIdx = Combo_Range->currentIndex();
    
    // 映射表：每个范围对应 [IV增益索引, 电压增益1索引, 电压增益2索引]
    const int rangeMapping[13][3] = {
        // 100 nA
        {3, 1, 3},  // Gain100K, Gain10X, Gain33X
        // 330 nA
        {3, 1, 2},  // Gain100K, Gain10X, Gain10X
        // 1 uA
        {3, 1, 1},  // Gain100K, Gain10X, Gain3.3X
        // 3.3 uA
        {3, 0, 2},  // Gain100K, Gain1X, Gain10X
        // 10 uA
        {3, 0, 1},  // Gain100K, Gain1X, Gain3.3X
        // 33 uA
        {3, 0, 0},  // Gain100K, Gain1X, Gain1X
        // 100 uA
        {2, 1, 1},  // Gain10K, Gain10X, Gain3.3X
        // 330 uA
        {2, 0, 0},  // Gain10K, Gain1X, Gain1X
        // 1 mA
        {1, 0, 1},  // Gain1K, Gain1X, Gain3.3X
        // 3.3 mA
        {1, 0, 0},  // Gain1K, Gain1X, Gain1X
        // 10 mA
        {0, 0, 2},  // Gain33, Gain1X, Gain10X
        // 30.3 mA
        {0, 0, 1},  // Gain33, Gain1X, Gain3.3X
        // 100 mA
        {0, 0, 0}   // Gain33, Gain1X, Gain1X
    };
    
    // 设置对应的值
    if (rangeIdx >= 0 && rangeIdx < 13) {
        Combo_Configs[1]->setCurrentIndex(rangeMapping[rangeIdx][0]);
        Combo_Configs[2]->setCurrentIndex(rangeMapping[rangeIdx][1]);
        Combo_Configs[3]->setCurrentIndex(rangeMapping[rangeIdx][2]);
    }

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
    SerialPort_ReceiveAear->appendPlainText(QString::fromUtf8("[Send Config] Mode=") + QString::number(Combo_Mode->currentIndex()) 
        + QString::fromUtf8(" Range=") + Combo_Range->currentText() 
        + QString::fromUtf8(" (IV:") + Combo_Configs[1]->currentText()
        + QString::fromUtf8(" V1:") + Combo_Configs[2]->currentText()
        + QString::fromUtf8(" V2:") + Combo_Configs[3]->currentText() + ")");
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
    processBufferOptimized();
}

void SerialPortAssistant::processBufferOptimized() {
    if (buffer.size() < 19 || !CheckBox_EnablePlot->isChecked()) return;

    int pos = 0;
    static int frameCount = 0;
    int currentFrames = 0;
    while (pos + 19 <= buffer.size()) {
        int headerPos = buffer.indexOf(QByteArray("\x55\xAA"), pos);
        if (headerPos == -1) break;
        pos = headerPos;

        if (pos + 4 > buffer.size()) break;

        uint16_t frameLen = static_cast<uint8_t>(buffer[pos + 2]) |
            (static_cast<uint8_t>(buffer[pos + 3]) << 8);
        if (frameLen < 19 || frameLen > 8192) {
            pos += 2;
            continue;
        }
        if (pos + frameLen > buffer.size()) break;

        uint8_t tail1 = static_cast<uint8_t>(buffer[pos + frameLen - 2]);
        uint8_t tail2 = static_cast<uint8_t>(buffer[pos + frameLen - 1]);
        if (tail1 != 0x0D || tail2 != 0x0A) {
            pos += 2;
            continue;
        }

        QByteArray frameData = buffer.mid(pos, frameLen - 4);
        uint16_t calculatedCRC = calculateCRC16(frameData);
        uint16_t frameCRC = static_cast<uint8_t>(buffer[pos + frameLen - 4]) |
            (static_cast<uint8_t>(buffer[pos + frameLen - 3]) << 8);
        if (calculatedCRC != frameCRC) {
            pos += 2;
            continue;
        }

        uint8_t id = static_cast<uint8_t>(buffer[pos + 4]);
        uint32_t fs = static_cast<uint8_t>(buffer[pos + 5]) |
            (static_cast<uint8_t>(buffer[pos + 6]) << 8) |
            (static_cast<uint8_t>(buffer[pos + 7]) << 16) |
            (static_cast<uint8_t>(buffer[pos + 8]) << 24);
        uint16_t count = static_cast<uint8_t>(buffer[pos + 13]) |
            (static_cast<uint8_t>(buffer[pos + 14]) << 8);
        if (count > 2048 || count < 1 || fs == 0) {
            pos += 2;
            continue;
        }

        if (baseSamplingRate == 0) {
            baseSamplingRate = fs;
        }

        int dataEndPos = pos + frameLen - 4;
        int floatsCanRead = (dataEndPos - (pos + 15)) / 4;
        if (floatsCanRead < 1) {
            pos += 2;
            continue;
        }

        int seriesCount = (id == 0x01) ? 2 : (id == 0x02 ? 3 : 1);
        while (seriesList.size() < seriesCount) {
            QLineSeries* s = new QLineSeries();
            chartView->chart()->addSeries(s);
            s->attachAxis(axisX);
            if (seriesList.size() == 2 && seriesCount == 3) {
                s->attachAxis(axisYRight);
            }
            else {
                s->attachAxis(axisY);
            }
            QPen pen;
            if (seriesList.size() == 0) {
                pen.setColor(Qt::blue);
                s->setName("Voltage");
            }
            else if (seriesList.size() == 1) {
                pen.setColor(Qt::red);
                s->setName("Current");
            }
            else {
                pen.setColor(Qt::green);
                s->setName("Optical");
            }
            pen.setWidth(2);
            s->setPen(pen);
            seriesList.append(s);
        }

        int dataPos = pos + 15;
        int floatsToRead = qMin((int)count, floatsCanRead);
        QVector<QPointF> newPoints[3];

        QVector<double> csvTimes;
        QVector<double> csvV;
        QVector<double> csvI;
        QVector<double> csvO;

        if (id == 0x01) {
            int pairsInThisFrame = count / 2;
            for (int pairIdx = 0; pairIdx < pairsInThisFrame; ++pairIdx) {
                uint64_t globalPairIdx = globalSamplePairCount + pairIdx;
                double timeX = (double)globalPairIdx / baseSamplingRate;

                if (dataPos + 4 <= dataEndPos) {
                    uint32_t rawVal = (uint8_t)buffer[dataPos] |
                        ((uint8_t)buffer[dataPos + 1]) << 8 |
                        ((uint8_t)buffer[dataPos + 2]) << 16 |
                        ((uint8_t)buffer[dataPos + 3]) << 24;
                    float voltageVal = *(float*)&rawVal;
                    if (std::isfinite(voltageVal)) {
                        newPoints[0].append(QPointF(timeX, voltageVal));
                        csvTimes.append(timeX);
                        csvV.append(voltageVal);
                        csvI.append(qQNaN());
                        csvO.append(qQNaN());
                    }
                    dataPos += 4;
                }
                else break;

                if (dataPos + 4 <= dataEndPos) {
                    uint32_t rawVal = (uint8_t)buffer[dataPos] |
                        ((uint8_t)buffer[dataPos + 1]) << 8 |
                        ((uint8_t)buffer[dataPos + 2]) << 16 |
                        ((uint8_t)buffer[dataPos + 3]) << 24;
                    float currentVal = *(float*)&rawVal;
                    if (std::isfinite(currentVal)) {
                        newPoints[1].append(QPointF(timeX, currentVal));
                        csvI.last() = currentVal;
                    }
                    dataPos += 4;
                }
                else break;
            }
            globalSamplePairCount += pairsInThisFrame;
        }
        else if (id == 0x02) {
            for (int i = 0; i < floatsToRead; ++i) {
                if (dataPos + 4 > dataEndPos) break;
                uint32_t rawVal = (uint8_t)buffer[dataPos] |
                    ((uint8_t)buffer[dataPos + 1]) << 8 |
                    ((uint8_t)buffer[dataPos + 2]) << 16 |
                    ((uint8_t)buffer[dataPos + 3]) << 24;
                float val = *(float*)&rawVal;
                if (std::isfinite(val)) {
                    uint64_t globalSampleIdx = globalOpticalSampleCount + i;
                    double timeX = (double)globalSampleIdx / baseSamplingRate;
                    newPoints[2].append(QPointF(timeX, val));
                    csvTimes.append(timeX);
                    csvV.append(qQNaN());
                    csvI.append(qQNaN());
                    csvO.append(val);
                }
                dataPos += 4;
            }
            globalOpticalSampleCount += floatsToRead;
        }
        else {
            for (int i = 0; i < floatsToRead; ++i) {
                if (dataPos + 4 > dataEndPos) break;
                uint32_t rawVal = (uint8_t)buffer[dataPos] |
                    ((uint8_t)buffer[dataPos + 1]) << 8 |
                    ((uint8_t)buffer[dataPos + 2]) << 16 |
                    ((uint8_t)buffer[dataPos + 3]) << 24;
                float val = *(float*)&rawVal;
                if (std::isfinite(val)) {
                    uint64_t globalSampleIdx = globalOpticalSampleCount + i;
                    double timeX = (double)globalSampleIdx / baseSamplingRate;
                    newPoints[0].append(QPointF(timeX, val));
                    csvTimes.append(timeX);
                    csvV.append(val);
                    csvI.append(qQNaN());
                    csvO.append(qQNaN());
                }
                dataPos += 4;
            }
            globalOpticalSampleCount += floatsToRead;
        }

        if (m_isSaving && !csvTimes.isEmpty()) {
            for (int i = 0; i < csvTimes.size(); ++i) {
                double t = csvTimes[i];
                QString vStr = qIsNaN(csvV[i]) ? "" : QString::number(csvV[i], 'f', 6);
                QString iStr = qIsNaN(csvI[i]) ? "" : QString::number(csvI[i], 'f', 6);
                QString oStr = qIsNaN(csvO[i]) ? "" : QString::number(csvO[i], 'f', 6);
                m_csvBuffer.append(QString("%1,%2,%3,%4")
                    .arg(t, 0, 'f', 6)
                    .arg(vStr)
                    .arg(iStr)
                    .arg(oStr));
            }
        }

        int xr = Edit_XRange->text().toInt();
        if (xr <= 0) xr = 100;
        if (xr > 50000) xr = 50000;
        for (int sIdx = 0; sIdx < seriesCount && sIdx < 3; ++sIdx) {
            if (newPoints[sIdx].isEmpty()) continue;
            QList<QPointF> allPoints = seriesList[sIdx]->points();
            for (const auto& p : newPoints[sIdx]) allPoints.append(p);
            int startIdx = qMax(0, (int)allPoints.size() - xr);
            QList<QPointF> displayPoints = allPoints.mid(startIdx);
            seriesList[sIdx]->clear();
            seriesList[sIdx]->replace(displayPoints);
        }

        currentFrames++;
        pos += frameLen;
    }

    double xMin = Edit_XMin->text().toDouble();
    double xMax = Edit_XMax->text().toDouble();
    if (xMin < xMax) axisX->setRange(xMin, xMax);
    double yMin = Edit_YMin->text().toDouble();
    double yMax = Edit_YMax->text().toDouble();
    if (yMin < yMax) axisY->setRange(yMin, yMax);
    double yRMin = Edit_YRightMin->text().toDouble();
    double yRMax = Edit_YRightMax->text().toDouble();
    if (yRMin < yRMax) axisYRight->setRange(yRMin, yRMax);

    chartView->chart()->update();
    chartView->update();

    static int diagCount = 0;
    if (diagCount++ % 10 == 0) {
        int xr = Edit_XRange->text().toInt();
        if (xr <= 0) xr = 100;
        int usedPoints = (seriesList.size() > 0) ? seriesList[0]->count() : 0;
        static int statusCount = 0;
        if (statusCount++ % 5 == 0) {
            SerialPort_ReceiveAear->appendPlainText(
                QString("[Waveform] Used: %1/%2 pts | Frames: %3 | Pairs: %4 | Optical: %5")
                .arg(usedPoints).arg(xr).arg(frameCount).arg(globalSamplePairCount).arg(globalOpticalSampleCount));
        }
    }

    if (pos > 0) {
        buffer.remove(0, pos);
    }
    else if (buffer.size() > 0) {
        int firstValidHeader = buffer.indexOf(QByteArray("\x55\xAA"));
        if (firstValidHeader > 0) {
            buffer.remove(0, firstValidHeader);
        }
    }
}

void SerialPortAssistant::togglePort(bool open) {
    if (open) {
        serialPort->setPortName(SerialPort_Number->currentText());
        serialPort->setBaudRate(SerialPort_BaudRate->currentText().toInt());
        serialPort->setReadBufferSize(4 * 1024 * 1024);
        if (serialPort->open(QIODevice::ReadWrite)) {
            serialPort->clear();
            buffer.clear();
            for (auto s : seriesList) s->clear();
            baseSamplingRate = 0;
            globalSamplePairCount = 0;
            globalOpticalSampleCount = 0;
            if (CheckBox_SaveCSV->isChecked()) {
                startCSVLogging();
            }
            SerialPort_Number->setEnabled(false);
            SerialPort_Connect->setEnabled(false);
            SerialPort_Disonnect->setEnabled(true);
        }
    }
    else {
        if (buffer.size() > 19) {
            bool wasEnabled = CheckBox_EnablePlot->isChecked();
            if (!wasEnabled) CheckBox_EnablePlot->setChecked(true);
            processBinaryBuffer();
            if (!wasEnabled) CheckBox_EnablePlot->setChecked(false);
            buffer.clear();
        }
        serialPort->close();
        SerialPort_Number->setEnabled(true);
        stopCSVLogging();
        SerialPort_Connect->setEnabled(true);
        SerialPort_Disonnect->setEnabled(false);
    }
}

void SerialPortAssistant::clearAllData() {
    for (auto s : seriesList) s->clear();
    baseSamplingRate = 0;
    globalSamplePairCount = 0;
    globalOpticalSampleCount = 0;
    SerialPort_ReceiveAear->appendPlainText(QString::fromUtf8("[System] Chart reset"));
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

// ************************** CSV 功能实现 **************************
void SerialPortAssistant::startCSVLogging() {
    if (m_isSaving) return;

    QString fileName = QDateTime::currentDateTime().toString("yyyyMMddHHmmss") + ".csv";
    m_csvFile = new QFile(fileName);
    if (!m_csvFile->open(QIODevice::WriteOnly | QIODevice::Text)) {
        SerialPort_ReceiveAear->appendPlainText("[CSV Error] Cannot open file: " + fileName);
        delete m_csvFile;
        m_csvFile = nullptr;
        return;
    }
    m_csvStream = new QTextStream(m_csvFile);
    *m_csvStream << "Time(s),Voltage(V),Current(A),OpticalSignal\n";
    m_isSaving = true;
    m_csvFlushTimer->start(1000);
    SerialPort_ReceiveAear->appendPlainText("[CSV] Started logging to " + fileName);
}

void SerialPortAssistant::stopCSVLogging() {
    if (!m_isSaving) return;

    flushCSVBuffer();
    m_csvFlushTimer->stop();

    if (m_csvStream) {
        m_csvStream->flush();
        delete m_csvStream;
        m_csvStream = nullptr;
    }
    if (m_csvFile) {
        m_csvFile->close();
        delete m_csvFile;
        m_csvFile = nullptr;
    }
    m_isSaving = false;
    m_csvBuffer.clear();
    SerialPort_ReceiveAear->appendPlainText("[CSV] Stopped logging.");
}

void SerialPortAssistant::flushCSVBuffer() {
    if (!m_isSaving || m_csvBuffer.isEmpty() || !m_csvStream) return;

    for (const QString& line : qAsConst(m_csvBuffer)) {
        *m_csvStream << line << "\n";
    }
    m_csvBuffer.clear();
}

void SerialPortAssistant::onSaveCSVToggled(bool checked) {
    if (!serialPort->isOpen()) return;

    if (checked) {
        startCSVLogging();
    }
    else {
        stopCSVLogging();
    }
}

void SerialPortAssistant::timerEvent(QTimerEvent*) { updatePortList(); }

SerialPortAssistant::~SerialPortAssistant() {
    stopCSVLogging();
}