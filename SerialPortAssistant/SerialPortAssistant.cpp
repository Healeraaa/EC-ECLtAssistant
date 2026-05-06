#include "SerialPortAssistant.h"

SerialPortAssistant::SerialPortAssistant(QWidget* parent) : QMainWindow(parent) {
    this->setWindowTitle(QString::fromLocal8Bit("ceio通信助手"));
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

    SerialPort_ReceiveAear = new QPlainTextEdit();
    SerialPort_ReceiveAear->setReadOnly(true);
    SerialPort_ReceiveAear->setMaximumBlockCount(500);
    leftLayout->addWidget(new QLabel(QString::fromLocal8Bit("数据日志:")), 0);
    leftLayout->addWidget(SerialPort_ReceiveAear, 2);

    QChart* chart = new QChart();
    chart->setAnimationOptions(QChart::NoAnimation);
    axisX = new QValueAxis(); axisX->setRange(0, 100); axisX->setLabelFormat("%d");
    axisY = new QValueAxis(); axisY->setRange(0, 100);
    chart->addAxis(axisX, Qt::AlignBottom);
    chart->addAxis(axisY, Qt::AlignLeft);
    chartView = new QChartView(chart);
    chartView->setRenderHint(QPainter::Antialiasing, false);
    leftLayout->addWidget(chartView, 5);

    ScrollBar_X = new QScrollBar(Qt::Horizontal);
    ScrollBar_X->setVisible(false);
    leftLayout->addWidget(ScrollBar_X);

    SerialPort_SendAear = new QPlainTextEdit();
    SerialPort_SendAear->setMaximumHeight(80);
    leftLayout->addWidget(new QLabel(QString::fromLocal8Bit("发送区:")), 0);
    leftLayout->addWidget(SerialPort_SendAear, 1);

    QVBoxLayout* rightLayout = new QVBoxLayout();
    QWidget* configGroup = new QWidget();
    configGroup->setFixedWidth(320);
    QGridLayout* grid = new QGridLayout(configGroup);

    SerialPort_Number = new QComboBox();
    SerialPort_BaudRate = new QComboBox();
    SerialPort_BaudRate->addItems({ "9600","115200","256000","921600" });
    SerialPort_BaudRate->setCurrentText("115200");
    SerialPort_DataBits = new QComboBox(); SerialPort_DataBits->addItems({ "8","7","6","5" });
    SerialPort_StopBits = new QComboBox(); SerialPort_StopBits->addItems({ "1","1.5","2" });
    SerialPort_CheckBits = new QComboBox(); SerialPort_CheckBits->addItems({ QString::fromLocal8Bit("无"),QString::fromLocal8Bit("奇"),QString::fromLocal8Bit("偶") });

    Edit_XRange = new QLineEdit("100");
    Edit_YMax = new QLineEdit("100");
    Edit_YMin = new QLineEdit("0");

    // 初始化UI状态
    Edit_YMax->setReadOnly(true);
    Edit_YMax->setStyleSheet("background-color: #F0F0F0;");
    Edit_YMin->setReadOnly(true);
    Edit_YMin->setStyleSheet("background-color: #F0F0F0;");

    CheckBox_EnablePlot = new QCheckBox(QString::fromLocal8Bit("启用波形绘制"));
    CheckBox_EnablePlot->setChecked(true);
    CheckBox_AutoScale = new QCheckBox(QString::fromLocal8Bit("Y轴全自动量程"));
    CheckBox_AutoScale->setChecked(true);
    CheckBox_AutoScroll = new QCheckBox(QString::fromLocal8Bit("X轴自动跟随"));
    CheckBox_AutoScroll->setChecked(true);
    CheckBox_Timestamp = new QCheckBox(QString::fromLocal8Bit("显示时间戳"));
    CheckBox_SaveCSV = new QCheckBox(QString::fromLocal8Bit("实时保存数据到CSV"));

    int r = 0;
    grid->addWidget(new QLabel(QString::fromLocal8Bit("端口:")), r, 0); grid->addWidget(SerialPort_Number, r++, 1);
    grid->addWidget(new QLabel(QString::fromLocal8Bit("波特率:")), r, 0); grid->addWidget(SerialPort_BaudRate, r++, 1);
    grid->addWidget(new QLabel(QString::fromLocal8Bit("数据位:")), r, 0); grid->addWidget(SerialPort_DataBits, r++, 1);
    grid->addWidget(new QLabel(QString::fromLocal8Bit("停止位:")), r, 0); grid->addWidget(SerialPort_StopBits, r++, 1);
    grid->addWidget(new QLabel(QString::fromLocal8Bit("校验位:")), r, 0); grid->addWidget(SerialPort_CheckBits, r++, 1);
    grid->addWidget(new QLabel(QString::fromLocal8Bit("X轴长度:")), r, 0); grid->addWidget(Edit_XRange, r++, 1);
    grid->addWidget(new QLabel(QString::fromLocal8Bit("Y轴上限:")), r, 0); grid->addWidget(Edit_YMax, r++, 1);
    grid->addWidget(new QLabel(QString::fromLocal8Bit("Y轴下限:")), r, 0); grid->addWidget(Edit_YMin, r++, 1);
    grid->addWidget(CheckBox_EnablePlot, r++, 1);
    grid->addWidget(CheckBox_AutoScale, r++, 1);
    grid->addWidget(CheckBox_AutoScroll, r++, 1);
    grid->addWidget(CheckBox_Timestamp, r++, 1);
    grid->addWidget(CheckBox_SaveCSV, r++, 1);

    rightLayout->addWidget(configGroup);
    rightLayout->addStretch();

    SerialPort_Connect = new QPushButton(QString::fromLocal8Bit("打开串口"));
    SerialPort_Disonnect = new QPushButton(QString::fromLocal8Bit("断开连接"));
    SerialPort_Disonnect->setEnabled(false);
    Btn_ClearLog = new QPushButton(QString::fromLocal8Bit("清空日志"));
    Btn_ResetPlot = new QPushButton(QString::fromLocal8Bit("重置波形"));
    SerialPort_Send = new QPushButton(QString::fromLocal8Bit("发送数据"));

    rightLayout->addWidget(SerialPort_Connect);
    rightLayout->addWidget(SerialPort_Disonnect);
    rightLayout->addWidget(Btn_ClearLog);
    rightLayout->addWidget(Btn_ResetPlot);
    rightLayout->addWidget(SerialPort_Send);

    mainLayout->addLayout(leftLayout, 1);
    mainLayout->addLayout(rightLayout, 0);
}

