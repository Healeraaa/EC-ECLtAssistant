#include "../CsvPlaybackData.h"

#include <QCoreApplication>
#include <QFile>
#include <QFutureWatcher>
#include <QTemporaryDir>
#include <QTextStream>
#include <QTimer>
#include <QtConcurrent/QtConcurrentRun>
#include <iostream>

int main(int argc, char* argv[])
{
    QCoreApplication application(argc, argv);
    QTemporaryDir temporaryDirectory;
    if (!temporaryDirectory.isValid()) return 1;

    const QString csvPath = temporaryDirectory.filePath("async.csv");
    QFile csvFile(csvPath);
    if (!csvFile.open(QIODevice::WriteOnly | QIODevice::Text)) return 1;
    QTextStream stream(&csvFile);
    stream << "Time(s),Voltage(V),Current(A),OpticalSignal\n";
    for (int index = 0; index < 10000; ++index) {
        stream << QString::number(index * 0.001, 'f', 6) << ",1,2,3\n";
    }
    csvFile.close();

    QFutureWatcher<CsvPlaybackLoadResult> watcher;
    QObject::connect(&watcher, &QFutureWatcher<CsvPlaybackLoadResult>::finished,
                     &application, &QCoreApplication::quit);
    QTimer::singleShot(10000, &application, &QCoreApplication::quit);
    watcher.setFuture(QtConcurrent::run(loadCsvPlaybackFile, csvPath));
    application.exec();

    if (!watcher.isFinished()) {
        std::cerr << "FAILED: asynchronous CSV load timed out\n";
        watcher.waitForFinished();
        return 1;
    }
    const CsvPlaybackLoadResult result = watcher.result();
    if (!result.succeeded || result.data.rows().size() != 10000) {
        std::cerr << "FAILED: asynchronous CSV load mismatch\n";
        return 1;
    }

    std::cout << "Async CSV load tests passed.\n";
    return 0;
}
