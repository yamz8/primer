#include "sequenceview.h"

#include <QCursor>
#include <QFontMetricsF>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QPainter>
#include <QPainterPath>
#include <cmath>

namespace {
constexpr int Group = 10;
constexpr qreal Padding = 16;
}

SequenceView::SequenceView(QQuickItem *parent) : QQuickPaintedItem(parent) {
    m_font = QFont("monospace");
    m_font.setStyleHint(QFont::Monospace);
    m_font.setPixelSize(15);
    setAcceptedMouseButtons(Qt::LeftButton);
    setCursor(Qt::IBeamCursor);
    setAntialiasing(true);
    relayout();
}

void SequenceView::setSequence(const QString &sequence) {
    if (sequence == m_sequence)
        return;
    m_sequence = sequence;
    m_contentY = 0;
    relayout();
    emit sequenceChanged();
    emit contentYChanged();
}

void SequenceView::setSelectionStart(int start) {
    if (start == m_selectionStart)
        return;
    m_selectionStart = start;
    emit selectionChanged();
    update();
}

void SequenceView::setSelectionEnd(int end) {
    if (end == m_selectionEnd)
        return;
    m_selectionEnd = end;
    emit selectionChanged();
    update();
}

void SequenceView::setMarks(const QVariantList &marks) {
    m_marksSource = marks;
    m_marks.clear();
    for (const QVariant &v : marks) {
        const QVariantMap m = v.toMap();
        m_marks.append({m.value("start").toInt(), m.value("end").toInt(), m.value("kind").toString()});
    }
    emit marksChanged();
    update();
}

void SequenceView::setAccent(const QColor &c) {
    m_accent = c;
    emit colorsChanged();
    update();
}

void SequenceView::setAccentForeground(const QColor &c) {
    m_accentForeground = c;
    emit colorsChanged();
    update();
}

void SequenceView::setFontPixelSize(int size) {
    m_font.setPixelSize(size);
    relayout();
}

void SequenceView::setContentY(qreal y) {
    const qreal limit = qMax<qreal>(0, contentHeight() - height());
    y = qBound<qreal>(0, y, limit);
    if (qFuzzyCompare(y + 1, m_contentY + 1))
        return;
    m_contentY = y;
    emit contentYChanged();
    update();
}

qreal SequenceView::contentHeight() const {
    const int rows = (int(m_sequence.size()) + m_perLine - 1) / m_perLine;
    return rows * rowHeight() + 2 * Padding;
}

qreal SequenceView::columnX(int column) const {
    return m_gutter + (column + column / Group) * m_charWidth;
}

void SequenceView::relayout() {
    const QFontMetricsF fm(m_font);
    m_charWidth = fm.horizontalAdvance('G');
    m_lineHeight = std::ceil(fm.height() * 1.15);
    m_rulerFont = m_font;
    m_rulerFont.setPixelSize(qMax(9, m_font.pixelSize() - 4));
    m_rulerHeight = std::ceil(QFontMetricsF(m_rulerFont).height()) + 2;
    m_gap = std::ceil(m_lineHeight * 0.55);
    const int digits = int(QString::number(qMax<qsizetype>(1, m_sequence.size())).size());
    m_gutter = Padding + (digits + 2) * m_charWidth;
    // Whole groups of ten, each ten bases and a space wide.
    const int groups = qMax(1, int((width() - m_gutter - Padding) / ((Group + 1) * m_charWidth)));
    m_perLine = groups * Group;
    setContentY(m_contentY);
    emit layoutChanged();
    update();
}

void SequenceView::geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry) {
    QQuickPaintedItem::geometryChange(newGeometry, oldGeometry);
    if (newGeometry.size() != oldGeometry.size())
        relayout();
}

int SequenceView::indexAt(qreal x, qreal y) const {
    if (m_sequence.isEmpty())
        return -1;
    const int row = qMax(0, int((y + m_contentY - Padding) / rowHeight()));
    const qreal cells = (x - m_gutter) / m_charWidth;
    int column = 0;
    if (cells > 0) {
        const int cell = int(cells);
        const int group = cell / (Group + 1), within = cell % (Group + 1);
        column = group * Group + qMin(within, Group - 1);
    }
    column = qBound(0, column, m_perLine - 1);
    return qBound(0, row * m_perLine + column, int(m_sequence.size()) - 1);
}

void SequenceView::ensureVisible(int index) {
    if (index < 0 || m_perLine <= 0)
        return;
    const qreal top = Padding + (index / m_perLine) * rowHeight();
    if (top < m_contentY || top + rowHeight() > m_contentY + height())
        setContentY(top - height() / 3);
}

void SequenceView::mousePressEvent(QMouseEvent *event) {
    m_anchor = indexAt(event->position().x(), event->position().y());
    if (m_anchor < 0)
        return;
    // Keeps the drag when the view sits inside something that flicks.
    setKeepMouseGrab(true);
    m_selectionStart = m_selectionEnd = m_anchor;
    emit selectionChanged();
    update();
}

void SequenceView::mouseMoveEvent(QMouseEvent *event) {
    if (m_anchor < 0)
        return;
    const qreal y = event->position().y();
    // Dragging past an edge scrolls.
    if (y < 0)
        setContentY(m_contentY - rowHeight() / 2);
    else if (y > height())
        setContentY(m_contentY + rowHeight() / 2);
    const int index = indexAt(event->position().x(), y);
    m_selectionStart = qMin(m_anchor, index);
    m_selectionEnd = qMax(m_anchor, index);
    emit selectionChanged();
    update();
}

