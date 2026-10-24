#pragma once

#include <QString>
#include <QVector>

#include "checks.h"

class QPainter;

// The exported report: the conditions, then each pair with its checks and
// structures. One painting function draws both the PDF and the SVG.
struct ReportInput {
    QString name;
    int templateLength = 0;          // 0 in check mode without a template
    int regionStart = -1, regionEnd = -1;
    checks::Settings settings;
    QVector<checks::PairReport> pairs;
    QVector<checks::PrimerReport> singles;  // check mode with one primer
};

namespace report {

// Paints from the top of the page down. With a page height it calls
// newPage() whenever the next block would not fit; without one it draws a
// single tall page. Returns the height used on the last page.
double paint(QPainter *painter, const ReportInput &input, double width, double pageHeight,
             const std::function<void()> &newPage);

bool exportPdf(const ReportInput &input, const QString &path, QString *error);
bool exportSvg(const ReportInput &input, const QString &path, QString *error);

} // namespace report
