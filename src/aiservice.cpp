#include "aiservice.h"

#include <QEventLoop>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSettings>
#include <QTimer>

namespace {

const QString addressSetting = QStringLiteral("ai/address");
const QString wireFormatSetting = QStringLiteral("ai/wireFormat");
const QString modelSetting = QStringLiteral("ai/model");
const QString apiKeySetting = QStringLiteral("ai/apiKey");
const QString timeoutSetting = QStringLiteral("ai/timeoutMs");
constexpr int defaultTimeoutMs = 15000;

QString environmentValue(std::initializer_list<const char *> names) {
    for (const char *name : names) {
        const QString value = QString::fromUtf8(qgetenv(name)).trimmed();
        if (!value.isEmpty())
            return value;
    }
    return {};
}

QString settingOrEnvironment(QSettings &settings, const QString &key,
                             std::initializer_list<const char *> names) {
    const QString stored = settings.value(key).toString().trimmed();
    return stored.isEmpty() ? environmentValue(names) : stored;
}

QString errorTextFromReply(const QByteArray &payload) {
    const QJsonDocument document = QJsonDocument::fromJson(payload);
    if (!document.isObject())
        return {};
    const QJsonValue error = document.object().value(QStringLiteral("error"));
    if (error.isString())
        return error.toString().trimmed();
    if (error.isObject())
        return error.toObject().value(QStringLiteral("message")).toString().trimmed();
    return {};
}

QJsonObject chatBody(const AiConfig &config, const QJsonArray &messages, int maxTokens) {
    QJsonObject body;
    body.insert(QStringLiteral("model"), config.model);

    if (config.wireFormat == QStringLiteral("anthropic")) {
        // Anthropic takes the system prompt beside the conversation, not as
        // one of its turns.
        QStringList system;
        QJsonArray conversation;
        for (const QJsonValue &value : messages) {
            const QJsonObject message = value.toObject();
            if (message.value(QStringLiteral("role")).toString() == QStringLiteral("system"))
                system.append(message.value(QStringLiteral("content")).toString());
            else
                conversation.append(message);
        }
        body.insert(QStringLiteral("max_tokens"), maxTokens > 0 ? maxTokens : 512);
        body.insert(QStringLiteral("messages"), conversation);
        if (!system.isEmpty())
            body.insert(QStringLiteral("system"), system.join(QStringLiteral("\n\n")));
        return body;
    }

    body.insert(QStringLiteral("messages"), messages);
    if (config.wireFormat == QStringLiteral("ollama")) {
        body.insert(QStringLiteral("stream"), false);
        if (maxTokens > 0) {
            body.insert(QStringLiteral("options"),
                        QJsonObject{{QStringLiteral("num_predict"), maxTokens}});
        }
    } else if (maxTokens > 0) {
        body.insert(QStringLiteral("max_tokens"), maxTokens);
    }
    return body;
}

QNetworkRequest chatRequest(const AiConfig &config) {
    QNetworkRequest request(config.address);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    request.setRawHeader(QByteArrayLiteral("Accept"), QByteArrayLiteral("application/json"));
    // A redirect is a second address the writer did not ask for. Refuse it
    // and let the caller report the 3xx answer instead.
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::ManualRedirectPolicy);

    if (config.wireFormat == QStringLiteral("anthropic")) {
        if (!config.apiKey.isEmpty()) {
            request.setRawHeader(QByteArrayLiteral("x-api-key"), config.apiKey.toUtf8());
        }
        request.setRawHeader(QByteArrayLiteral("anthropic-version"),
                             QByteArrayLiteral("2023-06-01"));
    } else if (!config.apiKey.isEmpty()) {
        request.setRawHeader(QByteArrayLiteral("Authorization"),
                             QStringLiteral("Bearer %1").arg(config.apiKey).toUtf8());
    }
    return request;
}

