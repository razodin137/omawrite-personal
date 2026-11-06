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

bool isWordCharacter(QChar character) {
    return character.isLetterOrNumber() || character == QLatin1Char('_')
        || character.category() == QChar::Mark_NonSpacing
        || character.category() == QChar::Mark_SpacingCombining;
}

// An apostrophe or hyphen only joins a word when letters sit on both sides, so
// a dash between words stays outside the run.
bool isWordJoiner(const QString &text, int position) {
    const QChar character = text.at(position);
    if (character != QLatin1Char('\'') && character != QLatin1Char('-')
            && character != QChar(0x2019))
        return false;
    return position > 0 && position + 1 < text.size()
        && isWordCharacter(text.at(position - 1))
        && isWordCharacter(text.at(position + 1));
}

bool isWordCharacterAt(const QString &text, int position) {
    return position >= 0 && position < text.size()
        && (isWordCharacter(text.at(position)) || isWordJoiner(text, position));
}

QPair<int, int> wordRangeAt(const QString &text, int position) {
    position = qBound(0, position, text.size());
    int start = position;
    while (start > 0 && isWordCharacterAt(text, start - 1))
        --start;
    int end = position;
    while (end < text.size() && isWordCharacterAt(text, end))
        ++end;
    return {start, end};
}

bool isBlankLine(const QString &text, int start, int end) {
    return text.mid(start, end - start).trimmed().isEmpty();
}

int paragraphStart(const QString &text, int position) {
    position = qBound(0, position, text.size());
    int lineStart = position == 0 ? 0 : text.lastIndexOf(QLatin1Char('\n'), position - 1) + 1;
    while (lineStart > 0) {
        const int previousEnd = lineStart - 1;
        const int previousStart = text.lastIndexOf(QLatin1Char('\n'), previousEnd - 1) + 1;
        if (isBlankLine(text, previousStart, previousEnd))
            break;
        lineStart = previousStart;
    }
    return lineStart;
}

int paragraphEnd(const QString &text, int position) {
    position = qBound(0, position, text.size());
    int lineEnd = text.indexOf(QLatin1Char('\n'), position);
    if (lineEnd < 0)
        return text.size();
    while (lineEnd < text.size()) {
        const int nextStart = lineEnd + 1;
        if (nextStart >= text.size())
            return text.size();
        int nextEnd = text.indexOf(QLatin1Char('\n'), nextStart);
        if (nextEnd < 0)
            nextEnd = text.size();
        if (isBlankLine(text, nextStart, nextEnd))
            return lineEnd;
        lineEnd = nextEnd;
    }
    return lineEnd;
}

bool isSentenceTerminator(QChar character) {
    return character == QLatin1Char('.') || character == QLatin1Char('!')
        || character == QLatin1Char('?') || character == QChar(0x2026);
}

bool isClosingPunctuation(QChar character) {
    static const QString closing = QStringLiteral("\"')]}”’");
    return closing.contains(character) || isSentenceTerminator(character);
}

QPair<int, int> sentenceAt(const QString &text, int position, int start, int end) {
    int cursor = start;
    while (cursor < end) {
        while (cursor < end && text.at(cursor).isSpace())
            ++cursor;
        if (cursor >= end)
            break;

        const int sentenceStart = cursor;
        int sentenceEnd = end;
        while (cursor < end) {
            if (isSentenceTerminator(text.at(cursor))) {
                int after = cursor + 1;
                while (after < end && isClosingPunctuation(text.at(after)))
                    ++after;
                if (after >= end || text.at(after).isSpace()) {
                    sentenceEnd = after;
                    cursor = after;
                    break;
                }
            }
            ++cursor;
        }
        if (position >= sentenceStart && position < sentenceEnd)
            return {sentenceStart, sentenceEnd};
    }
    return {start, end};
}

int trimmedEnd(const QString &text, int start, int end) {
    while (end > start && text.at(end - 1).isSpace())
        --end;
    return end;
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
        const QJsonValue scope = object.value(QStringLiteral("scope"));
        if (scope.isString()) {
            alternative.scope = scopeFromName(scope.toString());
        } else if (scope.isDouble()) {
            switch (scope.toInt()) {
            case 1:
                alternative.scope = AlternativeScope::Sentence;
                break;
            case 2:
                alternative.scope = AlternativeScope::Paragraph;
                break;
            default:
                alternative.scope = AlternativeScope::Word;
                break;
            }
        }
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

QString WriteCamState::scopeName(AlternativeScope scope) {
    switch (scope) {
    case AlternativeScope::Sentence:
        return QStringLiteral("sentence");
    case AlternativeScope::Paragraph:
        return QStringLiteral("paragraph");
    case AlternativeScope::Word:
        break;
    }
    return QStringLiteral("word");
}

AlternativeScope WriteCamState::scopeFromName(const QString &name) {
    if (name == QStringLiteral("sentence"))
        return AlternativeScope::Sentence;
    if (name == QStringLiteral("paragraph"))
        return AlternativeScope::Paragraph;
    return AlternativeScope::Word;
}

AlternativeRange WriteCamState::alternativeRange(const QString &body, int start, int end) {
    AlternativeRange range;
    if (start > end)
        std::swap(start, end);
    start = qBound(0, start, body.size());
    end = qBound(0, end, body.size());

    // A caret picks the word under it; a selection first drops the whitespace
    // it may have caught at either end.
    int first = start;
    int last = end;
    if (first == last) {
        const QPair<int, int> word = wordRangeAt(body, first);
        if (word.first < word.second) {
            range.start = word.first;
            range.length = word.second - word.first;
            range.scope = AlternativeScope::Word;
        }
        return range;
    }

    while (first < last && body.at(first).isSpace())
        ++first;
    while (last > first && body.at(last - 1).isSpace())
        --last;
    if (first >= last)
        return range;

    // One word, however much of it the selection caught.
    const QPair<int, int> firstWord = wordRangeAt(body, first);
    if (firstWord.first <= first && last <= firstWord.second) {
        range.start = firstWord.first;
        range.length = firstWord.second - firstWord.first;
        range.scope = AlternativeScope::Word;
        return range;
    }

    const int paragraphBegin = paragraphStart(body, first);
    const int paragraphFinish = paragraphEnd(body, last - 1);
    const QPair<int, int> firstSentence =
        sentenceAt(body, first, paragraphBegin, paragraphEnd(body, first));
    const QPair<int, int> lastSentence =
        sentenceAt(body, last - 1, paragraphStart(body, last - 1), paragraphFinish);
    if (firstSentence == lastSentence) {
        range.start = firstSentence.first;
        range.length = trimmedEnd(body, firstSentence.first, firstSentence.second)
            - firstSentence.first;
        range.scope = AlternativeScope::Sentence;
        return range;
    }

    range.start = paragraphBegin;
    range.length = trimmedEnd(body, paragraphBegin, paragraphFinish) - paragraphBegin;
    range.scope = AlternativeScope::Paragraph;
    return range;
}

QPair<int, int> WriteCamState::scopeForSelection(const QString &body, int start, int end) {
    const AlternativeRange range = alternativeRange(body, start, end);
    return {range.start, range.length};
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
            {QStringLiteral("scope"), scopeName(alternative.scope)},
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
