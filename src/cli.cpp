#include "cli.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QTextStream>

#include "checks.h"
#include "dna.h"
#include "document.h"
#include "report.h"

namespace cli {
namespace {

QTextStream &out() {
    static QTextStream stream(stdout);
    return stream;
}

QTextStream &err() {
    static QTextStream stream(stderr);
    return stream;
}

int fail(const QString &message) {
    err() << "primer: " << message << Qt::endl;
    return 1;
}

const char *Usage = R"(Usage: primer [open] [file]
       primer tm <sequence> [conditions] [--json]
       primer check <forward> [<reverse>] [--template file] [settings] [--json] [--report file]
       primer design <template> --region start..end [settings] [--json] [--csv file|-] [--report file] [--save file]
       primer skill install [--dir directory]

Conditions:
  --na mM         monovalent cations (50)        --mg mM     Mg2+ (1.5)
  --dntp mM       total dNTPs (0.6)              --conc nM   each primer (50)
Settings:
  --length a-b    primer length (18-25)          --length-opt n   (20)
  --tm a-b        primer Tm in °C (57-63)        --tm-opt t       (60)
  --tm-diff d     max pair Tm difference (2)     --gc a-b         GC% (40-60)
  --clamp n       3' G/C bases required (1)      --product a-b    product bp (100-1000)
  --pairs n       pairs to keep (50)

Positions are 1-based and inclusive. Every check is a prediction to confirm
in the lab. check exits 1 when any check fails; design exits 1 when no pair
passes.
)";

struct Options {
    QStringList positional;
    QHash<QString, QString> values;
    bool json = false;
};

bool parse(const QStringList &args, Options *o, QString *error) {
    static const QStringList valued{"--na", "--mg", "--dntp", "--conc", "--length", "--length-opt", "--tm", "--tm-opt",
                                    "--tm-diff", "--gc", "--clamp", "--product", "--pairs", "--region", "--template",
                                    "--csv", "--report", "--save", "--dir"};
    for (int i = 0; i < args.size(); ++i) {
        const QString &a = args[i];
        if (a == "--json") {
            o->json = true;
        } else if (a.startsWith("--")) {
            QString key = a, value;
            if (a.contains('=')) {
                key = a.section('=', 0, 0);
                value = a.section('=', 1);
            } else if (valued.contains(a)) {
                if (i + 1 >= args.size()) {
                    *error = QString("%1 needs a value").arg(a);
                    return false;
                }
                value = args[++i];
            }
            if (!valued.contains(key)) {
                *error = QString("unknown option %1").arg(key);
                return false;
            }
            o->values.insert(key, value);
        } else {
            o->positional.append(a);
        }
    }
    return true;
}

bool number(const Options &o, const QString &key, double *target, QString *error) {
    if (!o.values.contains(key))
        return true;
    bool ok = false;
    const double v = o.values.value(key).toDouble(&ok);
    if (!ok || v < 0) {
        *error = QString("%1 takes a number, not “%2”").arg(key, o.values.value(key));
        return false;
    }
    *target = v;
    return true;
}

bool range(const QString &text, double *low, double *high) {
    static const QRegularExpression pattern(R"(^\s*([0-9.]+)\s*(?:-|\.\.|–)\s*([0-9.]+)\s*$)");
    const auto m = pattern.match(text);
    if (!m.hasMatch())
        return false;
    bool a = false, b = false;
    *low = m.captured(1).toDouble(&a);
    *high = m.captured(2).toDouble(&b);
    return a && b && *low <= *high;
}

bool rangeOption(const Options &o, const QString &key, double *low, double *high, QString *error) {
    if (!o.values.contains(key))
        return true;
    if (!range(o.values.value(key), low, high)) {
        *error = QString("%1 takes a range like 18-25, not “%2”").arg(key, o.values.value(key));
        return false;
    }
    return true;
}

bool settings(const Options &o, checks::Settings *s, QString *error) {
    auto &c = s->conditions;
    double lengthMin = s->lengthMin, lengthMax = s->lengthMax, lengthOpt = s->lengthOpt;
    double productMin = s->productMin, productMax = s->productMax, clamp = s->gcClamp, pairs = s->pairs;
    if (!number(o, "--na", &c.sodium, error) || !number(o, "--mg", &c.magnesium, error)
        || !number(o, "--dntp", &c.dntp, error) || !number(o, "--conc", &c.primer, error)
        || !rangeOption(o, "--length", &lengthMin, &lengthMax, error) || !number(o, "--length-opt", &lengthOpt, error)
        || !rangeOption(o, "--tm", &s->tmMin, &s->tmMax, error) || !number(o, "--tm-opt", &s->tmOpt, error)
        || !number(o, "--tm-diff", &s->tmDifferenceMax, error) || !rangeOption(o, "--gc", &s->gcMin, &s->gcMax, error)
        || !number(o, "--clamp", &clamp, error) || !rangeOption(o, "--product", &productMin, &productMax, error)
        || !number(o, "--pairs", &pairs, error))
        return false;
    s->lengthMin = int(lengthMin);
    s->lengthMax = int(lengthMax);
    s->lengthOpt = o.values.contains("--length-opt") ? int(lengthOpt) : qBound(s->lengthMin, s->lengthOpt, s->lengthMax);
    if (!o.values.contains("--tm-opt"))
        s->tmOpt = qBound(s->tmMin, s->tmOpt, s->tmMax);
    s->productMin = int(productMin);
    s->productMax = int(productMax);
    s->gcClamp = int(clamp);
    s->pairs = qMax(1, int(pairs));
    if (c.primer <= 0) {
        *error = "--conc must be above 0";
        return false;
    }
    return true;
}

bool primerArgument(const QString &text, QByteArray *sequence, QString *error) {
    if (!dna::isSequence(text.toLatin1(), error))
        return false;
    *sequence = dna::clean(text.toLatin1());
    if (sequence->size() < 2) {
        *error = QString("“%1” is too short to be a primer").arg(text);
        return false;
    }
    return true;
}

bool readTemplate(const QString &path, dna::Record *record, QString *error) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        *error = QString("could not open %1: %2").arg(path, file.errorString());
        return false;
    }
    *record = dna::parse(file.readAll(), QFileInfo(path).completeBaseName(), error);
    return !record->sequence.isEmpty();
}

