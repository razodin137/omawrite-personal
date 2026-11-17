#pragma once

#include <QJsonArray>
#include <QString>
#include <QStringList>
#include <QUrl>

// The one place WriteCam learns how to reach a language model: where the
// request goes, which request shape it speaks, which model answers, and the
// key that proves the writer may ask. Settings are the source; when a setting
// is empty the matching WRITECAM_AI_* environment variable is used instead.
struct AiConfig {
    QUrl address;            // the complete endpoint, used exactly as entered
    QString wireFormat;      // "openai", "ollama", or "anthropic"
    QString model;           // sent as-is
    QString apiKey;          // empty = no Authorization header
    int timeoutMs = 15000;
};

class AiService {
public:
    static QStringList wireFormats();
    static QString wireFormatLabel(const QString &wireFormat);
    // Canonicalizes a known spelling. An unknown non-empty value is returned
    // trimmed and lowercased so validation can name it rather than hide it.
    static QString normalizeWireFormat(const QString &wireFormat);

    // Stored settings first, then the matching environment variable, field by
    // field. The key is read here and never handed to the QML page.
    static AiConfig loadConfig();

    // Fills *error when the configuration cannot be used. Nothing is fetched
    // or rewritten: the address must already be the endpoint to POST to.
    static bool validate(const AiConfig &config, QString *error = nullptr);

    // Blocking POST to config.address. Returns the reply text, or an empty
    // string with *error set when the server is unreachable, slow, or answers
    // outside the chosen wire format.
    static QString complete(const AiConfig &config, const QJsonArray &messages,
                            QString *error = nullptr, int maxTokens = 512);

    // Sends one tiny prompt to the same address as complete(). A valid answer
    // with no text still proves the address, wire format, and key work.
    static bool testConnection(const AiConfig &config, QString *error = nullptr);
};
