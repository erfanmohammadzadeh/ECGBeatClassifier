#include "src/ecgclassifier.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QFileInfo>
#include <QDir>
int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QCoreApplication::setApplicationName("ecg_classifier");

    // ─── Hard-coded paths ────────────────────────────────────────
    // All relative paths are anchored to the exe folder, so the app
    // works regardless of the current working directory.
    const QDir appDir(QCoreApplication::applicationDirPath());

    const QString csvPath     = appDir.absoluteFilePath("beat.csv");
    const QString modelPath   = appDir.absoluteFilePath("res/ecg_cnn.onnx");
    const QString classesPath = appDir.absoluteFilePath("res/classes.txt");

    // ─── Sanity checks ───────────────────────────────────────────
    if (!QFileInfo::exists(csvPath)) {
        qCritical("CSV not found: %s", qPrintable(csvPath));
        return 1;
    }
    if (!QFileInfo::exists(modelPath)) {
        qCritical("Model not found: %s", qPrintable(modelPath));
        return 1;
    }
    if (!QFileInfo::exists(classesPath)) {
        qCritical("Classes file not found: %s", qPrintable(classesPath));
        return 1;
    }

    qInfo() << "csvPath    =" << csvPath;
    qInfo() << "modelPath  =" << modelPath;
    qInfo() << "classesPath=" << classesPath;

    EcgClassifier w(csvPath, modelPath, classesPath);
    w.show();
    return app.exec();
}
