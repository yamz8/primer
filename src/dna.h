#pragma once

#include <QByteArray>
#include <QString>

// Plain DNA helpers. Sequences are upper-case Latin-1 byte arrays of A, C, G,
// T and N, written 5' to 3'.
namespace dna {

struct Record {
    QString name;
    QByteArray sequence;
};

char complement(char base);
QByteArray reverseComplement(const QByteArray &sequence);

// Upper-cases, turns U into T, and drops whitespace, digits, and the
// punctuation that sequence formats put between bases. Any other letter
// (IUPAC ambiguity codes) becomes N, so positions are kept.
QByteArray clean(const QByteArray &raw);

// True when the text holds nothing but sequence characters, so a pasted
// primer with a typo is reported instead of silently changed.
bool isSequence(const QByteArray &raw, QString *error = nullptr);

double gcPercent(const QByteArray &sequence);
int longestRun(const QByteArray &sequence);
int longestDinucleotideRepeat(const QByteArray &sequence);

// Reads FASTA (the first record), GenBank (the ORIGIN section), or bare
// sequence text. Returns an empty sequence and sets the error otherwise.
Record parse(const QByteArray &data, const QString &fallbackName, QString *error);

} // namespace dna
