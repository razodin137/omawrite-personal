#pragma once

#include <QJsonObject>
#include <QList>
#include <QPair>
#include <QString>
#include <QStringList>

// The largest piece of text a selection names: one word, one sentence or one
// paragraph. A group never sits inside another group.
enum class AlternativeScope {
    Word,
    Sentence,
    Paragraph,
};

// One run of text with alternatives. `start` and `length` point into the body
// WriteCam is editing; `versions` holds every wording and `showing` names the
// one in the body.
struct WriteCamAlternative {
    using Scope = AlternativeScope;
    static constexpr AlternativeScope Word = AlternativeScope::Word;
    static constexpr AlternativeScope Sentence = AlternativeScope::Sentence;
    static constexpr AlternativeScope Paragraph = AlternativeScope::Paragraph;

    int start = 0;
    int length = 0;
    QStringList versions;
    int showing = 0;
    AlternativeScope scope = AlternativeScope::Word;
};

// A range found by widening a selection to the largest scope it covers.
struct AlternativeRange {
    int start = 0;
    int length = 0;
    AlternativeScope scope = AlternativeScope::Word;
};

// A range that stays in the file while it is dimmed in the editor.
struct WriteCamGhost {
    int start = 0;
    int length = 0;
    QString text;
};

// The alternatives, the version showing and the ghosted runs that ride along
// in the trailing <!-- writecam --> comment of a Markdown file. The comment is
// the only home for this state: read it back with fromFileText(), write it
// with withComment().
class WriteCamState {
public:
    static QString bodyOf(const QString &fileText);
    static WriteCamState fromFileText(const QString &fileText, QString *body = nullptr);
    static WriteCamState fromJson(const QJsonObject &json, const QString &body);
    static QString withComment(const QString &body, const WriteCamState &state);

    // Widens a selection to the largest scope it covers. An empty selection
    // takes the word at the caret.
    static AlternativeRange alternativeRange(const QString &body, int start, int end);
    static AlternativeRange rangeForAlternative(const QString &body, int start, int end)
    {
        return alternativeRange(body, start, end);
    }
    static QPair<int, int> scopeForSelection(const QString &body, int start, int end);

    static QString scopeName(AlternativeScope scope);
    static AlternativeScope scopeFromName(const QString &name);

    bool isEmpty() const;
    const QList<WriteCamAlternative> &alternatives() const { return m_alternatives; }
    QList<WriteCamAlternative> &alternatives() { return m_alternatives; }
    const QList<WriteCamGhost> &ghosts() const { return m_ghosts; }
    QList<WriteCamGhost> &ghosts() { return m_ghosts; }

    // Drops alternatives and ghosts whose words have left the body, and moves
    // the ones that remain to where they now sit.
    void reconcile(const QString &body);
    QJsonObject toJson() const;

private:
    QList<WriteCamAlternative> m_alternatives;
    QList<WriteCamGhost> m_ghosts;
};
