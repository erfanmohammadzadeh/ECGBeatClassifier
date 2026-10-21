#include "onnxclassifier.h"
#include <QFile>
#include <QTextStream>
#include <stdexcept>
OnnxClassifier::OnnxClassifier(const QString& modelPath,
                               const QString& classesPath)
{
#ifdef _WIN32
    m_session = Ort::Session(m_env, modelPath.toStdWString().c_str(), Ort::SessionOptions{nullptr});
#else
    m_session = Ort::Session(m_env, modelPath.toStdString().c_str(), Ort::SessionOptions{nullptr});
#endif
    // Load classes file (one label per line)
    QFile f(classesPath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        throw std::runtime_error("Cannot open classes file");
    QTextStream in(&f);
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (!line.isEmpty()) m_classes.push_back(line.toStdString());
    }

    Ort::AllocatorWithDefaultOptions alloc;
    size_t nIn  = m_session.GetInputCount();
    size_t nOut = m_session.GetOutputCount();
    for (size_t i = 0; i < nIn; ++i) {
        auto name = m_session.GetInputNameAllocated(i, alloc);
        m_inputNameStore.emplace_back(name.get());
    }
    for (size_t i = 0; i < nOut; ++i) {
        auto name = m_session.GetOutputNameAllocated(i, alloc);
        m_outputNameStore.emplace_back(name.get());
    }
    for (auto& s : m_inputNameStore)  m_inputNames.push_back(s.c_str());
    for (auto& s : m_outputNameStore) m_outputNames.push_back(s.c_str());
}

std::vector<OnnxClassifier::Prediction>
OnnxClassifier::predict(
    const std::vector<std::array<float, Preprocessor::WINDOW_MITBIH>>& windows)
{
    std::vector<Prediction> out;
    if (windows.empty()) return out;

    const int N   = int(windows.size());
    const int LEN = Preprocessor::WINDOW_MITBIH;

    std::vector<float> flat;
    flat.reserve(size_t(N) * LEN);
    for (const auto& w : windows)
        for (float v : w) flat.push_back(v);

    std::vector<int64_t> shape = { N, LEN, 1 };
    Ort::MemoryInfo memInfo = Ort::MemoryInfo::CreateCpu(
        OrtArenaAllocator, OrtMemTypeDefault);

    Ort::Value input = Ort::Value::CreateTensor<float>(
        memInfo, flat.data(), flat.size(),
        shape.data(), shape.size());

    auto outputs = m_session.Run(
        Ort::RunOptions{nullptr},
        m_inputNames.data(), &input, 1,
        m_outputNames.data(), m_outputNames.size());

    auto info   = outputs[0].GetTensorTypeAndShapeInfo();
    auto oshape = info.GetShape();
    const int numClasses = int(oshape.back());
    const float* probs = outputs[0].GetTensorData<float>();

    for (int i = 0; i < N; ++i) {
        int   best  = 0;
        float bestP = probs[i * numClasses];
        for (int c = 1; c < numClasses; ++c) {
            float p = probs[i * numClasses + c];
            if (p > bestP) { bestP = p; best = c; }
        }
        out.push_back({ best, bestP });
    }
    return out;
}