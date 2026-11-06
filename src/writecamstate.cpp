#include "writecamstate.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonValue>

#include <utility>

namespace {

const QString writeCamMarker = QStringLiteral("<!-- writecam");
const QString commentEnd = QStringLiteral("-->");

struct CommentRange {
    bool found = false;
    int start = 0;
    QJsonObject json;
};

// Only the last comment counts, and only when nothing but whitespace follows
// it. A writer may quote the marker earlier in the piece without handing the
// document to WriteCam.
CommentRange trailingComment(const QString &text) {
    CommentRange range;
    int marker = text.lastIndexOf(writeCamMarker);
    while (marker >= 0) {
        const int close = text.indexOf(commentEnd, marker + writeCamMarker.size());
        if (close >= 0 && text.mid(close + commentEnd.size()).trimmed().isEmpty()) {
            const QString payload = text.mid(marker + writeCamMarker.size(),
                                             close - marker - writeCamMarker.size()).trimmed();
            const QJsonDocument document = QJsonDocument::fromJson(payload.toUtf8());
            if (document.isObject()) {
                range.found = true;
                range.start = marker;
                range.json = document.object();
                return range;
            }
        }
        marker = text.lastIndexOf(writeCamMarker, marker - 1);
    }
    return range;
}

QString bodyBefore(const QString &fileText, int commentStart) {
    // The comment follows the piece byte for byte. A body that already ends in
    // a newline puts the comment on its own line; one that does not keeps its
    // last character, and Markdown still hides the inline comment.
    return fileText.left(commentStart);
}

}  // namespace

QString WriteCamState::bodyOf(const QString &fileText) {
    const CommentRange comment = trailingComment(fileText);
    return comment.found ? bodyBefore(fileText, comment.start) : fileText;
}

WriteCamState WriteCamState::fromFileText(const QString &fileText, QString *body) {
    const CommentRange comment = trailingComment(fileText);
    const QString documentBody = comment.found ? bodyBefore(fileText, comment.start) : fileText;
    if (body)
        *body = documentBody;
    if (!comment.found)
        return {};
    return fromJson(comment.json, documentBody);
}

WriteCamState WriteCamState::fromJson(const QJsonObject &json, const QString &body) {
    WriteCamState state;
    const QJsonArray alternatives = json.value(QStringLiteral("alternatives")).toArray();
    for (const QJsonValue &value : alternatives) {
        const QJsonObject object = value.toObject();
        WriteCamAlternative alternative;
        const QJsonArray versions = object.value(QStringLiteral("versions")).toArray();
        for (const QJsonValue &version : versions) {
            if (version.isString() && !version.toString().isEmpty())
                alternative.versions.append(version.toString());
        }
        if (alternative.versions.isEmpty())
            continue;
        alternative.showing = qBound(0, object.value(QStringLiteral("showing")).toInt(0),
                                     int(alternative.versions.size()) - 1);
        state.m_alternatives.append(alternative);
    }

    const QJsonArray ghosts = json.value(QStringLiteral("ghosted")).toArray();
    for (const QJsonValue &value : ghosts) {
        const QString text = value.toObject().value(QStringLiteral("text")).toString();
        if (text.isEmpty())
            continue;
        WriteCamGhost ghost;
        ghost.text = text;
        state.m_ghosts.append(ghost);
    }

    state.reconcile(body);
    return state;
}

QString WriteCamState::withComment(const QString &body, const WriteCamState &state) {
    WriteCamState live = state;
    live.reconcile(body);
    if (live.isEmpty())
        return body;

    const QByteArray json = QJsonDocument(live.toJson()).toJson(QJsonDocument::Compact);
    QString document = body;
    document += writeCamMarker + QLatin1Char('\n') + QString::fromUtf8(json)
        + QLatin1Char('\n') + commentEnd;
    if (!document.endsWith(QLatin1Char('\n')))
        document += QLatin1Char('\n');
    return document;
}

bool WriteCamState::isEmpty() const {
    return m_alternatives.isEmpty() && m_ghosts.isEmpty();
}

void WriteCamState::reconcile(const QString &body) {
    QList<WriteCamAlternative> keptAlternatives;
    int alternativeCursor = 0;
    for (WriteCamAlternative alternative : std::as_const(m_alternatives)) {
        if (alternative.versions.isEmpty())
            continue;
        if (alternative.showing < 0 || alternative.showing >= alternative.versions.size())
            alternative.showing = 0;

        int foundStart = -1;
        int foundVersion = -1;

        // The showing version is the best clue to where the run moved. Only
        // when an outside edit replaced those words do the other versions get
        // the chance to keep the alternative alive.
        const QString showingText = alternative.versions.at(alternative.showing);
        if (!showingText.isEmpty()) {
            foundStart = body.indexOf(showingText, alternativeCursor);
            if (foundStart >= 0)
                foundVersion = alternative.showing;
        }
        if (foundStart < 0) {
            for (int version = 0; version < alternative.versions.size(); ++version) {
                const QString &text = alternative.versions.at(version);
                if (text.isEmpty())
                    continue;
                const int start = body.indexOf(text, alternativeCursor);
                if (start < 0)
                    continue;
                if (foundStart < 0 || start < foundStart) {
                    foundStart = start;
                    foundVersion = version;
                }
            }
        }
        if (foundStart < 0)
            continue;

        alternative.start = foundStart;
        alternative.length = alternative.versions.at(foundVersion).size();
        alternative.showing = foundVersion;
        alternativeCursor = foundStart + alternative.length;
        keptAlternatives.append(alternative);
    }
    m_alternatives = keptAlternatives;

    QList<WriteCamGhost> keptGhosts;
    int ghostCursor = 0;
    for (WriteCamGhost ghost : std::as_const(m_ghosts)) {
        if (ghost.text.isEmpty())
            continue;
        const int start = body.indexOf(ghost.text, ghostCursor);
        if (start < 0)
            continue;
        ghost.start = start;
        ghost.length = ghost.text.size();
        ghostCursor = start + ghost.length;
        keptGhosts.append(ghost);
    }
    m_ghosts = keptGhosts;
}

QJsonObject WriteCamState::toJson() const {
    QJsonArray alternatives;
    for (const WriteCamAlternative &alternative : m_alternatives) {
        QJsonArray versions;
        for (const QString &version : alternative.versions)
            versions.append(version);
        alternatives.append(QJsonObject{
            {QStringLiteral("versions"), versions},
            {QStringLiteral("showing"), alternative.showing},
        });
    }

    QJsonArray ghosts;
    for (const WriteCamGhost &ghost : m_ghosts)
        ghosts.append(QJsonObject{{QStringLiteral("text"), ghost.text}});

    return QJsonObject{
        {QStringLiteral("version"), 1},
        {QStringLiteral("alternatives"), alternatives},
        {QStringLiteral("ghosted"), ghosts},
    };
}
