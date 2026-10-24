#include "document.h"

#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSaveFile>

#include "dna.h"

namespace document {
namespace {

QString number(double v, int decimals = 1) {
    return QString::number(v, 'f', decimals);  // CSV and files stay in C locale
}

QByteArray csvField(const QString &value) {
    QString v = value;
    if (v.contains(',') || v.contains('"') || v.contains('\n'))
        v = '"' + v.replace("\"", "\"\"") + '"';
    return v.toUtf8();
}

} // namespace

QJsonObject settingsToJson(const checks::Settings &s) {
    return {
        {"length", QJsonObject{{"min", s.lengthMin}, {"opt", s.lengthOpt}, {"max", s.lengthMax}}},
        {"tm", QJsonObject{{"min", s.tmMin}, {"opt", s.tmOpt}, {"max", s.tmMax}}},
        {"tm_difference_max", s.tmDifferenceMax},
        {"gc", QJsonObject{{"min", s.gcMin}, {"max", s.gcMax}}},
        {"gc_clamp", s.gcClamp},
        {"product", QJsonObject{{"min", s.productMin}, {"max", s.productMax}}},
        {"pairs", s.pairs},
        {"conditions", QJsonObject{{"sodium_mM", s.conditions.sodium},
                                   {"magnesium_mM", s.conditions.magnesium},
                                   {"dntp_mM", s.conditions.dntp},
                                   {"primer_nM", s.conditions.primer}}},
    };
}

checks::Settings settingsFromJson(const QJsonObject &o, const checks::Settings &d) {
    checks::Settings s = d;
    const QJsonObject length = o.value("length").toObject(), tm = o.value("tm").toObject(),
                      gc = o.value("gc").toObject(), product = o.value("product").toObject(),
                      conditions = o.value("conditions").toObject();
    s.lengthMin = length.value("min").toInt(d.lengthMin);
    s.lengthOpt = length.value("opt").toInt(d.lengthOpt);
    s.lengthMax = length.value("max").toInt(d.lengthMax);
    s.tmMin = tm.value("min").toDouble(d.tmMin);
    s.tmOpt = tm.value("opt").toDouble(d.tmOpt);
    s.tmMax = tm.value("max").toDouble(d.tmMax);
    s.tmDifferenceMax = o.value("tm_difference_max").toDouble(d.tmDifferenceMax);
    s.gcMin = gc.value("min").toDouble(d.gcMin);
    s.gcMax = gc.value("max").toDouble(d.gcMax);
    s.gcClamp = o.value("gc_clamp").toInt(d.gcClamp);
    s.productMin = product.value("min").toInt(d.productMin);
    s.productMax = product.value("max").toInt(d.productMax);
    s.pairs = o.value("pairs").toInt(d.pairs);
    s.conditions.sodium = conditions.value("sodium_mM").toDouble(d.conditions.sodium);
    s.conditions.magnesium = conditions.value("magnesium_mM").toDouble(d.conditions.magnesium);
    s.conditions.dntp = conditions.value("dntp_mM").toDouble(d.conditions.dntp);
    s.conditions.primer = conditions.value("primer_nM").toDouble(d.conditions.primer);
    return s;
}

QJsonObject toJson(const Document &d) {
    QJsonArray picked;
    for (const auto &pair : d.picked)
        picked.append(QJsonObject{{"forward", QString::fromLatin1(pair.first)},
                                  {"reverse", QString::fromLatin1(pair.second)}});
    QJsonObject object{
        {"format", "primer"},
        {"version", FormatVersion},
        {"name", d.name},
        {"sequence", QString::fromLatin1(d.sequence)},
        {"settings", settingsToJson(d.settings)},
        {"picked", picked},
        {"check", QJsonObject{{"forward", QString::fromLatin1(d.checkForward)},
                              {"reverse", QString::fromLatin1(d.checkReverse)}}},
        {"mode", d.mode},
    };
    if (d.regionStart >= 0)
        object.insert("region", QJsonObject{{"start", d.regionStart + 1}, {"end", d.regionEnd + 1}});
    return object;
}

bool fromJson(const QJsonObject &o, Document *d, QString *error) {
    if (o.value("format").toString() != "primer") {
        *error = QCoreApplication::translate("Checks", "Not a .primer file");
        return false;
    }
    if (o.value("version").toInt() > FormatVersion) {
        *error = QCoreApplication::translate("Checks", "This file is from a newer version of Primer");
        return false;
    }
    Document result;
    result.name = o.value("name").toString();
    result.sequence = dna::clean(o.value("sequence").toString().toLatin1());
    result.settings = settingsFromJson(o.value("settings").toObject());
    const QJsonObject region = o.value("region").toObject();
    if (!region.isEmpty()) {
        result.regionStart = region.value("start").toInt() - 1;
        result.regionEnd = region.value("end").toInt() - 1;
        if (result.regionStart < 0 || result.regionEnd >= result.sequence.size() || result.regionStart > result.regionEnd)
            result.regionStart = result.regionEnd = -1;
    }
    for (const auto &value : o.value("picked").toArray()) {
        const QJsonObject pair = value.toObject();
        result.picked.append({dna::clean(pair.value("forward").toString().toLatin1()),
                              dna::clean(pair.value("reverse").toString().toLatin1())});
    }
    const QJsonObject check = o.value("check").toObject();
    result.checkForward = dna::clean(check.value("forward").toString().toLatin1());
    result.checkReverse = dna::clean(check.value("reverse").toString().toLatin1());
    result.mode = o.value("mode").toString() == "check" ? "check" : "design";
    *d = result;
    return true;
}

bool writeAtomically(const QString &path, const QByteArray &data, QString *error) {
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(data) != data.size() || !file.commit()) {
        *error = QCoreApplication::translate("Checks", "Could not write %1: %2").arg(path, file.errorString());
        return false;
    }
    return true;
}

