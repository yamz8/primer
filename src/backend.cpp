#include "backend.h"

#include <QClipboard>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QRegularExpression>

#include "dna.h"
#include "filepicker.h"
#include "report.h"

namespace {

constexpr int UndoLimit = 100;

QVariantMap structureMap(const thermo::Structure &s) {
    if (s.kind == thermo::Structure::None)
        return {};
    return {{"dG", s.dG}, {"basePairs", s.basePairs}, {"threePrime", s.threePrime},
            {"drawing", s.drawing.join('\n')}};
}

QVariantList checkList(const QVector<checks::Check> &list) {
    QVariantList result;
    for (const auto &c : list)
        result.append(QVariantMap{{"status", checks::statusName(c.status)}, {"message", checks::describe(c)},
                                  {"name", checks::checkName(c.kind)}});
    return result;
}

QVariantMap primerMap(const checks::PrimerReport &r) {
    return {{"sequence", QString::fromLatin1(r.sequence)},
            {"length", int(r.sequence.size())},
            {"tm", r.tm},
            {"gc", r.gc},
            {"endStability", r.endStability},
            {"status", checks::statusName(r.status)},
            {"checks", checkList(r.checks)},
            {"hairpin", structureMap(r.hairpin)},
            {"selfDimer", structureMap(r.selfDimer)},
            {"sites", int(r.sites.size())}};
}

QVariantMap pairMap(const checks::PairReport &p) {
    return {{"forward", primerMap(p.forward)},
            {"reverse", primerMap(p.reverse)},
            {"checks", checkList(p.checks)},
            {"crossDimer", structureMap(p.crossDimer)},
            {"tmDifference", p.tmDifference},
            {"productSize", p.productSize},
            {"start", p.start},
            {"end", p.end},
            {"score", p.score},
            {"status", checks::statusName(p.status)}};
}

// The site a primer was designed for, or its first forward/reverse one.
const checks::Site *primarySite(const checks::PrimerReport &r, bool forwardStrand, int near) {
    const checks::Site *best = nullptr;
    for (const auto &s : r.sites)
        if (s.forwardStrand == forwardStrand && (!best || (near >= 0 && (s.start == near || s.end == near))))
            best = &s;
    return best;
}

} // namespace

Backend::Backend(FilePicker *picker, QObject *parent) : QObject(parent), m_picker(picker) {
    if (m_picker)
        connect(m_picker, &FilePicker::failed, this, &Backend::setError);
}

QString Backend::regionText() const {
    return m_doc.regionStart < 0 ? QString() : QString("%1..%2").arg(m_doc.regionStart + 1).arg(m_doc.regionEnd + 1);
}

QVariantMap Backend::settings() const {
    const auto &s = m_doc.settings;
    return {{"lengthMin", s.lengthMin}, {"lengthOpt", s.lengthOpt}, {"lengthMax", s.lengthMax},
            {"tmMin", s.tmMin}, {"tmOpt", s.tmOpt}, {"tmMax", s.tmMax}, {"tmDifference", s.tmDifferenceMax},
            {"gcMin", s.gcMin}, {"gcMax", s.gcMax}, {"gcClamp", s.gcClamp},
            {"productMin", s.productMin}, {"productMax", s.productMax},
            {"sodium", s.conditions.sodium}, {"magnesium", s.conditions.magnesium},
            {"dntp", s.conditions.dntp}, {"primer", s.conditions.primer}};
}

QVariantList Backend::pairs() const {
    if (!m_pairsCacheValid) {
        m_pairsCache.clear();
        for (const auto &p : m_pairs) {
            QVariantMap m = pairMap(p);
            m.insert("picked", isPicked(p));
            m_pairsCache.append(m);
        }
        m_pairsCacheValid = true;
    }
    return m_pairsCache;
}

QVariantMap Backend::current() const {
    const QVariantList all = pairs();
    return m_current >= 0 && m_current < all.size() ? all[m_current].toMap() : QVariantMap();
}

void Backend::setCurrentIndex(int index) {
    if (m_pairs.isEmpty())
        index = -1;
    else
        index = qBound(0, index, int(m_pairs.size()) - 1);
    if (index == m_current)
        return;
    m_current = index;
    emit currentChanged();
    emit marksChanged();
    if (index >= 0)
        emit reveal(m_pairs[index].start);
}

