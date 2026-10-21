#include "csvloader.h"
#include "csvloader.h"
#include <QFile>
#include <QTextStream>
#include <QStringList>
#include <qdebug.h>
#include <stdexcept>
CsvLoader::CsvLoader() {}

Stm32Data CsvLoader::load(const QString& csvPath)
{
    QFile f(csvPath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        throw std::runtime_error(("Cannot open CSV: " + csvPath).toStdString());

    qInfo() << "[csv] opened:" << csvPath << "size:" << f.size() << "bytes";

    // ── Reject ZIP/XLSX early ──────────────────────────────────────────
    {
        char magic[2] = {0, 0};
        if (f.read(magic, 2) == 2) {
            f.seek(0);
            if (magic[0] == 'P' && magic[1] == 'K') {
                throw std::runtime_error(
                    ("File is a ZIP/XLSX, not CSV: " + csvPath +
                     "\nRe-save it as 'CSV UTF-8' from Excel.").toStdString());
            }
        }
    }

    QTextStream in(&f);
    in.setEncoding(QStringConverter::Utf8);

    Stm32Data data;

    // ── Read all lines (keep raw, decide about trimming later) ────────
    QStringList lines;
    while (!in.atEnd()) {
        QString l = in.readLine();
        if (l.startsWith(QChar(0xFEFF))) l.remove(0, 1);   // strip BOM
        lines << l;
    }

    if (lines.isEmpty()) {
        qWarning() << "[csv] file is empty";
        return data;
    }

    // ── Reproduce the Python parsing, but defensively ────────────────
    //
    // Expected (from classify_stm32.py):
    //   line 0: header "heure_utc;latitude;longitude;altitude_m"
    //   line 1: GPS data  "...;...;...;..."
    //   line 2: header "index;raw;filtered"
    //   line 3+: "<i>;<raw>;<filtered>"
    //
    // But: detect header presence; don't blindly assume line numbers.

    constexpr int kMinFields = 3;

    // --- Detect separator (Python uses ';' hard-coded) ---
    QChar sep = ';';
    for (const QString& l : lines) {
        if (l.trimmed().isEmpty()) continue;
        if (l.contains(';')) { sep = ';'; break; }
        if (l.contains(',')) { sep = ','; break; }
    }

    qInfo() << "[csv] first non-empty line:" << lines.first().left(80)
            << "  sep:" << sep;

    // --- Look for a header line with a "filtered" column ---
    int filteredIdx = -1;
    int headerLineIdx = -1;

    for (int i = 0; i < std::min<int>(5, lines.size()); ++i) {
        QStringList cols = lines[i].split(sep);
        for (int c = 0; c < cols.size(); ++c) {
            if (cols[c].trimmed().compare("filtered", Qt::CaseInsensitive) == 0) {
                filteredIdx = c;
                headerLineIdx = i;
                break;
            }
        }
        if (filteredIdx >= 0) break;
    }

    int dataStart = 0;
    if (filteredIdx >= 0) {
        qInfo() << "[csv] 'filtered' header found on line"
                << headerLineIdx << "  col" << filteredIdx;
        dataStart = headerLineIdx + 1;
    } else {
        // No header at all — assume 3-column `index;raw;filtered`,
        // 2-column `raw;filtered`, or 1-column `filtered`
        qInfo() << "[csv] no 'filtered' header; assuming last column of data";
        // Try to infer the column count from the first non-empty data line
        for (int i = 0; i < lines.size(); ++i) {
            if (lines[i].trimmed().isEmpty()) continue;
            QStringList probe = lines[i].split(sep);
            if (probe.size() >= 2) filteredIdx = probe.size() - 1;
            else                   filteredIdx = 0;
            dataStart = i;
            break;
        }
    }

    // --- Attempt to parse GPS line (line 1, matching Python) ---
    if (lines.size() >= 2 && lines[1].contains(sep)) {
        QStringList p = lines[1].split(sep);
        if (p.size() >= 4) {
            data.gpsInfo = QString("Heure=%1 | LAT=%2 | LON=%3 | ALT=%4m")
            .arg(p[0].trimmed(), p[1].trimmed(),
                 p[2].trimmed(), p[3].trimmed());
        }
    }
    if (data.gpsInfo.isEmpty()) data.gpsInfo = "N/A";

    // --- Parse data rows starting at dataStart ---
    int rows = 0, parsed = 0, skipped = 0;
    for (int i = dataStart; i < lines.size(); ++i) {
        const QString& raw = lines[i];
        if (raw.trimmed().isEmpty()) continue;
        ++rows;

        QStringList cols = raw.split(sep);
        if (filteredIdx >= cols.size()) {
            // Fallback: try last column
            if (cols.isEmpty()) { ++skipped; continue; }
            filteredIdx = cols.size() - 1;
        }

        bool ok = false;
        float v = cols[filteredIdx].trimmed().toFloat(&ok);
        if (!ok) {
            // Try last column as a last resort
            v = cols.last().trimmed().toFloat(&ok);
            if (!ok) { ++skipped; continue; }
        }
        data.signal.push_back(v);
        ++parsed;
    }

    qInfo() << "[csv] rows read:" << rows
            << "parsed:" << parsed
            << "skipped:" << skipped
            << "signal.size():" << data.signal.size();

    // --- Sanity: min/max to catch wrong columns / wrong scale ---
    if (!data.signal.empty()) {
        auto mm = std::minmax_element(data.signal.begin(), data.signal.end());
        qInfo() << "[csv] filtered min:" << *mm.first << "max:" << *mm.second;
        if (*mm.first < 0.5f && *mm.second < 1.5f) {
            qWarning() << "[csv] WARNING: values look normalized (0..1), not ADC (0..4095)."
                       << "Model expects values around 2000.";
        }
    }

    return data;
}