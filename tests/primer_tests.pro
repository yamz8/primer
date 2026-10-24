QT += core gui qml quick quickcontrols2 testlib dbus svg
CONFIG += c++17 testcase
TARGET = primer_tests
TEMPLATE = app

INCLUDEPATH += ../src

HEADERS += ../src/backend.h ../src/checks.h ../src/cli.h ../src/dna.h ../src/document.h ../src/filepicker.h \
           ../src/report.h ../src/theme.h ../src/thermo.h ../src/translator.h
SOURCES += primer_tests.cpp ../src/backend.cpp ../src/checks.cpp ../src/cli.cpp ../src/dna.cpp ../src/document.cpp \
           ../src/report.cpp ../src/theme.cpp ../src/thermo.cpp ../src/translator.cpp
RESOURCES += ../src/resources.qrc
