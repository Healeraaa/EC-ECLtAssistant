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
    uint16_t calculateCRC16(const QByteArray& data);

    // UI 组件
    QPlainTextEdit* SerialPort_ReceiveAear;
    QPushButton* SerialPort_Connect, * SerialPort_Disonnect, * SerialPort_Send, * Btn_ResetPlot;
    QComboBox* SerialPort_Number, * SerialPort_BaudRate, * Combo_Mode;

    // 修改处：将 SpinBox 数组改为 ComboBox 数组以支持下拉选择[cite: 16]
    QComboBox* Combo_Configs[4];

    QCheckBox* CheckBox_SaveCSV, * CheckBox_EnablePlot;
    QLineEdit* Edit_XRange;
    QLineEdit* Edit_XMin, * Edit_XMax;
    QLineEdit* Edit_YMin, * Edit_YMax;
    QScrollBar* ScrollBar_X;

    QDoubleSpinBox* Spin_Floats[6];
    QLabel* Label_Floats[6];

    // 图表与逻辑
    QChartView* chartView;
    QValueAxis* axisX, * axisY;
     QList<QLineSeries*> seriesList;
    QByteArray buffer;
    double plotCount = 0;
    QSerialPort* serialPort;
    QVector<QString> lastPortList;
};

#endif // SERIALPORTASSISTANT_H