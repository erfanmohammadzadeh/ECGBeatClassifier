#ifndef ONNXCLASSIFIER_H
#define ONNXCLASSIFIER_H
#include <QString>
#include <vector>
#include <array>
#include <string>
#include <onnxruntime_cxx_api.h>
#include "preprocessor.h"

class OnnxClassifier
{
public:
    struct Prediction {
        int    label;
        float  confidence;
    };
    OnnxClassifier();

    OnnxClassifier(const QString& modelPath, const QString& classesPath);

    std::vector<Prediction>
    predict(const std::vector<std::array<float, Preprocessor::WINDOW_MITBIH>>& windows);
    const std::vector<std::string>& classes() const { return m_classes; }

private:
    Ort::Env                              m_env{ORT_LOGGING_LEVEL_WARNING, "ecg"};
    Ort::Session                          m_session{nullptr};
    std::vector<std::string>              m_classes;
    std::vector<const char*>              m_inputNames;
    std::vector<const char*>              m_outputNames;
    std::vector<std::string>              m_inputNameStore;
    std::vector<std::string>              m_outputNameStore;
};

#endif // ONNXCLASSIFIER_H
