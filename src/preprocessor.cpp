#include "preprocessor.h"
#include <cmath>
Preprocessor::Preprocessor() {}

// Linear interpolation resampling (adequate for ECG here;
// replace with libsamplerate if you need exact FFT polyphase).
static std::vector<float> resampleLinear(const std::vector<float>& in,
                                         int outSize) {
    std::vector<float> out(outSize);
    const double ratio = double(in.size() - 1) / double(outSize - 1);
    for (int i = 0; i < outSize; ++i) {
        double pos = i * ratio;
        int    i0  = int(std::floor(pos));
        int    i1  = std::min(i0 + 1, int(in.size()) - 1);
        double t   = pos - i0;
        out[i] = float((1.0 - t) * in[i0] + t * in[i1]);
    }
    return out;
}

std::vector<std::array<float, Preprocessor::WINDOW_MITBIH>>
Preprocessor::process(const std::vector<float>& rawSignal) {
    // 1. Center
    std::vector<float> centered(rawSignal.size());
    for (size_t i = 0; i < rawSignal.size(); ++i)
        centered[i] = rawSignal[i] - ADC_BASELINE;

    // 2. Resample 1000 Hz -> 360 Hz
    int n360 = int(centered.size() * double(MITBIH_FS) / double(STM32_FS));
    std::vector<float> s360 = resampleLinear(centered, n360);

    // 3. Window with 50% overlap
    const int step = WINDOW_MITBIH / 2;
    std::vector<std::array<float, WINDOW_MITBIH>> windows;
    for (int i = 0; i + WINDOW_MITBIH <= n360; i += step) {
        std::array<float, WINDOW_MITBIH> w{};
        double mean = 0, sq = 0;
        for (int k = 0; k < WINDOW_MITBIH; ++k) {
            w[k] = s360[i + k];
            mean += w[k];
        }
        mean /= WINDOW_MITBIH;
        for (int k = 0; k < WINDOW_MITBIH; ++k) {
            double d = w[k] - mean;
            sq += d * d;
        }
        double stddev = std::sqrt(sq / WINDOW_MITBIH) + 1e-8;
        for (int k = 0; k < WINDOW_MITBIH; ++k)
            w[k] = float((w[k] - mean) / stddev);
        windows.push_back(w);
    }
    return windows;
}