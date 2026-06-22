#include "SerialPortAssistant.h"
#include <QDialog>
#include <QFrame>
#include <QSplitter>
#include <QGroupBox>
#include <QMessageBox>
#include <QTimerEvent>
#include <cmath>

// ===== Sci-Fi Dark Theme =====
static void applyTheme(QWidget* w) {
    w->setStyleSheet(QStringLiteral(
        "QMainWindow, QWidget { background-color: #0a0e17; color: #e2e8f0; }"
        "QGroupBox { border: 1px solid #1e293b; border-radius: 8px; margin-top: 14px;"
        "  padding-top: 14px; color: #00e5ff; font-weight: bold; font-size: 11px; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 12px; padding: 0 6px; }"
        "QLabel { color: #94a3b8; background: transparent; font-size: 12px; }"
        "QLabel#sectionLabel { color: #475569; font-size: 10px; font-weight: bold; letter-spacing: 2px; }"
        "QComboBox { background: #0f172a; border: 1px solid #334155; border-radius: 4px;"
        "  padding: 4px 8px; color: #e2e8f0; min-height: 24px; }"
        "QComboBox:hover { border-color: #00e5ff; }"
        "QComboBox::drop-down { border: none; width: 20px; }"
        "QComboBox QAbstractItemView { background: #111827; border: 1px solid #334155;"
        "  color: #e2e8f0; selection-background-color: #1e3a5f; }"
        "QDoubleSpinBox, QLineEdit { background: #0f172a; border: 1px solid #334155;"
        "  border-radius: 4px; padding: 4px 8px; color: #e2e8f0; min-height: 24px; }"
        "QDoubleSpinBox:hover, QLineEdit:hover { border-color: #00e5ff; }"
        "QDoubleSpinBox:focus, QLineEdit:focus { border-color: #00e5ff; }"
        "QCheckBox { color: #94a3b8; spacing: 8px; }"
        "QCheckBox::indicator { width: 16px; height: 16px; border: 1px solid #334155;"
        "  border-radius: 3px; background: #0f172a; }"
        "QCheckBox::indicator:checked { background: #00e5ff; border-color: #00e5ff; }"
        "QPushButton { background: #1e293b; border: 1px solid #334155; border-radius: 4px;"
        "  padding: 5px 14px; color: #e2e8f0; font-size: 12px; font-weight: bold; min-height: 24px; }"
        "QPushButton:hover { border-color: #00e5ff; color: #00e5ff; }"
        "QPushButton:pressed { background: #0f172a; }"
        "QPushButton#btnConnect { background: #0088cc; border: none; color: white; }"
        "QPushButton#btnConnect:hover { background: #00a8e8; }"
        "QPushButton#btnResult { background: #7c3aed; border: none; color: white; }"
        "QPushButton#btnResult:hover { background: #8b5cf6; }"
        "QPushButton#btnSend { background: transparent; border: 1px solid #00e5ff; color: #00e5ff; }"
        "QPushButton#btnSend:hover { background: #00e5ff; color: #0a0e17; }"
        "QPlainTextEdit#debugConsole { background: #000000; border: 1px solid #1e293b;"
        "  border-radius: 4px; color: #00ff88; font-family: Consolas, monospace; font-size: 11px; padding: 8px; }"
        "QScrollBar:horizontal { height: 6px; background: #0f172a; border: none; }"
        "QScrollBar::handle:horizontal { background: #334155; border-radius: 3px; min-width: 20px; }"
        "QScrollBar:vertical { width: 6px; background: #0f172a; border: none; }"
        "QScrollBar::handle:vertical { background: #334155; border-radius: 3px; min-height: 20px; }"
        "QScrollBar::add-line, QScrollBar::sub-line { height: 0px; width: 0px; }"
        "QChartView { background: #0f172a; border: 1px solid #1e293b; border-radius: 4px; }"
    ));
}

