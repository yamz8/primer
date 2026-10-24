#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <functional>

// Open and save dialogs, behind an interface so tests can answer them. The
// real one asks xdg-desktop-portal, after Omacut's FilePicker (MIT, David
// Heinemeier Hansson).
class FilePicker : public QObject {
    Q_OBJECT

public:
    struct Filter {
        QString name;
        QStringList patterns;  // "*.fasta"
    };
    // An empty path means the user cancelled.
    using Callback = std::function<void(const QString &path, int filter)>;

    using QObject::QObject;
    virtual void open(const QString &title, const QList<Filter> &filters, Callback done) = 0;
    virtual void save(const QString &title, const QString &suggestedName, const QList<Filter> &filters,
                      Callback done) = 0;

signals:
    void failed(const QString &message);
};
