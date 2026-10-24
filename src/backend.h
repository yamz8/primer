#pragma once

#include <QObject>
#include <QVariantList>
#include <QVariantMap>

#include "checks.h"
#include "document.h"

class FilePicker;

// The window's state: the template and target, the settings, the designed
// pairs, the user's picks, and check mode. QML only lays this out.
class Backend : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString name READ name NOTIFY documentChanged)
    Q_PROPERTY(QString sequence READ sequence NOTIFY documentChanged)
    Q_PROPERTY(int length READ length NOTIFY documentChanged)
    Q_PROPERTY(QString filePath READ filePath NOTIFY filePathChanged)
    Q_PROPERTY(int regionStart READ regionStart NOTIFY regionChanged)
    Q_PROPERTY(int regionEnd READ regionEnd NOTIFY regionChanged)
    Q_PROPERTY(QString regionText READ regionText NOTIFY regionChanged)
    Q_PROPERTY(QVariantMap settings READ settings NOTIFY settingsChanged)
    Q_PROPERTY(QVariantList pairs READ pairs NOTIFY pairsChanged)
    Q_PROPERTY(bool designed READ designed NOTIFY pairsChanged)
    Q_PROPERTY(int currentIndex READ currentIndex WRITE setCurrentIndex NOTIFY currentChanged)
    Q_PROPERTY(QVariantMap current READ current NOTIFY currentChanged)
    Q_PROPERTY(int pickedCount READ pickedCount NOTIFY pairsChanged)
    Q_PROPERTY(QVariantList marks READ marks NOTIFY marksChanged)
    Q_PROPERTY(QString mode READ mode WRITE setMode NOTIFY modeChanged)
    Q_PROPERTY(QString checkForward READ checkForward NOTIFY checkChanged)
    Q_PROPERTY(QString checkReverse READ checkReverse NOTIFY checkChanged)
    Q_PROPERTY(bool checkUsesTemplate READ checkUsesTemplate WRITE setCheckUsesTemplate NOTIFY checkChanged)
    Q_PROPERTY(QVariantMap checkReport READ checkReport NOTIFY checkChanged)
    Q_PROPERTY(QString searchQuery READ searchQuery NOTIFY searchChanged)
    Q_PROPERTY(int matchCount READ matchCount NOTIFY searchChanged)
    Q_PROPERTY(int matchIndex READ matchIndex NOTIFY searchChanged)
    Q_PROPERTY(QString message READ message NOTIFY messageChanged)
    Q_PROPERTY(QString error READ error NOTIFY messageChanged)
    Q_PROPERTY(bool dirty READ dirty NOTIFY dirtyChanged)
    Q_PROPERTY(bool canUndo READ canUndo NOTIFY historyChanged)
    Q_PROPERTY(bool canRedo READ canRedo NOTIFY historyChanged)

public:
    explicit Backend(FilePicker *picker = nullptr, QObject *parent = nullptr);

    QString name() const { return m_doc.name; }
    QString sequence() const { return QString::fromLatin1(m_doc.sequence); }
    int length() const { return int(m_doc.sequence.size()); }
    QString filePath() const { return m_filePath; }
    int regionStart() const { return m_doc.regionStart; }
    int regionEnd() const { return m_doc.regionEnd; }
    QString regionText() const;
    QVariantMap settings() const;
    QVariantList pairs() const;
    bool designed() const { return m_designed; }
    int currentIndex() const { return m_current; }
    void setCurrentIndex(int index);
    QVariantMap current() const;
    int pickedCount() const { return int(m_doc.picked.size()); }
    QVariantList marks() const;
    QString mode() const { return m_doc.mode; }
    void setMode(const QString &mode);
    QString checkForward() const { return QString::fromLatin1(m_doc.checkForward); }
    QString checkReverse() const { return QString::fromLatin1(m_doc.checkReverse); }
    bool checkUsesTemplate() const { return m_checkUsesTemplate; }
    void setCheckUsesTemplate(bool use);
    QVariantMap checkReport() const;
    QString searchQuery() const { return m_searchQuery; }
    int matchCount() const { return int(m_matches.size()); }
    int matchIndex() const { return m_matchIndex; }
    QString message() const { return m_message; }
    QString error() const { return m_error; }
    bool dirty() const { return m_dirty; }
    bool canUndo() const { return !m_undo.isEmpty(); }
    bool canRedo() const { return !m_redo.isEmpty(); }

    const QVector<checks::PairReport> &pairReports() const { return m_pairs; }
    const Document &document() const { return m_doc; }

    Q_INVOKABLE void openFile();
    Q_INVOKABLE bool openPath(const QString &path);
    Q_INVOKABLE bool loadText(const QString &text, const QString &name = {});
    Q_INVOKABLE bool pasteClipboard();
    Q_INVOKABLE void setRegion(int start, int end);
    Q_INVOKABLE bool setRegionText(const QString &text);
    Q_INVOKABLE bool setSetting(const QString &key, double value);
    Q_INVOKABLE void resetSettings();
    Q_INVOKABLE bool design();
    Q_INVOKABLE void move(int delta);
    Q_INVOKABLE void togglePick(int index = -1);
    Q_INVOKABLE void setCheckPrimers(const QString &forward, const QString &reverse);
    Q_INVOKABLE void search(const QString &query);
    Q_INVOKABLE void nextMatch(int delta = 1);
    Q_INVOKABLE void save();
    Q_INVOKABLE void saveAs();
    Q_INVOKABLE bool saveTo(const QString &path);
    Q_INVOKABLE void exportFile();
    Q_INVOKABLE bool exportTo(const QString &path);
    Q_INVOKABLE void undo();
    Q_INVOKABLE void redo();
    Q_INVOKABLE void dismissError();
    // Messages are formatted when read; call after the language changes.
    Q_INVOKABLE void retranslate();

signals:
    void documentChanged();
    void filePathChanged();
    void regionChanged();
    void settingsChanged();
    void pairsChanged();
    void currentChanged();
    void marksChanged();
    void modeChanged();
    void checkChanged();
    void searchChanged();
    void messageChanged();
    void dirtyChanged();
    void historyChanged();
    // The view should bring this template position into sight.
    void reveal(int index);
    void saved();
    void saveCancelled();

private:
    struct Snapshot {
        Document doc;
        bool designed;
    };
    void capture();
    void restore(const Snapshot &snapshot);
    void setDirty(bool dirty);
    void setMessage(const QString &message);
    void setError(const QString &error);
    void clearDesign();
    void runDesign(bool quiet);
    void runCheck();
    void replaceDocument(const Document &doc, const QString &path);
    bool isPicked(const checks::PairReport &pair) const;
    QVector<checks::PairReport> exportPairs(QVector<checks::PrimerReport> *singles) const;
    QByteArray checkTemplate() const;

    FilePicker *m_picker;
    Document m_doc;
    QString m_filePath;
    QVector<checks::PairReport> m_pairs;
    bool m_designed = false;
    int m_current = -1;
    bool m_checkUsesTemplate = true;
    checks::PairReport m_checkPair;
    checks::PrimerReport m_checkPrimer;
    QString m_checkError;
    QString m_searchQuery;
    QVector<QPair<int, int>> m_matches;
    int m_matchIndex = -1;
    QString m_message, m_error;
    bool m_dirty = false;
    QVector<Snapshot> m_undo, m_redo;
    mutable QVariantList m_pairsCache;
    mutable bool m_pairsCacheValid = false;
};