bool save(const Document &d, const QString &path, QString *error) {
    return writeAtomically(path, QJsonDocument(toJson(d)).toJson(QJsonDocument::Indented), error);
}

bool load(const QString &path, Document *d, QString *error) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        *error = QCoreApplication::translate("Checks", "Could not open %1: %2").arg(path, file.errorString());
        return false;
    }
    QJsonParseError parse;
    const QJsonDocument json = QJsonDocument::fromJson(file.readAll(), &parse);
    if (parse.error != QJsonParseError::NoError || !json.isObject()) {
        *error = QCoreApplication::translate("Checks", "%1 is not valid JSON: %2").arg(path, parse.errorString());
        return false;
    }
    return fromJson(json.object(), d, error);
}

QJsonObject structureToJson(const thermo::Structure &s) {
    if (s.kind == thermo::Structure::None)
        return {};
    return {{"dg_kcal_per_mol", qRound(s.dG * 100) / 100.0},
            {"base_pairs", s.basePairs},
            {"three_prime", s.threePrime},
            {"drawing", QJsonArray::fromStringList(s.drawing)}};
}

static QJsonArray checksToJson(const QVector<checks::Check> &list) {
    QJsonArray array;
    for (const auto &c : list)
        array.append(QJsonObject{{"check", checks::checkName(c.kind)},
                                 {"status", checks::statusName(c.status)},
                                 {"message", checks::describe(c)}});
    return array;
}

QJsonObject primerToJson(const checks::PrimerReport &r) {
    QJsonObject o{
        {"sequence", QString::fromLatin1(r.sequence)},
        {"length", int(r.sequence.size())},
        {"tm_c", qRound(r.tm * 100) / 100.0},
        {"gc_percent", qRound(r.gc * 10) / 10.0},
        {"end_stability_kcal_per_mol", qRound(r.endStability * 100) / 100.0},
        {"status", checks::statusName(r.status)},
        {"checks", checksToJson(r.checks)},
    };
    if (r.hairpin.kind != thermo::Structure::None)
        o.insert("hairpin", structureToJson(r.hairpin));
    if (r.selfDimer.kind != thermo::Structure::None)
        o.insert("self_dimer", structureToJson(r.selfDimer));
    if (r.hasTemplate) {
        QJsonArray sites;
        for (const auto &s : r.sites)
            sites.append(QJsonObject{{"strand", s.forwardStrand ? "+" : "-"},
                                     {"start", s.start + 1}, {"end", s.end + 1}, {"mismatches", s.mismatches}});
        o.insert("binding_sites", sites);
    }
    return o;
}

QJsonObject pairToJson(const checks::PairReport &p) {
    QJsonObject o{
        {"forward", primerToJson(p.forward)},
        {"reverse", primerToJson(p.reverse)},
        {"tm_difference_c", qRound(p.tmDifference * 100) / 100.0},
        {"status", checks::statusName(p.status)},
        {"score", p.score},
        {"checks", checksToJson(p.checks)},
    };
    if (p.crossDimer.kind != thermo::Structure::None)
        o.insert("cross_dimer", structureToJson(p.crossDimer));
    if (p.start >= 0) {
        o.insert("product_bp", p.productSize);
        o.insert("amplicon", QJsonObject{{"start", p.start + 1}, {"end", p.end + 1}});
    }
    return o;
}

QString safeName(const QString &name) {
    QString s = name.trimmed();
    s.replace(QRegularExpression("[^A-Za-z0-9_.-]+"), "_");
    return s.isEmpty() ? QString("primer") : s;
}

QByteArray csv(const QString &name, const QVector<checks::PairReport> &pairs) {
    QByteArray out = "Name,Sequence (5'-3'),Length,Tm (C),GC %,Product size (bp)\n";
    const QString base = safeName(name);
    for (int i = 0; i < pairs.size(); ++i) {
        const auto &p = pairs[i];
        const QString product = p.start >= 0 ? QString::number(p.productSize) : QString();
        for (const auto &[suffix, r] : {std::pair{QString("F"), &p.forward}, std::pair{QString("R"), &p.reverse}}) {
            out += csvField(QString("%1_%2%3").arg(base, suffix).arg(i + 1)) + ','
                 + r->sequence + ','
                 + QByteArray::number(r->sequence.size()) + ','
                 + number(r->tm).toLatin1() + ','
                 + number(r->gc).toLatin1() + ','
                 + product.toLatin1() + '\n';
        }
    }
    return out;
}

} // namespace document