QVariantList Backend::marks() const {
    QVariantList list;
    auto add = [&](int start, int end, const char *kind) {
        list.append(QVariantMap{{"start", start}, {"end", end}, {"kind", kind}});
    };
    if (m_doc.mode == "design" && m_current >= 0 && m_current < m_pairs.size()) {
        const auto &p = m_pairs[m_current];
        add(p.start, p.end, "amplicon");
        if (const auto *f = primarySite(p.forward, true, p.start))
            add(f->start, f->end, "forward");
        if (const auto *r = primarySite(p.reverse, false, p.end))
            add(r->start, r->end, "reverse");
    } else if (m_doc.mode == "check" && m_checkError.isEmpty()) {
        const bool pair = !m_doc.checkReverse.isEmpty();
        const checks::PrimerReport &f = pair ? m_checkPair.forward : m_checkPrimer;
        if (pair && m_checkPair.start >= 0)
            add(m_checkPair.start, m_checkPair.end, "amplicon");
        for (const auto &s : f.sites)
            add(s.start, s.end, s.forwardStrand ? "forward" : "reverse");
        if (pair)
            for (const auto &s : m_checkPair.reverse.sites)
                add(s.start, s.end, s.forwardStrand ? "forward" : "reverse");
    }
    for (int i = 0; i < m_matches.size(); ++i)
        add(m_matches[i].first, m_matches[i].second, i == m_matchIndex ? "current" : "match");
    return list;
}

void Backend::setMode(const QString &mode) {
    const QString m = mode == "check" ? "check" : "design";
    if (m == m_doc.mode)
        return;
    m_doc.mode = m;
    if (m == "check")
        runCheck();
    emit modeChanged();
    emit marksChanged();
}

void Backend::setCheckUsesTemplate(bool use) {
    if (use == m_checkUsesTemplate)
        return;
    m_checkUsesTemplate = use;
    runCheck();
}

QVariantMap Backend::checkReport() const {
    if (!m_checkError.isEmpty())
        return {{"kind", "error"}, {"message", m_checkError}};
    if (m_doc.checkForward.isEmpty())
        return {{"kind", "none"}};
    if (m_doc.checkReverse.isEmpty())
        return {{"kind", "primer"}, {"primer", primerMap(m_checkPrimer)}, {"hasTemplate", m_checkPrimer.hasTemplate}};
    return {{"kind", "pair"}, {"pair", pairMap(m_checkPair)}, {"hasTemplate", m_checkPair.forward.hasTemplate}};
}

QByteArray Backend::checkTemplate() const {
    return m_checkUsesTemplate ? m_doc.sequence : QByteArray();
}

void Backend::runCheck() {
    m_checkError.clear();
    QString error;
    const QByteArray f = m_doc.checkForward, r = m_doc.checkReverse;
    if (!f.isEmpty() && f.size() < 2)
        m_checkError = tr("The forward primer is too short");
    else if (!r.isEmpty() && f.isEmpty())
        m_checkError = tr("Enter a forward primer too");
    else if (!r.isEmpty())
        m_checkPair = checks::evaluatePair(f, r, m_doc.settings, checkTemplate());
    else if (!f.isEmpty())
        m_checkPrimer = checks::evaluatePrimer(f, m_doc.settings, checkTemplate());
    emit checkChanged();
    emit marksChanged();
}

void Backend::setCheckPrimers(const QString &forward, const QString &reverse) {
    QString error;
    const QByteArray f = forward.trimmed().toLatin1(), r = reverse.trimmed().toLatin1();
    if ((!f.isEmpty() && !dna::isSequence(f, &error)) || (!r.isEmpty() && !dna::isSequence(r, &error))) {
        m_checkError = error;
        emit checkChanged();
        emit marksChanged();
        return;
    }
    const QByteArray cf = dna::clean(f), cr = dna::clean(r);
    if (cf != m_doc.checkForward || cr != m_doc.checkReverse) {
        m_doc.checkForward = cf;
        m_doc.checkReverse = cr;
        setDirty(true);
    }
    runCheck();
}

