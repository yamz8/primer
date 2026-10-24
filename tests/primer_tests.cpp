#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <QtTest>
#include <cmath>

#include "backend.h"
#include "checks.h"
#include "cli.h"
#include "dna.h"
#include "document.h"
#include "filepicker.h"
#include "theme.h"
#include "thermo.h"
#include "translator.h"

namespace {

// A deterministic random template, so failures reproduce.
QByteArray randomSequence(int length, quint32 seed) {
    QByteArray s;
    s.reserve(length);
    quint32 state = seed;
    for (int i = 0; i < length; ++i) {
        state = state * 1664525u + 1013904223u;
        s.append("ACGT"[(state >> 24) & 3]);
    }
    return s;
}

// Answers dialogs with a fixed path, like a user who always picks it.
class FakePicker : public FilePicker {
public:
    QString answer;
    int filter = 0;
    void open(const QString &, const QList<Filter> &, Callback done) override { done(answer, filter); }
    void save(const QString &, const QString &, const QList<Filter> &, Callback done) override { done(answer, filter); }
};

} // namespace

class PrimerTests : public QObject {
    Q_OBJECT

private slots:
    // Reference Tm values from Primer3 (primer3-py 2.4.0, calc_tm with the
    // SantaLucia 1998 table and SantaLucia salt correction): its defaults
    // (50 mM Na+, 1.5 mM Mg2+, 0.6 mM dNTP, 50 nM), 50 mM Na+ alone at 250 nM,
    // and 1 M Na+ at 1 µM.
    void tmMatchesReferenceValues_data() {
        QTest::addColumn<QByteArray>("sequence");
        QTest::addColumn<double>("primer3Default");
        QTest::addColumn<double>("sodiumOnly");
        QTest::addColumn<double>("oneMolar");
        QTest::newRow("lac operator") << QByteArray("AGCGGATAACAATTTCACACAGGA") << 60.81 << 56.72 << 74.18;
        QTest::newRow("M13 forward") << QByteArray("GTAAAACGACGGCCAGT") << 54.70 << 51.67 << 68.62;
        QTest::newRow("M13 reverse") << QByteArray("CAGGAAACAGCTATGAC") << 49.10 << 46.09 << 62.94;
        QTest::newRow("T7 promoter") << QByteArray("TAATACGACTCACTATAGGG") << 50.32 << 46.78 << 63.95;
        QTest::newRow("SP6 promoter") << QByteArray("ATTTAGGTGACACTATAG") << 44.55 << 41.33 << 58.41;
    }
    void tmMatchesReferenceValues() {
        QFETCH(QByteArray, sequence);
        QFETCH(double, primer3Default);
        QFETCH(double, sodiumOnly);
        QFETCH(double, oneMolar);
        QVERIFY(std::abs(thermo::meltingTemperature(sequence, {50, 1.5, 0.6, 50}) - primer3Default) < 1.0);
        QVERIFY(std::abs(thermo::meltingTemperature(sequence, {50, 0, 0, 250}) - sodiumOnly) < 1.0);
        QVERIFY(std::abs(thermo::meltingTemperature(sequence, {1000, 0, 0, 1000}) - oneMolar) < 1.0);
    }

    // Summed by hand from SantaLucia (1998) Table 2: CG + GT + TT + TG + GA,
    // plus initiation with a terminal G·C and a terminal A·T.
    void nearestNeighborSumsThePublishedTable() {
        const auto e = thermo::nearestNeighbor("CGTTGA");
        QCOMPARE(qRound(e.dH * 10), qRound(-41.2 * 10));
        QCOMPARE(qRound(e.dS * 10), qRound(-115.4 * 10));
        // Self-complementary strands take the symmetry correction.
        const auto palindrome = thermo::nearestNeighbor("GAATTC");
        // GAATTC ends in G·C on both sides.
        QCOMPARE(qRound(palindrome.dS * 10), qRound((-22.2 - 22.2 - 20.4 - 22.2 - 22.2 - 2.8 - 2.8 - 1.4) * 10));
    }

