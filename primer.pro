QT += core gui qml quick quickcontrols2 dbus svg

CONFIG += c++17 release
TARGET = primer
TEMPLATE = app

HEADERS += \
    src/backend.h \
    src/checks.h \
    src/cli.h \
    src/dna.h \
    src/document.h \
    src/filepicker.h \
    src/portalfilepicker.h \
    src/report.h \
    src/sequenceview.h \
    src/theme.h \
    src/thermo.h \
    src/translator.h

SOURCES += \
    src/main.cpp \
    src/backend.cpp \
    src/checks.cpp \
    src/cli.cpp \
    src/dna.cpp \
    src/document.cpp \
    src/portalfilepicker.cpp \
    src/report.cpp \
    src/sequenceview.cpp \
    src/theme.cpp \
    src/thermo.cpp \
    src/translator.cpp

RESOURCES += src/resources.qrc
