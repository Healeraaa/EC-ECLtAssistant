#include "SerialPortAssistant.h"

#include <QAction>
#include <QApplication>
#include <QDoubleValidator>
#include <QFrame>
#include <QGroupBox>
#include <QIntValidator>
#include <QMenuBar>
#include <QPainter>
#include <QStatusBar>

namespace {

void applyTheme(QWidget* widget)
{
    widget->setStyleSheet(QStringLiteral(
        "QMainWindow, QWidget { background-color: #0a0e17; color: #e2e8f0; }"
        "QMenuBar { background: #0d1420; border-bottom: 1px solid #1e293b; padding: 2px; }"
        "QMenuBar::item { padding: 5px 10px; background: transparent; }"
        "QMenuBar::item:selected, QMenu::item:selected { background: #1e3a5f; color: #00e5ff; }"
        "QMenu { background: #0f172a; border: 1px solid #334155; padding: 4px; }"
        "QMenu::item { padding: 6px 28px 6px 12px; }"
        "QToolBar { background: #0d1420; border: none; border-bottom: 1px solid #1e293b;"
        "  spacing: 5px; padding: 5px 7px; }"
        "QToolBar::separator { background: #334155; width: 1px; margin: 4px 5px; }"
        "QDockWidget { color: #00e5ff; font-weight: bold; }"
        "QDockWidget::title { background: #0d1420; border: 1px solid #1e293b;"
        "  padding: 7px 10px; text-align: left; }"
        "QTabWidget::pane { border: 1px solid #1e293b; background: #0a0e17; }"
        "QTabBar::tab { background: #0f172a; color: #94a3b8; border: 1px solid #1e293b;"
        "  padding: 7px 12px; margin-right: 1px; }"
        "QTabBar::tab:selected { background: #152238; color: #00e5ff; border-bottom-color: #00e5ff; }"
        "QStatusBar { background: #0d1420; border-top: 1px solid #1e293b; }"
        "QStatusBar::item { border: none; }"
        "QGroupBox { border: 1px solid #1e293b; border-radius: 8px; margin-top: 14px;"
        "  padding-top: 14px; color: #00e5ff; font-weight: bold; font-size: 11px; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 12px; padding: 0 6px; }"
        "QLabel { color: #94a3b8; background: transparent; font-size: 12px; }"
        "QLabel#sectionLabel { color: #64748b; font-size: 10px; font-weight: bold; letter-spacing: 1px; }"
        "QComboBox { background: #0f172a; border: 1px solid #334155; border-radius: 4px;"
        "  padding: 4px 7px; color: #e2e8f0; min-height: 24px; }"
        "QComboBox:hover { border-color: #00e5ff; }"
        "QComboBox::drop-down { border: none; width: 18px; }"
        "QComboBox QAbstractItemView { background: #111827; border: 1px solid #334155;"
        "  color: #e2e8f0; selection-background-color: #1e3a5f; }"
        "QDoubleSpinBox, QLineEdit { background: #0f172a; border: 1px solid #334155;"
        "  border-radius: 4px; padding: 4px 7px; color: #e2e8f0; min-height: 24px; }"
        "QDoubleSpinBox:hover, QLineEdit:hover, QDoubleSpinBox:focus, QLineEdit:focus { border-color: #00e5ff; }"
        "QCheckBox { color: #94a3b8; spacing: 6px; }"
        "QCheckBox::indicator { width: 15px; height: 15px; border: 1px solid #334155;"
        "  border-radius: 3px; background: #0f172a; }"
        "QCheckBox::indicator:checked { background: #00e5ff; border-color: #00e5ff; }"
        "QPushButton { background: #1e293b; border: 1px solid #334155; border-radius: 4px;"
        "  padding: 5px 11px; color: #e2e8f0; font-size: 12px; font-weight: bold; min-height: 24px; }"
        "QPushButton:hover { border-color: #00e5ff; color: #00e5ff; }"
        "QPushButton:pressed { background: #0f172a; }"
        "QPushButton:disabled { color: #475569; background: #111827; border-color: #1e293b; }"
        "QPushButton#btnConnect { background: #0088cc; border: none; color: white; }"
        "QPushButton#btnConnect:hover { background: #00a8e8; }"
        "QPushButton#btnResult { background: #7c3aed; border: none; color: white; }"
        "QPushButton#btnResult:hover { background: #8b5cf6; }"
        "QPushButton#btnSend { background: transparent; border: 1px solid #00e5ff; color: #00e5ff; }"
        "QPushButton#btnSend:hover { background: #00e5ff; color: #0a0e17; }"
        "QPlainTextEdit#debugConsole { background: #000000; border: none; color: #00ff88;"
        "  font-family: Consolas, monospace; font-size: 11px; padding: 8px; }"
        "QScrollBar:horizontal { height: 7px; background: #0f172a; border: none; }"
        "QScrollBar::handle:horizontal { background: #334155; border-radius: 3px; min-width: 20px; }"
        "QScrollBar:vertical { width: 7px; background: #0f172a; border: none; }"
        "QScrollBar::handle:vertical { background: #334155; border-radius: 3px; min-height: 20px; }"
        "QScrollBar::add-line, QScrollBar::sub-line { height: 0px; width: 0px; }"
        "QChartView { background: #0f172a; border: 1px solid #1e293b; border-radius: 4px; }"
    ));
}

QLabel* toolbarLabel(const QString& text)
{
    QLabel* label = new QLabel(text);
    label->setStyleSheet(QStringLiteral("color: #64748b; padding-left: 2px;"));
    return label;
}

QFrame* statusSeparator()
{
    QFrame* separator = new QFrame();
    separator->setFrameShape(QFrame::VLine);
    separator->setFrameShadow(QFrame::Plain);
    separator->setFixedHeight(16);
    separator->setStyleSheet(QStringLiteral("color: #334155;"));
    return separator;
}

} // namespace