void Backend::capture() {
    m_undo.append({m_doc, m_designed});
    if (m_undo.size() > UndoLimit)
        m_undo.removeFirst();
    m_redo.clear();
    emit historyChanged();
}

void Backend::restore(const Snapshot &s) {
    const bool sameSequence = s.doc.sequence == m_doc.sequence && s.doc.name == m_doc.name;
    m_doc = s.doc;
    if (!sameSequence) {
        search({});
        emit documentChanged();
    }
    emit regionChanged();
    emit settingsChanged();
    emit modeChanged();
    if (s.designed)
        runDesign(true);
    else
        clearDesign();
    runCheck();
    setDirty(true);
}

void Backend::undo() {
    if (m_undo.isEmpty())
        return;
    m_redo.append({m_doc, m_designed});
    restore(m_undo.takeLast());
    emit historyChanged();
}

void Backend::redo() {
    if (m_redo.isEmpty())
        return;
    m_undo.append({m_doc, m_designed});
    restore(m_redo.takeLast());
    emit historyChanged();
}

void Backend::setDirty(bool dirty) {
    if (dirty == m_dirty)
        return;
    m_dirty = dirty;
    emit dirtyChanged();
}

void Backend::setMessage(const QString &message) {
    m_message = message;
    emit messageChanged();
}

void Backend::setError(const QString &error) {
    m_error = error;
    emit messageChanged();
}

void Backend::dismissError() {
    setError({});
}

void Backend::retranslate() {
    m_pairsCacheValid = false;
    emit pairsChanged();
    emit currentChanged();
    emit checkChanged();
}

void Backend::clearDesign() {
    m_pairs.clear();
    m_pairsCacheValid = false;
    m_designed = false;
    m_current = -1;
    emit pairsChanged();
    emit currentChanged();
    emit marksChanged();
}

void Backend::replaceDocument(const Document &doc, const QString &path) {
    m_doc = doc;
    m_filePath = path;
    m_undo.clear();
    m_redo.clear();
    search({});
    clearDesign();
    setError({});
    emit documentChanged();
    emit filePathChanged();
    emit regionChanged();
    emit settingsChanged();
    emit modeChanged();
    emit historyChanged();
    runCheck();
    // A saved document with a target and picks comes back designed.
    if (m_doc.regionStart >= 0 && !m_doc.sequence.isEmpty())
        runDesign(true);
    setDirty(false);
}

void Backend::openFile() {
    if (!m_picker)
        return;
    m_picker->open(tr("Open a template"),
                   {{tr("Sequences and Primer files"), {"*.primer", "*.fa", "*.fasta", "*.fna", "*.fas", "*.gb", "*.gbk", "*.genbank", "*.txt", "*.seq"}},
                    {tr("All files"), {"*"}}},
                   [this](const QString &path, int) {
                       if (!path.isEmpty())
                           openPath(path);
                   });
}

bool Backend::openPath(const QString &path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        setError(tr("Could not open %1: %2").arg(path, file.errorString()));
        return false;
    }
    const QByteArray data = file.readAll();
    QString error;
    Document doc;
    if (data.trimmed().startsWith('{')) {
        if (!document::load(path, &doc, &error)) {
            setError(error);
            return false;
        }
        replaceDocument(doc, path);
    } else {
        // Sequence files are only read; saving makes a .primer beside them.
        const dna::Record record = dna::parse(data, QFileInfo(path).completeBaseName(), &error);
        if (record.sequence.isEmpty()) {
            setError(error);
            return false;
        }
        doc.name = record.name;
        doc.sequence = record.sequence;
        doc.settings = m_doc.settings;
        replaceDocument(doc, {});
    }
    setMessage(tr("Opened %1").arg(QFileInfo(path).fileName()));
    return true;
}

