#ifndef CSVLOADER_H
#define CSVLOADER_H
#include <QString>
#include <vector>

struct Stm32Data {
    std::vector<float> signal;   // filtered
    QString gpsInfo;
};

class CsvLoader
{
public:
    CsvLoader();
    static Stm32Data load(const QString& csvPath);
};

#endif // CSVLOADER_H
