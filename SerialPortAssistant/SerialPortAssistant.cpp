#include "SerialPortAssistant.h"
#include "AppVersion.h"
#include <QMessageBox>
#include <QTimerEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QStandardPaths>
#include <QDir>
#include <QSettings>
#include <QDateTime>
#include <cmath>

SerialPortAssistant::SerialPortAssistant(QWidget* parent) : QMainWindow(parent) {
    this->setWindowTitle(QString("%1 v%2").arg(EC_ECL_APP_NAME, EC_ECL_APP_VERSION));
    this->resize(1400, 850);
    this->setMinimumSize(1000, 650);
    serialPort = new QSerialPort(this);

    initializeModeDefaults();
    initUI();
    loadSettings();
    setupConnections();

    m_processTimer = new QTimer(this);
    connect(m_processTimer, &QTimer::timeout, this, &SerialPortAssistant::processBinaryBuffer);
    m_processTimer->start(20);

    m_plotRefreshTimer = new QTimer(this);
    m_plotRefreshTimer->setInterval(100);
    connect(m_plotRefreshTimer, &QTimer::timeout, [this]() {
        if (!CheckBox_EnablePlot->isChecked()) return;
        if (CheckBox_PausePlot->isChecked()) trimPlotBuffers();
        else updatePlotSeries();
    });
    m_plotRefreshTimer->start();

    // CSV 缓冲区定时刷新（每秒写一次文件）
    m_csvFlushTimer = new QTimer(this);
    connect(m_csvFlushTimer, &QTimer::timeout, this, &SerialPortAssistant::flushCSVBuffer);

    m_statusTimer = new QTimer(this);
    connect(m_statusTimer, &QTimer::timeout, this, &SerialPortAssistant::updateStatusPanel);
    m_statusTimer->start(500);

    m_playbackTimer = new QTimer(this);
    m_playbackTimer->setInterval(30);
    connect(m_playbackTimer, &QTimer::timeout, this, &SerialPortAssistant::advanceCSVPlayback);

    m_csvLoadWatcher = new QFutureWatcher<CsvPlaybackLoadResult>(this);
    connect(
        m_csvLoadWatcher,
        &QFutureWatcher<CsvPlaybackLoadResult>::finished,
        this,
        &SerialPortAssistant::finishCSVPlaybackLoad);

    m_dataRateTimer.start();
    m_plotBuffer.clear();
    updateStatusPanel();

    this->startTimer(1000);
    updatePortList();
}