QString mark(checks::Status s) {
    switch (s) {
    case checks::Status::Pass: return "pass";
    case checks::Status::Warn: return "WARN";
    case checks::Status::Fail: return "FAIL";
    }
    return {};
}

void printPrimer(const QString &label, const checks::PrimerReport &r) {
    out() << label << "  5'-" << r.sequence << "-3'  " << mark(r.status) << "\n";
    out() << "  " << r.sequence.size() << " nt, Tm " << QString::number(r.tm, 'f', 1) << " °C, GC "
          << QString::number(r.gc, 'f', 1) << "%\n";
    for (const auto &c : r.checks)
        out() << "  [" << mark(c.status) << "] " << checks::describe(c) << "\n";
    for (const auto *s : {&r.hairpin, &r.selfDimer})
        if (s->kind != thermo::Structure::None && s->dG < -1)
            for (const QString &line : s->drawing)
                out() << "      " << line << "\n";
}

bool writeReport(const QString &path, const ReportInput &input, QString *error) {
    if (path.endsWith(".svg", Qt::CaseInsensitive))
        return report::exportSvg(input, path, error);
    if (path.endsWith(".pdf", Qt::CaseInsensitive))
        return report::exportPdf(input, path, error);
    *error = QString("--report needs a .pdf or .svg file name, not %1").arg(path);
    return false;
}