void SerialPortAssistant::setupConnections() {
    connect(SerialPort_Connect, &QPushButton::clicked, [=]() { togglePort(true); });
    connect(SerialPort_Disonnect, &QPushButton::clicked, [=]() { togglePort(false); });
    connect(Btn_ClearLog, &QPushButton::clicked, SerialPort_ReceiveAear, &QPlainTextEdit::clear);
    connect(Btn_ResetPlot, &QPushButton::clicked, this, &SerialPortAssistant::clearAllData);

    // --- 完全恢复的 CSV 逻辑 ---
    connect(CheckBox_SaveCSV, &QCheckBox::toggled, [=](bool checked) {
        if (checked && serialPort->isOpen() && !csvFile.isOpen()) {
            QString path = QCoreApplication::applicationDirPath() + "/" + QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss") + ".csv";
            csvFile.setFileName(path);
            if (csvFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
                csvFile.write("Timestamp,Data\n");
                csvFile.flush();
            }
        }
        else if (!checked && csvFile.isOpen()) {
            csvFile.close();
        }
        });

    connect(CheckBox_AutoScale, &QCheckBox::toggled, [=](bool checked) {
        Edit_YMax->setReadOnly(checked);
        Edit_YMin->setReadOnly(checked);
        if (checked) {
            Edit_YMax->setStyleSheet("background-color: #F0F0F0;");
            Edit_YMin->setStyleSheet("background-color: #F0F0F0;");
        }
        else {
            Edit_YMax->setStyleSheet("background-color: #FFFFFF;");
            Edit_YMin->setStyleSheet("background-color: #FFFFFF;");
            axisY->setRange(Edit_YMin->text().toDouble(), Edit_YMax->text().toDouble());
        }
        });

    auto manualYUpdate = [=]() {
        if (!CheckBox_AutoScale->isChecked()) axisY->setRange(Edit_YMin->text().toDouble(), Edit_YMax->text().toDouble());
        };
    connect(Edit_YMax, &QLineEdit::editingFinished, manualYUpdate);
    connect(Edit_YMin, &QLineEdit::editingFinished, manualYUpdate);

    connect(CheckBox_AutoScroll, &QCheckBox::toggled, [=](bool checked) {
        ScrollBar_X->setVisible(!checked);
        if (!checked) ScrollBar_X->setValue(ScrollBar_X->maximum());
        });

    connect(ScrollBar_X, &QScrollBar::valueChanged, [=](int value) {
        if (!CheckBox_AutoScroll->isChecked()) axisX->setRange(value, value + Edit_XRange->text().toInt());
        });

    connect(serialPort, &QSerialPort::readyRead, [=]() {
        buffer.append(serialPort->readAll());
        while (buffer.contains('\n')) {
            int pos = buffer.indexOf('\n');
            QString line = QString::fromLocal8Bit(buffer.left(pos)).trimmed();
            buffer.remove(0, pos + 1);
            if (line.isEmpty()) continue;

            QString timeStr = QDateTime::currentDateTime().toString("hh:mm:ss.zzz");
            bool isPlotMode = CheckBox_EnablePlot->isChecked() && line.startsWith("ceio:");
            QString pureData = isPlotMode ? line.mid(5) : line;

            SerialPort_ReceiveAear->appendPlainText(CheckBox_Timestamp->isChecked() ? "[" + timeStr + "] " + pureData : pureData);

            // CSV 实时写入部分
            if (CheckBox_SaveCSV->isChecked() && csvFile.isOpen()) {
                csvFile.write((timeStr + "," + pureData + "\n").toLocal8Bit());
                csvFile.flush();
            }

            if (isPlotMode) {
                QStringList vals = pureData.split(',');
                if (vals.size() > 20) continue;
                while (seriesList.size() < vals.size()) {
                    QLineSeries* s = new QLineSeries();
                    s->setUseOpenGL(true);
                    chartView->chart()->addSeries(s);
                    s->attachAxis(axisX); s->attachAxis(axisY);
                    seriesList.append(s);
                }
                for (int i = 0; i < vals.size(); ++i) {
                    bool ok; double v = vals[i].toDouble(&ok);
                    if (ok) seriesList[i]->append(plotCount, v);
                }
                plotCount++;

                perfCounter++;
                if (perfCounter >= 10) {
                    perfCounter = 0;
                    int xr = Edit_XRange->text().toInt();
                    ScrollBar_X->setRange(0, qMax(0, (int)plotCount - xr));
                    if (CheckBox_AutoScroll->isChecked()) {
                        axisX->setRange(qMax(0.0, plotCount - xr), plotCount);
                        ScrollBar_X->setValue(ScrollBar_X->maximum());
                    }
                    if (CheckBox_AutoScale->isChecked()) {
                        double vMin = 1e15, vMax = -1e15; bool hasData = false;
                        for (QLineSeries* s : seriesList) {
                            const QList<QPointF> points = s->points();
                            int scanStart = qMax(0, points.size() - xr);
                            for (int j = scanStart; j < points.size(); ++j) {
                                double y = points.at(j).y();
                                if (y < vMin) vMin = y; if (y > vMax) vMax = y;
                                hasData = true;
                            }
                        }
                        if (hasData) {
                            double diff = qMax(vMax - vMin, 0.1);
                            axisY->setRange(vMin - diff * 0.15, vMax + diff * 0.15);
                            Edit_YMax->setText(QString::number(axisY->max(), 'f', 1));
                            Edit_YMin->setText(QString::number(axisY->min(), 'f', 1));
                        }
                    }
                }
            }
        }
        });

    connect(SerialPort_Send, &QPushButton::clicked, [=]() {
        if (serialPort->isOpen()) serialPort->write(SerialPort_SendAear->toPlainText().toLocal8Bit());
        });
}

