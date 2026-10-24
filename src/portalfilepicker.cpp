// Portal requests follow Omacut's PortalFilePicker (MIT, David Heinemeier Hansson).
#include "portalfilepicker.h"

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusMetaType>
#include <QDBusObjectPath>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QUrl>

namespace {

// The portal's filter type, a(sa(us)): a name and (kind, pattern) rules,
// where kind 0 is a glob and 1 a MIME type.
struct PortalRule {
    uint kind = 0;
    QString pattern;
};
struct PortalFilter {
    QString name;
    QList<PortalRule> rules;
};

QDBusArgument &operator<<(QDBusArgument &a, const PortalRule &r) {
    a.beginStructure();
    a << r.kind << r.pattern;
    a.endStructure();
    return a;
}
const QDBusArgument &operator>>(const QDBusArgument &a, PortalRule &r) {
    a.beginStructure();
    a >> r.kind >> r.pattern;
    a.endStructure();
    return a;
}
QDBusArgument &operator<<(QDBusArgument &a, const PortalFilter &f) {
    a.beginStructure();
    a << f.name << f.rules;
    a.endStructure();
    return a;
}
const QDBusArgument &operator>>(const QDBusArgument &a, PortalFilter &f) {
    a.beginStructure();
    a >> f.name >> f.rules;
    a.endStructure();
    return a;
}

} // namespace

Q_DECLARE_METATYPE(PortalRule)
Q_DECLARE_METATYPE(PortalFilter)

PortalFilePicker::PortalFilePicker(QObject *parent) : FilePicker(parent) {
    qDBusRegisterMetaType<PortalRule>();
    qDBusRegisterMetaType<QList<PortalRule>>();
    qDBusRegisterMetaType<PortalFilter>();
    qDBusRegisterMetaType<QList<PortalFilter>>();
}

void PortalFilePicker::open(const QString &title, const QList<Filter> &filters, Callback done) {
    request("OpenFile", title, {{"multiple", false}}, filters, std::move(done));
}

void PortalFilePicker::save(const QString &title, const QString &suggestedName, const QList<Filter> &filters,
                            Callback done) {
    request("SaveFile", title, {{"current_name", suggestedName}}, filters, std::move(done));
}

void PortalFilePicker::request(const QString &method, const QString &title, QVariantMap options,
                               const QList<Filter> &filters, Callback done) {
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        emit failed(tr("No D-Bus session bus, so there is no file chooser"));
        return;
    }
    if (!m_handle.isEmpty())
        bus.disconnect("org.freedesktop.portal.Desktop", m_handle, "org.freedesktop.portal.Request", "Response",
                       this, SLOT(response(uint, QVariantMap)));

    // Subscribing to the predictable request path before calling avoids
    // missing a fast response.
    const QString token = QString("primer%1_%2").arg(QCoreApplication::applicationPid()).arg(++m_counter);
    QString sender = bus.baseService().mid(1);
    sender.replace('.', '_');
    m_handle = QString("/org/freedesktop/portal/desktop/request/%1/%2").arg(sender, token);
    m_done = std::move(done);
    m_filters = filters;
    bus.connect("org.freedesktop.portal.Desktop", m_handle, "org.freedesktop.portal.Request", "Response", this,
                SLOT(response(uint, QVariantMap)));

    QList<PortalFilter> portalFilters;
    for (const Filter &f : filters) {
        PortalFilter pf{f.name, {}};
        for (const QString &pattern : f.patterns)
            pf.rules.append({0, pattern});
        portalFilters.append(pf);
    }
    options.insert("handle_token", token);
    options.insert("modal", true);
    if (!portalFilters.isEmpty()) {
        options.insert("filters", QVariant::fromValue(portalFilters));
        options.insert("current_filter", QVariant::fromValue(portalFilters.first()));
    }

    QDBusMessage message = QDBusMessage::createMethodCall("org.freedesktop.portal.Desktop",
                                                          "/org/freedesktop/portal/desktop",
                                                          "org.freedesktop.portal.FileChooser", method);
    message << QString() << title << options;
    auto *watcher = new QDBusPendingCallWatcher(bus.asyncCall(message), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *w) {
        QDBusPendingReply<QDBusObjectPath> reply = *w;
        w->deleteLater();
        if (reply.isError())
            emit failed(tr("The file chooser is unavailable: %1").arg(reply.error().message()));
    });
}

void PortalFilePicker::response(uint code, const QVariantMap &results) {
    Callback done = std::move(m_done);
    m_done = nullptr;
    if (!done)
        return;
    if (code != 0) {  // 1 cancelled, 2 failed elsewhere
        done({}, -1);
        return;
    }
    const QStringList uris = qdbus_cast<QStringList>(results.value("uris"));
    int filter = -1;
    if (results.contains("current_filter")) {
        const PortalFilter chosen = qdbus_cast<PortalFilter>(results.value("current_filter"));
        for (int i = 0; i < m_filters.size(); ++i)
            if (m_filters[i].name == chosen.name)
                filter = i;
    }
    done(uris.isEmpty() ? QString() : QUrl(uris.first()).toLocalFile(), filter);
}
