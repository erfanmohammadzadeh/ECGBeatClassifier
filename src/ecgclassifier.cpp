// ecgclassifier.cpp
#include "ecgclassifier.h"
#include "csvloader.h"
#include "preprocessor.h"
#include "onnxclassifier.h"

#include <QtCharts/QLineSeries>
#include <QtCharts/QBarSeries>
#include <QtCharts/QBarSet>
#include <QtCharts/QBarCategoryAxis>
#include <QtCharts/QValueAxis>
#include <QVBoxLayout>
#include <QWidget>
#include <QLabel>
#include <QMap>
#include <QDebug>

static const QMap<QString, QString> CLASSES_FULL = {
    { "N", "Normal" },
    { "S", "Supraventriculaire (SVEB)" },
    { "V", "Ventriculaire (VEB)" },
    { "F", "Fusion" },
    { "Q", "Inconnu / Paced" }
};

EcgClassifier::EcgClassifier(const QString& csvPath, const QString &modelPath, const QString &classPath, QWidget* parent)
    : QMainWindow(parent)
{
    m_modelPath = modelPath;
    m_classPath = classPath;
    setWindowTitle("ECG Classifier — " + csvPath);
    resize(1200, 800);
    runClassification(csvPath);
}

void EcgClassifier::runClassification(const QString& csvPath) {
    // 1. Load
    Stm32Data data;
    try {
        data = CsvLoader::load(csvPath);
    } catch (const std::exception& e) {
        qCritical() << "Load error:" << e.what();
        return;
    }

    qInfo() << "GPS info:" << data.gpsInfo;
    qInfo() << "Signal length:" << data.signal.size()
            << "samples at" << Preprocessor::STM32_FS << "Hz ("
            << (double(data.signal.size()) / Preprocessor::STM32_FS) << " s)";

    // 2. Preprocess
    auto windows = Preprocessor::process(data.signal);
    qInfo() << "Windows:" << windows.size() << "x" << Preprocessor::WINDOW_MITBIH;

    try {
        OnnxClassifier clf(m_modelPath, m_classPath);
        auto preds = clf.predict(windows);
        constexpr float CONF_THRESHOLD = 0.60f;

        for (size_t i = 0; i < preds.size(); ++i) {
            const auto& p = preds[i];
            QString name = (p.label >= 0 && p.label < int(clf.classes().size()))
                               ? QString::fromStdString(clf.classes()[p.label])
                               : QString("?");
            qInfo().noquote() << QString("[pred %1] class=%2 (%3)  conf=%4%")
                                     .arg(i)
                                     .arg(name)
                                     .arg(CLASSES_FULL.value(name, name))
                                     .arg(p.confidence * 100.0, 0, 'f', 1);
        }

        std::vector<OnnxClassifier::Prediction> kept;
        double sumConf = 0.0;
        for (auto& p : preds)
            if (p.confidence >= CONF_THRESHOLD) {
                kept.push_back(p);
                sumConf += p.confidence;
            }

        int nKept = int(kept.size());
        double keptRatio = preds.empty() ? 0.0 :
                               double(nKept) / double(preds.size()) * 100.0;
        qInfo() << QString("Kept windows (conf >= %1%) : %2/%3 (%4%)")
                       .arg(CONF_THRESHOLD * 100)
                       .arg(nKept).arg(preds.size())
                       .arg(keptRatio, 0, 'f', 1);

        // 5. Counts
        const int numClasses = int(clf.classes().size());
        std::vector<int> counts(numClasses, 0);
        int total = std::max(nKept, 1);
        for (auto& p : kept) counts[p.label]++;

        // 6. Dominant class
        int domIdx = 0;
        for (int i = 1; i < numClasses; ++i)
            if (counts[i] > counts[domIdx]) domIdx = i;
        double domPct  = double(counts[domIdx]) / total * 100.0;
        double meanConf = nKept ? sumConf / nKept * 100.0 : 0.0;

        // 7. Console summary
        qInfo() << "----- CLASSIFICATION RESULTS -----";
        for (int i = 0; i < numClasses; ++i) {
            double pct = double(counts[i]) / total * 100.0;
            QString name = QString::fromStdString(clf.classes()[i]);
            qInfo().noquote() << QString("%1 (%2) : %3%")
                                     .arg(name, -8)
                                     .arg(CLASSES_FULL.value(name, name), -30)
                                     .arg(pct, 5, 'f', 1);
        }
        QString domName = QString::fromStdString(clf.classes()[domIdx]);
        qInfo().noquote() << QString("DOMINANT: %1 — %2")
                                 .arg(domName, CLASSES_FULL.value(domName, domName));
        qInfo().noquote() << QString("Presence : %1%   Mean conf : %2%")
                                 .arg(domPct, 0, 'f', 1)
                                 .arg(meanConf, 0, 'f', 1);

        buildUi(data.signal, data.gpsInfo, clf.classes(),
                counts, domIdx, domPct, meanConf);
    } catch (const std::exception& e) {
        qCritical() << "[classify] error:" << e.what();

        // Show something in the window so the user isn't stuck with white
        auto* central = new QWidget(this);
        auto* v = new QVBoxLayout(central);
        v->addWidget(new QLabel(
            QString("<b>Classification failed</b><br><br>%1<br><br>"
                    "Model: %2<br>Classes: %3")
                .arg(e.what(), m_modelPath, m_classPath)));
        setCentralWidget(central);
    }
}

