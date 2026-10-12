#include "systemone.h"

#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSaveFile>
#include <QTimer>

namespace {

QString schemaError(const QString &message) {
    return QStringLiteral("tag schema: %1").arg(message);
}


}  // namespace

QList<TagQuestion> SystemOne::parseSchema(const QByteArray &json, QString *error,
                                         int *stateChars) {
    if (error)
        *error = QString();
    const auto fail = [error](const QString &message) {
        if (error)
            *error = schemaError(message);
        return QList<TagQuestion>();
    };

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(json, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject())
        return fail(QStringLiteral("invalid JSON: %1").arg(parseError.errorString()));

    const QJsonObject root = document.object();
    const QJsonObject questions = root.value(QStringLiteral("questions")).toObject();
    if (questions.isEmpty())
        return fail(QStringLiteral("no questions defined"));
    if (stateChars) {
        const int configured = root.value(QStringLiteral("stateChars")).toInt(4000);
        *stateChars = configured > 0 ? configured : 4000;
    }


    QList<TagQuestion> parsed;
    for (auto it = questions.constBegin(); it != questions.constEnd(); ++it) {
        TagQuestion entry;
        entry.id = it.key();
        const QJsonObject definition = it.value().toObject();
        if (!definition.isObject())
            return fail(QStringLiteral("\"%1\" is not a question object").arg(it.key()));

        entry.type = definition.value(QStringLiteral("type")).toString();
        entry.instructions = definition.value(QStringLiteral("instructions")).toString();
        entry.criteria = definition.value(QStringLiteral("criteria")).toObject();
        entry.threshold = definition.value(QStringLiteral("threshold")).toDouble(0.90);

        if (entry.type == QStringLiteral("choice")) {
            entry.choiceTags = definition.value(QStringLiteral("tags")).toObject();
            if (entry.criteria.isEmpty())
                return fail(QStringLiteral("\"%1\" has no criteria options").arg(it.key()));
            if (entry.choiceTags.isEmpty())
                return fail(QStringLiteral("\"%1\" maps no option to a tag").arg(it.key()));
            for (auto tag = entry.choiceTags.constBegin(); tag != entry.choiceTags.constEnd();
                 ++tag) {
                if (!entry.criteria.contains(tag.key()))
                    return fail(QStringLiteral("\"%1\" tags unknown option \"%2\"")
                                    .arg(it.key(), tag.key()));
                if (tag.value().toString().trimmed().isEmpty())
                    return fail(QStringLiteral("\"%1\" maps \"%2\" to an empty tag")
                                    .arg(it.key(), tag.key()));
            }
        } else if (entry.type == QStringLiteral("noul")) {
            entry.noulTag = definition.value(QStringLiteral("tag")).toString().trimmed();
            if (entry.noulTag.isEmpty())
                return fail(QStringLiteral("\"%1\" has no tag").arg(it.key()));
        } else {
            return fail(QStringLiteral("\"%1\" has unsupported type \"%2\"")
                            .arg(it.key(), entry.type));
        }

        if (entry.instructions.trimmed().isEmpty())
            return fail(QStringLiteral("\"%1\" has no instructions").arg(it.key()));
        if (entry.threshold <= 0.0 || entry.threshold > 1.0)
            return fail(QStringLiteral("\"%1\" has a threshold outside (0, 1]").arg(it.key()));

        parsed.append(entry);
    }
    return parsed;
}

QByteArray SystemOne::defaultSchema() {
    QFile resource(QStringLiteral(":/tag-schema.json"));
    return resource.open(QIODevice::ReadOnly) ? resource.readAll() : QByteArray();
}

QList<TagQuestion> SystemOne::loadSchema(const QString &path, QString *error,
                                         int *stateChars) {
    QFile file(path);
    if (file.open(QIODevice::ReadOnly)) {
        return parseSchema(file.readAll(), error, stateChars);
    } else if (!file.exists()) {
        // First run: write the default schema out so it can be edited.
        const QByteArray fallback = defaultSchema();
        if (!path.isEmpty()) {
            QDir().mkpath(QFileInfo(path).absolutePath());
            QSaveFile fresh(path);
            if (fresh.open(QIODevice::WriteOnly)) {
                fresh.write(fallback);
                fresh.commit();
            }
        }
        return parseSchema(fallback, error, stateChars);
    }
    if (error)
        *error = schemaError(QStringLiteral("could not read %1").arg(path));
    return {};
}

