#pragma once
#ifndef _SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING
#define _SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING
#endif

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
#include <QDateTime>
#include <QFile>
#include <QCoreApplication>
#include <QList>
#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QtCharts/QValueAxis>
#include <algorithm> 

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

    QPlainTextEdit* SerialPort_ReceiveAear, * SerialPort_SendAear;
    QPushButton* SerialPort_Connect, * SerialPort_Disonnect, * SerialPort_Send, * Btn_ClearLog, * Btn_ResetPlot;
    QComboBox* SerialPort_Number, * SerialPort_BaudRate, * SerialPort_DataBits, * SerialPort_StopBits, * SerialPort_CheckBits;
    QCheckBox* CheckBox_Timestamp, * CheckBox_SaveCSV, * CheckBox_AutoScale, * CheckBox_AutoScroll, * CheckBox_EnablePlot;
    QLineEdit* Edit_YMax, * Edit_YMin, * Edit_XRange;
    QScrollBar* ScrollBar_X;

    QChartView* chartView;
    QValueAxis* axisX, * axisY;
    QList<QLineSeries*> seriesList;
    QByteArray buffer;
    double plotCount = 0;
    int perfCounter = 0;
    QSerialPort* serialPort;
    QFile csvFile; // 核心：CSV文件句柄
    QVector<QString> lastPortList;
};