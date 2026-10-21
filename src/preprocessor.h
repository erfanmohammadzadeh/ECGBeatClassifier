#ifndef PREPROCESSOR_H
#define PREPROCESSOR_H
#include <vector>
#include <array>

class Preprocessor
{
public:
    Preprocessor();
    static constexpr int   STM32_FS      = 178;
    static constexpr int   MITBIH_FS     = 360;
    static constexpr int   WINDOW_MITBIH = 360;
    static constexpr float ADC_BASELINE  = 0;

    // Returns a flat vector: N windows × WINDOW_MITBIH
    static std::vector<std::array<float, WINDOW_MITBIH>>
    process(const std::vector<float>& rawSignal);
};

#endif // PREPROCESSOR_H