void SerialPortAssistant::setupConnections() {
    connect(SerialPort_Connect, &QPushButton::clicked, [=]() { togglePort(true); });
    connect(SerialPort_Disonnect, &QPushButton::clicked, [=]() {
        if (serialPort->isOpen()) {
            togglePort(false);
        }
    });
    connect(Combo_Mode, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SerialPortAssistant::updateChemLabels);
    connect(SerialPort_Send, &QPushButton::clicked, this, &SerialPortAssistant::sendConfig);
    connect(Btn_ResetPlot, &QPushButton::clicked, this, &SerialPortAssistant::clearAllData);
    connect(Btn_About, &QPushButton::clicked, this, &SerialPortAssistant::showAboutDialog);
    connect(serialPort, &QSerialPort::readyRead, [this]() {
        const QByteArray receivedData = serialPort->readAll();
        m_receivedBytes += static_cast<quint64>(receivedData.size());
        m_frameParser.append(receivedData);
        });
    // 动态启停 CSV 记录
    connect(CheckBox_SaveCSV, &QCheckBox::toggled, this, &SerialPortAssistant::onSaveCSVToggled);
    for (QCheckBox* checkBox : CheckBox_ChannelVisible) {
        connect(checkBox, &QCheckBox::toggled, this, &SerialPortAssistant::updateSeriesVisibility);
    }
    connect(CheckBox_PausePlot, &QCheckBox::toggled, [this](bool paused) {
        if (!paused && CheckBox_EnablePlot->isChecked()) updatePlotSeries();
    });
    connect(CheckBox_AutoScale, &QCheckBox::toggled, [this](bool enabled) {
        if (enabled) fitChartToData();
    });
    connect(CheckBox_EnablePlot, &QCheckBox::toggled, [this](bool enabled) {
        if (enabled && !CheckBox_PausePlot->isChecked()) updatePlotSeries();
    });
    connect(Btn_FitChart, &QPushButton::clicked, this, &SerialPortAssistant::fitChartToData);
    connect(Btn_ResetZoom, &QPushButton::clicked, this, &SerialPortAssistant::resetChartZoom);
    connect(Btn_ExportChart, &QPushButton::clicked, this, &SerialPortAssistant::exportChartImage);
    connect(CheckBox_Crosshair, &QCheckBox::toggled, [this](bool enabled) {
        chartView->setCursorEnabled(enabled);
        if (!enabled) Label_CursorReadout->setText(QString::fromUtf8("十字光标已关闭"));
    });
    connect(CheckBox_Measurement, &QCheckBox::toggled, [this](bool enabled) {
        clearMeasurement();
        Label_Measurement->setText(enabled
            ? QString::fromUtf8("请在曲线上单击选择测量点 A")
            : QString::fromUtf8("测量未启用"));
    });
    chartView->setCursorMovedHandler(
        [this](const QPointF& position) { handleChartCursor(position); });
    chartView->setPlotClickedHandler(
        [this](const QPointF& position) { handleChartClick(position); });
    connect(Btn_SavePreset, &QPushButton::clicked, this, &SerialPortAssistant::savePreset);
    connect(Btn_LoadPreset, &QPushButton::clicked, this, &SerialPortAssistant::loadPreset);
    connect(Btn_LoadCSV, &QPushButton::clicked, this, &SerialPortAssistant::loadCSVForPlayback);
    connect(Btn_PlayPauseCSV, &QPushButton::clicked, this, &SerialPortAssistant::toggleCSVPlayback);
    connect(Btn_OpenRecentCSV, &QPushButton::clicked, this, &SerialPortAssistant::openRecentCSV);
    connect(Slider_Playback, &QSlider::valueChanged, this, &SerialPortAssistant::seekCSVPlayback);
}

