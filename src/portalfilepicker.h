#pragma once

#include <QDBusArgument>
#include <QVariantMap>

#include "filepicker.h"

// org.freedesktop.portal.FileChooser over D-Bus, following Omacut's
// PortalFilePicker (MIT, David Heinemeier Hansson). Never QFileDialog: the
// portal gives the desktop's own chooser and works inside sandboxes.
class PortalFilePicker : public FilePicker {
    Q_OBJECT

public:
    explicit PortalFilePicker(QObject *parent = nullptr);

    void open(const QString &title, const QList<Filter> &filters, Callback done) override;
    void save(const QString &title, const QString &suggestedName, const QList<Filter> &filters,
              Callback done) override;

private slots:
    void response(uint code, const QVariantMap &results);

private:
    void request(const QString &method, const QString &title, QVariantMap options, const QList<Filter> &filters,
                 Callback done);

    QString m_handle;
    Callback m_done;
    QList<Filter> m_filters;
    int m_counter = 0;
};