SerialPortAssistant::SerialPortAssistant(QWidget* parent) : QMainWindow(parent) {
    this->setWindowTitle(QString::fromUtf8("EC-ECL Recorder"));
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
    QHBoxLayout* centralOuterLayout = new QHBoxLayout(centralWidget);
    centralOuterLayout->setContentsMargins(12, 12, 12, 12);

    QSplitter* splitter = new QSplitter(Qt::Horizontal);
    splitter->setHandleWidth(3);
    splitter->setStyleSheet("QSplitter::handle { background: #334155; }"
                            "QSplitter::handle:hover { background: #00e5ff; }");

    // ===== LEFT PANEL: Debug Console + Chart =====
    QWidget* leftContainer = new QWidget();
    QVBoxLayout* leftLayout = new QVBoxLayout(leftContainer);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->setSpacing(6);

    QLabel* debugLabel = new QLabel("DEBUG CONSOLE");
    debugLabel->setObjectName("sectionLabel");
    leftLayout->addWidget(debugLabel);

    SerialPort_ReceiveAear = new QPlainTextEdit();
    SerialPort_ReceiveAear->setReadOnly(true);
    SerialPort_ReceiveAear->setMaximumBlockCount(500);
    SerialPort_ReceiveAear->setObjectName("debugConsole");
    leftLayout->addWidget(SerialPort_ReceiveAear, 2);

    // Chart with dark theme
    QChart* chart = new QChart();
    chart->setBackgroundBrush(QColor("#0f172a"));
    chart->setPlotAreaBackgroundBrush(QColor("#0a0e17"));
    chart->setBackgroundRoundness(0);
    chart->legend()->setVisible(true);
    chart->legend()->setAlignment(Qt::AlignBottom);
    chart->legend()->setLabelColor(QColor("#e2e8f0"));
    chart->legend()->setBackgroundVisible(true);
    chart->legend()->setBrush(QBrush(QColor(0x11, 0x18, 0x27, 220)));
    chart->legend()->setBorderColor(QColor("#1e293b"));
    chart->setAnimationOptions(QChart::NoAnimation);

    axisX = new QValueAxis(); axisX->setRange(0, 100);
    axisX->setTitleBrush(QColor("#94a3b8"));
    axisX->setLabelsColor(QColor("#94a3b8"));
    axisX->setGridLineColor(QColor("#1e293b"));
    axisX->setLinePenColor(QColor("#334155"));
    axisX->setShadesPen(Qt::NoPen);

    axisY = new QValueAxis(); axisY->setRange(-2, 2);
    axisY->setTitleBrush(QColor("#94a3b8"));
    axisY->setLabelsColor(QColor("#94a3b8"));
    axisY->setGridLineColor(QColor("#1e293b"));
    axisY->setLinePenColor(QColor("#334155"));

    axisYRight = new QValueAxis();
    axisYRight->setRange(-2, 2);
    axisYRight->setTitleBrush(QColor("#94a3b8"));
    axisYRight->setLabelsColor(QColor("#94a3b8"));
    axisYRight->setGridLineColor(QColor("#1e293b"));
    axisYRight->setLinePenColor(QColor("#334155"));

    chart->addAxis(axisX, Qt::AlignBottom);
    chart->addAxis(axisY, Qt::AlignLeft);
    chart->addAxis(axisYRight, Qt::AlignRight);

    chartView = new QChartView(chart);
    chartView->setRenderHint(QPainter::Antialiasing);
    chartView->setObjectName("chartView");

    QLabel* chartLabel = new QLabel("WAVEFORM");
    chartLabel->setObjectName("sectionLabel");
    leftLayout->addWidget(chartLabel);
    leftLayout->addWidget(chartView, 5);

    ScrollBar_X = new QScrollBar(Qt::Horizontal);
    ScrollBar_X->setVisible(false);
    leftLayout->addWidget(ScrollBar_X);

    // ===== RIGHT PANEL: Config Cards (scrollable) =====
    QScrollArea* rightScroll = new QScrollArea();
    rightScroll->setWidgetResizable(true);
    rightScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    rightScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    rightScroll->setFrameShape(QFrame::NoFrame);
    QWidget* configWidget = new QWidget();
    configWidget->setMinimumWidth(320);
    QVBoxLayout* configLayout = new QVBoxLayout(configWidget);
    configLayout->setContentsMargins(0, 0, 0, 0);
    configLayout->setSpacing(8);

    // ---- Hardware Card ----
    QGroupBox* hwCard = new QGroupBox("HARDWARE");
    QGridLayout* hwGrid = new QGridLayout(hwCard);
    hwGrid->setSpacing(6);
    hwGrid->setColumnStretch(0, 0);
    hwGrid->setColumnStretch(1, 1);
    hwGrid->setColumnStretch(2, 0);
    hwGrid->setColumnStretch(3, 1);

    int row = 0;
    SerialPort_Number = new QComboBox();
    SerialPort_BaudRate = new QComboBox();
    SerialPort_BaudRate->addItems({ "115200", "921600", "2000000", "3000000","600000" });
    SerialPort_BaudRate->setCurrentText("600000");
    hwGrid->addWidget(new QLabel("Port:"), row, 0);
    hwGrid->addWidget(SerialPort_Number, row, 1);
    hwGrid->addWidget(new QLabel("Baud:"), row, 2);
    hwGrid->addWidget(SerialPort_BaudRate, row++, 3);

    Combo_Mode = new QComboBox();
    Combo_Mode->addItems({ QString::fromUtf8("CV (循环伏安)"), QString::fromUtf8("DPV (差分脉冲)"), QString::fromUtf8("CA (计时电流)"), QString::fromUtf8("GPCI") });
    hwGrid->addWidget(new QLabel("Mode:"), row, 0);
    hwGrid->addWidget(Combo_Mode, row++, 1, 1, 3);

    Combo_Configs[0] = new QComboBox();
    Combo_Configs[0]->addItem("Channel 1", 2);
    Combo_Configs[0]->addItem("Channel 2", 3);
    Combo_Configs[0]->addItem("Channel 3", 1);
    Combo_Configs[0]->addItem("Channel 4", 0);
    hwGrid->addWidget(new QLabel("Channel:"), row, 0);
    hwGrid->addWidget(Combo_Configs[0], row++, 1, 1, 3);

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
    hwGrid->addWidget(new QLabel("Range:"), row, 0);
    hwGrid->addWidget(Combo_Range, row++, 1, 1, 3);

    configLayout->addWidget(hwCard);

    // Hidden internal combos
    for (int i = 1; i < 4; ++i) {
        Combo_Configs[i] = new QComboBox();
        Combo_Configs[i]->setVisible(false);
    }
    Combo_Configs[1]->addItem("Gain33", 0);
    Combo_Configs[1]->addItem("Gain1K", 1);
    Combo_Configs[1]->addItem("Gain10K", 2);
    Combo_Configs[1]->addItem("Gain100K", 3);
    Combo_Configs[2]->addItem("Gain1X", 0);
    Combo_Configs[2]->addItem("Gain10X", 1);
    Combo_Configs[3]->addItem("Gain1X", 0);
    Combo_Configs[3]->addItem("Gain3.3X", 1);
    Combo_Configs[3]->addItem("Gain10X", 2);
    Combo_Configs[3]->addItem("Gain33X", 3);
    Combo_Configs[1]->setCurrentIndex(3);
    Combo_Configs[2]->setCurrentIndex(1);
    Combo_Configs[3]->setCurrentIndex(3);

    // ---- Parameters Card ----
    QGroupBox* paramCard = new QGroupBox("PARAMETERS");
    QGridLayout* paramGrid = new QGridLayout(paramCard);
    paramGrid->setSpacing(6);
    paramGrid->setColumnStretch(0, 0);
    paramGrid->setColumnStretch(1, 1);

    for (int i = 0; i < 6; ++i) {
        Label_Floats[i] = new QLabel();
        Spin_Floats[i] = new QDoubleSpinBox();
        Spin_Floats[i]->setRange(-10.0, 10.0);
        Spin_Floats[i]->setDecimals(4);
        paramGrid->addWidget(Label_Floats[i], i, 0);
        paramGrid->addWidget(Spin_Floats[i], i, 1);
    }

    Edit_XRange = new QLineEdit("50000");
    paramGrid->addWidget(new QLabel("Display Points:"), 6, 0);
    paramGrid->addWidget(Edit_XRange, 6, 1);

    configLayout->addWidget(paramCard);

    // ---- Axis Range Card ----
    QGroupBox* axisCard = new QGroupBox("AXIS RANGE");
    QGridLayout* axisGrid = new QGridLayout(axisCard);
    axisGrid->setSpacing(4);
    axisGrid->setColumnStretch(0, 0);
    axisGrid->setColumnStretch(1, 1);
    axisGrid->setColumnStretch(2, 0);
    axisGrid->setColumnStretch(3, 1);

    Edit_XMin = new QLineEdit("0");
    Edit_XMax = new QLineEdit("100");
    axisGrid->addWidget(new QLabel("X Min:"), 0, 0);
    axisGrid->addWidget(Edit_XMin, 0, 1);
    axisGrid->addWidget(new QLabel("X Max:"), 0, 2);
    axisGrid->addWidget(Edit_XMax, 0, 3);

    Edit_YMin = new QLineEdit("-2");
    Edit_YMax = new QLineEdit("2");
    axisGrid->addWidget(new QLabel("Y1 Min:"), 1, 0);
    axisGrid->addWidget(Edit_YMin, 1, 1);
    axisGrid->addWidget(new QLabel("Y1 Max:"), 1, 2);
    axisGrid->addWidget(Edit_YMax, 1, 3);

    Edit_YRightMin = new QLineEdit("-2");
    Edit_YRightMax = new QLineEdit("2");
    axisGrid->addWidget(new QLabel("Y2 Min:"), 2, 0);
    axisGrid->addWidget(Edit_YRightMin, 2, 1);
    axisGrid->addWidget(new QLabel("Y2 Max:"), 2, 2);
    axisGrid->addWidget(Edit_YRightMax, 2, 3);

    QPushButton* Btn_ApplyXAxis = new QPushButton("Apply X");
    QPushButton* Btn_ApplyYAxis = new QPushButton("Apply Y1");
    QPushButton* Btn_ApplyYRight = new QPushButton("Apply Y2");
    QHBoxLayout* axisBtnRow = new QHBoxLayout();
    axisBtnRow->addWidget(Btn_ApplyXAxis);
    axisBtnRow->addWidget(Btn_ApplyYAxis);
    axisBtnRow->addWidget(Btn_ApplyYRight);
    axisGrid->addLayout(axisBtnRow, 3, 0, 1, 4);

    configLayout->addWidget(axisCard);

    // Checkboxes
    QHBoxLayout* checkRow = new QHBoxLayout();
    CheckBox_EnablePlot = new QCheckBox("Plot Enabled");
    CheckBox_EnablePlot->setChecked(true);
    CheckBox_SaveCSV = new QCheckBox("Save CSV");
    CheckBox_SaveCSV->setChecked(true);
    checkRow->addWidget(CheckBox_EnablePlot);
    checkRow->addWidget(CheckBox_SaveCSV);
    configLayout->addLayout(checkRow);

    // ---- Action Buttons ----
    SerialPort_Connect = new QPushButton("CONNECT");
    SerialPort_Connect->setObjectName("btnConnect");
    SerialPort_Disonnect = new QPushButton("DISCONNECT");
    SerialPort_Disonnect->setObjectName("btnResult");
    SerialPort_Send = new QPushButton("SEND CONFIG");
    SerialPort_Send->setObjectName("btnSend");
    Btn_ResetPlot = new QPushButton("RESET CHART");

    configLayout->addWidget(SerialPort_Connect);
    configLayout->addWidget(SerialPort_Disonnect);
    configLayout->addWidget(SerialPort_Send);
    configLayout->addWidget(Btn_ResetPlot);
    configLayout->addStretch();

    // 让布局内容的最小尺寸触发滚动条
    configLayout->setSizeConstraint(QLayout::SetMinimumSize);

    rightScroll->setWidget(configWidget);

    // Button connections for axis apply
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

    splitter->addWidget(leftContainer);
    splitter->addWidget(rightScroll);
    splitter->setStretchFactor(0, 7);
    splitter->setStretchFactor(1, 3);

    centralOuterLayout->addWidget(splitter);

    applyTheme(this);
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
    else if (index == 3) {
        labels << "Pre-excitation (mV)" << "Reaction (mV)" << "Recovery (mV)" << "Rest Time (s)" << "Pulse Duration (s)" << "Cycles";
        Spin_Floats[0]->setRange(-5000.0, 5000.0);
        Spin_Floats[1]->setRange(-5000.0, 5000.0);
        Spin_Floats[2]->setRange(-5000.0, 5000.0);
        Spin_Floats[3]->setRange(0.0, 1000.0);
        Spin_Floats[4]->setRange(0.0, 1000.0);
        Spin_Floats[5]->setRange(1.0, 1000.0);
        Spin_Floats[0]->setValue(0.0);
        Spin_Floats[1]->setValue(0.0);
        Spin_Floats[2]->setValue(0.0);
        Spin_Floats[3]->setValue(0.0);
        Spin_Floats[4]->setValue(0.0);
        Spin_Floats[5]->setValue(1.0);
    }
    for (int i = 0; i < 6; ++i) {
        Label_Floats[i]->setText(labels[i]);
    }
}