void SerialPortAssistant::sendConfig() {
    if (!serialPort->isOpen()) return;

    QString validationMessage;
    if (!validateConfig(&validationMessage)) {
        QMessageBox::warning(this, QString::fromUtf8("参数无效"), validationMessage);
        return;
    }

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
        {2, 0, 1},  // Gain10K, Gain1X, Gain3.3X
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

void SerialPortAssistant::processBinaryBuffer() {
    const QVector<ProtocolFrame> frames = m_frameParser.takeFrames();
    if (frames.isEmpty()) return;

    for (const ProtocolFrame& frame : frames) {
        processFrame(frame);
    }

    const ProtocolParserStats& stats = m_frameParser.stats();
    if (stats.validFrames - m_lastReportedFrames >= 50) {
        m_lastReportedFrames = stats.validFrames;
        SerialPort_ReceiveAear->appendPlainText(
            QString("[Data] Frames: %1 | CRC errors: %2 | Invalid: %3 | Dropped bytes: %4 | IV pairs: %5 | Optical: %6")
                .arg(stats.validFrames)
                .arg(stats.crcErrors)
                .arg(stats.lengthErrors + stats.unknownFrames)
                .arg(stats.discardedBytes)
                .arg(globalSamplePairCount)
                .arg(globalOpticalSampleCount));
    }
}

void SerialPortAssistant::processFrame(const ProtocolFrame& frame) {
    ensureSeriesCreated();

    const bool plotEnabled = CheckBox_EnablePlot->isChecked();
    if (frame.type == ProtocolFrame::Type::IV) {
        ivSamplingRate = frame.sampleRate;
        const int pairCount = frame.values.size() / 2;
        for (int pairIndex = 0; pairIndex < pairCount; ++pairIndex) {
            const float voltage = frame.values[pairIndex * 2];
            const float current = frame.values[pairIndex * 2 + 1];
            const double time = m_ivTimeSeconds;
            m_ivTimeSeconds += 1.0 / static_cast<double>(frame.sampleRate);

            m_plotBuffer.append(0, time, voltage, plotEnabled);
            m_plotBuffer.append(1, time, current, plotEnabled);
            if (m_csvRecorder.isRecording()) {
                m_csvRecorder.appendIv(time, voltage, current);
            }
        }
        globalSamplePairCount += static_cast<quint64>(pairCount);
    }
    else if (frame.type == ProtocolFrame::Type::Light) {
        lightSamplingRate = frame.sampleRate;
        for (float value : frame.values) {
            const double time = m_lightTimeSeconds;
            m_lightTimeSeconds += 1.0 / static_cast<double>(frame.sampleRate);

            m_plotBuffer.append(2, time, value, plotEnabled);
            if (m_csvRecorder.isRecording()) {
                m_csvRecorder.appendLight(time, value);
            }
        }
        globalOpticalSampleCount += static_cast<quint64>(frame.values.size());
    }
}

void SerialPortAssistant::togglePort(bool open) {
    if (open) {
        if (m_csvLoadWatcher->isRunning()) {
            QMessageBox::information(
                this,
                QString::fromUtf8("正在加载 CSV"),
                QString::fromUtf8("请等待 CSV 加载完成后再连接串口。"));
            return;
        }
        stopCSVPlayback();
        serialPort->setPortName(SerialPort_Number->currentText());
        serialPort->setBaudRate(SerialPort_BaudRate->currentText().toInt());
        serialPort->setReadBufferSize(4 * 1024 * 1024);
        if (serialPort->open(QIODevice::ReadWrite)) {
            serialPort->clear();
            m_frameParser.clear();
            for (QLineSeries* series : seriesList) series->clear();
            m_plotBuffer.clear();
            clearMeasurement();

            ivSamplingRate = 0;
            lightSamplingRate = 0;
            globalSamplePairCount = 0;
            globalOpticalSampleCount = 0;
            m_ivTimeSeconds = 0.0;
            m_lightTimeSeconds = 0.0;
            m_lastReportedFrames = 0;
            m_receivedBytes = 0;
            m_lastStatusBytes = 0;
            m_lastStatusFrames = 0;
            m_dataRateTimer.restart();

            if (CheckBox_SaveCSV->isChecked()) {
                startCSVLogging();
            }
            SerialPort_Number->setEnabled(false);
            SerialPort_BaudRate->setEnabled(false);
            SerialPort_Connect->setEnabled(false);
            SerialPort_Disonnect->setEnabled(true);
            SerialPort_Send->setEnabled(true);
            Btn_LoadCSV->setEnabled(false);
            Btn_PlayPauseCSV->setEnabled(false);
            Btn_OpenRecentCSV->setEnabled(false);
            Slider_Playback->setEnabled(false);
            updateStatusPanel();
        }
    }
    else {
        processBinaryBuffer();
        m_frameParser.clear();
        serialPort->close();
        SerialPort_Number->setEnabled(true);
        SerialPort_BaudRate->setEnabled(true);
        stopCSVLogging();
        SerialPort_Connect->setEnabled(true);
        SerialPort_Disonnect->setEnabled(false);
        SerialPort_Send->setEnabled(false);
        Btn_LoadCSV->setEnabled(true);
        Btn_PlayPauseCSV->setEnabled(!m_playbackData.isEmpty());
        Btn_OpenRecentCSV->setEnabled(!m_recentCsvFiles.isEmpty());
        Slider_Playback->setEnabled(!m_playbackData.isEmpty());
        updateStatusPanel();
        saveSettings();
    }
}

void SerialPortAssistant::clearAllData() {
    for (QLineSeries* series : seriesList) series->clear();
    m_plotBuffer.clear();
    clearMeasurement();
    SerialPort_ReceiveAear->appendPlainText(QString::fromUtf8("[System] Chart reset; acquisition time was preserved."));
}

void SerialPortAssistant::showAboutDialog() {
    QMessageBox::about(
        this,
        QString::fromUtf8("关于 EC-ECL Recorder"),
        QString::fromUtf8(
            "<h3>EC-ECL Recorder v%1</h3>"
            "<p>用于电化学与电化学发光数据采集、保存、回放和分析。</p>"
            "<p>Qt 版本：%2<br>构建时间：%3</p>"
            "<p>CSV 原始数据完整保存；界面抽样仅影响曲线显示。</p>")
            .arg(EC_ECL_APP_VERSION, qVersion(), EC_ECL_BUILD_TIMESTAMP));
}

void SerialPortAssistant::updatePortList() {
    const QList<QSerialPortInfo> infos = QSerialPortInfo::availablePorts();
    QStringList availablePorts;
    for (const QSerialPortInfo& info : infos) {
        availablePorts.append(info.portName());
    }
    bool portsUnchanged = availablePorts.size() == lastPortList.size();
    for (int index = 0; portsUnchanged && index < availablePorts.size(); ++index) {
        portsUnchanged = availablePorts.at(index) == lastPortList.at(index);
    }
    if (portsUnchanged) return;

    const QString selectedPort = SerialPort_Number->currentText();
    SerialPort_Number->clear();
    SerialPort_Number->addItems(availablePorts);
    if (availablePorts.contains(selectedPort)) {
        SerialPort_Number->setCurrentText(selectedPort);
    }
    lastPortList = availablePorts;
}

void SerialPortAssistant::startCSVLogging() {
    if (m_csvRecorder.isRecording()) return;

    QDir directory(Edit_CSVDirectory->text());
    if (!directory.exists() && !directory.mkpath(QStringLiteral("."))) {
        Label_CSVStatus->setText(QString::fromUtf8("无法创建保存目录"));
        SerialPort_ReceiveAear->appendPlainText("[CSV Error] Cannot create directory: " + directory.absolutePath());
        CheckBox_SaveCSV->setChecked(false);
        return;
    }

    const QString filePath = createCSVFilePath();
    QString errorMessage;
    if (!m_csvRecorder.start(filePath, &errorMessage)) {
        Label_CSVStatus->setText(QString::fromUtf8("文件创建失败"));
        SerialPort_ReceiveAear->appendPlainText("[CSV Error] " + errorMessage + ": " + filePath);
        CheckBox_SaveCSV->setChecked(false);
        return;
    }

    m_csvFlushTimer->start(1000);
    Label_CSVStatus->setText(QString::fromUtf8("正在记录：") + QDir::toNativeSeparators(filePath));
    SerialPort_ReceiveAear->appendPlainText("[CSV] Started logging to " + QDir::toNativeSeparators(filePath));
    saveSettings();
}

void SerialPortAssistant::stopCSVLogging() {
    if (!m_csvRecorder.isRecording()) return;

    m_csvFlushTimer->stop();
    const QString filePath = m_csvRecorder.filePath();
    const quint64 rowCount = m_csvRecorder.rowsWritten() + static_cast<quint64>(m_csvRecorder.pendingRows());
    QString errorMessage;
    const bool succeeded = m_csvRecorder.stop(&errorMessage);

    if (succeeded) {
        Label_CSVStatus->setText(
            QString::fromUtf8("已保存 %1 行：%2").arg(rowCount).arg(QDir::toNativeSeparators(filePath)));
        SerialPort_ReceiveAear->appendPlainText(
            QString("[CSV] Saved %1 rows to %2").arg(rowCount).arg(QDir::toNativeSeparators(filePath)));
    }
    else {
        Label_CSVStatus->setText(QString::fromUtf8("保存失败：") + errorMessage);
        SerialPort_ReceiveAear->appendPlainText("[CSV Error] " + errorMessage);
    }
}

void SerialPortAssistant::flushCSVBuffer() {
    if (!m_csvRecorder.isRecording()) return;

    QString errorMessage;
    if (!m_csvRecorder.flush(&errorMessage)) {
        SerialPort_ReceiveAear->appendPlainText("[CSV Error] " + errorMessage);
        stopCSVLogging();
        CheckBox_SaveCSV->setChecked(false);
    }
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

void SerialPortAssistant::browseCSVDirectory() {
    const QString selectedDirectory = QFileDialog::getExistingDirectory(
        this,
        QString::fromUtf8("选择 CSV 保存目录"),
        Edit_CSVDirectory->text(),
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    if (selectedDirectory.isEmpty()) return;

    Edit_CSVDirectory->setText(QDir::toNativeSeparators(selectedDirectory));
    saveSettings();
}

QString SerialPortAssistant::createCSVFilePath() const {
    QDir directory(Edit_CSVDirectory->text());
    const QString modeNames[] = { "CV", "DPV", "CA", "GPCI" };
    const int modeIndex = qBound(0, Combo_Mode->currentIndex(), 3);
    QString channelName = Combo_Configs[0]->currentText();
    channelName.replace(' ', '_');

    const QString baseName = QString("%1_%2_%3")
        .arg(QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss_zzz"))
        .arg(modeNames[modeIndex])
        .arg(channelName);

    QString filePath = directory.filePath(baseName + ".csv");
    int suffix = 1;
    while (QFileInfo::exists(filePath)) {
        filePath = directory.filePath(QString("%1_%2.csv").arg(baseName).arg(suffix++));
    }
    return filePath;
}

void SerialPortAssistant::initializeModeDefaults() {
    m_modeValues = {{
        {{ 0.0, 0.0, 500.0, -500.0, 30.0, 3.0 }},
        {{ -500.0, 500.0, 5.0, 50.0, 50.0, 200.0 }},
        {{ 0.0, 3.0, 1000.0, 1.0, 0.0, 3.0 }},
        {{ 0.0, 0.0, 0.0, 1.0, 1.0, 1.0 }}
    }};
}

void SerialPortAssistant::loadSettings() {
    QSettings settings("EC-ECL", "Recorder");

    const QString defaultDirectory =
        QDir(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)).filePath("EC-ECL");
    Edit_CSVDirectory->setText(QDir::toNativeSeparators(
        settings.value("csv/directory", defaultDirectory).toString()));

    SerialPort_BaudRate->setCurrentText(settings.value("serial/baudRate", "600000").toString());
    Combo_Configs[0]->setCurrentIndex(qBound(0, settings.value("experiment/channel", 0).toInt(), Combo_Configs[0]->count() - 1));
    Combo_Range->setCurrentIndex(qBound(0, settings.value("experiment/range", 0).toInt(), Combo_Range->count() - 1));

    for (int mode = 0; mode < 4; ++mode) {
        for (int parameter = 0; parameter < 6; ++parameter) {
            const QString key = QString("modes/%1/parameter%2").arg(mode).arg(parameter);
            m_modeValues[mode][parameter] = settings.value(key, m_modeValues[mode][parameter]).toDouble();
        }
    }

    Edit_XRange->setText(settings.value("plot/displayPoints", "50000").toString());
    Edit_RenderPoints->setText(settings.value("plot/renderPoints", "4000").toString());
    Edit_XMin->setText(settings.value("plot/xMin", "0").toString());
    Edit_XMax->setText(settings.value("plot/xMax", "100").toString());
    Edit_YMin->setText(settings.value("plot/yMin", "-2").toString());
    Edit_YMax->setText(settings.value("plot/yMax", "2").toString());
    Edit_YRightMin->setText(settings.value("plot/yRightMin", "-2").toString());
    Edit_YRightMax->setText(settings.value("plot/yRightMax", "2").toString());
    CheckBox_AutoScale->setChecked(settings.value("plot/autoScale", false).toBool());
    CheckBox_Crosshair->setChecked(settings.value("plot/crosshair", true).toBool());
    chartView->setCursorEnabled(CheckBox_Crosshair->isChecked());
    for (int channel = 0; channel < 3; ++channel) {
        CheckBox_ChannelVisible[channel]->setChecked(
            settings.value(QString("plot/channel%1Visible").arg(channel), true).toBool());
    }
    Combo_PlaybackSpeed->setCurrentIndex(
        qBound(0, settings.value("playback/speedIndex", 0).toInt(), Combo_PlaybackSpeed->count() - 1));
    m_recentCsvFiles = settings.value("playback/recentFiles").toStringList();
    updateRecentCSVList();

    const int mode = qBound(0, settings.value("experiment/mode", 0).toInt(), Combo_Mode->count() - 1);
    m_currentMode = -1;
    Combo_Mode->setCurrentIndex(mode);
    updateChemLabels(mode);
}

void SerialPortAssistant::saveSettings() {
    if (m_currentMode >= 0) {
        for (int parameter = 0; parameter < 6; ++parameter) {
            m_modeValues[m_currentMode][parameter] = Spin_Floats[parameter]->value();
        }
    }

    QSettings settings("EC-ECL", "Recorder");
    settings.setValue("csv/directory", Edit_CSVDirectory->text());
    settings.setValue("serial/baudRate", SerialPort_BaudRate->currentText());
    settings.setValue("experiment/mode", Combo_Mode->currentIndex());
    settings.setValue("experiment/channel", Combo_Configs[0]->currentIndex());
    settings.setValue("experiment/range", Combo_Range->currentIndex());
    settings.setValue("plot/displayPoints", Edit_XRange->text());
    settings.setValue("plot/renderPoints", Edit_RenderPoints->text());
    settings.setValue("plot/xMin", Edit_XMin->text());
    settings.setValue("plot/xMax", Edit_XMax->text());
    settings.setValue("plot/yMin", Edit_YMin->text());
    settings.setValue("plot/yMax", Edit_YMax->text());
    settings.setValue("plot/yRightMin", Edit_YRightMin->text());
    settings.setValue("plot/yRightMax", Edit_YRightMax->text());
    settings.setValue("plot/autoScale", CheckBox_AutoScale->isChecked());
    settings.setValue("plot/crosshair", CheckBox_Crosshair->isChecked());
    for (int channel = 0; channel < 3; ++channel) {
        settings.setValue(
            QString("plot/channel%1Visible").arg(channel),
            CheckBox_ChannelVisible[channel]->isChecked());
    }
    settings.setValue("playback/speedIndex", Combo_PlaybackSpeed->currentIndex());
    settings.setValue("playback/recentFiles", m_recentCsvFiles);

    for (int mode = 0; mode < 4; ++mode) {
        for (int parameter = 0; parameter < 6; ++parameter) {
            settings.setValue(
                QString("modes/%1/parameter%2").arg(mode).arg(parameter),
                m_modeValues[mode][parameter]);
        }
    }
}

bool SerialPortAssistant::validateConfig(QString* message) const {
    for (int parameter = 0; parameter < 6; ++parameter) {
        if (!std::isfinite(Spin_Floats[parameter]->value())) {
            if (message) *message = QString::fromUtf8("参数中包含无效数值。");
            return false;
        }
    }

    if (Combo_Mode->currentIndex() == 1
        && Spin_Floats[4]->value() >= Spin_Floats[5]->value()) {
        if (message) {
            *message = QString::fromUtf8("DPV 脉冲周期必须大于脉冲宽度。");
        }
        return false;
    }
    return true;
}

void SerialPortAssistant::timerEvent(QTimerEvent*) { updatePortList(); }

SerialPortAssistant::~SerialPortAssistant() {
    if (m_csvLoadWatcher->isRunning()) {
        m_csvLoadWatcher->cancel();
        m_csvLoadWatcher->waitForFinished();
    }
    stopCSVLogging();
    saveSettings();
}