bool Backend::loadText(const QString &text, const QString &name) {
    QString error;
    const dna::Record record = dna::parse(text.toUtf8(), name.isEmpty() ? tr("Pasted sequence") : name, &error);
    if (record.sequence.isEmpty()) {
        setError(error);
        return false;
    }
    capture();
    Document doc;
    doc.name = record.name;
    doc.sequence = record.sequence;
    doc.settings = m_doc.settings;
    doc.mode = m_doc.mode;
    doc.checkForward = m_doc.checkForward;
    doc.checkReverse = m_doc.checkReverse;
    const auto undo = m_undo;
    replaceDocument(doc, {});
    m_undo = undo;
    emit historyChanged();
    setDirty(true);
    setMessage(tr("Loaded %1 bp").arg(record.sequence.size()));
    return true;
}

bool Backend::pasteClipboard() {
    const QString text = QGuiApplication::clipboard()->text();
    if (text.trimmed().isEmpty()) {
        setError(tr("The clipboard has no text"));
        return false;
    }
    return loadText(text);
}

void Backend::setRegion(int start, int end) {
    if (start > end)
        std::swap(start, end);
    start = qMax(0, start);
    end = qMin(length() - 1, end);
    if (start == m_doc.regionStart && end == m_doc.regionEnd)
        return;
    capture();
    m_doc.regionStart = start;
    m_doc.regionEnd = end;
    setDirty(true);
    emit regionChanged();
}

bool Backend::setRegionText(const QString &text) {
    static const QRegularExpression pattern(R"(^\s*(\d+)\s*(?:\.\.|-|–|:)\s*(\d+)\s*$)");
    const auto m = pattern.match(text);
    if (!m.hasMatch()) {
        setError(tr("Type the target as start..end, like 120..340"));
        return false;
    }
    const int start = m.captured(1).toInt(), end = m.captured(2).toInt();
    if (start < 1 || end > length() || start > end) {
        setError(tr("The target must lie within 1..%1").arg(length()));
        return false;
    }
    setError({});
    setRegion(start - 1, end - 1);
    emit reveal(start - 1);
    return true;
}

bool Backend::setSetting(const QString &key, double v) {
    checks::Settings s = m_doc.settings;
    auto &c = s.conditions;
    const int i = int(std::lround(v));
    if (key == "lengthMin") s.lengthMin = i;
    else if (key == "lengthOpt") s.lengthOpt = i;
    else if (key == "lengthMax") s.lengthMax = i;
    else if (key == "tmMin") s.tmMin = v;
    else if (key == "tmOpt") s.tmOpt = v;
    else if (key == "tmMax") s.tmMax = v;
    else if (key == "tmDifference") s.tmDifferenceMax = v;
    else if (key == "gcMin") s.gcMin = v;
    else if (key == "gcMax") s.gcMax = v;
    else if (key == "gcClamp") s.gcClamp = i;
    else if (key == "productMin") s.productMin = i;
    else if (key == "productMax") s.productMax = i;
    else if (key == "sodium") c.sodium = v;
    else if (key == "magnesium") c.magnesium = v;
    else if (key == "dntp") c.dntp = v;
    else if (key == "primer") c.primer = v;
    else return false;

    if (v < 0 || s.lengthMin < checks::SiteAnchor || s.lengthMin > s.lengthMax || s.tmMin > s.tmMax || s.gcMin > s.gcMax
        || s.gcMax > 100 || s.productMin > s.productMax || c.primer <= 0 || s.gcClamp > 5) {
        setError(tr("That setting would leave an empty or impossible range"));
        emit settingsChanged();  // puts the field back
        return false;
    }
    s.lengthOpt = qBound(s.lengthMin, s.lengthOpt, s.lengthMax);
    s.tmOpt = qBound(s.tmMin, s.tmOpt, s.tmMax);
    capture();
    m_doc.settings = s;
    setError({});
    setDirty(true);
    emit settingsChanged();
    runCheck();
    if (m_designed)
        runDesign(true);
    return true;
}

void Backend::resetSettings() {
    capture();
    m_doc.settings = checks::Settings();
    setDirty(true);
    emit settingsChanged();
    runCheck();
    if (m_designed)
        runDesign(true);
}

bool Backend::design() {
    if (m_doc.sequence.isEmpty()) {
        setError(tr("Open or paste a template first"));
        return false;
    }
    if (m_doc.regionStart < 0) {
        setError(tr("Select a target first: drag across the sequence or type start..end"));
        return false;
    }
    if (!m_designed)
        capture();
    runDesign(false);
    return !m_pairs.isEmpty();
}