    void saltCorrection() {
        // Free Mg2+ (1.5 − 0.6 mM) counts as 120·√0.9 mM of sodium.
        QVERIFY(std::abs(thermo::sodiumEquivalent({50, 1.5, 0.6, 50}) - (50 + 120 * std::sqrt(0.9))) < 1e-9);
        // dNTPs that bind all the Mg2+ leave only the sodium.
        QCOMPARE(thermo::sodiumEquivalent({50, 0.5, 0.8, 50}), 50.0);
        // The correction is 0.368 · (N − 1) · ln[Na+].
        QVERIFY(std::abs(thermo::saltCorrectedEntropy(-100, 20, {100, 0, 0, 50}) - (-100 + 0.368 * 19 * std::log(0.1))) < 1e-9);
        const QByteArray s = "AGCGGATAACAATTTCACACAGGA";
        const double low = thermo::meltingTemperature(s, {20, 0, 0, 250});
        const double mid = thermo::meltingTemperature(s, {50, 0, 0, 250});
        const double high = thermo::meltingTemperature(s, {200, 0, 0, 250});
        QVERIFY(low < mid && mid < high);
        // Mg2+ acts exactly like its sodium equivalent.
        const double withMg = thermo::meltingTemperature(s, {50, 2, 0.8, 250});
        const double asSodium = thermo::meltingTemperature(s, {50 + 120 * std::sqrt(1.2), 0, 0, 250});
        QVERIFY(std::abs(withMg - asSodium) < 1e-9);
        // More primer, higher Tm.
        QVERIFY(thermo::meltingTemperature(s, {50, 0, 0, 1000}) > thermo::meltingTemperature(s, {50, 0, 0, 50}));
    }

    void reverseComplement() {
        QCOMPARE(dna::reverseComplement("AACGTTTG"), QByteArray("CAAACGTT"));
        QCOMPARE(dna::reverseComplement("GAATTC"), QByteArray("GAATTC"));
        QCOMPARE(dna::reverseComplement("ACNT"), QByteArray("ANGT"));
        QCOMPARE(dna::reverseComplement(dna::reverseComplement("GATTACAGGC")), QByteArray("GATTACAGGC"));
        QCOMPARE(dna::clean("ac gu\n12 rY"), QByteArray("ACGTNN"));
    }

    void parsesSequenceFiles() {
        QString error;
        auto fasta = dna::parse(">seq1 a test\nACGT\nacgt\n>seq2\nTTTT\n", "x", &error);
        QCOMPARE(fasta.name, QString("seq1"));
        QCOMPARE(fasta.sequence, QByteArray("ACGTACGT"));
        auto genbank = dna::parse("LOCUS       pTEST  12 bp DNA\nFEATURES\nORIGIN\n        1 acgtacgtac gt\n//\n", "x", &error);
        QCOMPARE(genbank.name, QString("pTEST"));
        QCOMPARE(genbank.sequence, QByteArray("ACGTACGTACGT"));
        auto plain = dna::parse("  1 acgt acgg\n", "pasted", &error);
        QCOMPARE(plain.sequence, QByteArray("ACGTACGG"));
        QCOMPARE(plain.name, QString("pasted"));
        auto bad = dna::parse("Hello, world", "x", &error);
        QVERIFY(bad.sequence.isEmpty());
        QVERIFY(!error.isEmpty());
    }

    void runsAndRepeats() {
        QCOMPARE(dna::longestRun("ACGGGGGTA"), 5);
        QCOMPARE(dna::longestDinucleotideRepeat("GATATATATAGC"), 4);
        QCOMPARE(dna::longestDinucleotideRepeat("GCATATATATAC"), 4);
        QCOMPARE(dna::gcPercent("GGCCAATT"), 50.0);
    }

