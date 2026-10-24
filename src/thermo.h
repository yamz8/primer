#pragma once

#include <QByteArray>
#include <QStringList>
#include <QVector>

// Nearest-neighbor thermodynamics after SantaLucia (1998), PNAS 95:1460, with
// the monovalent salt correction from the same paper and Mg2+ folded into a
// sodium equivalent after von Ahsen et al. (2001), as Primer3 does.
namespace thermo {

struct Conditions {
    double sodium = 50;     // mM, all monovalent cations
    double magnesium = 1.5; // mM
    double dntp = 0.6;      // mM, total dNTPs (they chelate Mg2+)
    double primer = 50;     // nM, each strand
};

struct Enthalpy {
    double dH = 0;  // kcal/mol
    double dS = 0;  // cal/(K·mol)
};

constexpr double R = 1.987;  // cal/(K·mol)
constexpr double Kelvin = 273.15;

// Free Mg2+ (after the dNTPs bind it) as the monovalent concentration that
// stabilises a duplex as much: [Na+]eq = [Na+] + 120·sqrt([Mg2+] − [dNTP]).
double sodiumEquivalent(const Conditions &conditions);

// ΔH and ΔS at 1 M NaCl: the stacks, initiation for each terminal pair, and
// the symmetry term for self-complementary sequences.
Enthalpy nearestNeighbor(const QByteArray &sequence);

// ΔS + 0.368 · (N − 1) · ln[Na+]eq, for N base pairs.
double saltCorrectedEntropy(double dS, int basePairs, const Conditions &conditions);

// Melting temperature in °C of the primer against its perfect complement.
double meltingTemperature(const QByteArray &sequence, const Conditions &conditions);

double freeEnergy(const Enthalpy &e, double celsius = 37);

// ΔG37 of the last five 3' bases paired with their complement, at 1 M salt;
// Primer3's "end stability". More negative is stickier.
double endStability(const QByteArray &sequence, int bases = 5);

// The most stable fully paired structure found, drawn as aligned base pairs.
struct Structure {
    enum Kind { None, Hairpin, Dimer };
    Kind kind = None;
    double dG = 0;           // kcal/mol at 37 °C
    int basePairs = 0;
    bool threePrime = false; // a primer's 3' end is paired, so it can extend
    QStringList drawing;
};

// Every perfectly paired stretch of three or more base pairs between two
// strands (pass the same sequence twice for a self-dimer), most stable first.
QVector<Structure> dimers(const QByteArray &a, const QByteArray &b, const Conditions &conditions);

// Every stem of three or more base pairs closing a loop of at least three,
// most stable first. Loop penalties from SantaLucia and Hicks (2004).
QVector<Structure> hairpins(const QByteArray &sequence, const Conditions &conditions);

double hairpinLoopFreeEnergy(int loopLength);

} // namespace thermo