int tm(const Options &o) {
    if (o.positional.size() != 1)
        return fail("tm takes one sequence");
    QString error;
    checks::Settings s;
    QByteArray sequence;
    if (!settings(o, &s, &error) || !primerArgument(o.positional[0], &sequence, &error))
        return fail(error);
    const double t = thermo::meltingTemperature(sequence, s.conditions);
    if (o.json) {
        out() << QJsonDocument(QJsonObject{
                     {"sequence", QString::fromLatin1(sequence)},
                     {"length", int(sequence.size())},
                     {"tm_c", qRound(t * 100) / 100.0},
                     {"gc_percent", qRound(dna::gcPercent(sequence) * 10) / 10.0},
                     {"conditions", document::settingsToJson(s).value("conditions")},
                 }).toJson(QJsonDocument::Indented);
    } else {
        out() << QString::number(t, 'f', 1) << " °C\n";
    }
    return 0;
}

int check(const Options &o) {
    if (o.positional.isEmpty() || o.positional.size() > 2)
        return fail("check takes a forward primer and, optionally, a reverse primer");
    QString error;
    checks::Settings s;
    if (!settings(o, &s, &error))
        return fail(error);
    QByteArray forward, reverse;
    if (!primerArgument(o.positional[0], &forward, &error)
        || (o.positional.size() == 2 && !primerArgument(o.positional[1], &reverse, &error)))
        return fail(error);
    dna::Record templateRecord;
    if (o.values.contains("--template") && !readTemplate(o.values.value("--template"), &templateRecord, &error))
        return fail(error);

    ReportInput input;
    input.name = templateRecord.name;
    input.templateLength = int(templateRecord.sequence.size());
    input.settings = s;
    checks::Status status;
    QJsonObject json{{"tool", "primer"}, {"settings", document::settingsToJson(s)}};
    if (reverse.isEmpty()) {
        const auto r = checks::evaluatePrimer(forward, s, templateRecord.sequence);
        status = r.status;
        input.singles = {r};
        json.insert("primer", document::primerToJson(r));
        if (!o.json)
            printPrimer("Primer", r);
    } else {
        const auto p = checks::evaluatePair(forward, reverse, s, templateRecord.sequence);
        status = p.status;
        input.pairs = {p};
        json.insert("pair", document::pairToJson(p));
        if (!o.json) {
            printPrimer("Forward", p.forward);
            printPrimer("Reverse", p.reverse);
            out() << "Pair  " << mark(p.status) << "\n";
            for (const auto &c : p.checks)
                out() << "  [" << mark(c.status) << "] " << checks::describe(c) << "\n";
            if (p.crossDimer.kind != thermo::Structure::None && p.crossDimer.dG < -1)
                for (const QString &line : p.crossDimer.drawing)
                    out() << "      " << line << "\n";
        }
    }
    json.insert("status", checks::statusName(status));
    if (o.json)
        out() << QJsonDocument(json).toJson(QJsonDocument::Indented);
    else
        out() << "Predictions only; confirm in the lab.\n";
    if (o.values.contains("--report") && !writeReport(o.values.value("--report"), input, &error))
        return fail(error);
    out().flush();
    if (status == checks::Status::Fail) {
        err() << "primer: a check failed" << Qt::endl;
        return 1;
    }
    return 0;
}