    // Two copies of GGATCC around a short loop fold back on themselves.
    void detectsHairpins() {
        const auto hairpins = thermo::hairpins("GGATCCGAATTCGGATCC", {});
        QVERIFY(!hairpins.isEmpty());
        QVERIFY(hairpins.first().dG < -3);
        QVERIFY(hairpins.first().basePairs >= 4);
        QCOMPARE(hairpins.first().drawing.size(), 3);

        // A designed stem of CCGAG and CTCGG around TTTT.
        const auto stem = thermo::hairpins("CCGAGTTTTCTCGGATCAAGT", {});
        QVERIFY(!stem.isEmpty());
        QCOMPARE(stem.first().basePairs, 5);
        QVERIFY(stem.first().drawing[0].contains("CCGAG"));
        QVERIFY(!stem.first().threePrime);

        // A primer whose 3' end folds back is graded harder.
        const auto report = checks::evaluatePrimer("ATCGGCCGTTTTCGGCCG", {});
        QVERIFY(report.hairpin.threePrime);
        QCOMPARE(statusOf(report.checks, checks::Check::Hairpin), checks::Status::Fail);

        // Nothing here pairs well enough to fold.
        const auto clean = checks::evaluatePrimer("AGCGGATAACAATTTCACACAGGA", {});
        QCOMPARE(clean.hairpin.kind, thermo::Structure::None);
        QCOMPARE(thermo::hairpinLoopFreeEnergy(4), 3.5);
        QVERIFY(thermo::hairpinLoopFreeEnergy(40) > thermo::hairpinLoopFreeEnergy(30));
    }

    void detectsDimers() {
        // A palindrome pairs with a copy of itself end to end.
        const auto self = checks::evaluatePrimer("GGATCCGAATTCGGATCC", {});
        QCOMPARE(self.selfDimer.basePairs, 18);
        QVERIFY(self.selfDimer.threePrime);
        QVERIFY(self.selfDimer.dG < -15);
        QCOMPARE(self.status, checks::Status::Fail);
        QCOMPARE(self.selfDimer.drawing[1].count('|'), 18);

        // 3' ends that complement each other make a primer dimer.
        const QByteArray forward = "TTACTGATCAGTAGAATTCCGG", reverse = "CAGTCCTAATGTACCGGAATTC";
        const auto pair = checks::evaluatePair(forward, reverse, {});
        QVERIFY(pair.crossDimer.threePrime);
        QVERIFY(pair.crossDimer.basePairs >= 9);
        QVERIFY(pair.crossDimer.dG < -7);
        bool flagged = false;
        for (const auto &c : pair.checks)
            flagged |= c.kind == checks::Check::CrossDimer && c.status == checks::Status::Fail;
        QVERIFY(flagged);

        // The M13 primers do not pair with each other.
        const auto m13 = checks::evaluatePair("GTAAAACGACGGCCAGT", "CAGGAAACAGCTATGAC", {});
        for (const auto &c : m13.checks)
            if (c.kind == checks::Check::CrossDimer)
                QCOMPARE(c.status, checks::Status::Pass);
    }

    void findsBindingSitesOnBothStrands() {
        QByteArray t = randomSequence(1200, 7);
        const QByteArray primer = "GATCGTAGCTAGGCTAACGT";
        t.replace(100, primer.size(), primer);
        t.replace(700, primer.size(), dna::reverseComplement(primer));
        auto sites = checks::bindingSites(primer, t);
        QCOMPARE(sites.size(), 2);
        QVERIFY(sites[0].forwardStrand);
        QCOMPARE(sites[0].start, 100);
        QCOMPARE(sites[0].end, 119);
        QVERIFY(!sites[1].forwardStrand);
        QCOMPARE(sites[1].start, 700);
        QCOMPARE(sites[1].end, 719);

        const auto report = checks::evaluatePrimer(primer, {}, t);
        QCOMPARE(report.sites.size(), 2);
        QCOMPARE(report.status, checks::Status::Fail);

        // Two 5' mismatches still anneal; a 3' mismatch does not.
        QByteArray near = primer;
        near[0] = dna::complement(near[0]);
        near[2] = dna::complement(near[2]);
        QByteArray offTarget = randomSequence(400, 9);
        offTarget.replace(50, near.size(), near);
        QCOMPARE(checks::bindingSites(primer, offTarget).size(), 1);
        QCOMPARE(checks::bindingSites(primer, offTarget).first().mismatches, 2);
        QByteArray far = primer;
        far[far.size() - 1] = dna::complement(far[far.size() - 1]);
        offTarget.replace(50, far.size(), far);
        QCOMPARE(checks::bindingSites(primer, offTarget).size(), 0);
    }