void SerialPortAssistant::togglePort(bool open) {
    if (open) {
        serialPort->setPortName(SerialPort_Number->currentText());
        serialPort->setBaudRate(SerialPort_BaudRate->currentText().toInt());
        if (serialPort->open(QIODevice::ReadWrite)) {
            serialPort->clear(); buffer.clear();
            // 如果打开串口前已经勾选了CSV，立即创建文件
            if (CheckBox_SaveCSV->isChecked()) emit CheckBox_SaveCSV->toggled(true);
            SerialPort_Connect->setEnabled(false); SerialPort_Disonnect->setEnabled(true);
            SerialPort_Number->setEnabled(false);
        }
    }
    else {
        if (csvFile.isOpen()) csvFile.close();
        serialPort->close();
        SerialPort_Connect->setEnabled(true); SerialPort_Disonnect->setEnabled(false);
        SerialPort_Number->setEnabled(true);
    }
}

void SerialPortAssistant::clearAllData() {
    plotCount = 0; perfCounter = 0;
    for (auto s : seriesList) { chartView->chart()->removeSeries(s); s->deleteLater(); }
    seriesList.clear();
    axisX->setRange(0, Edit_XRange->text().toInt());
    SerialPort_ReceiveAear->clear();
}

void SerialPortAssistant::updatePortList() {
    QList<QSerialPortInfo> infos = QSerialPortInfo::availablePorts();
    if (infos.size() != lastPortList.size()) {
        QString currentSelection = SerialPort_Number->currentText();
        lastPortList.clear();
        QList<QString> portNames;
        for (const auto& info : infos) portNames << info.portName();
        std::sort(portNames.begin(), portNames.end(), [](const QString& a, const QString& b) {
            if (a.length() != b.length()) return a.length() < b.length();
            return a < b;
            });
        SerialPort_Number->clear();
        for (const auto& name : portNames) {
            SerialPort_Number->addItem(name);
            lastPortList << name;
        }
        SerialPort_Number->setCurrentText(currentSelection);
    }
}

void SerialPortAssistant::timerEvent(QTimerEvent*) { updatePortList(); }
SerialPortAssistant::~SerialPortAssistant() { if (csvFile.isOpen()) csvFile.close(); }