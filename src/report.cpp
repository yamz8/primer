#include "report.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QFontMetricsF>
#include <QImage>
#include <QLocale>
#include <QPageSize>
#include <QPainter>
#include <QPdfWriter>
#include <QSaveFile>
#include <QSvgGenerator>
#include <cmath>
#include <functional>

namespace report {
namespace {

constexpr double Margin = 40;
constexpr double PageWidth = 595, PageHeight = 842;  // A4 in points

QString tr(const char *text) { return QCoreApplication::translate("Report", text); }

QColor statusColor(checks::Status s) {
    switch (s) {
    case checks::Status::Pass: return QColor("#2e7d32");
    case checks::Status::Warn: return QColor("#b26a00");
    case checks::Status::Fail: return QColor("#c62828");
    }
    return Qt::black;
}

QString statusLabel(checks::Status s) {
    switch (s) {
    case checks::Status::Pass: return tr("PASS");
    case checks::Status::Warn: return tr("WARN");
    case checks::Status::Fail: return tr("FAIL");
    }
    return {};
}

// Draws in blocks, each kept whole on one page.
class Writer {
public:
    Writer(QPainter *p, double width, double pageHeight, const std::function<void()> &newPage)
        : m_p(p), m_width(width), m_pageHeight(pageHeight), m_newPage(newPage) {
        m_sans = QFont("sans-serif");
        m_sans.setPixelSize(10);
        m_bold = m_sans;
        m_bold.setBold(true);
        m_title = m_bold;
        m_title.setPixelSize(18);
        m_heading = m_bold;
        m_heading.setPixelSize(13);
        m_mono = QFont("monospace");
        m_mono.setStyleHint(QFont::Monospace);
        m_mono.setPixelSize(9);
        m_y = Margin;
    }

    double y() const { return m_y; }

    void ensure(double height) {
        if (m_pageHeight > 0 && m_y + height > m_pageHeight - Margin && m_y > Margin) {
            m_newPage();
            m_y = Margin;
        }
    }

    void text(const QString &s, const QFont &font, const QColor &color = Qt::black, double indent = 0) {
        const QFontMetricsF fm(font);
        const QRectF bounds(Margin + indent, m_y, m_width - 2 * Margin - indent, 10000);
        const QRectF needed = fm.boundingRect(bounds, Qt::TextWordWrap, s);
        ensure(needed.height());
        if (m_p) {
            m_p->setFont(font);
            m_p->setPen(color);
            m_p->drawText(QRectF(bounds.topLeft(), QSizeF(bounds.width(), needed.height())), Qt::TextWordWrap, s);
        }
        m_y += needed.height() + 2;
    }

    void space(double h) { m_y += h; }

    void rule() {
        if (m_p) {
            m_p->setPen(QPen(QColor("#cccccc"), 0.5));
            m_p->drawLine(QPointF(Margin, m_y), QPointF(m_width - Margin, m_y));
        }
        m_y += 6;
    }

    void check(const checks::Check &c, double indent) {
        const QFontMetricsF fm(m_bold);
        const double badge = fm.horizontalAdvance("WARNW");
        ensure(fm.height());
        if (m_p) {
            m_p->setFont(m_bold);
            m_p->setPen(statusColor(c.status));
            m_p->drawText(QPointF(Margin + indent, m_y + fm.ascent()), statusLabel(c.status));
            m_p->setFont(m_sans);
            m_p->setPen(Qt::black);
            m_p->drawText(QPointF(Margin + indent + badge, m_y + fm.ascent()), checks::describe(c));
        }
        m_y += fm.height() + 1;
    }

    void drawing(const QStringList &lines, double indent) {
        const QFontMetricsF fm(m_mono);
        ensure(fm.height() * lines.size() + 4);
        if (m_p) {
            m_p->setFont(m_mono);
            m_p->setPen(QColor("#333333"));
        }
        for (const QString &line : lines) {
            if (m_p)
                m_p->drawText(QPointF(Margin + indent, m_y + fm.ascent()), line);
            m_y += fm.height();
        }
        m_y += 4;
    }