    void designsPairsOnASyntheticTemplate() {
        const QByteArray t = randomSequence(1500, 42);
        checks::Settings s;
        const int start = 699, end = 759;  // 700..760, 1-based
        const auto result = checks::design(t, start, end, s);
        QVERIFY2(!result.pairs.isEmpty(), qPrintable(result.error));
        QVERIFY(result.pairs.size() <= s.pairs);
        double previous = -1;
        for (const auto &p : result.pairs) {
            QVERIFY(p.penalty >= previous);
            previous = p.penalty;
            QVERIFY(p.status != checks::Status::Fail);
            QVERIFY(p.start < start && p.end > end);
            QCOMPARE(p.productSize, p.end - p.start + 1);
            QVERIFY(p.productSize >= s.productMin && p.productSize <= s.productMax);
            QVERIFY(p.tmDifference <= s.tmDifferenceMax + 1e-9);
            QCOMPARE(p.products, 1);
            for (const auto *r : {&p.forward, &p.reverse}) {
                QVERIFY(r->tm >= s.tmMin && r->tm <= s.tmMax);
                QVERIFY(r->gc >= s.gcMin && r->gc <= s.gcMax);
                QVERIFY(r->sequence.size() >= s.lengthMin && r->sequence.size() <= s.lengthMax);
                QCOMPARE(r->sites.size(), 1);
                QVERIFY(r->sequence.endsWith('G') || r->sequence.endsWith('C'));
            }
            QCOMPARE(t.mid(p.start, p.forward.sequence.size()), p.forward.sequence);
            QCOMPARE(dna::reverseComplement(t.mid(p.end - p.reverse.sequence.size() + 1, p.reverse.sequence.size())),
                     p.reverse.sequence);
            QVERIFY(p.score >= 0 && p.score <= 100);
        }
        // The same answer twice.
        QCOMPARE(checks::design(t, start, end, s).pairs.first().forward.sequence, result.pairs.first().forward.sequence);

        // A target that cannot fit reports why.
        s.productMax = 80;
        QVERIFY(!checks::design(t, start, end, s).error.isEmpty());
        QVERIFY(!checks::design(t, 1400, 1600, {}).error.isEmpty());
    }

    void designAvoidsRepeatedSites() {
        // Copy the region a good forward primer would come from elsewhere;
        // no pair may use a primer from it.
        QByteArray t = randomSequence(1500, 42);
        const auto first = checks::design(t, 699, 759, {});
        QVERIFY(!first.pairs.isEmpty());
        const QByteArray best = first.pairs.first().forward.sequence;
        t.replace(1300, best.size(), best);
        const auto second = checks::design(t, 699, 759, {});
        for (const auto &p : second.pairs)
            QVERIFY(p.forward.sequence != best);
    }

    void checksWithoutATemplate() {
        const auto p = checks::evaluatePair("GTAAAACGACGGCCAGT", "CAGGAAACAGCTATGAC", {});
        QVERIFY(!p.forward.hasTemplate);
        QCOMPARE(p.start, -1);
        for (const auto &c : p.checks)
            QVERIFY(c.kind != checks::Check::Product);
        // 54.7 and 49.1 °C: out of the 57–63 window and 5.6 °C apart.
        QCOMPARE(p.status, checks::Status::Fail);
    }

