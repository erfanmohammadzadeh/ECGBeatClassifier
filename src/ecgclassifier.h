#ifndef ECGCLASSIFIER_H
#define ECGCLASSIFIER_H

// ecgclassifier.h
#pragma once
#include <QMainWindow>
#include <QtCharts/QChartView>
#include <QtCharts/QChart>

class EcgClassifier : public QMainWindow {
    Q_OBJECT
public:
    explicit EcgClassifier(const QString& csvPath, const QString& modelPath, const QString& classPath, QWidget* parent = nullptr);

private:
    void runClassification(const QString& csvPath);
    void buildUi(const std::vector<float>& signal,
                 const QString& gpsInfo,
                 const std::vector<std::string>& classes,
                 const std::vector<int>& counts,
                 int dominantIdx,
                 double dominantPct,
                 double meanConf);

    QChartView* m_signalView  = nullptr;
    QChartView* m_classesView = nullptr;

    QString m_modelPath;
    QString m_classPath;
};

#endif // ECGCLASSIFIER_H