QString textFromObject(const QJsonObject &object, const QString &wireFormat,
                       QString *error) {
    if (object.contains(QStringLiteral("error"))) {
        const QString message = errorTextFromReply(
            QJsonDocument(object).toJson(QJsonDocument::Compact));
        if (error)
            *error = message.isEmpty() ? QStringLiteral("The server reported an error.")
                                       : message;
        return {};
    }

    if (wireFormat == QStringLiteral("anthropic")) {
        const QJsonValue content = object.value(QStringLiteral("content"));
        if (!content.isArray()) {
            if (error)
                *error = QStringLiteral("The server did not answer in the Anthropic format.");
            return {};
        }
        QString text;
        for (const QJsonValue &part : content.toArray()) {
            const QJsonObject block = part.toObject();
            if (block.value(QStringLiteral("type")).toString() == QStringLiteral("text"))
                text += block.value(QStringLiteral("text")).toString();
        }
        return text;
    }

    if (wireFormat == QStringLiteral("ollama")) {
        const QJsonObject message = object.value(QStringLiteral("message")).toObject();
        if (message.contains(QStringLiteral("content")))
            return message.value(QStringLiteral("content")).toString();
        if (object.contains(QStringLiteral("response")))
            return object.value(QStringLiteral("response")).toString();
        if (error)
            *error = QStringLiteral("The server did not answer in the Ollama format.");
        return {};
    }

    const QJsonValue choices = object.value(QStringLiteral("choices"));
    if (!choices.isArray() || choices.toArray().isEmpty()) {
        if (error)
            *error = QStringLiteral("The server did not answer in the OpenAI format.");
        return {};
    }
    const QJsonObject message = choices.toArray().first().toObject()
                                    .value(QStringLiteral("message")).toObject();
    const QJsonValue content = message.value(QStringLiteral("content"));
    if (content.isString())
        return content.toString();
    if (content.isArray()) {
        QString text;
        for (const QJsonValue &part : content.toArray()) {
            const QJsonObject block = part.toObject();
            if (block.value(QStringLiteral("text")).isString())
                text += block.value(QStringLiteral("text")).toString();
        }
        return text;
    }
    if (error)
        *error = QStringLiteral("The server did not answer in the OpenAI format.");
    return {};
}

QString perform(const AiConfig &config, const QJsonArray &messages, int maxTokens,
                bool requireText, QString *error) {
    if (error)
        error->clear();

    QString problem;
    if (!AiService::validate(config, &problem)) {
        if (error)
            *error = problem;
        return {};
    }

    QNetworkAccessManager network;
    QNetworkRequest request = chatRequest(config);
    const QByteArray body =
        QJsonDocument(chatBody(config, messages, maxTokens)).toJson(QJsonDocument::Compact);
    QNetworkReply *reply = network.post(request, body);

    QTimer timeout;
    timeout.setSingleShot(true);
    QObject::connect(&timeout, &QTimer::timeout, reply, &QNetworkReply::abort);
    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    if (!reply->isFinished()) {
        timeout.start(config.timeoutMs > 0 ? config.timeoutMs : defaultTimeoutMs);
        loop.exec();
    }
    timeout.stop();

    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QByteArray payload = reply->readAll();
    if (reply->error() != QNetworkReply::NoError) {
        if (status > 0) {
            const QString detail = errorTextFromReply(payload);
            reply->deleteLater();
            if (error) {
                *error = detail.isEmpty()
                    ? QStringLiteral("The language model answered HTTP %1.").arg(status)
                    : QStringLiteral("The language model answered HTTP %1: %2")
                          .arg(status)
                          .arg(detail);
            }
            return {};
        }
        const QString message = reply->error() == QNetworkReply::OperationCanceledError
            ? QStringLiteral("The language model timed out.")
            : reply->errorString();
        reply->deleteLater();
        if (error)
            *error = message;
        return {};
    }

    reply->deleteLater();
    if (status < 200 || status >= 300) {
        const QString detail = errorTextFromReply(payload);
        if (error) {
            *error = detail.isEmpty()
                ? QStringLiteral("The language model answered HTTP %1.").arg(status)
                : QStringLiteral("The language model answered HTTP %1: %2")
                      .arg(status)
                      .arg(detail);
        }
        return {};
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(payload, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        if (error)
            *error = QStringLiteral("The language model sent invalid JSON.");
        return {};
    }

    const QString text = textFromObject(document.object(), config.wireFormat, error);
    if (error && !error->isEmpty())
        return {};
    if (requireText && text.trimmed().isEmpty()) {
        if (error)
            *error = QStringLiteral("The language model returned no text.");
        return {};
    }
    return text;
}

}  // namespace

QStringList AiService::wireFormats() {
    return {QStringLiteral("openai"), QStringLiteral("ollama"),
            QStringLiteral("anthropic")};
}