    void writesCsv() {
        checks::PairReport p;
        p.forward.sequence = "ACGTACGTACGTACGTAC";
        p.forward.tm = 58.04;
        p.forward.gc = 50;
        p.reverse.sequence = "GGCCTTAAGGCCTTAAGG";
        p.reverse.tm = 59.96;
        p.reverse.gc = 55.555;
        p.start = 10;
        p.end = 209;
        p.productSize = 200;
        const QByteArray csv = document::csv("pUC19 MCS", {p});
        QCOMPARE(csv, QByteArray("Name,Sequence (5'-3'),Length,Tm (C),GC %,Product size (bp)\n"
                                 "pUC19_MCS_F1,ACGTACGTACGTACGTAC,18,58.0,50.0,200\n"
                                 "pUC19_MCS_R1,GGCCTTAAGGCCTTAAGG,18,60.0,55.6,200\n"));
    }

    void savesAndLoadsDocuments() {
        QTemporaryDir dir;
        Document d;
        d.name = "test";
        d.sequence = randomSequence(300, 3);
        d.regionStart = 100;
        d.regionEnd = 150;
        d.settings.tmMin = 55;
        d.settings.conditions.magnesium = 2.5;
        d.picked = {{"ACGTACGTAC", "GGGTTTAAAC"}};
        d.checkForward = "ACGTTT";
        d.mode = "check";
        QString error;
        const QString path = dir.filePath("t.primer");
        QVERIFY(document::save(d, path, &error));
        Document back;
        QVERIFY2(document::load(path, &back, &error), qPrintable(error));
        QCOMPARE(back.name, d.name);
        QCOMPARE(back.sequence, d.sequence);
        QCOMPARE(back.regionStart, 100);
        QCOMPARE(back.regionEnd, 150);
        QCOMPARE(back.settings.tmMin, 55.0);
        QCOMPARE(back.settings.conditions.magnesium, 2.5);
        QCOMPARE(back.picked, d.picked);
        QCOMPARE(back.checkForward, QByteArray("ACGTTT"));
        QCOMPARE(back.mode, QString("check"));
        QFile other(dir.filePath("x.primer"));
        QVERIFY(other.open(QIODevice::WriteOnly));
        other.write("{\"format\": \"something\"}");
        other.close();
        QVERIFY(!document::load(other.fileName(), &back, &error));
    }

    void backendDesignsPicksAndExports() {
        QTemporaryDir dir;
        FakePicker picker;
        Backend backend(&picker);
        QVERIFY(!backend.design());  // no template yet
        QVERIFY(backend.loadText(">synthetic\n" + randomSequence(1500, 42)));
        QCOMPARE(backend.name(), QString("synthetic"));
        QVERIFY(!backend.setRegionText("900..20000"));
        QVERIFY(backend.setRegionText("700..760"));
        QCOMPARE(backend.regionStart(), 699);
        QVERIFY(backend.design());
        QVERIFY(backend.pairs().size() > 1);
        QCOMPARE(backend.currentIndex(), 0);
        backend.move(1);
        QCOMPARE(backend.currentIndex(), 1);
        backend.togglePick();
        QCOMPARE(backend.pickedCount(), 1);
        QVERIFY(backend.pairs()[1].toMap().value("picked").toBool());
        QVERIFY(backend.marks().size() >= 3);

        QVERIFY(backend.exportTo(dir.filePath("order.csv")));
        QFile csv(dir.filePath("order.csv"));
        QVERIFY(csv.open(QIODevice::ReadOnly));
        const QList<QByteArray> lines = csv.readAll().trimmed().split('\n');
        QCOMPARE(lines.size(), 3);
        QVERIFY(lines[1].startsWith("synthetic_F1,"));
        QVERIFY(backend.exportTo(dir.filePath("report.pdf")));
        QVERIFY(QFileInfo(dir.filePath("report.pdf")).size() > 1000);
        QVERIFY(backend.exportTo(dir.filePath("report.svg")));
        QFile svg(dir.filePath("report.svg"));
        QVERIFY(svg.open(QIODevice::ReadOnly));
        QVERIFY(svg.readAll().contains("<svg"));

        // Undo walks back the pick; redo brings it back.
        backend.undo();
        QCOMPARE(backend.pickedCount(), 0);
        backend.redo();
        QCOMPARE(backend.pickedCount(), 1);

        // Settings redesign right away; impossible ones are refused.
        QVERIFY(!backend.setSetting("tmMin", 70));
        QVERIFY(backend.setSetting("productMax", 400));
        for (const auto &v : backend.pairs())
            QVERIFY(v.toMap().value("productSize").toInt() <= 400);

        picker.answer = dir.filePath("saved");
        backend.save();
        QVERIFY(QFile::exists(dir.filePath("saved.primer")));
        QVERIFY(!backend.dirty());

        Backend reopened(&picker);
        QVERIFY(reopened.openPath(dir.filePath("saved.primer")));
        QCOMPARE(reopened.pickedCount(), 1);
        QVERIFY(reopened.designed());
        QCOMPARE(reopened.document().settings.productMax, 400);
    }