void Backend::runDesign(bool quiet) {
    const checks::Design result = checks::design(m_doc.sequence, m_doc.regionStart, m_doc.regionEnd, m_doc.settings);
    m_pairs = result.pairs;
    m_pairsCacheValid = false;
    m_designed = true;
    m_current = m_pairs.isEmpty() ? -1 : 0;
    // Picks that are no longer in the list still stay picked: they are the
    // user's choice, and export works from their sequences.
    emit pairsChanged();
    emit currentChanged();
    emit marksChanged();
    if (!m_pairs.isEmpty()) {
        setError({});
        if (!quiet)
            setMessage(tr("%n pairs", nullptr, int(m_pairs.size())));
        emit reveal(m_pairs.first().start);
    } else {
        setError(result.error);
    }
}

void Backend::move(int delta) {
    if (m_pairs.isEmpty())
        return;
    setCurrentIndex(m_current < 0 ? 0 : m_current + delta);
}

bool Backend::isPicked(const checks::PairReport &p) const {
    for (const auto &pick : m_doc.picked)
        if (pick.first == p.forward.sequence && pick.second == p.reverse.sequence)
            return true;
    return false;
}

void Backend::togglePick(int index) {
    if (index < 0)
        index = m_current;
    if (index < 0 || index >= m_pairs.size())
        return;
    capture();
    const auto &p = m_pairs[index];
    const QPair<QByteArray, QByteArray> key{p.forward.sequence, p.reverse.sequence};
    if (isPicked(p)) {
        m_doc.picked.removeAll(key);
        setMessage(tr("Unpicked pair %1").arg(index + 1));
    } else {
        m_doc.picked.append(key);
        setMessage(tr("Picked pair %1").arg(index + 1));
    }
    m_pairsCacheValid = false;
    setDirty(true);
    emit pairsChanged();
    emit currentChanged();
}

void Backend::search(const QString &query) {
    m_searchQuery = query.trimmed();
    m_matches.clear();
    m_matchIndex = -1;
    const QByteArray q = dna::clean(m_searchQuery.toLatin1());
    bool isNumber = false;
    const int position = m_searchQuery.toInt(&isNumber);
    if (isNumber && position >= 1 && position <= length()) {
        // A number jumps to that position.
        m_matches.append({position - 1, position - 1});
    } else if (q.size() >= 2 && dna::isSequence(m_searchQuery.toLatin1())) {
        for (const QByteArray &needle : {q, dna::reverseComplement(q)}) {
            for (qsizetype at = m_doc.sequence.indexOf(needle); at >= 0; at = m_doc.sequence.indexOf(needle, at + 1)) {
                const QPair<int, int> match{int(at), int(at + needle.size() - 1)};
                if (!m_matches.contains(match))
                    m_matches.append(match);
            }
        }
        std::sort(m_matches.begin(), m_matches.end());
    }
    if (!m_matches.isEmpty())
        m_matchIndex = 0;
    emit searchChanged();
    emit marksChanged();
    if (m_matchIndex >= 0)
        emit reveal(m_matches[0].first);
}

void Backend::nextMatch(int delta) {
    if (m_matches.isEmpty())
        return;
    m_matchIndex = (m_matchIndex + delta + int(m_matches.size())) % int(m_matches.size());
    emit searchChanged();
    emit marksChanged();
    emit reveal(m_matches[m_matchIndex].first);
}

void Backend::save() {
    if (m_filePath.isEmpty())
        saveAs();
    else
        saveTo(m_filePath);
}

void Backend::saveAs() {
    if (!m_picker)
        return;
    m_picker->save(tr("Save"), document::safeName(m_doc.name) + ".primer", {{tr("Primer files"), {"*.primer"}}},
                   [this](const QString &path, int) {
                       if (path.isEmpty())
                           emit saveCancelled();
                       else
                           saveTo(path.endsWith(".primer") ? path : path + ".primer");
                   });
}

