#pragma once

#include <QColor>
#include <QFont>
#include <QQuickPaintedItem>
#include <QVariantList>

// The template, wrapped in groups of ten under a position ruler, with the
// target, primers, amplicon, and search matches drawn over it. It is its own
// viewport (scrolled by contentY) so a long template never becomes one huge
// texture. Dragging selects a target.
class SequenceView : public QQuickPaintedItem {
    Q_OBJECT
    Q_PROPERTY(QString sequence READ sequence WRITE setSequence NOTIFY sequenceChanged)
    Q_PROPERTY(int selectionStart READ selectionStart WRITE setSelectionStart NOTIFY selectionChanged)
    Q_PROPERTY(int selectionEnd READ selectionEnd WRITE setSelectionEnd NOTIFY selectionChanged)
    Q_PROPERTY(QVariantList marks READ marks WRITE setMarks NOTIFY marksChanged)
    Q_PROPERTY(QColor accent MEMBER m_accent WRITE setAccent NOTIFY colorsChanged)
    Q_PROPERTY(QColor accentForeground MEMBER m_accentForeground WRITE setAccentForeground NOTIFY colorsChanged)
    Q_PROPERTY(QColor textColor MEMBER m_text NOTIFY colorsChanged)
    Q_PROPERTY(QColor mutedColor MEMBER m_muted NOTIFY colorsChanged)
    Q_PROPERTY(int fontPixelSize READ fontPixelSize WRITE setFontPixelSize NOTIFY layoutChanged)
    Q_PROPERTY(qreal contentY READ contentY WRITE setContentY NOTIFY contentYChanged)
    Q_PROPERTY(qreal contentHeight READ contentHeight NOTIFY layoutChanged)
    Q_PROPERTY(int basesPerLine READ basesPerLine NOTIFY layoutChanged)

public:
    explicit SequenceView(QQuickItem *parent = nullptr);

    QString sequence() const { return m_sequence; }
    void setSequence(const QString &sequence);
    int selectionStart() const { return m_selectionStart; }
    int selectionEnd() const { return m_selectionEnd; }
    void setSelectionStart(int start);
    void setSelectionEnd(int end);
    QVariantList marks() const { return m_marksSource; }
    void setMarks(const QVariantList &marks);
    void setAccent(const QColor &c);
    void setAccentForeground(const QColor &c);
    int fontPixelSize() const { return m_font.pixelSize(); }
    void setFontPixelSize(int size);
    qreal contentY() const { return m_contentY; }
    void setContentY(qreal y);
    qreal contentHeight() const;
    int basesPerLine() const { return m_perLine; }

    Q_INVOKABLE int indexAt(qreal x, qreal y) const;
    Q_INVOKABLE void ensureVisible(int index);

    void paint(QPainter *painter) override;

signals:
    void sequenceChanged();
    void selectionChanged();
    void marksChanged();
    void colorsChanged();
    void layoutChanged();
    void contentYChanged();
    // Only for selections the user made by dragging.
    void selectionEdited(int start, int end);

protected:
    void geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

private:
    struct Mark {
        int start, end;
        QString kind;
    };
    void relayout();
    qreal rowHeight() const { return m_rulerHeight + m_lineHeight + m_gap; }
    qreal columnX(int column) const;

    QString m_sequence;
    int m_selectionStart = -1, m_selectionEnd = -1, m_anchor = -1;
    QVariantList m_marksSource;
    QVector<Mark> m_marks;
    QColor m_accent{"#FFD60A"}, m_accentForeground{"black"}, m_text{"#e6e6e6"}, m_muted{"#6e6e78"};
    QFont m_font, m_rulerFont;
    qreal m_charWidth = 8, m_lineHeight = 16, m_rulerHeight = 12, m_gap = 10, m_gutter = 60;
    int m_perLine = 60;
    qreal m_contentY = 0;
};