    void backendCheckMode() {
        Backend backend;
        backend.setMode("check");
        backend.setCheckPrimers("GTAAAACGACGGCCAGT", "");
        QCOMPARE(backend.checkReport().value("kind").toString(), QString("primer"));
        backend.setCheckPrimers("GTAAAACGACGGCCAGT", "CAGGAAACAGCTATGAC");
        QCOMPARE(backend.checkReport().value("kind").toString(), QString("pair"));
        backend.setCheckPrimers("GTAAAACGXCGGCCAGT", "");
        QCOMPARE(backend.checkReport().value("kind").toString(), QString("error"));
    }

    void searchFindsBothStrandsAndPositions() {
        Backend backend;
        QVERIFY(backend.loadText("AAAAGGATCCAAAACCCTTGAAAA"));
        backend.search("CCCTTG");
        QCOMPARE(backend.matchCount(), 1);
        backend.search("CAAGGG");  // its reverse complement
        QCOMPARE(backend.matchCount(), 1);
        backend.search("12");
        QCOMPARE(backend.matchCount(), 1);
        backend.search("TTTTTTTT");
        QCOMPARE(backend.matchCount(), 0);
    }

    void commandLine() {
        QTemporaryDir dir;
        const QString fasta = dir.filePath("t.fasta");
        QFile f(fasta);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(">t\n" + randomSequence(1500, 42) + "\n");
        f.close();
        QVERIFY(cli::isCommand({"primer", "tm", "ACGT"}));
        QVERIFY(!cli::isCommand({"primer", "file.fasta"}));
        QCOMPARE(cli::run({"primer", "tm", "AGCGGATAACAATTTCACACAGGA", "--na", "50"}), 0);
        QCOMPARE(cli::run({"primer", "tm", "HELLO"}), 1);
        QCOMPARE(cli::run({"primer", "check", "GGATCCGAATTCGGATCC"}), 1);
        QCOMPARE(cli::run({"primer", "check", "AGACGGTCACAGCTTGTCTG", "--json"}), 0);
        QCOMPARE(cli::run({"primer", "design", fasta, "--region", "700..760", "--pairs", "3",
                           "--csv", dir.filePath("o.csv"), "--report", dir.filePath("r.svg")}), 0);
        QVERIFY(QFile::exists(dir.filePath("o.csv")));
        QVERIFY(QFile::exists(dir.filePath("r.svg")));
        QCOMPARE(cli::run({"primer", "design", fasta}), 1);
        QCOMPARE(cli::run({"primer", "design", fasta, "--region", "700..760", "--bogus", "1"}), 1);
        QCOMPARE(cli::run({"primer", "skill", "install", "--dir", dir.path()}), 0);
        QVERIFY(QFile::exists(dir.filePath("primer/SKILL.md")));
    }

    void themeFallsBack() {
        QTemporaryDir dir;
        Theme theme(dir.path());
        QCOMPARE(theme.accent(), QString("#FFD60A"));
        QCOMPARE(theme.accentForeground(), QString("black"));
    }

    void themeReadsAccent() {
        QTemporaryDir dir;
        QDir(dir.path()).mkpath("theme");
        write(dir.path() + "/theme/colors.toml", "accent = \"#112233\"\n");
        Theme theme(dir.path());
        QCOMPARE(theme.accent(), QString("#112233"));
        QCOMPARE(theme.accentForeground(), QString("white"));
    }

