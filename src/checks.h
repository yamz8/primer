#pragma once

#include <QByteArray>
#include <QString>
#include <QVector>

#include "thermo.h"

// Primer and pair checks, binding-site search, and pair design. Plain C++ on
// byte arrays, shared by the window and the command line.
namespace checks {

enum class Status { Pass, Warn, Fail };

struct Settings {
    int lengthMin = 18, lengthOpt = 20, lengthMax = 25;
    double tmMin = 57, tmOpt = 60, tmMax = 63;
    double tmDifferenceMax = 2;
    double gcMin = 40, gcMax = 60;
    int gcClamp = 1;  // G or C bases required at the very 3' end
    int productMin = 100, productMax = 1000;
    int pairs = 50;   // candidate pairs kept
    thermo::Conditions conditions;
};

// One line of a report. The message is formatted when shown, so it follows
// the language the window is in.
struct Check {
    enum Kind { Length, Tm, Gc, Clamp, EndStability, Run, Repeat, Ambiguous, Hairpin, SelfDimer,
                Sites, TmDifference, CrossDimer, Product };
    Kind kind;
    Status status;
    double value = 0;
    double limit = 0;
    double limit2 = 0;
};

// Where a primer anneals: its 3' end matches perfectly and the rest has few
// mismatches. Coordinates are 0-based on the forward template.
struct Site {
    bool forwardStrand = true;
    int start = 0, end = 0;
    int mismatches = 0;
};

struct PrimerReport {
    QByteArray sequence;
    double tm = 0, gc = 0, endStability = 0;
    int run = 0, repeat = 0;
    thermo::Structure hairpin, selfDimer;
    bool hasTemplate = false;
    QVector<Site> sites;
    QVector<Check> checks;
    Status status = Status::Pass;
    double penalty = 0;
};

struct PairReport {
    PrimerReport forward, reverse;
    int start = -1, end = -1;  // the amplicon on the template, 0-based inclusive
    int productSize = 0;
    int products = 0;          // amplicons the template would give
    double tmDifference = 0;
    thermo::Structure crossDimer;
    QVector<Check> checks;     // the pair's own checks
    Status status = Status::Pass;
    double penalty = 0;
    int score = 0;             // 0–100, higher is better
};

struct Design {
    QVector<PairReport> pairs;
    QString error;
    int forwardCandidates = 0, reverseCandidates = 0;
};

// Mismatches allowed for an off-target site, and how many 3' bases must match.
constexpr int SiteMismatches = 3;
constexpr int SiteAnchor = 8;
// Amplicons longer than this are not counted as products.
constexpr int ProductLimit = 5000;

QVector<Site> bindingSites(const QByteArray &primer, const QByteArray &templateSequence);

PrimerReport evaluatePrimer(const QByteArray &sequence, const Settings &settings,
                            const QByteArray &templateSequence = {});
PairReport evaluatePair(const QByteArray &forward, const QByteArray &reverse, const Settings &settings,
                        const QByteArray &templateSequence = {});

// Pairs flanking target[start..end] (0-based inclusive), best first.
Design design(const QByteArray &templateSequence, int targetStart, int targetEnd, const Settings &settings);

Status worst(Status a, Status b);
QString statusName(Status status);  // "pass", "warn", "fail" — stable, for files and the CLI
QString checkName(Check::Kind kind);
QString describe(const Check &check); // translated, for people

} // namespace checks
