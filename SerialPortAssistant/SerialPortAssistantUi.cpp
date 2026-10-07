#include "SerialPortAssistant.h"

#include <QDoubleValidator>
#include <QFrame>
#include <QGroupBox>
#include <QIntValidator>
#include <QPainter>
#include <QScrollArea>
#include <QSplitter>

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
    Edit_XRange->setValidator(new QIntValidator(1, 50000, Edit_XRange));
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

    for (QLineEdit* edit : { Edit_XMin, Edit_XMax, Edit_YMin, Edit_YMax, Edit_YRightMin, Edit_YRightMax }) {
        auto* validator = new QDoubleValidator(-1.0e12, 1.0e12, 8, edit);
        validator->setNotation(QDoubleValidator::StandardNotation);
        edit->setValidator(validator);
    }

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

    // ---- CSV Card ----
    QGroupBox* csvCard = new QGroupBox("CSV RECORDING");
    QGridLayout* csvGrid = new QGridLayout(csvCard);
    Edit_CSVDirectory = new QLineEdit();
    Edit_CSVDirectory->setReadOnly(true);
    QPushButton* Btn_BrowseCSV = new QPushButton("BROWSE...");
    Label_CSVStatus = new QLabel("Not recording");
    Label_CSVStatus->setWordWrap(true);
    csvGrid->addWidget(new QLabel("Directory:"), 0, 0);
    csvGrid->addWidget(Edit_CSVDirectory, 0, 1);
    csvGrid->addWidget(Btn_BrowseCSV, 0, 2);
    csvGrid->addWidget(Label_CSVStatus, 1, 0, 1, 3);
    configLayout->addWidget(csvCard);
    connect(Btn_BrowseCSV, &QPushButton::clicked, this, &SerialPortAssistant::browseCSVDirectory);

    // ---- Action Buttons ----
    SerialPort_Connect = new QPushButton("CONNECT");
    SerialPort_Connect->setObjectName("btnConnect");
    SerialPort_Disonnect = new QPushButton("DISCONNECT");
    SerialPort_Disonnect->setObjectName("btnResult");
    SerialPort_Disonnect->setEnabled(false);
    SerialPort_Send = new QPushButton("SEND CONFIG");
    SerialPort_Send->setObjectName("btnSend");
    SerialPort_Send->setEnabled(false);
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
    if (index < 0 || index >= static_cast<int>(m_modeValues.size())) return;

    if (m_currentMode >= 0 && m_currentMode != index) {
        for (int parameter = 0; parameter < 6; ++parameter) {
            m_modeValues[m_currentMode][parameter] = Spin_Floats[parameter]->value();
        }
    }

    QStringList labels;
    if (index == 0) {
        labels << QString::fromUtf8("初始电位(mV)") << QString::fromUtf8("终止电位(mV)") << QString::fromUtf8("扫描极限1(mV)") << QString::fromUtf8("扫描极限2(mV)") << QString::fromUtf8("扫描速率(mV/s)") << QString::fromUtf8("循环次数");
        Spin_Floats[0]->setRange(-5000.0, 5000.0);
        Spin_Floats[1]->setRange(-5000.0, 5000.0);
        Spin_Floats[2]->setRange(-5000.0, 5000.0);
        Spin_Floats[3]->setRange(-5000.0, 5000.0);
        Spin_Floats[4]->setRange(0.0001, 2000.0);
        Spin_Floats[5]->setRange(1.0, 1000.0);
    }
    else if (index == 1) {
        labels << QString::fromUtf8("初始电位(mV)") << QString::fromUtf8("终止电位(mV)") << QString::fromUtf8("步进电位(mV)") << QString::fromUtf8("脉冲幅度(mV)") << QString::fromUtf8("脉冲宽度(ms)") << QString::fromUtf8("脉冲周期(ms)");
        Spin_Floats[0]->setRange(-5000.0, 5000.0);
        Spin_Floats[1]->setRange(-5000.0, 5000.0);
        Spin_Floats[2]->setRange(1.0, 1000.0);
        Spin_Floats[3]->setRange(1.0, 5000.0);
        Spin_Floats[4]->setRange(1.0, 10000.0);
        Spin_Floats[5]->setRange(10.0, 10000.0);
    }
    else if (index == 2) {
        labels << QString::fromUtf8("初始电位(mV)") << QString::fromUtf8("初始电位时间(s)") << QString::fromUtf8("阶跃1电位(mV)") << QString::fromUtf8("阶跃1时间(s)") << QString::fromUtf8("阶跃2电位(mV)") << QString::fromUtf8("阶跃2时间(s)");
        Spin_Floats[0]->setRange(-5000.0, 5000.0);
        Spin_Floats[1]->setRange(0.0, 1000.0);
        Spin_Floats[2]->setRange(-5000.0, 5000.0);
        Spin_Floats[3]->setRange(0.0, 10000.0);
        Spin_Floats[4]->setRange(-5000.0, 5000.0);
        Spin_Floats[5]->setRange(0.0, 10000.0);
    }
    else if (index == 3) {
        labels << "Pre-excitation (mV)" << "Reaction (mV)" << "Recovery (mV)" << "Rest Time (s)" << "Pulse Duration (s)" << "Cycles";
        Spin_Floats[0]->setRange(-5000.0, 5000.0);
        Spin_Floats[1]->setRange(-5000.0, 5000.0);
        Spin_Floats[2]->setRange(-5000.0, 5000.0);
        Spin_Floats[3]->setRange(0.0, 1000.0);
        Spin_Floats[4]->setRange(0.0, 1000.0);
        Spin_Floats[5]->setRange(1.0, 1000.0);
    }
    for (int i = 0; i < 6; ++i) {
        Label_Floats[i]->setText(labels[i]);
        Spin_Floats[i]->setValue(m_modeValues[index][i]);
    }
    m_currentMode = index;
}