    void themeFollowsHyprlandRounding() {
        QTemporaryDir dir;
        const QString response = dir.filePath("response.json"), command = dir.filePath("hyprctl");
        write(command, "#!/bin/sh\ncat '" + response.toUtf8() + "'\n");
        QVERIFY(QFile::setPermissions(command, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
        write(response, "{\"int\":0}");
        Theme theme(dir.path(), nullptr, command);
        QCOMPARE(theme.radius(), 0);
        write(response, "{\"int\":6}");
        QTRY_COMPARE(theme.radius(), 6);
    }

    void translatorFallsBackToEnglish() {
        QTemporaryDir dir;
        Translator translator(dir.path() + "/language", dir.path());
        QCOMPARE(translator.language(), QString("en"));
        QVERIFY(translator.translate("Main", "Save").isNull());
    }

    void translatorFollowsTheLanguageFile() {
        QTemporaryDir dir;
        write(dir.path() + "/language", "pt-BR\n");
        write(dir.path() + "/pt.json", R"({"Save": "Guardar", "Open": "Abrir"})");
        write(dir.path() + "/pt-BR.json", R"({"Save": "Salvar"})");
        Translator translator(dir.path() + "/language", dir.path());
        QCOMPARE(translator.language(), QString("pt-BR"));
        QCOMPARE(translator.translate("Main", "Save"), QString("Salvar"));
        QCOMPARE(translator.translate("Main", "Open"), QString("Abrir"));
        QVERIFY(translator.translate("Main", "Quit").isNull());
    }

    void translatorPicksPluralForms() {
        QTemporaryDir dir;
        write(dir.path() + "/language", "ru\n");
        write(dir.path() + "/en.json", R"({"%n clips": {"one": "%n clip", "other": "%n clips"}})");
        write(dir.path() + "/ru.json", R"({"%n clips": {"one": "%n клип", "few": "%n клипа", "many": "%n клипов", "other": "%n клипа"}})");
        Translator translator(dir.path() + "/language", dir.path());
        QCOMPARE(translator.translate("Main", "%n clips", nullptr, 21), QString("%n клип"));
        QCOMPARE(translator.translate("Main", "%n clips", nullptr, 22), QString("%n клипа"));
        QCOMPARE(translator.translate("Main", "%n clips", nullptr, 25), QString("%n клипов"));
        QCOMPARE(Translator::pluralCategory("ar", 11), QString("many"));
        QCOMPARE(Translator::pluralCategory("ja", 1), QString("other"));
    }

    // Every catalog parses, and every translation keeps its placeholders.
    void catalogsAreComplete() {
        const QStringList catalogs = QDir(":/i18n").entryList({"*.json"});
        QVERIFY(catalogs.contains("en.json"));
        QVERIFY(catalogs.size() >= 2);
        for (const QString &name : catalogs) {
            QFile file(":/i18n/" + name);
            QVERIFY(file.open(QIODevice::ReadOnly));
            QJsonParseError parse;
            const QJsonObject object = QJsonDocument::fromJson(file.readAll(), &parse).object();
            QVERIFY2(parse.error == QJsonParseError::NoError, qPrintable(name));
            static const QRegularExpression placeholder("%[0-9n]");
            for (auto it = object.begin(); it != object.end(); ++it) {
                if (!it.value().isString())
                    continue;
                auto found = [](const QString &s) {
                    QStringList list;
                    for (auto m = placeholder.globalMatch(s); m.hasNext();)
                        list.append(m.next().captured());
                    list.sort();
                    return list;
                };
                QVERIFY2(found(it.key()) == found(it.value().toString()), qPrintable(name + ": " + it.key()));
            }
        }
    }

private:
    static checks::Status statusOf(const QVector<checks::Check> &list, checks::Check::Kind kind) {
        for (const auto &c : list)
            if (c.kind == kind)
                return c.status;
        return checks::Status::Pass;
    }

    static void write(const QString &path, const QByteArray &contents) {
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(contents);
    }
};

QTEST_MAIN(PrimerTests)
#include "primer_tests.moc"
