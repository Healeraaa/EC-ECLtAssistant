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
#include <QDataStream>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QTimer>
#include <QMap>
#include <cmath>

QT_CHARTS_USE_NAMESPACE

// 用于统一 CSV 输出的数据结构
struct CsvRow {
    double voltage = NAN;
    double current = NAN;
    double optical = NAN;
};

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
    uint16_t calculateCRC16(const QByteArray& data);
    void processBufferOptimized();

    // CSV 保存相关
    void startCSVLogging();
    void stopCSVLogging();
    void flushCSVBuffer();
    void onSaveCSVToggled(bool checked);

    // UI 组件
    QPlainTextEdit* SerialPort_ReceiveAear;
    QPushButton* SerialPort_Connect, * SerialPort_Disonnect, * SerialPort_Send, * Btn_ResetPlot;
    QComboBox* SerialPort_Number, * SerialPort_BaudRate, * Combo_Mode;
    QComboBox* Combo_Configs[4];
    QCheckBox* CheckBox_SaveCSV, * CheckBox_EnablePlot;
    QLineEdit* Edit_XRange, * Edit_XMin, * Edit_XMax, * Edit_YMin, * Edit_YMax;
    QLineEdit* Edit_YRightMin, * Edit_YRightMax;
    QScrollBar* ScrollBar_X;
    QDoubleSpinBox* Spin_Floats[6];
    QLabel* Label_Floats[6];

    // 图表与逻辑
    QChartView* chartView;
    QValueAxis* axisX, * axisY, * axisYRight;
    QList<QLineSeries*> seriesList;
    QByteArray buffer;
    QSerialPort* serialPort;
    QVector<QString> lastPortList;

    // 全局时间跟踪
    uint32_t baseSamplingRate = 0;
    uint64_t globalSamplePairCount = 0;
    uint64_t globalOpticalSampleCount = 0;

    // CSV 保存相关成员
    QFile* m_csvFile = nullptr;
    QTextStream* m_csvStream = nullptr;
    QTimer* m_csvFlushTimer;
    bool m_isSaving = false;

    // 统一时间轴 CSV 容器（时间微秒 → 数据行）
    QMap<qint64, CsvRow> m_csvMap;
};

#endif // SERIALPORTASSISTANT_H