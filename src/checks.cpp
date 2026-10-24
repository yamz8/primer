#include "checks.h"

#include <QCoreApplication>
#include <QLocale>
#include <algorithm>
#include <cmath>

#include "dna.h"

namespace checks {
namespace {

// Structure thresholds in kcal/mol at 37 °C. A structure that pairs a 3' end
// can be extended by the polymerase, so it is held to a stricter limit.
constexpr double HairpinWarn3 = -2, HairpinFail3 = -3;
constexpr double HairpinWarn = -3, HairpinFail = -6;
constexpr double DimerWarn3 = -5, DimerFail3 = -7;
constexpr double DimerWarn = -6, DimerFail = -9;
constexpr double EndStabilityWarn = -9;

Status grade(double value, double warn, double fail) {
    return value <= fail ? Status::Fail : value <= warn ? Status::Warn : Status::Pass;
}

Status structureStatus(const thermo::Structure &s, bool hairpin) {
    if (s.kind == thermo::Structure::None)
        return Status::Pass;
    if (hairpin)
        return s.threePrime ? grade(s.dG, HairpinWarn3, HairpinFail3) : grade(s.dG, HairpinWarn, HairpinFail);
    return s.threePrime ? grade(s.dG, DimerWarn3, DimerFail3) : grade(s.dG, DimerWarn, DimerFail);
}

// The structure that grades worst, the more stable one on a tie.
thermo::Structure worstStructure(const QVector<thermo::Structure> &all, bool hairpin) {
    thermo::Structure best;
    for (const auto &s : all) {
        if (s.dG >= 0)
            continue;
        if (best.kind == thermo::Structure::None) {
            best = s;
            continue;
        }
        const int a = int(structureStatus(s, hairpin)), b = int(structureStatus(best, hairpin));
        if (a > b || (a == b && s.dG < best.dG))
            best = s;
    }
    return best;
}

// In range passes; a little outside warns; further fails.
Status rangeStatus(double value, double low, double high, double slack) {
    if (value >= low - 1e-9 && value <= high + 1e-9)
        return Status::Pass;
    return (value >= low - slack && value <= high + slack) ? Status::Warn : Status::Fail;
}

int clampCount(const QByteArray &s) {
    int n = 0;
    for (qsizetype i = s.size() - 1; i >= 0 && (s[i] == 'G' || s[i] == 'C'); --i)
        ++n;
    return n;
}

void finish(PrimerReport &r) {
    r.status = Status::Pass;
    for (const Check &c : r.checks)
        r.status = worst(r.status, c.status);
}

// The cheap measurements, enough to drop most design candidates.
PrimerReport measure(const QByteArray &s, const Settings &settings) {
    PrimerReport r;
    r.sequence = s;
    r.tm = thermo::meltingTemperature(s, settings.conditions);
    r.gc = dna::gcPercent(s);
    r.endStability = thermo::endStability(s);
    r.run = dna::longestRun(s);
    r.repeat = dna::longestDinucleotideRepeat(s);
    return r;
}

void addCheapChecks(PrimerReport &r, const Settings &settings) {
    const int length = int(r.sequence.size());
    r.checks.append({Check::Length, rangeStatus(length, settings.lengthMin, settings.lengthMax, 5),
                     double(length), double(settings.lengthMin), double(settings.lengthMax)});
    r.checks.append({Check::Tm, rangeStatus(r.tm, settings.tmMin, settings.tmMax, 2), r.tm, settings.tmMin, settings.tmMax});
    r.checks.append({Check::Gc, rangeStatus(r.gc, settings.gcMin, settings.gcMax, 5), r.gc, settings.gcMin, settings.gcMax});

    // The clamp asks for G or C at the very end; more than three G or C in
    // the last five bases makes the end sticky enough to misprime.
    const int clamp = clampCount(r.sequence);
    int endGc = 0;
    for (char c : r.sequence.right(5))
        endGc += (c == 'G' || c == 'C');
    Status clampStatus = clamp >= settings.gcClamp ? Status::Pass : Status::Warn;
    if (endGc > 3)
        clampStatus = Status::Warn;
    r.checks.append({Check::Clamp, clampStatus, double(clamp), double(settings.gcClamp), double(endGc)});

    r.checks.append({Check::EndStability, r.endStability <= EndStabilityWarn ? Status::Warn : Status::Pass,
                     r.endStability, EndStabilityWarn});
    r.checks.append({Check::Run, r.run > 5 ? Status::Fail : r.run == 5 ? Status::Warn : Status::Pass, double(r.run), 4});
    r.checks.append({Check::Repeat, r.repeat >= 5 ? Status::Fail : r.repeat == 4 ? Status::Warn : Status::Pass,
                     double(r.repeat), 3});
    const int ambiguous = int(r.sequence.count('N'));
    if (ambiguous > 0)
        r.checks.append({Check::Ambiguous, Status::Warn, double(ambiguous)});
}

void addStructureChecks(PrimerReport &r, const Settings &settings) {
    r.hairpin = worstStructure(thermo::hairpins(r.sequence, settings.conditions), true);
    r.selfDimer = worstStructure(thermo::dimers(r.sequence, r.sequence, settings.conditions), false);
    r.checks.append({Check::Hairpin, structureStatus(r.hairpin, true), r.hairpin.dG, double(r.hairpin.threePrime)});
    r.checks.append({Check::SelfDimer, structureStatus(r.selfDimer, false), r.selfDimer.dG, double(r.selfDimer.threePrime)});
}

void addSiteCheck(PrimerReport &r, const QByteArray &templateSequence) {
    r.hasTemplate = true;
    r.sites = bindingSites(r.sequence, templateSequence);
    const int count = int(r.sites.size());
    r.checks.append({Check::Sites, count == 1 ? Status::Pass : count == 0 ? Status::Warn : Status::Fail, double(count)});
}

// Penalty in "degrees off": one per °C from the optimum Tm, and so on.
double primerPenalty(const PrimerReport &r, const Settings &settings) {
    double p = std::abs(r.tm - settings.tmOpt);
    p += 0.5 * std::abs(int(r.sequence.size()) - settings.lengthOpt);
    const double gcMid = (settings.gcMin + settings.gcMax) / 2;
    p += 0.05 * std::abs(r.gc - gcMid);
    if (r.hairpin.kind != thermo::Structure::None)
        p += qMax(0.0, -r.hairpin.dG - 1) * (r.hairpin.threePrime ? 1.0 : 0.5);
    if (r.selfDimer.kind != thermo::Structure::None)
        p += qMax(0.0, -r.selfDimer.dG - 4) * (r.selfDimer.threePrime ? 1.0 : 0.5);
    p += qMax(0.0, -r.endStability - 8);
    for (const Check &c : r.checks)
        if (c.status == Status::Warn)
            p += 1;
    return p;
}

void addPairChecks(PairReport &p, const Settings &settings, bool hasTemplate) {
    p.tmDifference = std::abs(p.forward.tm - p.reverse.tm);
    p.checks.append({Check::TmDifference, rangeStatus(p.tmDifference, 0, settings.tmDifferenceMax, 2),
                     p.tmDifference, settings.tmDifferenceMax});
    p.crossDimer = worstStructure(thermo::dimers(p.forward.sequence, p.reverse.sequence, settings.conditions), false);
    p.checks.append({Check::CrossDimer, structureStatus(p.crossDimer, false), p.crossDimer.dG,
                     double(p.crossDimer.threePrime)});
    if (!hasTemplate)
        return;

    // Every primer site on the forward strand against every site on the
    // reverse strand within reach is a product, including one primer priming
    // twice; the intended one starts at a forward site of the forward primer.
    QVector<Site> plus, minus;
    for (const PrimerReport *r : {&p.forward, &p.reverse})
        for (const Site &s : r->sites)
            (s.forwardStrand ? plus : minus).append(s);
    int products = 0;
    for (const Site &f : plus)
        for (const Site &r : minus)
            if (r.end > f.start && r.end - f.start + 1 <= ProductLimit)
                ++products;
    p.products = products;
    for (const Site &f : p.forward.sites) {
        if (!f.forwardStrand)
            continue;
        for (const Site &r : p.reverse.sites)
            if (!r.forwardStrand && r.end > f.start && (p.start < 0 || r.end - f.start + 1 < p.productSize)) {
                p.start = f.start;
                p.end = r.end;
                p.productSize = r.end - f.start + 1;
            }
    }
    Status status;
    if (products == 0 || p.start < 0)
        status = Status::Fail;
    else if (products > 1)
        status = Status::Fail;
    else
        status = rangeStatus(p.productSize, settings.productMin, settings.productMax, 0) == Status::Pass
                     ? Status::Pass : Status::Warn;
    p.checks.append({Check::Product, status, double(p.productSize), double(products)});
}

void finishPair(PairReport &p) {
    p.status = worst(p.forward.status, p.reverse.status);
    for (const Check &c : p.checks)
        p.status = worst(p.status, c.status);
}

} // namespace

Status worst(Status a, Status b) {
    return int(a) > int(b) ? a : b;
}

QVector<Site> bindingSites(const QByteArray &primer, const QByteArray &templateSequence) {
    QVector<Site> sites;
    const int n = int(primer.size()), length = int(templateSequence.size());
    if (n < SiteAnchor || length < n)
        return sites;
    const QByteArray reverse = dna::reverseComplement(templateSequence);
    for (int strand = 0; strand < 2; ++strand) {
        const QByteArray &t = strand == 0 ? templateSequence : reverse;
        const char *p = primer.constData(), *base = t.constData();
        for (int x = 0; x + n <= length; ++x) {
            const char *window = base + x;
            // The 3' anchor first: it rules out nearly every position cheaply.
            bool anchored = true;
            for (int k = n - SiteAnchor; k < n; ++k)
                if (window[k] != p[k] || p[k] == 'N') {
                    anchored = false;
                    break;
                }
            if (!anchored)
                continue;
            int mismatches = 0;
            for (int k = 0; k < n - SiteAnchor && mismatches <= SiteMismatches; ++k)
                mismatches += window[k] != p[k] || p[k] == 'N';
            if (mismatches > SiteMismatches)
                continue;
            Site site;
            site.forwardStrand = strand == 0;
            site.start = strand == 0 ? x : length - (x + n);
            site.end = site.start + n - 1;
            site.mismatches = mismatches;
            sites.append(site);
        }
    }
    return sites;
}

PrimerReport evaluatePrimer(const QByteArray &sequence, const Settings &settings, const QByteArray &templateSequence) {
    PrimerReport r = measure(sequence, settings);
    addCheapChecks(r, settings);
    addStructureChecks(r, settings);
    if (!templateSequence.isEmpty())
        addSiteCheck(r, templateSequence);
    finish(r);
    r.penalty = primerPenalty(r, settings);
    return r;
}

PairReport evaluatePair(const QByteArray &forward, const QByteArray &reverse, const Settings &settings,
                        const QByteArray &templateSequence) {
    PairReport p;
    p.forward = evaluatePrimer(forward, settings, templateSequence);
    p.reverse = evaluatePrimer(reverse, settings, templateSequence);
    addPairChecks(p, settings, !templateSequence.isEmpty());
    finishPair(p);
    p.penalty = p.forward.penalty + p.reverse.penalty + p.tmDifference;
    p.score = qBound(0, int(std::lround(100 - 5 * p.penalty)), 100);
    return p;
}

Design design(const QByteArray &t, int targetStart, int targetEnd, const Settings &settings) {
    Design result;
    const int length = int(t.size());
    if (t.isEmpty()) {
        result.error = QCoreApplication::translate("Checks", "Open or paste a template first");
        return result;
    }
    if (targetStart < 0 || targetEnd >= length || targetStart > targetEnd) {
        result.error = QCoreApplication::translate("Checks", "The target must lie within 1..%1").arg(length);
        return result;
    }
    if (settings.lengthMin < SiteAnchor || settings.lengthMin > settings.lengthMax) {
        result.error = QCoreApplication::translate("Checks", "Primer lengths must run from at least %1 upward").arg(SiteAnchor);
        return result;
    }
    const int target = targetEnd - targetStart + 1;
    if (target + 2 * settings.lengthMin > settings.productMax) {
        result.error = QCoreApplication::translate("Checks", "A %1 bp target and two primers do not fit in a %2 bp product")
                           .arg(target).arg(settings.productMax);
        return result;
    }

    // Candidates that pass the cheap checks, then the structure checks, then
    // the best of those get a binding-site search on both strands.
    auto collect = [&](bool reverse) {
        QVector<PrimerReport> kept;
        const int reach = settings.productMax - target - settings.lengthMin;
        for (int len = settings.lengthMin; len <= settings.lengthMax; ++len) {
            int from, to;
            if (!reverse) {  // 3' end before the target
                from = qMax(0, targetStart - reach - len);
                to = targetStart - len;
            } else {         // 5' end, on the forward strand, after the target
                from = targetEnd + 1;
                to = qMin(length - len, targetEnd + reach);
            }
            for (int start = from; start <= to; ++start) {
                QByteArray s = t.mid(start, len);
                if (s.contains('N'))
                    continue;
                if (reverse)
                    s = dna::reverseComplement(s);
                PrimerReport r = measure(s, settings);
                if (r.tm < settings.tmMin || r.tm > settings.tmMax || r.gc < settings.gcMin || r.gc > settings.gcMax
                    || clampCount(s) < settings.gcClamp || r.run > 5 || r.repeat >= 5)
                    continue;
                addCheapChecks(r, settings);
                addStructureChecks(r, settings);
                finish(r);
                if (r.status == Status::Fail)
                    continue;
                r.penalty = primerPenalty(r, settings);
                kept.append(r);
            }
        }
        std::sort(kept.begin(), kept.end(), [](const PrimerReport &a, const PrimerReport &b) { return a.penalty < b.penalty; });
        if (kept.size() > 150)
            kept.resize(150);
        QVector<PrimerReport> unique;
        for (PrimerReport &r : kept) {
            addSiteCheck(r, t);
            finish(r);
            if (r.status != Status::Fail)
                unique.append(r);
        }
        return unique;
    };
    const QVector<PrimerReport> forwards = collect(false), reverses = collect(true);
    result.forwardCandidates = int(forwards.size());
    result.reverseCandidates = int(reverses.size());

    struct Option { int f, r; double penalty; };
    QVector<Option> options;
    for (int f = 0; f < forwards.size(); ++f) {
        const Site &fs = forwards[f].sites.first();
        for (int r = 0; r < reverses.size(); ++r) {
            const Site &rs = reverses[r].sites.first();
            const int size = rs.end - fs.start + 1;
            const double difference = std::abs(forwards[f].tm - reverses[r].tm);
            if (size < settings.productMin || size > settings.productMax || difference > settings.tmDifferenceMax)
                continue;
            options.append({f, r, forwards[f].penalty + reverses[r].penalty + difference});
        }
    }
    std::sort(options.begin(), options.end(), [](const Option &a, const Option &b) { return a.penalty < b.penalty; });

    // Cross-dimers and product counts are the slow part, so stop after a
    // generous number of tries rather than walking every combination.
    QVector<PairReport> pairs;
    int tried = 0;
    for (const Option &o : options) {
        if (pairs.size() >= settings.pairs || ++tried > 20 * settings.pairs)
            break;
        PairReport p;
        p.forward = forwards[o.f];
        p.reverse = reverses[o.r];
        addPairChecks(p, settings, true);
        finishPair(p);
        if (p.status == Status::Fail)
            continue;
        p.penalty = o.penalty;
        for (const Check &c : p.checks)
            if (c.status == Status::Warn)
                p.penalty += 1;
        p.score = qBound(0, int(std::lround(100 - 5 * p.penalty)), 100);
        pairs.append(p);
    }
    std::stable_sort(pairs.begin(), pairs.end(), [](const PairReport &a, const PairReport &b) { return a.penalty < b.penalty; });
    result.pairs = pairs;
    if (pairs.isEmpty())
        result.error = QCoreApplication::translate("Checks", "No pair passes the checks; widen the settings or move the target");
    return result;
}

QString statusName(Status status) {
    switch (status) {
    case Status::Pass: return "pass";
    case Status::Warn: return "warn";
    case Status::Fail: return "fail";
    }
    return {};
}

QString checkName(Check::Kind kind) {
    switch (kind) {
    case Check::Length: return "length";
    case Check::Tm: return "tm";
    case Check::Gc: return "gc";
    case Check::Clamp: return "gc_clamp";
    case Check::EndStability: return "end_stability";
    case Check::Run: return "runs";
    case Check::Repeat: return "repeats";
    case Check::Ambiguous: return "ambiguous";
    case Check::Hairpin: return "hairpin";
    case Check::SelfDimer: return "self_dimer";
    case Check::Sites: return "binding_sites";
    case Check::TmDifference: return "tm_difference";
    case Check::CrossDimer: return "cross_dimer";
    case Check::Product: return "product";
    }
    return {};
}

QString describe(const Check &c) {
    auto tr = [](const char *text) { return QCoreApplication::translate("Checks", text); };
    const QLocale locale;
    auto num = [&](double v, int decimals = 1) { return locale.toString(v, 'f', decimals); };
    switch (c.kind) {
    case Check::Length:
        return tr("Length %1 nt (%2–%3)").arg(int(c.value)).arg(int(c.limit)).arg(int(c.limit2));
    case Check::Tm:
        return tr("Tm %1 °C (%2–%3)").arg(num(c.value), num(c.limit), num(c.limit2));
    case Check::Gc:
        return tr("GC %1% (%2–%3)").arg(num(c.value), num(c.limit, 0), num(c.limit2, 0));
    case Check::Clamp:
        if (c.limit2 > 3)
            return tr("3' end has %1 G/C in the last 5 bases").arg(int(c.limit2));
        return tr("GC clamp: %1 G/C at the 3' end (needs %2)").arg(int(c.value)).arg(int(c.limit));
    case Check::EndStability:
        return tr("3' end stability ΔG %1 kcal/mol").arg(num(c.value));
    case Check::Run:
        return tr("Longest single-base run: %1").arg(int(c.value));
    case Check::Repeat:
        return tr("Longest dinucleotide repeat: %1×").arg(int(c.value));
    case Check::Ambiguous:
        return QCoreApplication::translate("Checks", "%n ambiguous bases", nullptr, int(c.value));
    case Check::Hairpin:
        if (c.status == Status::Pass && c.value == 0)
            return tr("No hairpin");
        return (c.limit ? tr("Hairpin at the 3' end, ΔG %1 kcal/mol") : tr("Hairpin, ΔG %1 kcal/mol")).arg(num(c.value));
    case Check::SelfDimer:
        if (c.status == Status::Pass && c.value == 0)
            return tr("No self-dimer");
        return (c.limit ? tr("Self-dimer at the 3' end, ΔG %1 kcal/mol") : tr("Self-dimer, ΔG %1 kcal/mol")).arg(num(c.value));
    case Check::CrossDimer:
        if (c.status == Status::Pass && c.value == 0)
            return tr("No cross-dimer");
        return (c.limit ? tr("Cross-dimer at a 3' end, ΔG %1 kcal/mol") : tr("Cross-dimer, ΔG %1 kcal/mol")).arg(num(c.value));
    case Check::Sites:
        if (c.value == 0)
            return tr("No binding site on the template");
        if (c.value == 1)
            return tr("Binds one site on the template");
        return QCoreApplication::translate("Checks", "Not unique: binds %n sites", nullptr, int(c.value));
    case Check::TmDifference:
        return tr("Tm difference %1 °C (max %2)").arg(num(c.value), num(c.limit));
    case Check::Product:
        // value is the intended product's size, limit the number of products.
        if (c.limit == 0 || c.value == 0)
            return tr("No product on the template");
        if (c.limit > 1)
            return QCoreApplication::translate("Checks", "%n products on the template", nullptr, int(c.limit));
        return tr("Product %1 bp").arg(int(c.value));
    }
    return {};
}

} // namespace checks