bool Backend::saveTo(const QString &path) {
    QString error;
    if (!document::save(m_doc, path, &error)) {
        setError(error);
        return false;
    }
    if (path != m_filePath) {
        m_filePath = path;
        emit filePathChanged();
    }
    setDirty(false);
    setError({});
    setMessage(tr("Saved %1").arg(QFileInfo(path).fileName()));
    emit saved();
    return true;
}

QVector<checks::PairReport> Backend::exportPairs(QVector<checks::PrimerReport> *singles) const {
    QVector<checks::PairReport> list;
    if (m_doc.mode == "check") {
        if (!m_checkError.isEmpty() || m_doc.checkForward.isEmpty())
            return list;
        if (m_doc.checkReverse.isEmpty())
            singles->append(m_checkPrimer);
        else
            list.append(m_checkPair);
        return list;
    }
    // The picked pairs, in the order picked; without picks, the one shown.
    for (const auto &pick : m_doc.picked) {
        auto p = checks::evaluatePair(pick.first, pick.second, m_doc.settings, m_doc.sequence);
        for (const auto &designed : m_pairs)
            if (designed.forward.sequence == pick.first && designed.reverse.sequence == pick.second)
                p.score = designed.score;
        list.append(p);
    }
    if (list.isEmpty() && m_current >= 0 && m_current < m_pairs.size())
        list.append(m_pairs[m_current]);
    return list;
}

void Backend::exportFile() {
    QVector<checks::PrimerReport> singles;
    if (exportPairs(&singles).isEmpty() && singles.isEmpty()) {
        setError(m_doc.mode == "check" ? tr("Enter a primer to export") : tr("Design and pick a pair to export"));
        return;
    }
    if (!m_picker)
        return;
    const QString base = document::safeName(m_doc.mode == "check" && m_doc.name.isEmpty() ? QString("primers") : m_doc.name);
    const QList<FilePicker::Filter> filters{{tr("Oligo order (CSV)"), {"*.csv"}},
                                            {tr("Report (PDF)"), {"*.pdf"}},
                                            {tr("Report (SVG)"), {"*.svg"}}};
    m_picker->save(tr("Export"), base + ".csv", filters, [this](const QString &path, int filter) {
        if (path.isEmpty())
            return;
        QString target = path;
        const QString suffix = QFileInfo(path).suffix().toLower();
        if (suffix != "csv" && suffix != "pdf" && suffix != "svg")
            target += filter == 1 ? ".pdf" : filter == 2 ? ".svg" : ".csv";
        exportTo(target);
    });
}

bool Backend::exportTo(const QString &path) {
    QVector<checks::PrimerReport> singles;
    const QVector<checks::PairReport> list = exportPairs(&singles);
    if (list.isEmpty() && singles.isEmpty()) {
        setError(tr("Nothing to export yet"));
        return false;
    }
    QString error;
    bool ok;
    const QString suffix = QFileInfo(path).suffix().toLower();
    if (suffix == "csv") {
        QByteArray csv;
        if (!singles.isEmpty()) {
            csv = "Name,Sequence (5'-3'),Length,Tm (C),GC %,Product size (bp)\n";
            csv += "primer_1," + singles[0].sequence + ',' + QByteArray::number(singles[0].sequence.size()) + ','
                 + QByteArray::number(singles[0].tm, 'f', 1) + ',' + QByteArray::number(singles[0].gc, 'f', 1) + ",\n";
        } else {
            csv = document::csv(m_doc.mode == "check" && m_doc.name.isEmpty() ? QString("primer") : m_doc.name, list);
        }
        ok = document::writeAtomically(path, csv, &error);
    } else {
        ReportInput input;
        input.name = m_doc.name;
        input.templateLength = m_doc.mode == "check" && !m_checkUsesTemplate ? 0 : length();
        input.regionStart = m_doc.mode == "design" ? m_doc.regionStart : -1;
        input.regionEnd = m_doc.regionEnd;
        input.settings = m_doc.settings;
        input.pairs = list;
        input.singles = singles;
        ok = suffix == "svg" ? report::exportSvg(input, path, &error) : report::exportPdf(input, path, &error);
    }
    if (!ok) {
        setError(error);
        return false;
    }
    setError({});
    setMessage(tr("Exported %1").arg(QFileInfo(path).fileName()));
    return true;
}
