#include "dna.h"

#include <QCoreApplication>
#include <QList>

namespace dna {

char complement(char base) {
    switch (base) {
    case 'A': return 'T';
    case 'T': return 'A';
    case 'G': return 'C';
    case 'C': return 'G';
    default: return 'N';
    }
}

QByteArray reverseComplement(const QByteArray &sequence) {
    QByteArray result(sequence.size(), 'N');
    for (qsizetype i = 0, n = sequence.size(); i < n; ++i)
        result[n - 1 - i] = complement(sequence[i]);
    return result;
}

QByteArray clean(const QByteArray &raw) {
    QByteArray result;
    result.reserve(raw.size());
    for (char c : raw) {
        const char upper = (c >= 'a' && c <= 'z') ? char(c - 32) : c;
        if (upper == 'A' || upper == 'C' || upper == 'G' || upper == 'T')
            result.append(upper);
        else if (upper == 'U')
            result.append('T');
        else if (upper >= 'A' && upper <= 'Z')
            result.append('N');
    }
    return result;
}

bool isSequence(const QByteArray &raw, QString *error) {
    static const QByteArray allowed = "ACGTUNRYSWKMBDHV";
    int bases = 0;
    for (char c : raw) {
        const char upper = (c >= 'a' && c <= 'z') ? char(c - 32) : c;
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '-')
            continue;
        if (!allowed.contains(upper)) {
            if (error)
                *error = QCoreApplication::translate("Checks", "“%1” is not a DNA base").arg(QChar(c));
            return false;
        }
        ++bases;
    }
    if (bases == 0 && error)
        *error = QCoreApplication::translate("Checks", "The sequence is empty");
    return bases > 0;
}

double gcPercent(const QByteArray &sequence) {
    if (sequence.isEmpty())
        return 0;
    int gc = 0;
    for (char c : sequence)
        gc += (c == 'G' || c == 'C');
    return 100.0 * gc / sequence.size();
}

int longestRun(const QByteArray &sequence) {
    int best = 0, run = 0;
    for (qsizetype i = 0; i < sequence.size(); ++i) {
        run = (i > 0 && sequence[i] == sequence[i - 1]) ? run + 1 : 1;
        best = qMax(best, run);
    }
    return best;
}

// The most times one dinucleotide (AT, CA, ...) repeats back to back.
int longestDinucleotideRepeat(const QByteArray &sequence) {
    int best = 0;
    for (int phase = 0; phase < 2; ++phase) {
        int repeats = 0;
        QByteArray previous;
        for (qsizetype i = phase; i + 1 < sequence.size(); i += 2) {
            const QByteArray unit = sequence.mid(i, 2);
            if (unit[0] == unit[1]) {
                repeats = 0;
                previous.clear();
                continue;
            }
            repeats = unit == previous ? repeats + 1 : 1;
            previous = unit;
            best = qMax(best, repeats);
        }
    }
    return best;
}

Record parse(const QByteArray &data, const QString &fallbackName, QString *error) {
    Record record{fallbackName, {}};
    const QList<QByteArray> lines = data.split('\n');
    const QByteArray first = data.trimmed().left(5).toUpper();

    if (data.trimmed().startsWith('>')) {
        bool inRecord = false;
        for (const QByteArray &rawLine : lines) {
            const QByteArray line = rawLine.trimmed();
            if (line.startsWith('>')) {
                if (inRecord)
                    break;  // Only the first record; a multi-FASTA would need a picker.
                inRecord = true;
                const QString header = QString::fromUtf8(line.mid(1)).trimmed();
                if (!header.isEmpty())
                    record.name = header.section(' ', 0, 0);
                continue;
            }
            if (!line.startsWith(';'))
                record.sequence += clean(line);
        }
    } else if (first == "LOCUS") {
        bool inOrigin = false;
        for (const QByteArray &rawLine : lines) {
            if (rawLine.startsWith("LOCUS")) {
                const QByteArray name = rawLine.mid(5).simplified().split(' ').value(0);
                if (!name.isEmpty())
                    record.name = QString::fromUtf8(name);
            } else if (rawLine.startsWith("ORIGIN")) {
                inOrigin = true;
            } else if (rawLine.startsWith("//")) {
                break;
            } else if (inOrigin) {
                record.sequence += clean(rawLine);
            }
        }
        if (!inOrigin && error) {
            *error = QCoreApplication::translate("Checks", "The GenBank file has no ORIGIN section");
            return {fallbackName, {}};
        }
    } else {
        QString problem;
        QByteArray stripped;
        for (char c : data)
            if (!(c >= '0' && c <= '9'))
                stripped.append(c);
        if (!isSequence(stripped, &problem)) {
            if (error)
                *error = QCoreApplication::translate("Checks", "Not a FASTA, GenBank, or plain sequence: %1").arg(problem);
            return {fallbackName, {}};
        }
        record.sequence = clean(stripped);
    }

    if (record.sequence.isEmpty() && error)
        *error = QCoreApplication::translate("Checks", "The sequence is empty");
    return record;
}

} // namespace dna