QByteArray SystemOne::buildRequestBody(const QString &state, const QList<TagQuestion> &questions,
                                       const QString &model) {
    QJsonObject questionMap;
    for (const TagQuestion &question : questions) {
        QJsonObject definition{{QStringLiteral("type"), question.type},
                                {QStringLiteral("instructions"), question.instructions}};
        if (!question.criteria.isEmpty())
            definition.insert(QStringLiteral("criteria"), question.criteria);
        questionMap.insert(question.id, definition);
    }
    const QJsonObject body{{QStringLiteral("model"), model},
                           {QStringLiteral("state"), state},
                           {QStringLiteral("questions"), questionMap}};
    return QJsonDocument(body).toJson(QJsonDocument::Compact);
}

QStringList SystemOne::tagsFromAnswers(const QJsonObject &answers,
                                       const QList<TagQuestion> &questions) {
    QStringList tags;
    for (const TagQuestion &question : questions) {
        const QJsonObject answer = answers.value(question.id).toObject();
        if (answer.isEmpty())
            continue;

        double confidence = -1.0;
        QString tag;
        if (question.type == QStringLiteral("choice")) {
            const QString chosen = answer.value(QStringLiteral("choice")).toString();
            if (chosen.isEmpty())
                continue;
            const QJsonValue probability =
                answer.value(QStringLiteral("probabilities")).toObject().value(chosen);
            if (probability.isDouble())
                confidence = probability.toDouble();
            else
                confidence = answer.value(QStringLiteral("answer_confidence"))
                                  .toDouble(answer.value(QStringLiteral("confidence")).toDouble(-1.0));
            tag = question.choiceTags.value(chosen).toString();
        } else {
            confidence = answer.value(QStringLiteral("noul")).toDouble(-1.0);
            tag = question.noulTag;
        }

        if (confidence >= question.threshold && !tag.trimmed().isEmpty()
                && !tags.contains(tag, Qt::CaseInsensitive))
            tags.append(tag);
    }
    return tags;
}

QStringList SystemOne::decideTags(const DecisionServer &server, const QString &state,
                                  const QList<TagQuestion> &questions, QString *error) {
    if (error)
        *error = QString();
    const auto fail = [error](const QString &message) {
        if (error)
            *error = message;
        return QStringList();
    };
    if (questions.isEmpty())
        return {};
    if (!server.endpoint.isValid())
        return fail(QStringLiteral("no decision server configured"));

    QNetworkAccessManager network;
    QNetworkRequest request(server.endpoint);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    if (!server.apiKey.isEmpty())
        request.setRawHeader(QByteArrayLiteral("Authorization"),
                             QStringLiteral("Bearer %1").arg(server.apiKey).toUtf8());

    QTimer timeout;
    timeout.setSingleShot(true);
    QNetworkReply *reply = network.post(request, buildRequestBody(state, questions, server.model));
    QObject::connect(&timeout, &QTimer::timeout, reply, &QNetworkReply::abort);
    QObject::connect(reply, &QNetworkReply::finished, &timeout, &QTimer::stop);

    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    if (!reply->isFinished()) {
        timeout.start(server.timeoutMs);
        loop.exec();
    }

    if (reply->error() != QNetworkReply::NoError) {
        return fail(reply->error() == QNetworkReply::OperationCanceledError
                        ? QStringLiteral("decision server timed out")
                        : reply->errorString());
    }
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (status != 200)
        return fail(QStringLiteral("decision server answered HTTP %1").arg(status));

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(reply->readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject())
        return fail(QStringLiteral("decision server sent invalid JSON"));
    return tagsFromAnswers(document.object().value(QStringLiteral("answers")).toObject(),
                           questions);
}