void SequenceView::mouseReleaseEvent(QMouseEvent *) {
    if (m_anchor < 0)
        return;
    m_anchor = -1;
    setKeepMouseGrab(false);
    emit selectionEdited(m_selectionStart, m_selectionEnd);
}

void SequenceView::wheelEvent(QWheelEvent *event) {
    const QPoint pixels = event->pixelDelta();
    const qreal delta = !pixels.isNull() ? pixels.y() : event->angleDelta().y() / 120.0 * rowHeight() * 3;
    setContentY(m_contentY - delta);
    event->accept();
}

void SequenceView::paint(QPainter *p) {
    const int length = int(m_sequence.size());
    if (length == 0)
        return;
    const QFontMetricsF fm(m_font), rulerMetrics(m_rulerFont);
    const qreal row = rowHeight();
    const int first = qMax(0, int((m_contentY - Padding) / row));
    const int last = qMin((length - 1) / m_perLine, int((m_contentY + height() - Padding) / row) + 1);

    auto colorFor = [&](const QString &kind) {
        QColor c = m_accent;
        if (kind == "amplicon") c.setAlphaF(0.16);
        else if (kind == "match") c.setAlphaF(0.30);
        else if (kind == "current") c.setAlphaF(0.65);
        return c;
    };
    auto inside = [](int i, const Mark &m) { return i >= m.start && i <= m.end; };

    for (int r = first; r <= last; ++r) {
        const qreal top = Padding + r * row - m_contentY;
        const qreal baseTop = top + m_rulerHeight;
        const int lineStart = r * m_perLine, lineEnd = qMin(length, lineStart + m_perLine) - 1;

        // Ruler: the position of every tenth base, over it.
        p->setFont(m_rulerFont);
        p->setPen(m_muted);
        for (int column = Group - 1; lineStart + column <= lineEnd; column += Group) {
            const qreal right = columnX(column) + m_charWidth;
            p->drawText(QRectF(right - 8 * m_charWidth, top, 8 * m_charWidth, m_rulerHeight),
                        Qt::AlignRight | Qt::AlignBottom, QString::number(lineStart + column + 1));
        }
        p->setFont(m_font);
        p->drawText(QRectF(Padding, baseTop, m_gutter - Padding - m_charWidth, m_lineHeight),
                    Qt::AlignRight | Qt::AlignVCenter, QString::number(lineStart + 1));

        // Backgrounds, one cell at a time, so spans break cleanly at line
        // ends and the gaps between groups.
        for (int i = lineStart; i <= lineEnd; ++i) {
            const qreal x = columnX(i - lineStart);
            const bool joined = (i - lineStart) % Group != Group - 1 && i < lineEnd;
            const qreal w = m_charWidth * (joined ? 2 : 1);
            for (const Mark &m : m_marks)
                if (inside(i, m) && (m.kind == "amplicon" || m.kind == "match" || m.kind == "current"))
                    p->fillRect(QRectF(x, baseTop, (inside(i + 1, m) && joined) ? w : m_charWidth, m_lineHeight),
                                colorFor(m.kind));
            if (i >= m_selectionStart && i <= m_selectionEnd && m_selectionStart >= 0) {
                QColor c = m_text;
                c.setAlphaF(0.14);
                p->fillRect(QRectF(x, baseTop, (i < m_selectionEnd && joined) ? w : m_charWidth, m_lineHeight), c);
                p->fillRect(QRectF(x, baseTop + m_lineHeight - 2, (i < m_selectionEnd && joined) ? w : m_charWidth, 2), m_text);
            }
            for (const Mark &m : m_marks)
                if (inside(i, m) && (m.kind == "forward" || m.kind == "reverse"))
                    p->fillRect(QRectF(x, baseTop, (inside(i + 1, m) && joined) ? w : m_charWidth, m_lineHeight), m_accent);
        }

        // Bases.
        for (int i = lineStart; i <= lineEnd; ++i) {
            bool onPrimer = false;
            for (const Mark &m : m_marks)
                onPrimer |= inside(i, m) && (m.kind == "forward" || m.kind == "reverse");
            p->setPen(onPrimer ? m_accentForeground : m_text);
            p->drawText(QRectF(columnX(i - lineStart), baseTop, m_charWidth, m_lineHeight), Qt::AlignCenter,
                        QString(m_sequence[i]));
        }

        // Direction: an arrow under each primer toward its 3' end.
        p->setPen(QPen(m_accent, 1.5));
        for (const Mark &m : m_marks) {
            if ((m.kind != "forward" && m.kind != "reverse") || m.end < lineStart || m.start > lineEnd)
                continue;
            const int a = qMax(m.start, lineStart), b = qMin(m.end, lineEnd);
            const qreal y = baseTop + m_lineHeight + 3;
            const qreal x1 = columnX(a - lineStart), x2 = columnX(b - lineStart) + m_charWidth;
            p->drawLine(QPointF(x1, y), QPointF(x2, y));
            const bool tipHere = m.kind == "forward" ? b == m.end : a == m.start;
            if (tipHere) {
                const qreal tip = m.kind == "forward" ? x2 : x1, back = m.kind == "forward" ? -5 : 5;
                QPainterPath head;
                head.moveTo(tip, y);
                head.lineTo(tip + back, y - 3.5);
                head.lineTo(tip + back, y + 3.5);
                head.closeSubpath();
                p->fillPath(head, m_accent);
            }
        }
    }
}