QString AiService::wireFormatLabel(const QString &wireFormat) {
    const QString canonical = normalizeWireFormat(wireFormat);
    if (canonical == QStringLiteral("ollama"))
        return QStringLiteral("Ollama");
    if (canonical == QStringLiteral("anthropic"))
        return QStringLiteral("Anthropic");
    return QStringLiteral("OpenAI");
}

QString AiService::normalizeWireFormat(const QString &wireFormat) {
    const QString trimmed = wireFormat.trimmed().toLower();
    if (trimmed == QStringLiteral("openai") || trimmed == QStringLiteral("openai-compatible")
            || trimmed == QStringLiteral("chat"))
        return QStringLiteral("openai");
    if (trimmed == QStringLiteral("ollama"))
        return QStringLiteral("ollama");
    if (trimmed == QStringLiteral("anthropic") || trimmed == QStringLiteral("claude"))
        return QStringLiteral("anthropic");
    return trimmed;
}

AiConfig AiService::loadConfig() {
    QSettings settings;
    AiConfig config;
    config.address = QUrl(settingOrEnvironment(
        settings, addressSetting,
        {"WRITECAM_AI_ADDRESS", "WRITECAM_AI_ENDPOINT",
         "WRITECAM_LLM_ADDRESS", "WRITECAM_LLM_ENDPOINT"}));

    const QString wireFormat = settingOrEnvironment(
        settings, wireFormatSetting,
        {"WRITECAM_AI_WIRE_FORMAT", "WRITECAM_AI_FORMAT",
         "WRITECAM_LLM_WIRE_FORMAT", "WRITECAM_LLM_FORMAT"});
    config.wireFormat = wireFormat.isEmpty() ? QStringLiteral("openai")
                                             : normalizeWireFormat(wireFormat);

    config.model = settingOrEnvironment(
        settings, modelSetting,
        {"WRITECAM_AI_MODEL", "WRITECAM_LLM_MODEL"});
    config.apiKey = settingOrEnvironment(
        settings, apiKeySetting,
        {"WRITECAM_AI_KEY", "WRITECAM_AI_API_KEY",
         "WRITECAM_LLM_KEY", "WRITECAM_LLM_API_KEY"});

    bool timeoutOk = false;
    const int storedTimeout = settings.value(timeoutSetting).toInt(&timeoutOk);
    config.timeoutMs = timeoutOk && storedTimeout > 0 ? storedTimeout : defaultTimeoutMs;
    if (!timeoutOk) {
        bool environmentOk = false;
        const int environmentTimeout =
            QString::fromUtf8(qgetenv("WRITECAM_AI_TIMEOUT_MS")).toInt(&environmentOk);
        if (environmentOk && environmentTimeout > 0)
            config.timeoutMs = environmentTimeout;
    }
    return config;
}

bool AiService::validate(const AiConfig &config, QString *error) {
    if (error)
        error->clear();
    const auto fail = [error](const QString &message) {
        if (error)
            *error = message;
        return false;
    };

    if (config.address.isEmpty() || !config.address.isValid())
        return fail(QStringLiteral("No address is set."));
    const QString scheme = config.address.scheme().toLower();
    if (scheme != QStringLiteral("http") && scheme != QStringLiteral("https"))
        return fail(QStringLiteral("The address must start with http:// or https://."));
    if (config.address.host().isEmpty())
        return fail(QStringLiteral("The address has no host."));
    if (!wireFormats().contains(config.wireFormat))
        return fail(QStringLiteral("Unknown wire format \"%1\".").arg(config.wireFormat));
    if (config.model.trimmed().isEmpty())
        return fail(QStringLiteral("No model is set."));
    return true;
}

QString AiService::complete(const AiConfig &config, const QJsonArray &messages,
                            QString *error, int maxTokens) {
    if (messages.isEmpty()) {
        if (error)
            *error = QStringLiteral("There is nothing to ask the language model.");
        return {};
    }
    return perform(config, messages, maxTokens, true, error);
}

bool AiService::testConnection(const AiConfig &config, QString *error) {
    const QJsonArray messages{QJsonObject{{QStringLiteral("role"), QStringLiteral("user")},
                                          {QStringLiteral("content"),
                                           QStringLiteral("Reply with the single word OK.")}}};
    QString problem;
    perform(config, messages, 8, false, &problem);
    if (!problem.isEmpty()) {
        if (error)
            *error = problem;
        return false;
    }
    if (error)
        error->clear();
    return true;
}