void SerialPortAssistant::setupConnections() {
    connect(SerialPort_Connect, &QPushButton::clicked, [=]() { togglePort(true); });
    connect(SerialPort_Disonnect, &QPushButton::clicked, [=]() {
        if (serialPort->isOpen()) {
            QString csvPath;
            if (m_csvFile) {
                csvPath = m_csvFile->fileName();
            }
            togglePort(false);
            if (!csvPath.isEmpty()) {
                alignCSV(csvPath);
            }
        }
    });
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
                pen.setColor(QColor("#00e5ff"));
                s->setName("Voltage");
            }
            else if (seriesList.size() == 1) {
                pen.setColor(QColor("#ff6bc1"));
                s->setName("Current");
            }
            else {
                pen.setColor(QColor("#00ff88"));
                s->setName("ECL");
            }
            pen.setWidth(3);
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

void SerialPortAssistant::alignCSV(const QString& filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        SerialPort_ReceiveAear->appendPlainText("[CSV Align] Cannot open: " + filePath);
        return;
    }

    // 读表头 + 所有数据行
    QTextStream in(&file);
    QString header = in.readLine();
    QStringList lines;
    while (!in.atEnd()) {
        QString line = in.readLine();
        if (!line.isEmpty()) lines.append(line);
    }
    file.close();

    // 按时间戳合并行: map<time, tuple<v,i,o>>
    QMap<double, std::tuple<double, double, double>> merged;
    for (const QString& line : lines) {
        QStringList cols = line.split(',');
        if (cols.size() < 4) continue;

        bool ok = false;
        double t = cols[0].toDouble(&ok);
        if (!ok) continue;

        double v = cols[1].isEmpty() ? qQNaN() : cols[1].toDouble();
        double i = cols[2].isEmpty() ? qQNaN() : cols[2].toDouble();
        double o = cols[3].isEmpty() ? qQNaN() : cols[3].toDouble();

        if (!merged.contains(t)) {
            merged[t] = { qQNaN(), qQNaN(), qQNaN() };
        }
        auto& row = merged[t];
        if (!qIsNaN(v)) std::get<0>(row) = v;
        if (!qIsNaN(i)) std::get<1>(row) = i;
        if (!qIsNaN(o)) std::get<2>(row) = o;
    }

    // 写回对齐后的文件
    QFile outFile(filePath);
    if (!outFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        SerialPort_ReceiveAear->appendPlainText("[CSV Align] Cannot write: " + filePath);
        return;
    }
    QTextStream out(&outFile);
    out << header << "\n";

    for (auto it = merged.begin(); it != merged.end(); ++it) {
        double v = std::get<0>(it.value());
        double i = std::get<1>(it.value());
        double o = std::get<2>(it.value());
        out << QString::number(it.key(), 'f', 6) << ","
            << (qIsNaN(v) ? "" : QString::number(v, 'f', 6)) << ","
            << (qIsNaN(i) ? "" : QString::number(i, 'f', 6)) << ","
            << (qIsNaN(o) ? "" : QString::number(o, 'f', 6)) << "\n";
    }
    outFile.close();

    SerialPort_ReceiveAear->appendPlainText(
        QString("[CSV Align] Aligned %1 rows -> %2 unique times")
            .arg(lines.size()).arg(merged.size()));
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