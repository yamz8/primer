#pragma once

#include <QJsonObject>
#include <QPair>
#include <QString>
#include <QVector>

#include "checks.h"

// What a .primer file holds: the template, the target, the settings, the
// pairs the user picked, and the primers in check mode. Designs are not
// stored; they are recomputed from the rest.
struct Document {
    QString name;
    QByteArray sequence;
    int regionStart = -1, regionEnd = -1;  // 0-based inclusive, -1 for none
    checks::Settings settings;
    QVector<QPair<QByteArray, QByteArray>> picked;
    QByteArray checkForward, checkReverse;
    QString mode = "design";
};

namespace document {

constexpr int FormatVersion = 1;

QJsonObject toJson(const Document &document);
bool fromJson(const QJsonObject &object, Document *document, QString *error);
bool save(const Document &document, const QString &path, QString *error);
bool load(const QString &path, Document *document, QString *error);

QJsonObject settingsToJson(const checks::Settings &settings);
checks::Settings settingsFromJson(const QJsonObject &object, const checks::Settings &defaults = {});

QJsonObject structureToJson(const thermo::Structure &structure);
QJsonObject primerToJson(const checks::PrimerReport &report);
QJsonObject pairToJson(const checks::PairReport &pair);

// For an oligo order: one row per primer, names <name>_F1, <name>_R1, ...
QByteArray csv(const QString &name, const QVector<checks::PairReport> &pairs);

// Writes beside the target, then renames over it.
bool writeAtomically(const QString &path, const QByteArray &data, QString *error);

QString safeName(const QString &name);

} // namespace document
