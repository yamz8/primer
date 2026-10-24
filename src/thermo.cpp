#include "thermo.h"

#include <QHash>
#include <algorithm>
#include <cmath>

#include "dna.h"

namespace thermo {
namespace {

// SantaLucia (1998) unified nearest-neighbor parameters, Table 2, keyed by the
// top strand 5'→3'. Each pair and its reverse complement share a value.
struct Stack {
    const char *pair;
    double dH, dS;
};
constexpr Stack Stacks[] = {
    {"AA", -7.9, -22.2}, {"TT", -7.9, -22.2},
    {"AT", -7.2, -20.4},
    {"TA", -7.2, -21.3},
    {"CA", -8.5, -22.7}, {"TG", -8.5, -22.7},
    {"GT", -8.4, -22.4}, {"AC", -8.4, -22.4},
    {"CT", -7.8, -21.0}, {"AG", -7.8, -21.0},
    {"GA", -8.2, -22.2}, {"TC", -8.2, -22.2},
    {"CG", -10.6, -27.2},
    {"GC", -9.8, -24.4},
    {"GG", -8.0, -19.9}, {"CC", -8.0, -19.9},
};

// Initiation, per terminal pair, and the self-complementary correction.
constexpr Enthalpy InitGC{0.1, -2.8};
constexpr Enthalpy InitAT{2.3, 4.1};
constexpr double SymmetryEntropy = -1.4;

bool stack(char first, char second, Enthalpy *out) {
    for (const Stack &s : Stacks)
        if (s.pair[0] == first && s.pair[1] == second) {
            *out = {s.dH, s.dS};
            return true;
        }
    return false;
}

Enthalpy initiation(char terminal) {
    return (terminal == 'G' || terminal == 'C') ? InitGC : InitAT;
}

// The stacks of a perfectly paired stretch, without initiation.
Enthalpy stacks(const QByteArray &top) {
    Enthalpy total;
    for (qsizetype i = 0; i + 1 < top.size(); ++i) {
        Enthalpy e;
        if (stack(top[i], top[i + 1], &e)) {
            total.dH += e.dH;
            total.dS += e.dS;
        } else {
            // An N pairs with nothing known; count it as an average stack
            // so the length still counts.
            total.dH += -8.2;
            total.dS += -22.3;
        }
    }
    return total;
}

bool pairs(char a, char b) {
    return a != 'N' && b != 'N' && dna::complement(a) == b;
}

double duplexFreeEnergy(const QByteArray &top, const Conditions &conditions) {
    Enthalpy e = stacks(top);
    const Enthalpy first = initiation(top.front()), last = initiation(top.back());
    e.dH += first.dH + last.dH;
    e.dS += first.dS + last.dS;
    e.dS = saltCorrectedEntropy(e.dS, int(top.size()), conditions);
    return freeEnergy(e);
}

QString pad(int n) { return QString(qMax(0, n), ' '); }

} // namespace

double sodiumEquivalent(const Conditions &c) {
    const double freeMg = c.magnesium - c.dntp;
    return c.sodium + (freeMg > 0 ? 120.0 * std::sqrt(freeMg) : 0.0);
}

Enthalpy nearestNeighbor(const QByteArray &sequence) {
    if (sequence.isEmpty())
        return {};
    Enthalpy e = stacks(sequence);
    const Enthalpy first = initiation(sequence.front()), last = initiation(sequence.back());
    e.dH += first.dH + last.dH;
    e.dS += first.dS + last.dS;
    if (sequence == dna::reverseComplement(sequence))
        e.dS += SymmetryEntropy;
    return e;
}

double saltCorrectedEntropy(double dS, int basePairs, const Conditions &conditions) {
    const double molar = sodiumEquivalent(conditions) / 1000.0;
    if (molar <= 0 || basePairs < 2)
        return dS;
    return dS + 0.368 * (basePairs - 1) * std::log(molar);
}

double meltingTemperature(const QByteArray &sequence, const Conditions &conditions) {
    if (sequence.size() < 2 || conditions.primer <= 0)
        return 0;
    Enthalpy e = nearestNeighbor(sequence);
    e.dS = saltCorrectedEntropy(e.dS, int(sequence.size()), conditions);
    // Two strands at equal concentration CT melt where half are paired:
    // R·ln(CT/4). A self-complementary strand pairs with itself: R·ln(CT).
    const double total = conditions.primer * 1e-9;
    const bool symmetric = sequence == dna::reverseComplement(sequence);
    const double concentration = symmetric ? total : total / 4.0;
    return 1000.0 * e.dH / (e.dS + R * std::log(concentration)) - Kelvin;
}

double freeEnergy(const Enthalpy &e, double celsius) {
    return e.dH - (celsius + Kelvin) * e.dS / 1000.0;
}

double endStability(const QByteArray &sequence, int bases) {
    if (sequence.size() < 2)
        return 0;
    const QByteArray end = sequence.right(bases);
    Enthalpy e = stacks(end);
    const Enthalpy first = initiation(end.front()), last = initiation(end.back());
    e.dH += first.dH + last.dH;
    e.dS += first.dS + last.dS;
    return freeEnergy(e);
}

QVector<Structure> dimers(const QByteArray &a, const QByteArray &b, const Conditions &conditions) {
    QVector<Structure> found;
    if (a.isEmpty() || b.isEmpty())
        return found;
    // The second strand runs 3'→5' under the first; shifting it by `offset`
    // lines its base j up with a's base j + offset.
    QByteArray under = b;
    std::reverse(under.begin(), under.end());
    const int na = int(a.size()), nb = int(under.size());
    for (int offset = -(nb - 1); offset < na; ++offset) {
        int i = qMax(0, offset);
        const int last = qMin(na, nb + offset);
        while (i < last) {
            if (!pairs(a[i], under[i - offset])) {
                ++i;
                continue;
            }
            int end = i;
            while (end + 1 < last && pairs(a[end + 1], under[end + 1 - offset]))
                ++end;
            const int length = end - i + 1;
            if (length >= 3) {
                Structure s;
                s.kind = Structure::Dimer;
                s.basePairs = length;
                s.dG = duplexFreeEnergy(a.mid(i, length), conditions);
                s.threePrime = end == na - 1 || i - offset == 0;
                const int left = qMax(0, -offset);  // indent of the top strand
                QString bars = pad(3 + left + i);
                bars += QString(length, '|');
                s.drawing = {
                    "5' " + pad(left) + QString::fromLatin1(a) + " 3'",
                    bars,
                    "3' " + pad(left + offset) + QString::fromLatin1(under) + " 5'",
                };
                found.append(s);
            }
            i = end + 1;
        }
    }
    std::sort(found.begin(), found.end(), [](const Structure &x, const Structure &y) { return x.dG < y.dG; });
    return found;
}

double hairpinLoopFreeEnergy(int n) {
    // SantaLucia and Hicks (2004), Table 4, kcal/mol at 37 °C; longer loops
    // extrapolate with the Jacobson–Stockmayer term 2.44·R·T·ln(n/30).
    static const QHash<int, double> table{{3, 3.5}, {4, 3.5}, {5, 3.3}, {6, 4.0}, {7, 4.2}, {8, 4.3},
                                          {9, 4.5}, {10, 4.6}, {12, 5.0}, {14, 5.1}, {16, 5.3},
                                          {18, 5.5}, {20, 5.7}, {25, 6.1}, {30, 6.3}};
    if (n < 3)
        return 1e9;
    if (table.contains(n))
        return table.value(n);
    if (n > 30)
        return 6.3 + 2.44 * R / 1000.0 * (37 + Kelvin) * std::log(n / 30.0);
    int below = n - 1;
    while (!table.contains(below))
        --below;
    int above = n + 1;
    while (!table.contains(above))
        ++above;
    return table.value(below) + (table.value(above) - table.value(below)) * (n - below) / double(above - below);
}

QVector<Structure> hairpins(const QByteArray &s, const Conditions &conditions) {
    QVector<Structure> found;
    const int n = int(s.size());
    // A stem pairs s[i + t] with s[j − t]; start only from outermost pairs
    // and grow inward while the loop keeps at least three bases.
    for (int i = 0; i < n; ++i) {
        for (int j = n - 1; j > i + 3; --j) {
            if (!pairs(s[i], s[j]))
                continue;
            if (i > 0 && j + 1 < n && pairs(s[i - 1], s[j + 1]))
                continue;
            int stem = 1;
            while (j - i - 2 * (stem + 1) + 1 >= 3 && pairs(s[i + stem], s[j - stem]))
                ++stem;
            if (stem < 3)
                continue;
            const int loop = j - i - 2 * stem + 1;
            Enthalpy e = stacks(s.mid(i, stem));
            e.dS = saltCorrectedEntropy(e.dS, stem, conditions);
            Structure h;
            h.kind = Structure::Hairpin;
            h.basePairs = stem;
            h.dG = freeEnergy(e) + hairpinLoopFreeEnergy(loop);
            h.threePrime = j == n - 1;

            // Stem across, loop folded around the right edge.
            const QByteArray tail5 = s.left(i), arm5 = s.mid(i, stem);
            const QByteArray loopSeq = s.mid(i + stem, loop);
            QByteArray arm3 = s.mid(j - stem + 1, stem), tail3 = s.mid(j + 1);
            std::reverse(arm3.begin(), arm3.end());
            std::reverse(tail3.begin(), tail3.end());
            const int half = loop / 2;
            QByteArray loopBottom = loopSeq.right(half);
            std::reverse(loopBottom.begin(), loopBottom.end());
            const QString turn = loop % 2 ? QString(QChar(loopSeq[half])) : QString(")");
            const int width = int(qMax(tail5.size(), tail3.size()));
            h.drawing = {
                "5' " + pad(width - int(tail5.size())) + QString::fromLatin1(tail5 + arm5 + loopSeq.left(half)),
                pad(3 + width) + QString(stem, '|') + pad(half) + turn,
                "3' " + pad(width - int(tail3.size())) + QString::fromLatin1(tail3 + arm3 + loopBottom),
            };
            found.append(h);
        }
    }
    std::sort(found.begin(), found.end(), [](const Structure &x, const Structure &y) { return x.dG < y.dG; });
    return found;
}

} // namespace thermo
