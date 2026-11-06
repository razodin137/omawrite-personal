#pragma once

#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QUrl>

class QNetworkAccessManager;

// One decision from the tag schema: a System One question plus the rules
// that turn its answer into a front-matter tag.
struct TagQuestion {
    QString id;               // question key in the request and answer maps
    QString type;             // "choice" or "noul"
    QString instructions;
    QJsonObject criteria;     // choice: option -> description; noul: optional
    QJsonObject choiceTags;   // choice: option -> tag; unmapped options stay silent
    QString noulTag;          // noul: tag added when P(true) clears the threshold
    double threshold = 0.90;   // answers below this confidence add nothing
};

// A System One decision server. The parts are swappable: the same client
// talks to laya-serve (Laya), Ollama's decision models, or TypeSafe Jev by
// changing the endpoint, model name, and API key — they share the
// /v1/systemone request and response shape.
struct DecisionServer {
    QUrl endpoint;            // e.g. http://127.0.0.1:8000/v1/systemone
    QString model;            // sent as-is; servers that route ignore it
    QString apiKey;           // empty = no Authorization header
    int timeoutMs = 6000;
};

class SystemOne {
public:
    // Parses a tag schema document:
    // {"stateChars": 4000, "questions": {"topic": {"type": "choice",
    //  "instructions": "...", "criteria": {"a": "..."}, "tags": {"a": "a"},
    //  "threshold": 0.9}, "journal": {"type": "noul", "instructions": "...",
    //  "tag": "journal"}}}
    // Returns an empty list and fills *error on any malformed entry. When
    // stateChars is given it receives the schema's state truncation length.
    static QList<TagQuestion> parseSchema(const QByteArray &json, QString *error = nullptr,
                                          int *stateChars = nullptr);

    // Loads the schema at path, writing the embedded default there first so
    // it can be edited. Falls back to the embedded default when the
    // directory is not writable.
    static QList<TagQuestion> loadSchema(const QString &path, QString *error = nullptr,
                                         int *stateChars = nullptr);
    static QByteArray defaultSchema();

    static QByteArray buildRequestBody(const QString &state, const QList<TagQuestion> &questions,
                                       const QString &model);

    // Turns one answer map into tags: a choice contributes the tag its
    // chosen option maps to when the option's own probability clears the
    // question's threshold; a noul contributes its tag when P(true) does.
    static QStringList tagsFromAnswers(const QJsonObject &answers,
                                       const QList<TagQuestion> &questions);

    // Blocking POST to the server's /v1/systemone. Returns the decided tags,
    // or an empty list with *error set when the server is unreachable, slow,
    // or answers with anything but 200 + parseable JSON.
    static QStringList decideTags(const DecisionServer &server, const QString &state,
                                  const QList<TagQuestion> &questions, QString *error = nullptr);
};