void EcgClassifier::buildUi(const std::vector<float>& signal,
                            const QString& gpsInfo,
                            const std::vector<std::string>& classes,
                            const std::vector<int>& counts,
                            int dominantIdx,
                            double dominantPct,
                            double meanConf)
{
    auto* central = new QWidget(this);
    auto* layout  = new QVBoxLayout(central);

    auto* header = new QLabel(
        QString("GPS : %1\nDominant : %2 (%3%) — Mean conf : %4%")
            .arg(gpsInfo)
            .arg(QString::fromStdString(classes[dominantIdx]))
            .arg(dominantPct, 0, 'f', 1)
            .arg(meanConf, 0, 'f', 1),
        central);
    header->setStyleSheet("font-size: 12px; padding: 6px; background:#F0F4F8;");
    layout->addWidget(header);

    // ---- Signal plot ----
    auto* lineSeries = new QLineSeries();
    for (size_t i = 0; i < signal.size(); ++i) {
        double t = double(i) / Preprocessor::STM32_FS;
        lineSeries->append(t, signal[i] - Preprocessor::ADC_BASELINE);
    }
    auto* sigChart = new QChart();
    sigChart->addSeries(lineSeries);
    sigChart->setTitle("ECG Signal (filtered, centered)");
    sigChart->createDefaultAxes();
    sigChart->axes(Qt::Horizontal).first()->setTitleText("Time (s)");
    sigChart->axes(Qt::Vertical).first()->setTitleText("Amplitude (ADC - 2000)");
    m_signalView = new QChartView(sigChart, central);
    m_signalView->setRenderHint(QPainter::Antialiasing);
    layout->addWidget(m_signalView, 1);

    // ---- Classification bar chart ----
    int total = 0;
    for (int c : counts) total += c;
    if (total == 0) total = 1;

    auto* barSet = new QBarSet("Windows (%)");
    QStringList categories;
    QColor domColor("#1D9E75");
    QColor otherColor("#E6F1FB");

    for (size_t i = 0; i < classes.size(); ++i) {
        double pct = double(counts[i]) / total * 100.0;
        *barSet << pct;
        QString name = QString::fromStdString(classes[i]);
        categories << QString("%1\n%2").arg(name, CLASSES_FULL.value(name, name));
    }
    // Color bars individually
    if (dominantIdx < barSet->count())
        barSet->setColor(domColor);
    else
        barSet->setColor(otherColor);

    auto* barSeries = new QBarSeries();
    barSeries->append(barSet);

    auto* clsChart = new QChart();
    clsChart->addSeries(barSeries);
    clsChart->setTitle("Detected class distribution");
    auto* axisX = new QBarCategoryAxis();
    axisX->append(categories);
    clsChart->addAxis(axisX, Qt::AlignBottom);
    barSeries->attachAxis(axisX);

    auto* axisY = new QValueAxis();
    axisY->setRange(0, 100);
    axisY->setTitleText("Windows (%)");
    clsChart->addAxis(axisY, Qt::AlignLeft);
    barSeries->attachAxis(axisY);

    m_classesView = new QChartView(clsChart, central);
    m_classesView->setRenderHint(QPainter::Antialiasing);
    layout->addWidget(m_classesView, 1);

    setCentralWidget(central);
}