int design(const Options &o) {
    if (o.positional.size() != 1)
        return fail("design takes one template file (FASTA, GenBank, or plain sequence)");
    if (!o.values.contains("--region"))
        return fail("design needs --region start..end");
    QString error;
    checks::Settings s;
    if (!settings(o, &s, &error))
        return fail(error);
    dna::Record record;
    if (!readTemplate(o.positional[0], &record, &error))
        return fail(error);
    double start = 0, end = 0;
    if (!range(o.values.value("--region"), &start, &end) || start < 1)
        return fail(QString("--region takes start..end, 1-based, not “%1”").arg(o.values.value("--region")));

    const auto result = checks::design(record.sequence, int(start) - 1, int(end) - 1, s);
    if (!result.error.isEmpty() && result.pairs.isEmpty())
        return fail(result.error);

    if (o.values.contains("--save")) {
        Document d;
        d.name = record.name;
        d.sequence = record.sequence;
        d.regionStart = int(start) - 1;
        d.regionEnd = int(end) - 1;
        d.settings = s;
        if (!document::save(d, o.values.value("--save"), &error))
            return fail(error);
    }
    if (o.values.contains("--csv")) {
        const QByteArray csv = document::csv(record.name, result.pairs);
        if (o.values.value("--csv") == "-")
            out() << csv;
        else if (!document::writeAtomically(o.values.value("--csv"), csv, &error))
            return fail(error);
    }
    if (o.values.contains("--report")) {
        ReportInput input;
        input.name = record.name;
        input.templateLength = int(record.sequence.size());
        input.regionStart = int(start) - 1;
        input.regionEnd = int(end) - 1;
        input.settings = s;
        input.pairs = result.pairs.mid(0, 10);
        if (!writeReport(o.values.value("--report"), input, &error))
            return fail(error);
    }

    if (o.json) {
        QJsonArray pairs;
        for (const auto &p : result.pairs)
            pairs.append(document::pairToJson(p));
        out() << QJsonDocument(QJsonObject{
                     {"tool", "primer"},
                     {"template", QJsonObject{{"name", record.name}, {"length", int(record.sequence.size())}}},
                     {"region", QJsonObject{{"start", int(start)}, {"end", int(end)}}},
                     {"settings", document::settingsToJson(s)},
                     {"pairs", pairs},
                 }).toJson(QJsonDocument::Indented);
    } else if (o.values.value("--csv") != "-") {
        out() << record.name << ": " << record.sequence.size() << " bp, target " << int(start) << ".." << int(end) << "\n";
        out() << QString("%1  %2  %3  %4  %5  %6  %7\n")
                     .arg("#", 3).arg("score", 5).arg("bp", 5).arg("forward 5'-3'", -26).arg("Tm", 5)
                     .arg("reverse 5'-3'", -26).arg("Tm", 5);
        for (int i = 0; i < result.pairs.size(); ++i) {
            const auto &p = result.pairs[i];
            out() << QString("%1  %2  %3  %4  %5  %6  %7%8\n")
                         .arg(i + 1, 3).arg(p.score, 5).arg(p.productSize, 5)
                         .arg(QString::fromLatin1(p.forward.sequence), -26).arg(p.forward.tm, 5, 'f', 1)
                         .arg(QString::fromLatin1(p.reverse.sequence), -26).arg(p.reverse.tm, 5, 'f', 1)
                         .arg(p.status == checks::Status::Warn ? "  (warnings)" : "");
        }
        out() << "Predictions only; confirm in the lab.\n";
    }
    out().flush();
    return result.pairs.isEmpty() ? 1 : 0;
}

int skill(const Options &o) {
    if (o.positional.value(0) != "install")
        return fail("usage: primer skill install [--dir directory]");
    const QString base = o.values.value("--dir", QDir::homePath() + "/.claude/skills");
    const QString directory = base + "/primer";
    if (!QDir().mkpath(directory))
        return fail(QString("could not create %1").arg(directory));
    QFile bundled(":/skill/SKILL.md");
    if (!bundled.open(QIODevice::ReadOnly))
        return fail("the bundled skill is missing");
    QString error;
    if (!document::writeAtomically(directory + "/SKILL.md", bundled.readAll(), &error))
        return fail(error);
    out() << "Installed " << directory << "/SKILL.md\n";
    return 0;
}

} // namespace

bool isCommand(const QStringList &arguments) {
    static const QStringList commands{"tm", "check", "design", "skill", "help", "--help", "-h", "--version", "-v"};
    return arguments.size() > 1 && commands.contains(arguments[1]);
}

int run(const QStringList &arguments) {
    const QString command = arguments.value(1);
    if (command == "help" || command == "--help" || command == "-h") {
        out() << Usage;
        return 0;
    }
    if (command == "--version" || command == "-v") {
        out() << "primer " << QCoreApplication::applicationVersion() << "\n";
        return 0;
    }
    Options o;
    QString error;
    if (!parse(arguments.mid(2), &o, &error))
        return fail(error);
    if (command == "tm")
        return tm(o);
    if (command == "check")
        return check(o);
    if (command == "design")
        return design(o);
    return skill(o);
}

} // namespace cli
