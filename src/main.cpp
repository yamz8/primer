// primer — design and check PCR primers.

#include <QFileInfo>
#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QUrl>

#include "backend.h"
#include "cli.h"
#include "portalfilepicker.h"
#include "sequenceview.h"
#include "theme.h"
#include "translator.h"

int main(int argc, char *argv[]) {
    QStringList arguments;
    for (int i = 0; i < argc; ++i)
        arguments.append(QString::fromLocal8Bit(argv[i]));

    // Commands run without a display, so agents and scripts can use them.
    const bool command = cli::isCommand(arguments);
    if (command) {
        qputenv("QT_QPA_PLATFORM", "offscreen");
        qputenv("NO_AT_BRIDGE", "1");
    }

    QGuiApplication app(argc, argv);
    app.setApplicationName("primer");
    app.setApplicationVersion("0.1.0");
    if (command)
        return cli::run(arguments);

    // Matches the window to primer.desktop, so the compositor (Wayland app_id)
    // and the launcher pick up the installed icon.
    app.setDesktopFileName("primer");
    app.setWindowIcon(QIcon::fromTheme("primer"));

    QQuickStyle::setStyle("Material");

    // Installed before anything is translated, and every qsTr() binding is
    // re-evaluated when the Omarchy language changes.
    Translator translator;
    app.installTranslator(&translator);

    Theme theme;
    PortalFilePicker picker;
    Backend backend(&picker);

    qmlRegisterType<SequenceView>("Primer", 1, 0, "SequenceView");
    QQmlApplicationEngine engine;
    QObject::connect(&translator, &Translator::changed, &engine, &QQmlEngine::retranslate);
    QObject::connect(&translator, &Translator::changed, &backend, &Backend::retranslate);
    engine.rootContext()->setContextProperty("translator", &translator);
    engine.rootContext()->setContextProperty("theme", &theme);
    engine.rootContext()->setContextProperty("backend", &backend);
    engine.load(QUrl("qrc:/Main.qml"));
    if (engine.rootObjects().isEmpty())
        return 1;

    // primer [open] <file>
    QStringList files = arguments.mid(1);
    if (files.value(0) == "open")
        files.removeFirst();
    if (!files.isEmpty())
        backend.openPath(QFileInfo(files.first()).absoluteFilePath());

    return app.exec();
}