void SerialPortAssistant::initUI()
{
    setDockNestingEnabled(true);

    QWidget* centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);
    QVBoxLayout* centralLayout = new QVBoxLayout(centralWidget);
    centralLayout->setContentsMargins(8, 8, 8, 6);
    centralLayout->setSpacing(6);

    QFrame* chartControls = new QFrame(centralWidget);
    chartControls->setObjectName(QStringLiteral("chartControls"));
    chartControls->setStyleSheet(QStringLiteral(
        "QFrame#chartControls { background: #0d1420; border: 1px solid #1e293b; border-radius: 6px; }"));
    QGridLayout* chartControlGrid = new QGridLayout(chartControls);
    chartControlGrid->setContentsMargins(10, 7, 10, 7);
    chartControlGrid->setHorizontalSpacing(12);
    chartControlGrid->setVerticalSpacing(6);

    CheckBox_EnablePlot = new QCheckBox(QString::fromUtf8("启用绘图"));
    CheckBox_EnablePlot->setChecked(true);
    CheckBox_ChannelVisible[0] = new QCheckBox(QString::fromUtf8("电压"));
    CheckBox_ChannelVisible[1] = new QCheckBox(QString::fromUtf8("电流"));
    CheckBox_ChannelVisible[2] = new QCheckBox(QStringLiteral("ECL"));
    for (QCheckBox* checkBox : CheckBox_ChannelVisible) checkBox->setChecked(true);
    CheckBox_PausePlot = new QCheckBox(QString::fromUtf8("暂停显示"));
    CheckBox_AutoScale = new QCheckBox(QString::fromUtf8("自动缩放"));
    CheckBox_Crosshair = new QCheckBox(QString::fromUtf8("十字光标"));
    CheckBox_Crosshair->setChecked(true);
    CheckBox_Measurement = new QCheckBox(QString::fromUtf8("两点测量"));
    CheckBox_EnableFilter = new QCheckBox(QString::fromUtf8("GPCI 滤波"));
    CheckBox_EnableFilter->setChecked(true);

    Combo_FilterDisplay = new QComboBox();
    Combo_FilterDisplay->addItem(QString::fromUtf8("显示滤波结果"), true);
    Combo_FilterDisplay->addItem(QString::fromUtf8("显示原始数据"), false);
    Combo_FilterDisplay->setMinimumWidth(120);
    Btn_FitChart = new QPushButton(QString::fromUtf8("适应数据"));
    Btn_ResetZoom = new QPushButton(QString::fromUtf8("恢复缩放"));
    Btn_ExportChart = new QPushButton(QString::fromUtf8("导出 PNG"));
    Btn_ResetPlot = new QPushButton(QString::fromUtf8("清空曲线"));
    Btn_ResetPlot->setMinimumWidth(82);
    Btn_FitChart->setMinimumWidth(82);
    Btn_ResetZoom->setMinimumWidth(82);
    Btn_ExportChart->setMinimumWidth(82);
    CheckBox_PausePlot->setToolTip(QString::fromUtf8(
        "仅暂停曲线刷新，不停止串口接收、数据处理和 CSV 保存。"));
    CheckBox_AutoScale->setToolTip(QString::fromUtf8(
        "根据当前可见曲线持续自动调整坐标轴范围。"));
    CheckBox_EnableFilter->setToolTip(QString::fromUtf8(
        "仅 GPCI 模式可用；原始数据仍会保留在 CSV 中。"));
    Combo_FilterDisplay->setToolTip(QString::fromUtf8(
        "切换图表使用滤波结果或原始数据，不改变 CSV 已保存内容。"));
    Btn_ResetPlot->setToolTip(QString::fromUtf8(
        "清空上位机曲线缓存，不重置设备端实验状态。"));
    Btn_FitChart->setToolTip(QString::fromUtf8(
        "根据当前缓存中的可见数据调整坐标轴范围。"));
    Btn_ResetZoom->setToolTip(QString::fromUtf8(
        "取消鼠标缩放，恢复自动范围或图表设置中的手动范围。"));
    Btn_ExportChart->setToolTip(QString::fromUtf8(
        "将当前图表视图导出为 PNG 图片。"));

    Label_CursorReadout = new QLabel(QString::fromUtf8("移动鼠标读取曲线数值"));
    Label_Measurement = new QLabel(QString::fromUtf8("测量未启用"));
    Label_CursorReadout->setTextInteractionFlags(Qt::TextSelectableByMouse);
    Label_Measurement->setTextInteractionFlags(Qt::TextSelectableByMouse);

    chartControlGrid->addWidget(toolbarLabel(QString::fromUtf8("显示")), 0, 0);
    chartControlGrid->addWidget(CheckBox_EnablePlot, 0, 1);
    chartControlGrid->addWidget(CheckBox_ChannelVisible[0], 0, 2);
    chartControlGrid->addWidget(CheckBox_ChannelVisible[1], 0, 3);
    chartControlGrid->addWidget(CheckBox_ChannelVisible[2], 0, 4);
    chartControlGrid->addWidget(CheckBox_PausePlot, 0, 5);
    chartControlGrid->addWidget(CheckBox_AutoScale, 0, 6);
    chartControlGrid->setColumnStretch(7, 1);

    chartControlGrid->addWidget(toolbarLabel(QString::fromUtf8("测量")), 1, 0);
    chartControlGrid->addWidget(CheckBox_Crosshair, 1, 1);
    chartControlGrid->addWidget(CheckBox_Measurement, 1, 2);
    chartControlGrid->addWidget(Label_CursorReadout, 1, 3, 1, 3);
    chartControlGrid->addWidget(Label_Measurement, 1, 6, 1, 3);

    chartControlGrid->addWidget(toolbarLabel(QString::fromUtf8("处理")), 2, 0);
    chartControlGrid->addWidget(CheckBox_EnableFilter, 2, 1);
    chartControlGrid->addWidget(Combo_FilterDisplay, 2, 2, 1, 2);
    chartControlGrid->addWidget(Btn_ResetPlot, 2, 4);
    chartControlGrid->addWidget(Btn_FitChart, 2, 5);
    chartControlGrid->addWidget(Btn_ResetZoom, 2, 6);
    chartControlGrid->addWidget(Btn_ExportChart, 2, 7);
    chartControlGrid->setColumnStretch(8, 1);
    centralLayout->addWidget(chartControls);

    QChart* chart = new QChart();
    chart->setBackgroundBrush(QColor(QStringLiteral("#0f172a")));
    chart->setPlotAreaBackgroundBrush(QColor(QStringLiteral("#0a0e17")));
    chart->setPlotAreaBackgroundVisible(true);
    chart->setBackgroundRoundness(0);
    chart->legend()->setVisible(true);
    chart->legend()->setAlignment(Qt::AlignBottom);
    chart->legend()->setLabelColor(QColor(QStringLiteral("#e2e8f0")));
    chart->legend()->setBackgroundVisible(true);
    chart->legend()->setBrush(QBrush(QColor(0x11, 0x18, 0x27, 220)));
    chart->legend()->setBorderColor(QColor(QStringLiteral("#1e293b")));
    chart->setAnimationOptions(QChart::NoAnimation);

    axisX = new QValueAxis();
    axisX->setRange(0, 100);
    axisX->setTitleText(QString::fromUtf8("时间 (s)"));
    axisX->setTitleBrush(QColor(QStringLiteral("#94a3b8")));
    axisX->setLabelsColor(QColor(QStringLiteral("#94a3b8")));
    axisX->setGridLineColor(QColor(QStringLiteral("#1e293b")));
    axisX->setLinePenColor(QColor(QStringLiteral("#334155")));
    axisX->setShadesPen(Qt::NoPen);

    axisY = new QValueAxis();
    axisY->setRange(-2, 2);
    axisY->setTitleText(QString::fromUtf8("电压 / 电流"));
    axisY->setTitleBrush(QColor(QStringLiteral("#94a3b8")));
    axisY->setLabelsColor(QColor(QStringLiteral("#94a3b8")));
    axisY->setGridLineColor(QColor(QStringLiteral("#1e293b")));
    axisY->setLinePenColor(QColor(QStringLiteral("#334155")));

    axisYRight = new QValueAxis();
    axisYRight->setRange(-2, 2);
    axisYRight->setTitleText(QStringLiteral("ECL"));
    axisYRight->setTitleBrush(QColor(QStringLiteral("#94a3b8")));
    axisYRight->setLabelsColor(QColor(QStringLiteral("#94a3b8")));
    axisYRight->setGridLineColor(QColor(QStringLiteral("#1e293b")));
    axisYRight->setLinePenColor(QColor(QStringLiteral("#334155")));

    chart->addAxis(axisX, Qt::AlignBottom);
    chart->addAxis(axisY, Qt::AlignLeft);
    chart->addAxis(axisYRight, Qt::AlignRight);

    chartView = new ChartInteractionView(chart);
    chartView->setRenderHint(QPainter::Antialiasing);
    chartView->setObjectName(QStringLiteral("chartView"));
    chartView->setRubberBand(QChartView::RectangleRubberBand);
    centralLayout->addWidget(chartView, 1);

    ScrollBar_X = new QScrollBar(Qt::Horizontal);
    ScrollBar_X->setVisible(false);
    centralLayout->addWidget(ScrollBar_X);

    m_experimentToolBar = new QToolBar(QString::fromUtf8("设备与采集"), this);
    m_experimentToolBar->setObjectName(QStringLiteral("experimentToolBar"));
    m_experimentToolBar->toggleViewAction()->setText(QString::fromUtf8("设备与采集工具栏"));
    m_experimentToolBar->setMovable(true);
    m_experimentToolBar->setFloatable(false);
    m_experimentToolBar->setAllowedAreas(Qt::TopToolBarArea | Qt::BottomToolBarArea);
    addToolBar(Qt::TopToolBarArea, m_experimentToolBar);

    SerialPort_Number = new QComboBox();
    SerialPort_Number->setMinimumWidth(80);
    SerialPort_BaudRate = new QComboBox();
    SerialPort_BaudRate->addItems({ QStringLiteral("115200"), QStringLiteral("921600"),
        QStringLiteral("2000000"), QStringLiteral("3000000"), QStringLiteral("600000") });
    SerialPort_BaudRate->setCurrentText(QStringLiteral("600000"));
    SerialPort_BaudRate->setMinimumWidth(82);

    Combo_Mode = new QComboBox();
    Combo_Mode->addItems({ QString::fromUtf8("CV (循环伏安)"), QString::fromUtf8("DPV (差分脉冲)"),
        QString::fromUtf8("CA (计时电流)"), QStringLiteral("GPCI") });
    Combo_Mode->setMinimumWidth(118);

    Combo_Configs[0] = new QComboBox();
    Combo_Configs[0]->addItem(QString::fromUtf8("通道 1"), 2);
    Combo_Configs[0]->addItem(QString::fromUtf8("通道 2"), 3);
    Combo_Configs[0]->addItem(QString::fromUtf8("通道 3"), 1);
    Combo_Configs[0]->addItem(QString::fromUtf8("通道 4"), 0);
    Combo_Configs[0]->setMinimumWidth(72);

    Combo_Range = new QComboBox();
    Combo_Range->addItems({ QStringLiteral("100 nA"), QStringLiteral("330 nA"), QStringLiteral("1 uA"),
        QStringLiteral("3.3 uA"), QStringLiteral("10 uA"), QStringLiteral("33 uA"),
        QStringLiteral("100 uA"), QStringLiteral("330 uA"), QStringLiteral("1 mA"),
        QStringLiteral("3.3 mA"), QStringLiteral("10 mA"), QStringLiteral("30.3 mA"),
        QStringLiteral("100 mA") });
    Combo_Range->setMinimumWidth(76);

    SerialPort_Connect = new QPushButton(QString::fromUtf8("连接"));
    SerialPort_Connect->setObjectName(QStringLiteral("btnConnect"));
    SerialPort_Disonnect = new QPushButton(QString::fromUtf8("断开"));
    SerialPort_Disonnect->setObjectName(QStringLiteral("btnResult"));
    SerialPort_Disonnect->setEnabled(false);
    SerialPort_Send = new QPushButton(QString::fromUtf8("发送配置"));
    SerialPort_Send->setObjectName(QStringLiteral("btnSend"));
    SerialPort_Send->setEnabled(false);
    CheckBox_SaveCSV = new QCheckBox(QString::fromUtf8("保存 CSV"));
    CheckBox_SaveCSV->setChecked(true);
    CheckBox_SaveCSV->setToolTip(QString::fromUtf8(
        "连接串口后自动记录 CSV，取消勾选会结束当前记录并安全写盘。"));
    SerialPort_Connect->setToolTip(QString::fromUtf8(
        "按当前串口和波特率连接设备。"));
    SerialPort_Disonnect->setToolTip(QString::fromUtf8(
        "断开设备并刷新滤波尾部、脉冲汇总和 CSV 缓冲区。"));
    SerialPort_Send->setToolTip(QString::fromUtf8(
        "将当前模式、通道、量程和实验参数发送给设备。"));

    m_experimentToolBar->addWidget(toolbarLabel(QString::fromUtf8("串口")));
    m_experimentToolBar->addWidget(SerialPort_Number);
    m_experimentToolBar->addWidget(toolbarLabel(QString::fromUtf8("波特率")));
    m_experimentToolBar->addWidget(SerialPort_BaudRate);
    m_experimentToolBar->addSeparator();
    m_experimentToolBar->addWidget(toolbarLabel(QString::fromUtf8("模式")));
    m_experimentToolBar->addWidget(Combo_Mode);
    m_experimentToolBar->addWidget(toolbarLabel(QString::fromUtf8("通道")));
    m_experimentToolBar->addWidget(Combo_Configs[0]);
    m_experimentToolBar->addWidget(toolbarLabel(QString::fromUtf8("量程")));
    m_experimentToolBar->addWidget(Combo_Range);
    m_experimentToolBar->addSeparator();
    m_experimentToolBar->addWidget(SerialPort_Connect);
    m_experimentToolBar->addWidget(SerialPort_Disonnect);
    m_experimentToolBar->addWidget(SerialPort_Send);
    m_experimentToolBar->addSeparator();
    m_experimentToolBar->addWidget(CheckBox_SaveCSV);

    for (int index = 1; index < 4; ++index) {
        Combo_Configs[index] = new QComboBox(this);
        Combo_Configs[index]->setVisible(false);
    }
    Combo_Configs[1]->addItem(QStringLiteral("Gain33"), 0);
    Combo_Configs[1]->addItem(QStringLiteral("Gain1K"), 1);
    Combo_Configs[1]->addItem(QStringLiteral("Gain10K"), 2);
    Combo_Configs[1]->addItem(QStringLiteral("Gain100K"), 3);
    Combo_Configs[2]->addItem(QStringLiteral("Gain1X"), 0);
    Combo_Configs[2]->addItem(QStringLiteral("Gain10X"), 1);
    Combo_Configs[3]->addItem(QStringLiteral("Gain1X"), 0);
    Combo_Configs[3]->addItem(QStringLiteral("Gain3.3X"), 1);
    Combo_Configs[3]->addItem(QStringLiteral("Gain10X"), 2);
    Combo_Configs[3]->addItem(QStringLiteral("Gain33X"), 3);
    Combo_Configs[1]->setCurrentIndex(3);
    Combo_Configs[2]->setCurrentIndex(1);
    Combo_Configs[3]->setCurrentIndex(3);

    m_settingsDock = new QDockWidget(QString::fromUtf8("实验工具"), this);
    m_settingsDock->setObjectName(QStringLiteral("settingsDock"));
    m_settingsDock->toggleViewAction()->setText(QString::fromUtf8("实验工具面板"));
    m_settingsDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    m_settingsDock->setMinimumWidth(350);
    m_settingsDock->setFeatures(QDockWidget::DockWidgetClosable
        | QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
    m_settingsTabs = new QTabWidget(m_settingsDock);
    m_settingsTabs->setDocumentMode(true);
    m_settingsDock->setWidget(m_settingsTabs);
    addDockWidget(Qt::RightDockWidgetArea, m_settingsDock);

    QWidget* experimentTab = new QWidget();
    experimentTab->setObjectName(QStringLiteral("experimentTab"));
    QVBoxLayout* experimentLayout = new QVBoxLayout(experimentTab);
    experimentLayout->setContentsMargins(8, 8, 8, 8);
    experimentLayout->setSpacing(8);

    QGroupBox* parameterCard = new QGroupBox(QString::fromUtf8("实验参数"));
    QGridLayout* parameterGrid = new QGridLayout(parameterCard);
    parameterGrid->setHorizontalSpacing(10);
    parameterGrid->setVerticalSpacing(8);
    parameterGrid->setColumnStretch(0, 1);
    parameterGrid->setColumnStretch(1, 1);
    for (int index = 0; index < 6; ++index) {
        QWidget* field = new QWidget();
        QVBoxLayout* fieldLayout = new QVBoxLayout(field);
        fieldLayout->setContentsMargins(0, 0, 0, 0);
        fieldLayout->setSpacing(3);
        Label_Floats[index] = new QLabel();
        Spin_Floats[index] = new QDoubleSpinBox();
        Spin_Floats[index]->setRange(-10.0, 10.0);
        Spin_Floats[index]->setDecimals(4);
        Spin_Floats[index]->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        fieldLayout->addWidget(Label_Floats[index]);
        fieldLayout->addWidget(Spin_Floats[index]);
        parameterGrid->addWidget(field, index / 2, index % 2);
    }
    experimentLayout->addWidget(parameterCard);

    QGroupBox* presetCard = new QGroupBox(QString::fromUtf8("实验预设"));
    QHBoxLayout* presetLayout = new QHBoxLayout(presetCard);
    Btn_SavePreset = new QPushButton(QString::fromUtf8("保存预设"));
    Btn_LoadPreset = new QPushButton(QString::fromUtf8("加载预设"));
    presetLayout->addWidget(Btn_SavePreset);
    presetLayout->addWidget(Btn_LoadPreset);
    experimentLayout->addWidget(presetCard);

    QGroupBox* csvCard = new QGroupBox(QString::fromUtf8("CSV 记录"));
    QGridLayout* csvGrid = new QGridLayout(csvCard);
    Edit_CSVDirectory = new QLineEdit();
    Edit_CSVDirectory->setReadOnly(true);
    QPushButton* browseCsvButton = new QPushButton(QString::fromUtf8("选择目录…"));
    csvGrid->addWidget(new QLabel(QString::fromUtf8("保存目录")), 0, 0, 1, 2);
    csvGrid->addWidget(Edit_CSVDirectory, 1, 0);
    csvGrid->addWidget(browseCsvButton, 1, 1);
    experimentLayout->addWidget(csvCard);
    experimentLayout->addStretch();
    m_settingsTabs->addTab(experimentTab, QString::fromUtf8("实验参数"));

    QWidget* analysisTab = new QWidget();
    analysisTab->setObjectName(QStringLiteral("analysisTab"));
    QVBoxLayout* analysisLayout = new QVBoxLayout(analysisTab);
    analysisLayout->setContentsMargins(8, 8, 8, 8);
    analysisLayout->setSpacing(8);

    QGroupBox* resultCard = new QGroupBox(QString::fromUtf8("实时分析结果"));
    QVBoxLayout* resultLayout = new QVBoxLayout(resultCard);
    Label_PeakStatus = new QLabel(QString::fromUtf8("暂无数据"));
    Label_PulseAreaStatus = new QLabel(QString::fromUtf8("ECL 积分：暂无脉冲"));
    Label_FilterQualityStatus = new QLabel(QString::fromUtf8("滤波质量：暂无有效脉冲"));
    Label_RenderStatus = new QLabel(QString::fromUtf8("绘图 0 点 | 0 ms"));
    for (QLabel* label : { Label_PeakStatus, Label_PulseAreaStatus,
        Label_FilterQualityStatus, Label_RenderStatus }) {
        label->setWordWrap(true);
        label->setTextInteractionFlags(Qt::TextSelectableByMouse);
        resultLayout->addWidget(label);
    }
    analysisLayout->addWidget(resultCard);

    QWidget* chartSettingsTab = new QWidget();
    chartSettingsTab->setObjectName(QStringLiteral("chartSettingsTab"));
    QVBoxLayout* chartSettingsLayout = new QVBoxLayout(chartSettingsTab);
    chartSettingsLayout->setContentsMargins(8, 8, 8, 8);
    chartSettingsLayout->setSpacing(8);

    QGroupBox* axisCard = new QGroupBox(QString::fromUtf8("坐标轴范围"));
    QGridLayout* axisGrid = new QGridLayout(axisCard);
    Edit_XMin = new QLineEdit(QStringLiteral("0"));
    Edit_XMax = new QLineEdit(QStringLiteral("100"));
    Edit_YMin = new QLineEdit(QStringLiteral("-2"));
    Edit_YMax = new QLineEdit(QStringLiteral("2"));
    Edit_YRightMin = new QLineEdit(QStringLiteral("-2"));
    Edit_YRightMax = new QLineEdit(QStringLiteral("2"));
    for (QLineEdit* edit : { Edit_XMin, Edit_XMax, Edit_YMin, Edit_YMax,
        Edit_YRightMin, Edit_YRightMax }) {
        QDoubleValidator* validator = new QDoubleValidator(-1.0e12, 1.0e12, 8, edit);
        validator->setNotation(QDoubleValidator::StandardNotation);
        edit->setValidator(validator);
    }
    axisGrid->addWidget(new QLabel(QStringLiteral("X Min")), 0, 0);
    axisGrid->addWidget(Edit_XMin, 0, 1);
    axisGrid->addWidget(new QLabel(QStringLiteral("X Max")), 0, 2);
    axisGrid->addWidget(Edit_XMax, 0, 3);
    axisGrid->addWidget(new QLabel(QStringLiteral("Y1 Min")), 1, 0);
    axisGrid->addWidget(Edit_YMin, 1, 1);
    axisGrid->addWidget(new QLabel(QStringLiteral("Y1 Max")), 1, 2);
    axisGrid->addWidget(Edit_YMax, 1, 3);
    axisGrid->addWidget(new QLabel(QStringLiteral("Y2 Min")), 2, 0);
    axisGrid->addWidget(Edit_YRightMin, 2, 1);
    axisGrid->addWidget(new QLabel(QStringLiteral("Y2 Max")), 2, 2);
    axisGrid->addWidget(Edit_YRightMax, 2, 3);
    QPushButton* applyXAxisButton = new QPushButton(QString::fromUtf8("应用 X"));
    QPushButton* applyYAxisButton = new QPushButton(QString::fromUtf8("应用 Y1"));
    QPushButton* applyYRightButton = new QPushButton(QString::fromUtf8("应用 Y2"));
    axisGrid->addWidget(applyXAxisButton, 3, 0, 1, 2);
    axisGrid->addWidget(applyYAxisButton, 3, 2);
    axisGrid->addWidget(applyYRightButton, 3, 3);
    chartSettingsLayout->addWidget(axisCard);

    QGroupBox* qualityCard = new QGroupBox(QString::fromUtf8("滤波质量阈值"));
    QGridLayout* qualityGrid = new QGridLayout(qualityCard);
    Spin_MaxAreaDifference = new QDoubleSpinBox();
    Spin_MaxAreaDifference->setRange(0.1, 100.0);
    Spin_MaxAreaDifference->setDecimals(1);
    Spin_MaxAreaDifference->setSingleStep(0.5);
    Spin_MaxAreaDifference->setSuffix(QStringLiteral(" %"));
    Spin_MaxAreaDifference->setValue(5.0);
    Spin_MaxAreaDifference->setToolTip(QString::fromUtf8("滤波前后积分面积绝对偏差超过此值时产生质量警告。"));
    Spin_MinNoiseReduction = new QDoubleSpinBox();
    Spin_MinNoiseReduction->setRange(-100.0, 100.0);
    Spin_MinNoiseReduction->setDecimals(1);
    Spin_MinNoiseReduction->setSingleStep(1.0);
    Spin_MinNoiseReduction->setSuffix(QStringLiteral(" %"));
    Spin_MinNoiseReduction->setValue(0.0);
    Spin_MinNoiseReduction->setToolTip(QString::fromUtf8("背景噪声下降比例低于此值时产生质量警告。"));
    Spin_MinSnrImprovement = new QDoubleSpinBox();
    Spin_MinSnrImprovement->setRange(-60.0, 60.0);
    Spin_MinSnrImprovement->setDecimals(1);
    Spin_MinSnrImprovement->setSingleStep(0.5);
    Spin_MinSnrImprovement->setSuffix(QStringLiteral(" dB"));
    Spin_MinSnrImprovement->setValue(0.0);
    Spin_MinSnrImprovement->setToolTip(QString::fromUtf8("信噪比提升低于此值时产生质量警告。"));
    qualityGrid->addWidget(new QLabel(QString::fromUtf8("最大面积偏差")), 0, 0);
    qualityGrid->addWidget(Spin_MaxAreaDifference, 0, 1);
    qualityGrid->addWidget(new QLabel(QString::fromUtf8("最低噪声下降")), 1, 0);
    qualityGrid->addWidget(Spin_MinNoiseReduction, 1, 1);
    qualityGrid->addWidget(new QLabel(QString::fromUtf8("最低 SNR 提升")), 2, 0);
    qualityGrid->addWidget(Spin_MinSnrImprovement, 2, 1);
    analysisLayout->addWidget(qualityCard);

    QGroupBox* performanceCard = new QGroupBox(QString::fromUtf8("显示性能"));
    QGridLayout* performanceGrid = new QGridLayout(performanceCard);
    Edit_XRange = new QLineEdit(QStringLiteral("50000"));
    Edit_XRange->setValidator(new QIntValidator(1, 50000, Edit_XRange));
    Edit_RenderPoints = new QLineEdit(QStringLiteral("4000"));
    Edit_RenderPoints->setValidator(new QIntValidator(200, 20000, Edit_RenderPoints));
    performanceGrid->addWidget(new QLabel(QString::fromUtf8("缓存点数")), 0, 0);
    performanceGrid->addWidget(Edit_XRange, 0, 1);
    performanceGrid->addWidget(new QLabel(QString::fromUtf8("绘图点数")), 1, 0);
    performanceGrid->addWidget(Edit_RenderPoints, 1, 1);
    chartSettingsLayout->addWidget(performanceCard);
    analysisLayout->addStretch();
    m_settingsTabs->addTab(analysisTab, QString::fromUtf8("分析结果"));
    chartSettingsLayout->addStretch();
    m_settingsTabs->addTab(chartSettingsTab, QString::fromUtf8("图表设置"));

    QWidget* playbackTab = new QWidget();
    playbackTab->setObjectName(QStringLiteral("playbackTab"));
    QVBoxLayout* playbackLayout = new QVBoxLayout(playbackTab);
    playbackLayout->setContentsMargins(8, 8, 8, 8);
    QGroupBox* playbackCard = new QGroupBox(QString::fromUtf8("CSV 回放"));
    QGridLayout* playbackGrid = new QGridLayout(playbackCard);
    Btn_LoadCSV = new QPushButton(QString::fromUtf8("加载 CSV"));
    Btn_PlayPauseCSV = new QPushButton(QString::fromUtf8("播放"));
    Btn_PlayPauseCSV->setEnabled(false);
    Combo_PlaybackSpeed = new QComboBox();
    Combo_PlaybackSpeed->addItem(QStringLiteral("1x"), 1.0);
    Combo_PlaybackSpeed->addItem(QStringLiteral("5x"), 5.0);
    Combo_PlaybackSpeed->addItem(QStringLiteral("20x"), 20.0);
    Combo_PlaybackSpeed->addItem(QStringLiteral("100x"), 100.0);
    Slider_Playback = new QSlider(Qt::Horizontal);
    Slider_Playback->setRange(0, 1000);
    Slider_Playback->setEnabled(false);
    Label_PlaybackFile = new QLabel(QString::fromUtf8("未加载 CSV"));
    Label_PlaybackFile->setWordWrap(true);
    Combo_RecentCSV = new QComboBox();
    Combo_RecentCSV->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    Combo_RecentCSV->setMinimumContentsLength(18);
    Btn_OpenRecentCSV = new QPushButton(QString::fromUtf8("打开最近文件"));
    Btn_OpenRecentCSV->setEnabled(false);
    playbackGrid->addWidget(Btn_LoadCSV, 0, 0);
    playbackGrid->addWidget(Btn_PlayPauseCSV, 0, 1);
    playbackGrid->addWidget(Combo_PlaybackSpeed, 0, 2);
    playbackGrid->addWidget(Slider_Playback, 1, 0, 1, 3);
    playbackGrid->addWidget(Label_PlaybackFile, 2, 0, 1, 3);
    playbackGrid->addWidget(Combo_RecentCSV, 3, 0, 1, 2);
    playbackGrid->addWidget(Btn_OpenRecentCSV, 3, 2);
    playbackLayout->addWidget(playbackCard);
    playbackLayout->addStretch();
    m_settingsTabs->addTab(playbackTab, QString::fromUtf8("CSV 回放"));

    m_logDock = new QDockWidget(QString::fromUtf8("运行日志"), this);
    m_logDock->setObjectName(QStringLiteral("logDock"));
    m_logDock->toggleViewAction()->setText(QString::fromUtf8("运行日志面板"));
    m_logDock->setAllowedAreas(Qt::TopDockWidgetArea | Qt::BottomDockWidgetArea);
    m_logDock->setFeatures(QDockWidget::DockWidgetClosable
        | QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
    SerialPort_ReceiveAear = new QPlainTextEdit();
    SerialPort_ReceiveAear->setReadOnly(true);
    SerialPort_ReceiveAear->setMaximumBlockCount(500);
    SerialPort_ReceiveAear->setObjectName(QStringLiteral("debugConsole"));
    m_logDock->setWidget(SerialPort_ReceiveAear);
    addDockWidget(Qt::BottomDockWidgetArea, m_logDock);

    Label_ConnectionStatus = new QLabel(QString::fromUtf8("未连接"));
    Label_DataRate = new QLabel(QStringLiteral("0 B/s"));
    Label_FrameStatus = new QLabel(QString::fromUtf8("有效帧 0 | CRC 0 | 无效帧 0"));
    Label_SampleStatus = new QLabel(QStringLiteral("IV 0 | ECL 0"));
    Label_CSVStatus = new QLabel(QString::fromUtf8("未记录"));
    Label_CSVStatus->setMaximumWidth(320);
    Label_CSVStatus->setTextInteractionFlags(Qt::TextSelectableByMouse);
    Label_ConnectionStatus->setMinimumWidth(72);
    Label_DataRate->setMinimumWidth(72);
    statusBar()->addWidget(Label_ConnectionStatus);
    statusBar()->addWidget(statusSeparator());
    statusBar()->addWidget(Label_DataRate);
    statusBar()->addWidget(statusSeparator());
    statusBar()->addWidget(Label_FrameStatus, 1);
    statusBar()->addWidget(statusSeparator());
    statusBar()->addWidget(Label_SampleStatus, 1);
    statusBar()->addPermanentWidget(statusSeparator());
    statusBar()->addPermanentWidget(Label_CSVStatus);

    Btn_About = new QPushButton(this);
    Btn_About->setVisible(false);

    QMenu* fileMenu = menuBar()->addMenu(QString::fromUtf8("文件(&F)"));
    QAction* loadCsvAction = fileMenu->addAction(QString::fromUtf8("加载 CSV…"));
    QAction* chooseDirectoryAction = fileMenu->addAction(QString::fromUtf8("选择 CSV 保存目录…"));
    QAction* exportChartAction = fileMenu->addAction(QString::fromUtf8("导出当前图表…"));
    fileMenu->addSeparator();
    QAction* exitAction = fileMenu->addAction(QString::fromUtf8("退出"));

    QMenu* viewMenu = menuBar()->addMenu(QString::fromUtf8("视图(&V)"));
    viewMenu->addAction(m_experimentToolBar->toggleViewAction());
    viewMenu->addAction(m_settingsDock->toggleViewAction());
    viewMenu->addAction(m_logDock->toggleViewAction());
    viewMenu->addSeparator();
    QAction* restoreLayoutAction = viewMenu->addAction(QString::fromUtf8("恢复默认布局"));

    QMenu* toolsMenu = menuBar()->addMenu(QString::fromUtf8("工具(&T)"));
    QAction* clearPlotAction = toolsMenu->addAction(QString::fromUtf8("清空曲线"));
    toolsMenu->addSeparator();
    QAction* savePresetAction = toolsMenu->addAction(QString::fromUtf8("保存实验预设…"));
    QAction* loadPresetAction = toolsMenu->addAction(QString::fromUtf8("加载实验预设…"));

    QMenu* helpMenu = menuBar()->addMenu(QString::fromUtf8("帮助(&H)"));
    QAction* aboutAction = helpMenu->addAction(QString::fromUtf8("关于 / 版本信息"));

    connect(browseCsvButton, &QPushButton::clicked,
        this, &SerialPortAssistant::browseCSVDirectory);
    connect(applyXAxisButton, &QPushButton::clicked,
        this, &SerialPortAssistant::applyManualAxisRanges);
    connect(applyYAxisButton, &QPushButton::clicked,
        this, &SerialPortAssistant::applyManualAxisRanges);
    connect(applyYRightButton, &QPushButton::clicked,
        this, &SerialPortAssistant::applyManualAxisRanges);
    connect(loadCsvAction, &QAction::triggered, Btn_LoadCSV, &QPushButton::click);
    connect(chooseDirectoryAction, &QAction::triggered, browseCsvButton, &QPushButton::click);
    connect(exportChartAction, &QAction::triggered, Btn_ExportChart, &QPushButton::click);
    connect(exitAction, &QAction::triggered, qApp, &QApplication::closeAllWindows);
    connect(clearPlotAction, &QAction::triggered, Btn_ResetPlot, &QPushButton::click);
    connect(savePresetAction, &QAction::triggered, Btn_SavePreset, &QPushButton::click);
    connect(loadPresetAction, &QAction::triggered, Btn_LoadPreset, &QPushButton::click);
    connect(aboutAction, &QAction::triggered, Btn_About, &QPushButton::click);
    connect(restoreLayoutAction, &QAction::triggered, this, [this]() {
        removeToolBar(m_experimentToolBar);
        addToolBar(Qt::TopToolBarArea, m_experimentToolBar);
        m_experimentToolBar->show();
        m_settingsDock->setFloating(false);
        m_logDock->setFloating(false);
        addDockWidget(Qt::RightDockWidgetArea, m_settingsDock);
        addDockWidget(Qt::BottomDockWidgetArea, m_logDock);
        m_settingsDock->show();
        m_logDock->show();
        resizeDocks(QList<QDockWidget*>() << m_settingsDock,
            QList<int>() << 390, Qt::Horizontal);
        resizeDocks(QList<QDockWidget*>() << m_logDock,
            QList<int>() << 150, Qt::Vertical);
    });

    resizeDocks(QList<QDockWidget*>() << m_settingsDock,
        QList<int>() << 390, Qt::Horizontal);
    resizeDocks(QList<QDockWidget*>() << m_logDock,
        QList<int>() << 150, Qt::Vertical);

    applyTheme(this);
    updateChemLabels(0);
}

void SerialPortAssistant::updateChemLabels(int index)
{
    if (index < 0 || index >= static_cast<int>(m_modeValues.size())) return;

    if (m_currentMode >= 0 && m_currentMode != index) {
        for (int parameter = 0; parameter < 6; ++parameter) {
            m_modeValues[m_currentMode][parameter] = Spin_Floats[parameter]->value();
        }
    }

    QStringList labels;
    if (index == 0) {
        labels << QString::fromUtf8("初始电位 (mV)") << QString::fromUtf8("终止电位 (mV)")
               << QString::fromUtf8("扫描极限 1 (mV)") << QString::fromUtf8("扫描极限 2 (mV)")
               << QString::fromUtf8("扫描速率 (mV/s)") << QString::fromUtf8("循环次数");
        Spin_Floats[0]->setRange(-5000.0, 5000.0);
        Spin_Floats[1]->setRange(-5000.0, 5000.0);
        Spin_Floats[2]->setRange(-5000.0, 5000.0);
        Spin_Floats[3]->setRange(-5000.0, 5000.0);
        Spin_Floats[4]->setRange(0.0001, 2000.0);
        Spin_Floats[5]->setRange(1.0, 1000.0);
    }
    else if (index == 1) {
        labels << QString::fromUtf8("初始电位 (mV)") << QString::fromUtf8("终止电位 (mV)")
               << QString::fromUtf8("步进电位 (mV)") << QString::fromUtf8("脉冲幅度 (mV)")
               << QString::fromUtf8("脉冲宽度 (ms)") << QString::fromUtf8("脉冲周期 (ms)");
        Spin_Floats[0]->setRange(-5000.0, 5000.0);
        Spin_Floats[1]->setRange(-5000.0, 5000.0);
        Spin_Floats[2]->setRange(1.0, 1000.0);
        Spin_Floats[3]->setRange(1.0, 5000.0);
        Spin_Floats[4]->setRange(1.0, 10000.0);
        Spin_Floats[5]->setRange(10.0, 10000.0);
    }
    else if (index == 2) {
        labels << QString::fromUtf8("初始电位 (mV)") << QString::fromUtf8("初始电位时间 (s)")
               << QString::fromUtf8("阶跃 1 电位 (mV)") << QString::fromUtf8("阶跃 1 时间 (s)")
               << QString::fromUtf8("阶跃 2 电位 (mV)") << QString::fromUtf8("阶跃 2 时间 (s)");
        Spin_Floats[0]->setRange(-5000.0, 5000.0);
        Spin_Floats[1]->setRange(0.0, 1000.0);
        Spin_Floats[2]->setRange(-5000.0, 5000.0);
        Spin_Floats[3]->setRange(0.0, 10000.0);
        Spin_Floats[4]->setRange(-5000.0, 5000.0);
        Spin_Floats[5]->setRange(0.0, 10000.0);
    }
    else {
        labels << QString::fromUtf8("预激发电位 (mV)") << QString::fromUtf8("反应电位 (mV)")
               << QString::fromUtf8("恢复电位 (mV)") << QString::fromUtf8("静置时间 (s)")
               << QString::fromUtf8("脉冲持续时间 (s)") << QString::fromUtf8("循环次数");
        Spin_Floats[0]->setRange(-5000.0, 5000.0);
        Spin_Floats[1]->setRange(-5000.0, 5000.0);
        Spin_Floats[2]->setRange(-5000.0, 5000.0);
        Spin_Floats[3]->setRange(0.0, 1000.0);
        Spin_Floats[4]->setRange(0.0, 1000.0);
        Spin_Floats[5]->setRange(1.0, 1000.0);
    }

    for (int parameter = 0; parameter < 6; ++parameter) {
        Label_Floats[parameter]->setText(labels[parameter]);
        Spin_Floats[parameter]->setValue(m_modeValues[index][parameter]);
    }
    m_currentMode = index;
    if (CheckBox_EnableFilter) {
        CheckBox_EnableFilter->setEnabled(index == 3 && !serialPort->isOpen());
    }
}