    QFont m_sans, m_bold, m_title, m_heading, m_mono;

private:
    QPainter *m_p;
    double m_width, m_pageHeight;
    std::function<void()> m_newPage;
    double m_y;
};

void primerBlock(Writer &w, const QString &label, const checks::PrimerReport &r) {
    const QLocale locale;
    QFont sequenceFont = w.m_mono;
    sequenceFont.setPixelSize(11);
    sequenceFont.setBold(true);
    w.text(QString("%1  5'-%2-3'").arg(label, QString::fromLatin1(r.sequence)), sequenceFont);
    w.text(tr("%1 nt · Tm %2 °C · GC %3% · 3' ΔG %4 kcal/mol")
               .arg(r.sequence.size())
               .arg(locale.toString(r.tm, 'f', 1), locale.toString(r.gc, 'f', 1), locale.toString(r.endStability, 'f', 1)),
           w.m_sans, QColor("#444444"), 12);
    for (const auto &c : r.checks)
        w.check(c, 12);
    if (r.hairpin.kind != thermo::Structure::None) {
        w.text(tr("Hairpin"), w.m_bold, Qt::black, 12);
        w.drawing(r.hairpin.drawing, 12);
    }
    if (r.selfDimer.kind != thermo::Structure::None) {
        w.text(tr("Self-dimer"), w.m_bold, Qt::black, 12);
        w.drawing(r.selfDimer.drawing, 12);
    }
}

} // namespace

double paint(QPainter *painter, const ReportInput &in, double width, double pageHeight,
             const std::function<void()> &newPage) {
    Writer w(painter, width, pageHeight, newPage);
    const QLocale locale;
    const auto &c = in.settings.conditions;

    w.text(in.name.isEmpty() ? tr("Primer report") : tr("Primer report: %1").arg(in.name), w.m_title);
    w.text(QLocale().toString(QDateTime::currentDateTime(), QLocale::LongFormat), w.m_sans, QColor("#666666"));
    w.space(4);
    if (in.templateLength > 0) {
        QString line = tr("Template: %1 bp").arg(in.templateLength);
        if (in.regionStart >= 0)
            line += " · " + tr("Target %1..%2").arg(in.regionStart + 1).arg(in.regionEnd + 1);
        w.text(line, w.m_sans);
    }
    w.text(tr("Tm by SantaLucia (1998) nearest neighbors · Na+ %1 mM · Mg2+ %2 mM · dNTP %3 mM · primer %4 nM")
               .arg(locale.toString(c.sodium), locale.toString(c.magnesium), locale.toString(c.dntp), locale.toString(c.primer)),
           w.m_sans);
    w.text(tr("These are predictions. Confirm primers in the lab before relying on them."), w.m_bold, QColor("#b26a00"));
    w.space(6);
    w.rule();

    for (int i = 0; i < in.singles.size(); ++i) {
        w.ensure(120);
        primerBlock(w, tr("Primer"), in.singles[i]);
        w.rule();
    }

    for (int i = 0; i < in.pairs.size(); ++i) {
        const auto &p = in.pairs[i];
        w.ensure(160);
        QString heading = tr("Pair %1").arg(i + 1);
        if (p.score > 0)
            heading += " · " + tr("score %1").arg(p.score);
        if (p.start >= 0)
            heading += " · " + tr("%1 bp product (%2..%3)").arg(p.productSize).arg(p.start + 1).arg(p.end + 1);
        w.text(heading, w.m_heading);
        w.space(2);
        primerBlock(w, tr("Forward"), p.forward);
        w.space(2);
        primerBlock(w, tr("Reverse"), p.reverse);
        w.space(2);
        w.text(tr("Pair"), w.m_bold);
        for (const auto &check : p.checks)
            w.check(check, 12);
        if (p.crossDimer.kind != thermo::Structure::None) {
            w.text(tr("Cross-dimer"), w.m_bold, Qt::black, 12);
            w.drawing(p.crossDimer.drawing, 12);
        }
        w.rule();
    }
    return w.y() + Margin;
}

bool exportPdf(const ReportInput &input, const QString &path, QString *error) {
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        *error = QCoreApplication::translate("Checks", "Could not write %1: %2").arg(path, file.errorString());
        return false;
    }
    {
        QPdfWriter pdf(&file);
        pdf.setPageSize(QPageSize(QPageSize::A4));
        pdf.setPageMargins(QMarginsF(0, 0, 0, 0));
        pdf.setResolution(72);
        pdf.setTitle(input.name.isEmpty() ? tr("Primer report") : tr("Primer report: %1").arg(input.name));
        pdf.setCreator("Primer");
        QPainter painter(&pdf);
        painter.setRenderHint(QPainter::Antialiasing);
        paint(&painter, input, PageWidth, PageHeight, [&] { pdf.newPage(); });
    }
    if (!file.commit()) {
        *error = QCoreApplication::translate("Checks", "Could not write %1: %2").arg(path, file.errorString());
        return false;
    }
    return true;
}

bool exportSvg(const ReportInput &input, const QString &path, QString *error) {
    // Measure first, so the single SVG page is exactly as tall as the report.
    const double height = paint(nullptr, input, PageWidth, 0, [] {});
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        *error = QCoreApplication::translate("Checks", "Could not write %1: %2").arg(path, file.errorString());
        return false;
    }
    {
        QSvgGenerator svg;
        svg.setOutputDevice(&file);
        svg.setSize(QSize(int(PageWidth), int(std::ceil(height))));
        svg.setViewBox(QRectF(0, 0, PageWidth, std::ceil(height)));
        svg.setResolution(72);
        svg.setTitle(input.name.isEmpty() ? tr("Primer report") : tr("Primer report: %1").arg(input.name));
        QPainter painter(&svg);
        painter.fillRect(QRectF(0, 0, PageWidth, std::ceil(height)), Qt::white);
        paint(&painter, input, PageWidth, 0, [] {});
    }
    if (!file.commit()) {
        *error = QCoreApplication::translate("Checks", "Could not write %1: %2").arg(path, file.errorString());
        return false;
    }
    return true;
}

} // namespace report
