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
#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QtCharts/QValueAxis>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QTimer>
#include <QVector>
#include <array>

#include "CsvRecorder.h"
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
    void updatePlotSeries();
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

    // UI 组件
    QPlainTextEdit* SerialPort_ReceiveAear;
    QPushButton* SerialPort_Connect, * SerialPort_Disonnect, * SerialPort_Send, * Btn_ResetPlot;
    QComboBox* SerialPort_Number, * SerialPort_BaudRate, * Combo_Mode;

    QComboBox* Combo_Configs[4];
    QComboBox* Combo_Range;  // 新的范围选择下拉框，替代 Combo_Configs[1], [2], [3]

    QCheckBox* CheckBox_SaveCSV, * CheckBox_EnablePlot;
    QLineEdit* Edit_CSVDirectory;
    QLabel* Label_CSVStatus;
    QLineEdit* Edit_XRange;
    QLineEdit* Edit_XMin, * Edit_XMax;
    QLineEdit* Edit_YMin, * Edit_YMax;
    QLineEdit* Edit_YRightMin;
    QLineEdit* Edit_YRightMax;
    QScrollBar* ScrollBar_X;

    QDoubleSpinBox* Spin_Floats[6];
    QLabel* Label_Floats[6];

    // 图表与逻辑
    QChartView* chartView;
    QValueAxis* axisX, * axisY;
    QValueAxis* axisYRight;
    QList<QLineSeries*> seriesList;
    QVector<QPointF> m_plotData[3];
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

    std::array<std::array<double, 6>, 4> m_modeValues;
    int m_currentMode = -1;
    quint64 m_lastReportedFrames = 0;
};

#endif // SERIALPORTASSISTANT_H
