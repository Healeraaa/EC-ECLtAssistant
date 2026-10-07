#ifndef SERIALPORTASSISTANT_H
#define SERIALPORTASSISTANT_H

#include <QtWidgets/QMainWindow>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QComboBox>
#include <QLabel>
#include <QCheckBox>
#include <QLineEdit>
#include <QScrollBar>
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QtCharts/QLineSeries>
#include <QtCharts/QValueAxis>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QTimer>
#include <QVector>
#include <QSlider>
#include <QElapsedTimer>
#include <QFutureWatcher>
#include <array>

#include "ChartInteractionView.h"
#include "CsvPlaybackData.h"
#include "CsvRecorder.h"
#include "ExperimentPreset.h"
#include "PlotDataBuffer.h"
#include "ProtocolFrameParser.h"

QT_CHARTS_USE_NAMESPACE

class SerialPortAssistant : public QMainWindow {
    Q_OBJECT
public:
    SerialPortAssistant(QWidget* parent = nullptr);
    ~SerialPortAssistant();

protected:
    void timerEvent(QTimerEvent* event) override;

private:
    void initUI();
    void setupConnections();
    void updatePortList();
    void togglePort(bool open);
    void clearAllData();
    void updateChemLabels(int index);
    void sendConfig();
    void processBinaryBuffer();
    void processFrame(const ProtocolFrame& frame);
    void ensureSeriesCreated();
    void updatePlotSeries();
    void trimPlotBuffers();
    void applyManualAxisRanges();
    void fitChartToData();
    void updateSeriesVisibility();
    void handleChartCursor(const QPointF& position);
    void handleChartClick(const QPointF& position);
    void clearMeasurement();
    void resetChartZoom();
    void exportChartImage();
    void updateStatusPanel();
    bool validateConfig(QString* message) const;
    void initializeModeDefaults();
    void loadSettings();
    void saveSettings();

    // CSV 保存相关
    void startCSVLogging();
    void stopCSVLogging();
    void flushCSVBuffer();
    void onSaveCSVToggled(bool checked);
    void browseCSVDirectory();
    QString createCSVFilePath() const;

    // 预设与 CSV 回放
    void savePreset();
    void loadPreset();
    void loadCSVForPlayback();
    bool openCSVForPlayback(const QString& filePath);
    void finishCSVPlaybackLoad();
    void openRecentCSV();
    void addRecentCSVFile(const QString& filePath);
    void updateRecentCSVList();
    void toggleCSVPlayback();
    void advanceCSVPlayback();
    void seekCSVPlayback(int sliderValue);
    void stopCSVPlayback();
    void appendPlaybackRow(const CsvPlaybackRow& row);

    // UI 组件
    QPlainTextEdit* SerialPort_ReceiveAear;
    QPushButton* SerialPort_Connect, * SerialPort_Disonnect, * SerialPort_Send, * Btn_ResetPlot;
    QComboBox* SerialPort_Number, * SerialPort_BaudRate, * Combo_Mode;

    QComboBox* Combo_Configs[4];
    QComboBox* Combo_Range;  // 新的范围选择下拉框，替代 Combo_Configs[1], [2], [3]

    QCheckBox* CheckBox_SaveCSV, * CheckBox_EnablePlot;
    QCheckBox* CheckBox_PausePlot, * CheckBox_AutoScale;
    QCheckBox* CheckBox_Crosshair, * CheckBox_Measurement;
    QCheckBox* CheckBox_ChannelVisible[3];
    QLineEdit* Edit_CSVDirectory;
    QLabel* Label_CSVStatus;
    QLabel* Label_ConnectionStatus;
    QLabel* Label_DataRate;
    QLabel* Label_FrameStatus;
    QLabel* Label_SampleStatus;
    QLabel* Label_PeakStatus;
    QLabel* Label_RenderStatus;
    QLabel* Label_PlaybackFile;
    QLabel* Label_CursorReadout;
    QLabel* Label_Measurement;
    QPushButton* Btn_FitChart;
    QPushButton* Btn_ResetZoom;
    QPushButton* Btn_ExportChart;
    QPushButton* Btn_SavePreset;
    QPushButton* Btn_LoadPreset;
    QPushButton* Btn_LoadCSV;
    QPushButton* Btn_PlayPauseCSV;
    QPushButton* Btn_OpenRecentCSV;
    QComboBox* Combo_PlaybackSpeed;
    QComboBox* Combo_RecentCSV;
    QSlider* Slider_Playback;
    QLineEdit* Edit_XRange;
    QLineEdit* Edit_RenderPoints;
    QLineEdit* Edit_XMin, * Edit_XMax;
    QLineEdit* Edit_YMin, * Edit_YMax;
    QLineEdit* Edit_YRightMin;
    QLineEdit* Edit_YRightMax;
    QScrollBar* ScrollBar_X;

    QDoubleSpinBox* Spin_Floats[6];
    QLabel* Label_Floats[6];

    // 图表与逻辑
    ChartInteractionView* chartView;
    QValueAxis* axisX, * axisY;
    QValueAxis* axisYRight;
    QList<QLineSeries*> seriesList;
    PlotDataBuffer m_plotBuffer;
    ProtocolFrameParser m_frameParser;
    QSerialPort* serialPort;
    QStringList lastPortList;

    // 全局时间跟踪
    uint32_t ivSamplingRate = 0;
    uint32_t lightSamplingRate = 0;
    uint64_t globalSamplePairCount = 0;
    uint64_t globalOpticalSampleCount = 0;
    double m_ivTimeSeconds = 0.0;
    double m_lightTimeSeconds = 0.0;

    // CSV 保存相关成员
    CsvRecorder m_csvRecorder;
    QTimer* m_csvFlushTimer;
    QTimer* m_processTimer;
    QTimer* m_plotRefreshTimer;
    QTimer* m_statusTimer;

    CsvPlaybackData m_playbackData;
    QFutureWatcher<CsvPlaybackLoadResult>* m_csvLoadWatcher;
    QString m_pendingCsvPath;
    QTimer* m_playbackTimer;
    int m_playbackRowIndex = 0;
    double m_playbackTimeSeconds = 0.0;
    bool m_playbackActive = false;
    QStringList m_recentCsvFiles;

    struct MeasurementPoint {
        bool valid = false;
        double timeSeconds = 0.0;
        std::array<bool, PlotDataBuffer::ChannelCount> hasValue{};
        std::array<double, PlotDataBuffer::ChannelCount> values{};
    };
    MeasurementPoint m_measurementPointA;
    MeasurementPoint m_measurementPointB;
    int m_measurementClickCount = 0;

    QElapsedTimer m_dataRateTimer;
    quint64 m_receivedBytes = 0;
    quint64 m_lastStatusBytes = 0;
    quint64 m_lastStatusFrames = 0;
    double m_lastPlotRenderMilliseconds = 0.0;
    int m_lastRenderedPoints = 0;

    std::array<std::array<double, 6>, 4> m_modeValues;
    int m_currentMode = -1;
    quint64 m_lastReportedFrames = 0;
};

#endif // SERIALPORTASSISTANT_H
