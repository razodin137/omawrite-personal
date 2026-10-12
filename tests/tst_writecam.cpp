#include <QtTest>
#include <QFont>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickStyle>
#include <QJsonDocument>
#include <QTcpServer>
#include <QTcpSocket>
#include <functional>

#include "backend.h"
#include "markdownhighlighter.h"
#include "systemone.h"

// A stand-in /v1/systemone server: records each request and answers with a
// canned response, so the wire format is exercised without a real model.
class FakeDecisionServer : public QObject {
    Q_OBJECT

public:
    using Responder = std::function<QJsonObject(const QJsonObject &request)>;

    explicit FakeDecisionServer(Responder responder, int status = 200, QObject *parent = nullptr)
        : QObject(parent), m_responder(std::move(responder)), m_status(status) {
        connect(&m_server, &QTcpServer::newConnection, this, [this]() {
            QTcpSocket *socket = m_server.nextPendingConnection();
            m_buffers.insert(socket, QByteArray());
            connect(socket, &QTcpSocket::disconnected, socket, &QTcpSocket::deleteLater);
            connect(socket, &QTcpSocket::disconnected, this, [this, socket]() {
                m_buffers.remove(socket);
            });
            connect(socket, &QTcpSocket::readyRead, this, [this, socket]() { serve(socket); });
        });
        m_server.listen(QHostAddress::LocalHost);
    }

    quint16 port() const { return m_server.serverPort(); }
    int requestCount() const { return m_requestCount; }
    const QByteArray &lastRequestLine() const { return m_lastRequestLine; }
    const QByteArray &lastHeaders() const { return m_lastHeaders; }
    QByteArray lastBody() const { return m_lastBody; }

private:
    void serve(QTcpSocket *socket) {
        m_buffers[socket] += socket->readAll();
        const QByteArray raw = m_buffers.value(socket);
        const int headerEnd = raw.indexOf("\r\n\r\n");
        if (headerEnd < 0)
            return;
        const QByteArray headers = raw.left(headerEnd);
        int contentLength = 0;
        for (const QByteArray &line : headers.split('\n')) {
            if (line.startsWith("Content-Length:", Qt::CaseInsensitive))
                contentLength = line.mid(15).trimmed().toInt();
        }
        if (raw.size() < headerEnd + 4 + contentLength)
            return;

        ++m_requestCount;
        m_lastRequestLine = headers.left(headers.indexOf('\r'));
        m_lastHeaders = headers;
        m_lastBody = raw.mid(headerEnd + 4, contentLength);
        m_buffers.remove(socket);

        if (!m_responder)
            return;  // a server that never answers, for timeout tests
        const QJsonObject response = m_responder(QJsonDocument::fromJson(m_lastBody).object());
        const QByteArray body = QJsonDocument(response).toJson(QJsonDocument::Compact);
        socket->write("HTTP/1.1 " + QByteArray::number(m_status) + " OK\r\n"
                      "Content-Type: application/json\r\n"
                      "Content-Length: " + QByteArray::number(body.size()) + "\r\n"
                      "Connection: close\r\n\r\n" + body);
        socket->disconnectFromHost();
    }

    QTcpServer m_server;
    Responder m_responder;
    int m_status = 200;
    int m_requestCount = 0;
    QByteArray m_lastRequestLine;
    QByteArray m_lastHeaders;
    QByteArray m_lastBody;
    QHash<QTcpSocket *, QByteArray> m_buffers;
};

class WriteCamTest : public QObject {
    Q_OBJECT

private slots:
    void initTestCase() {
        QVERIFY(m_settingsDirectory.isValid());
        QQuickStyle::setStyle(QStringLiteral("Material"));
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope,
                           m_settingsDirectory.path());
        // Tagging is opt-in per test: the default suite never touches a
        // decision server, not even a locally running one.
        QSettings().setValue(QStringLiteral("tagging/enabled"), false);
    }

    void countsWords() {
        QCOMPARE(Backend::countWords(QStringLiteral("one two-three don't 42")), 4);
        QCOMPARE(Backend::countWords(QStringLiteral("你好 世界")), 2);
        QCOMPARE(Backend::countWords(QString()), 0);
    }

    void normalizesLinks() {
        QCOMPARE(Backend::normalizedLinkUrl(QStringLiteral("www.example.com/path")),
                 QStringLiteral("https://www.example.com/path"));
        QCOMPARE(Backend::normalizedLinkUrl(QStringLiteral("mailto:writer@example.com")),
                 QStringLiteral("mailto:writer@example.com"));
        QVERIFY(Backend::normalizedLinkUrl(QStringLiteral("example.com")).isEmpty());
        QVERIFY(Backend::normalizedLinkUrl(QStringLiteral("file:///tmp/private")).isEmpty());
    }

    void suggestsSafeNames() {
        const QDate today(2026, 10, 12);
        QCOMPARE(Backend::suggestedFileName(QStringLiteral("My first draft\nBody"), today),
                 QStringLiteral("2026-10-12-my-first-draft.md"));
        QCOMPARE(Backend::suggestedFileName(QStringLiteral("A/B"), today),
                 QStringLiteral("2026-10-12-a-b.md"));
        QCOMPARE(Backend::suggestedFileName(QString(), today),
                 QStringLiteral("2026-10-12-untitled.md"));
        QCOMPARE(Backend::suggestedFileName(QStringLiteral("Already.md"), today),
                 QStringLiteral("2026-10-12-already.md"));
        QCOMPARE(Backend::suggestedFileName(QStringLiteral("# Real Title\nBody"), today),
                 QStringLiteral("2026-10-12-real-title.md"));
        QCOMPARE(Backend::suggestedFileName(
                     QStringLiteral("Punctuation, everywhere! It's fine."), today),
                 QStringLiteral("2026-10-12-punctuation-everywhere-it-s-fine.md"));
        // A title that already starts with a date is not prefixed again.
        QCOMPARE(Backend::suggestedFileName(
                     QStringLiteral("2026-10-11 Written yesterday\nBody"), today),
                 QStringLiteral("2026-10-11-written-yesterday.md"));
        QCOMPARE(Backend::suggestedFileName(
                     QStringLiteral("---\ntitle: Metadata\n---\n\n# Real Title\nBody"), today),
                 QStringLiteral("2026-10-12-real-title.md"));
    }

    void addsFrontMatterToNewDocuments() {
        const QDateTime now(QDate(2026, 10, 12), QTime(14, 23));
        QCOMPARE(Backend::applyFrontMatter(
                     QStringLiteral("# Hello World\n\nBody"), true, true,
                     QStringLiteral("fallback"), now),
                 QStringLiteral("---\ndate: 2026-10-12\nupdated: []\n"
                                "title: Hello World\ndraft: true\ncategories: []\n"
                                "tags: []\n---\n\n# Hello World\n\nBody"));
        QCOMPARE(Backend::applyFrontMatter(QString(), true, false,
                                           QStringLiteral("fallback"), now),
                 QStringLiteral("---\ndate: 2026-10-12\nupdated: []\n"
                                "title: fallback\ndraft: true\ncategories: []\n"
                                "tags: []\n---\n"));
        QCOMPARE(Backend::applyFrontMatter(
                     QStringLiteral("\n\nPlain first line"), true, true,
                     QStringLiteral("fallback"), now),
                 QStringLiteral("---\ndate: 2026-10-12\nupdated: []\n"
                                "title: Plain first line\ndraft: true\ncategories: []\n"
                                "tags: []\n---\n\nPlain first line"));
    }

    void leavesDocumentsWithoutFrontMatterAlone() {
        const QDateTime now(QDate(2026, 10, 12), QTime(14, 23));
        const QString plain = QStringLiteral("# Just a post\n\nBody\n");
        QCOMPARE(Backend::applyFrontMatter(plain, false, true, QStringLiteral("fallback"), now),
                 plain);

        // Front matter without an `updated` field has the log started on a
        // modified save, right after the `date` line.
        const QString withoutUpdated = QStringLiteral(
            "---\nlayout: post\ntitle: Someone else's metadata\ndate: 2020-03-04\n"
            "---\n\n# Post");
        QCOMPARE(Backend::applyFrontMatter(withoutUpdated, false, true,
                                           QStringLiteral("fallback"), now),
                 QStringLiteral("---\nlayout: post\ntitle: Someone else's metadata\n"
                                "date: 2020-03-04\nupdated:\n  - 2026-10-12 14:23\n"
                                "---\n\n# Post"));

        // Without a `date` either, the log opens the front matter.
        QCOMPARE(Backend::applyFrontMatter(
                     QStringLiteral("---\ntitle: Undated\n---\n\n# Post"),
                     false, true, QStringLiteral("fallback"), now),
                 QStringLiteral("---\nupdated:\n  - 2026-10-12 14:23\ntitle: Undated\n"
                                "---\n\n# Post"));

        // An opening delimiter that is never closed is not front matter.
        const QString unclosed = QStringLiteral("---\ndate: 2020-01-01\n\n# Post");
        QCOMPARE(Backend::applyFrontMatter(unclosed, false, true, QStringLiteral("fallback"), now),
                 unclosed);

        // Saving an unmodified document never touches the update log.
        const QString settled = QStringLiteral("---\nupdated:\n  - 2026-10-12 09:00\n---\n");
        QCOMPARE(Backend::applyFrontMatter(settled, false, false, QStringLiteral("f"), now),
                 settled);
    }

    void appendsEverySaveToTheUpdatedLog() {
        const QDateTime now(QDate(2026, 10, 12), QTime(14, 23));
        const QString stamp = QStringLiteral("2026-10-12 14:23");

        // A scalar value migrates into the log as its first entry.
        const QString scalar = QStringLiteral(
            "---\ndate: 2020-01-01\nupdated: 2020-01-02\ntitle: Kept\n"
            "tags: [one, two]\n---\n\n# Post body");
        QCOMPARE(Backend::applyFrontMatter(scalar, false, true, QStringLiteral("f"), now),
                 QStringLiteral("---\ndate: 2020-01-01\nupdated:\n  - 2020-01-02\n"
                                "  - 2026-10-12 14:23\ntitle: Kept\ntags: [one, two]\n"
                                "---\n\n# Post body"));

        // An existing log grows at the bottom, keeping the writer's entries.
        const QString listed = QStringLiteral(
            "---\nupdated:\n  - 2026-10-12 09:00\n  - 2026-10-12 11:30\n---\n\nbody");
        QCOMPARE(Backend::applyFrontMatter(listed, false, true, QStringLiteral("f"), now),
                 QStringLiteral("---\nupdated:\n  - 2026-10-12 09:00\n  - 2026-10-12 11:30\n"
                                "  - 2026-10-12 14:23\n---\n\nbody"));

        // Empty and flow-empty fields start the log.
        QCOMPARE(Backend::applyFrontMatter(QStringLiteral("---\nupdated:\n---\n"),
                                           false, true, QStringLiteral("f"), now),
                 QStringLiteral("---\nupdated:\n  - 2026-10-12 14:23\n---\n"));
        QCOMPARE(Backend::applyFrontMatter(QStringLiteral("---\nupdated: []\n---\n"),
                                           false, true, QStringLiteral("f"), now),
                 QStringLiteral("---\nupdated:\n  - 2026-10-12 14:23\n---\n"));

        // Saving twice within the same minute logs a single entry.
        const QString fresh = QStringLiteral("---\nupdated:\n  - %1\n---\n\nbody").arg(stamp);
        QCOMPARE(Backend::applyFrontMatter(fresh, false, true, QStringLiteral("f"), now),
                 fresh);

        // Indentation is preserved, including nested fields.
        const QString nested = QStringLiteral("---\nmeta:\n  updated: 2020-05-05\n---\n");
        QCOMPARE(Backend::applyFrontMatter(nested, false, true, QStringLiteral("f"), now),
                 QStringLiteral("---\nmeta:\n  updated:\n    - 2020-05-05\n"
                                 "    - 2026-10-12 14:23\n---\n"));
    }

    void derivesTitlesFromTheFirstContent() {
        QCOMPARE(Backend::derivedTitle(QStringLiteral("# My Post\n\nBody"),
                                        QStringLiteral("fallback")),
                 QStringLiteral("My Post"));
        QCOMPARE(Backend::derivedTitle(QStringLiteral("### Deeper Heading"),
                                        QStringLiteral("fallback")),
                 QStringLiteral("Deeper Heading"));
        QCOMPARE(Backend::derivedTitle(QStringLiteral("\n\nFirst body line"),
                                        QStringLiteral("fallback")),
                 QStringLiteral("First body line"));
        QCOMPARE(Backend::derivedTitle(QStringLiteral("---\n---\n\n# After delimiters"),
                                        QStringLiteral("fallback")),
                 QStringLiteral("After delimiters"));
        QCOMPARE(Backend::derivedTitle(QString(), QStringLiteral("fallback")),
                 QStringLiteral("fallback"));
        QCOMPARE(Backend::derivedTitle(QString(), QString()), QStringLiteral("Untitled"));
    }

    void quotesTrickyYamlScalars() {
        QCOMPARE(Backend::yamlScalar(QStringLiteral("Plain title")),
                 QStringLiteral("Plain title"));
        QCOMPARE(Backend::yamlScalar(QStringLiteral("A: Colon Story")),
                 QStringLiteral("\"A: Colon Story\""));
        QCOMPARE(Backend::yamlScalar(QStringLiteral("true")), QStringLiteral("\"true\""));
        QCOMPARE(Backend::yamlScalar(QStringLiteral("42")), QStringLiteral("\"42\""));
        QCOMPARE(Backend::yamlScalar(QStringLiteral("2026-10-12")),
                 QStringLiteral("\"2026-10-12\""));
        QCOMPARE(Backend::yamlScalar(QStringLiteral("Say \"hi\"")),
                 QStringLiteral("\"Say \\\"hi\\\"\""));
    }

    void savesFrontMatterThroughTheEditor() {
        const QString mainQmlPath = QFINDTESTDATA("../src/Main.qml");
        QVERIFY(!mainQmlPath.isEmpty());

        QTemporaryDir directory;
        QVERIFY(directory.isValid());

        Backend backend;
        QQmlEngine engine;
        engine.rootContext()->setContextProperty(QStringLiteral("backend"), &backend);
        QQmlComponent component(&engine, QUrl::fromLocalFile(mainQmlPath));
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        QScopedPointer<QObject> window(component.create());
        QVERIFY2(window, qPrintable(component.errorString()));

        QObject *editor = window->findChild<QObject *>(QStringLiteral("sourceEditor"));
        QVERIFY(editor);
        QVERIFY(editor->setProperty("text", QStringLiteral("# My New Post\n\nSome body")));

        const QString firstPath = directory.filePath(QStringLiteral("post.md"));
        backend.saveAs(QUrl::fromLocalFile(firstPath));

        QFile firstFile(firstPath);
        QVERIFY(firstFile.open(QIODevice::ReadOnly | QIODevice::Text));
        const QString firstContents = QString::fromUtf8(firstFile.readAll());
        const QString expectedFrontMatter = QStringLiteral(
            "---\ndate: %1\nupdated: []\ntitle: My New Post\ndraft: true\n"
            "categories: []\ntags: []\n---\n\n").arg(QDate::currentDate().toString(Qt::ISODate));
        QVERIFY2(firstContents.startsWith(expectedFrontMatter),
                 qPrintable(firstContents));
        QVERIFY(firstContents.endsWith(QStringLiteral("# My New Post\n\nSome body")));
        // The editor buffer carries the front matter too, matching the file.
        QVERIFY(editor->property("text").toString().startsWith(QStringLiteral("---\n")));

        // Editing the post and saving again appends to the `updated` log.
        QString editedContents = firstContents;
        editedContents.replace(QStringLiteral("Some body"), QStringLiteral("More body"));
        QVERIFY(editor->setProperty("text", editedContents));

        const QString secondPath = directory.filePath(QStringLiteral("post-edited.md"));
        backend.saveAs(QUrl::fromLocalFile(secondPath));

        QFile secondFile(secondPath);
        QVERIFY(secondFile.open(QIODevice::ReadOnly | QIODevice::Text));
        const QString secondContents = QString::fromUtf8(secondFile.readAll());
        const QRegularExpression loggedEntry(
            QStringLiteral("updated:\n  - \\d{4}-\\d{2}-\\d{2} \\d{2}:\\d{2}\n"));
        QVERIFY2(loggedEntry.match(secondContents).hasMatch(), qPrintable(secondContents));
        QVERIFY(secondContents.contains(
            QStringLiteral("date: ") + QDate::currentDate().toString(Qt::ISODate)));
        QVERIFY(secondContents.contains(QStringLiteral("More body")));

        // Saving without further edits does not grow the log.
        const QString thirdPath = directory.filePath(QStringLiteral("post-again.md"));
        backend.saveAs(QUrl::fromLocalFile(thirdPath));
        QFile thirdFile(thirdPath);
        QVERIFY(thirdFile.open(QIODevice::ReadOnly | QIODevice::Text));
        QCOMPARE(QString::fromUtf8(thirdFile.readAll()), secondContents);
    }

    void keepsNotoFromCrowdingTheFontList() {
        const QStringList fonts = Backend::selectableFontFamilies({
            QStringLiteral("Noto Sans Devanagari"), QStringLiteral("Liberation Serif"),
            QStringLiteral("Noto Serif"), QStringLiteral("Noto Sans Tamil UI"),
            QStringLiteral("adwaita Sans"), QStringLiteral("IBM Plex Mono"),
            QStringLiteral("Noto Sans"), QStringLiteral("Noto Sans Mono"),
            QStringLiteral("Liberation Serif"), QStringLiteral("Monospace"),
            QStringLiteral("Standard Symbols PS"), QStringLiteral("D050000L"),
            QStringLiteral("Noto Color Emoji"), QStringLiteral("Nimbus Sans [UKWN]"),
            QStringLiteral("Nimbus Sans [URW ]")});
        QCOMPARE(fonts, QStringList({QStringLiteral("IBM Plex Mono"),
                                     QStringLiteral("adwaita Sans"),
                                     QStringLiteral("Liberation Serif"),
                                     QStringLiteral("Nimbus Sans"),
                                     QStringLiteral("Noto Sans"),
                                     QStringLiteral("Noto Sans Mono"),
                                     QStringLiteral("Noto Serif")}));
    }

    void remembersEditorFont() {
        const QString installed = Backend().availableFonts().value(1);
        QVERIFY(!installed.isEmpty());

        {
            Backend backend;
            QCOMPARE(backend.editorFont(), Backend::defaultEditorFont());
            QSignalSpy fontSpy(&backend, &Backend::editorFontChanged);
            backend.setEditorFont(installed);
            QCOMPARE(fontSpy.count(), 1);
        }

        QCOMPARE(Backend().editorFont(), installed);

        // A remembered font that has since been uninstalled falls back.
        QSettings().setValue(QStringLiteral("editor/font"), QStringLiteral("No Such Font"));
        QCOMPARE(Backend().editorFont(), Backend::defaultEditorFont());
        QSettings().remove(QStringLiteral("editor/font"));
    }

    void findsInlineMarkdownRanges() {
        const auto markup = MarkdownHighlighter::inlineMarkup(
            QStringLiteral("**bold** and *italic* and [site](https://example.com)"));
        QCOMPARE(markup.size(), 3);
        QCOMPARE(markup.at(0).content.start, 2);
        QCOMPARE(markup.at(0).content.length, 4);
        QCOMPARE(markup.at(2).content.length, 4);
        QCOMPARE(markup.at(2).markers[0].length, 1);
    }

    void loadsCurrentOmarchyTheme() {
        QTemporaryDir homeDirectory;
        QVERIFY(homeDirectory.isValid());

        const QByteArray originalHome = qgetenv("HOME");
        struct HomeRestorer {
            QByteArray value;
            ~HomeRestorer() { qputenv("HOME", value); }
        } restoreHome{originalHome};
        QVERIFY(qputenv("HOME", homeDirectory.path().toUtf8()));

        const QString themeDirectory = homeDirectory.path()
            + QStringLiteral("/.local/state/omarchy/current/theme");
        QVERIFY(QDir().mkpath(themeDirectory));

        QFile colorsFile(themeDirectory + QStringLiteral("/colors.toml"));
        QVERIFY(colorsFile.open(QIODevice::WriteOnly | QIODevice::Text));
        const QByteArray palette(
            "mode = \"light\"\n"
            "accent = \"#112233\"\n"
            "selection = \"#445566\"\n"
            "background = \"#fefefe\"\n"
            "foreground = \"#101010\"\n");
        QCOMPARE(colorsFile.write(palette), qint64(palette.size()));
        colorsFile.close();

        Backend backend;
        QCOMPARE(backend.themeBackground(), QStringLiteral("#fefefe"));
        QCOMPARE(backend.themeForeground(), QStringLiteral("#101010"));
        QCOMPARE(backend.themeAccent(), QStringLiteral("#112233"));
        QCOMPARE(backend.themeSelection(), QStringLiteral("#445566"));
        QVERIFY(!backend.darkMode());
    }

    void ignoresFileWatcherEventsForSavedContents() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());

        const QString path = directory.filePath(QStringLiteral("first-save.md"));
        Backend backend;
        QSignalSpy externalChangeSpy(&backend, &Backend::externalChangeDetected);

        backend.saveAs(QUrl::fromLocalFile(path));
        QVERIFY(QFileInfo::exists(path));

        QFile sameContents(path);
        QVERIFY(sameContents.open(QIODevice::WriteOnly | QIODevice::Truncate));
        sameContents.close();
        QTest::qWait(100);
        QCOMPARE(externalChangeSpy.count(), 0);

        QFile changedContents(path);
        QVERIFY(changedContents.open(QIODevice::WriteOnly | QIODevice::Truncate));
        QCOMPARE(changedContents.write("changed elsewhere"), qint64(17));
        changedContents.close();
        QTRY_COMPARE(externalChangeSpy.count(), 1);
    }

    void keepsCursorAndSelectionStableAcrossInsertions() {
        const QString mutationsPath = QFINDTESTDATA("../src/EditorMutations.js");
        QVERIFY(!mutationsPath.isEmpty());

        QQmlEngine engine;
        QQmlComponent component(&engine);
        const QByteArray harness = R"QML(
            import QtQuick
            import "EditorMutations.js" as EditorMutations

            TextEdit {
                property string insertionText
                property int insertionCursor
                property string wrappedText
                property int wrappedSelectionStart
                property int wrappedSelectionEnd

                Component.onCompleted: {
                    text = "alpha omega";
                    cursorPosition = 5;
                    EditorMutations.replaceRange(this, 5, 5, "one\r\ntwo");
                    insertionText = text;
                    insertionCursor = cursorPosition;

                    text = "alpha beta omega";
                    select(6, 10);
                    EditorMutations.replaceRange(this, selectionStart, selectionEnd,
                                                 "**beta**", 2, 6);
                    wrappedText = text;
                    wrappedSelectionStart = selectionStart;
                    wrappedSelectionEnd = selectionEnd;
                }
            }
        )QML";
        const QUrl harnessUrl = QUrl::fromLocalFile(
            QFileInfo(mutationsPath).absolutePath() + QStringLiteral("/MutationHarness.qml"));
        component.setData(harness, harnessUrl);
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        QScopedPointer<QObject> editor(component.create());
        QVERIFY2(editor, qPrintable(component.errorString()));

        QCOMPARE(editor->property("insertionText").toString(),
                 QStringLiteral("alphaone\ntwo omega"));
        QCOMPARE(editor->property("insertionCursor").toInt(), 12);
        QCOMPARE(editor->property("wrappedText").toString(),
                 QStringLiteral("alpha **beta** omega"));
        QCOMPARE(editor->property("wrappedSelectionStart").toInt(), 8);
        QCOMPARE(editor->property("wrappedSelectionEnd").toInt(), 12);
    }

    void savesAndOpensFromFooterButtons() {
        const QString mainQmlPath = QFINDTESTDATA("../src/Main.qml");
        QVERIFY(!mainQmlPath.isEmpty());

        Backend backend;
        QQmlEngine engine;
        engine.rootContext()->setContextProperty(QStringLiteral("backend"), &backend);
        QQmlComponent component(&engine, QUrl::fromLocalFile(mainQmlPath));
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        QScopedPointer<QObject> window(component.create());
        QVERIFY2(window, qPrintable(component.errorString()));

        QVERIFY(window->findChild<QObject *>(QStringLiteral("sourceEditor")));
        QVERIFY(!window->findChild<QObject *>(QStringLiteral("renderedPreview")));
        QVERIFY(!window->findChild<QObject *>(QStringLiteral("modeToggle")));

        QObject *saveButton = window->findChild<QObject *>(QStringLiteral("saveButton"));
        QObject *openButton = window->findChild<QObject *>(QStringLiteral("openButton"));
        QVERIFY(saveButton);
        QVERIFY(openButton);

        QSignalSpy saveDialogSpy(&backend, &Backend::saveDialogRequested);
        QVERIFY(QMetaObject::invokeMethod(saveButton, "clicked"));
        QCOMPARE(saveDialogSpy.count(), 1);

        QSignalSpy openDialogSpy(&backend, &Backend::openDialogRequested);
        QVERIFY(QMetaObject::invokeMethod(openButton, "clicked"));
        QCOMPARE(openDialogSpy.count(), 1);
    }

    void scalesTextWithDesktopTextSize() {
        const QString mainQmlPath = QFINDTESTDATA("../src/Main.qml");
        QVERIFY(!mainQmlPath.isEmpty());

        Backend backend;
        QQmlEngine engine;
        engine.rootContext()->setContextProperty(QStringLiteral("backend"), &backend);
        QQmlComponent component(&engine, QUrl::fromLocalFile(mainQmlPath));
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        QScopedPointer<QObject> window(component.create());
        QVERIFY2(window, qPrintable(component.errorString()));

        QObject *editor = window->findChild<QObject *>(QStringLiteral("sourceEditor"));
        QVERIFY(editor);
        QCOMPARE(editor->property("font").value<QFont>().pixelSize(), 20);

        // `omarchy display text size 16` sets the GNOME factor to 16/12.
        backend.setTextScale(16.0 / 12.0);
        QCOMPARE(window->property("editorFontPixelSize").toInt(), 27);
        QCOMPARE(editor->property("font").value<QFont>().pixelSize(), 27);

        backend.setTextScale(9.0 / 12.0);
        QCOMPARE(window->property("editorFontPixelSize").toInt(), 15);
        QCOMPARE(editor->property("font").value<QFont>().pixelSize(), 15);
    }

    void startsTheLogOnOpenedFilesWithoutOne() {
        const QString mainQmlPath = QFINDTESTDATA("../src/Main.qml");
        QVERIFY(!mainQmlPath.isEmpty());

        QTemporaryDir directory;
        QVERIFY(directory.isValid());

        Backend backend;
        QQmlEngine engine;
        engine.rootContext()->setContextProperty(QStringLiteral("backend"), &backend);
        QQmlComponent component(&engine, QUrl::fromLocalFile(mainQmlPath));
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        QScopedPointer<QObject> window(component.create());
        QVERIFY2(window, qPrintable(component.errorString()));
        QObject *editor = window->findChild<QObject *>(QStringLiteral("sourceEditor"));
        QVERIFY(editor);

        // A pre-existing post whose front matter never tracked `updated`.
        const QString existing = QStringLiteral(
            "---\nlayout: post\ntitle: Old writing\ndate: 2020-03-04\ncategories: misc\n"
            "---\n\nSome old body.\n");
        const QString path = directory.filePath(QStringLiteral("old-post.md"));
        QFile existingFile(path);
        QVERIFY(existingFile.open(QIODevice::WriteOnly | QIODevice::Text));
        QCOMPARE(existingFile.write(existing.toUtf8()), qint64(existing.toUtf8().size()));
        existingFile.close();

        backend.open(QUrl::fromLocalFile(path));
        QVERIFY(editor->setProperty("text",
                 QStringLiteral("---\nlayout: post\ntitle: Old writing\ndate: 2020-03-04\n"
                                 "categories: misc\n---\n\nSome old body, edited.\n")));
        backend.save();

        QFile savedFile(path);
        QVERIFY(savedFile.open(QIODevice::ReadOnly | QIODevice::Text));
        const QString saved = QString::fromUtf8(savedFile.readAll());
        QVERIFY2(QRegularExpression(
                     QStringLiteral("date: 2020-03-04\nupdated:\n  - \\d{4}-\\d{2}-\\d{2} "
                                    "\\d{2}:\\d{2}\ncategories: misc\n"))
                     .match(saved)
                     .hasMatch(),
                 qPrintable(saved));

        // Front matter that already tracks `updated` keeps growing on save.
        const QString tracked = QStringLiteral(
            "---\ndate: 2020-03-04\nupdated:\n  - 2020-05-05 08:00\ntitle: Tracked\n"
            "---\n\nBody.\n");
        const QString trackedPath = directory.filePath(QStringLiteral("tracked-post.md"));
        QFile trackedFile(trackedPath);
        QVERIFY(trackedFile.open(QIODevice::WriteOnly | QIODevice::Text));
        trackedFile.write(tracked.toUtf8());
        trackedFile.close();

        backend.open(QUrl::fromLocalFile(trackedPath));
        QString editedBody = tracked;
        editedBody.replace(QStringLiteral("Body."), QStringLiteral("Body, edited."));
        QVERIFY(editor->setProperty("text", editedBody));
        backend.save();

        QFile resavedFile(trackedPath);
        QVERIFY(resavedFile.open(QIODevice::ReadOnly | QIODevice::Text));
        const QString resaved = QString::fromUtf8(resavedFile.readAll());
        QVERIFY2(QRegularExpression(
                     QStringLiteral("updated:\n  - 2020-05-05 08:00\n  - \\d{4}-\\d{2}-\\d{2} "
                                    "\\d{2}:\\d{2}\n"))
                     .match(resaved)
                     .hasMatch(),
                 qPrintable(resaved));
    }

    void remembersLastSaveDirectory() {
        QTemporaryDir saveDirectory;
        QVERIFY(saveDirectory.isValid());

        const QString savedPath = saveDirectory.filePath(QStringLiteral("first.md"));
        Backend savedDocument;
        savedDocument.saveAs(QUrl::fromLocalFile(savedPath));

        Backend nextDocument;
        QSignalSpy saveDialogSpy(&nextDocument, &Backend::saveDialogRequested);
        nextDocument.saveAsDialog();
        QCOMPARE(saveDialogSpy.count(), 1);

        const QUrl suggestedUrl = saveDialogSpy.takeFirst().constFirst().toUrl();
        QCOMPARE(QFileInfo(suggestedUrl.toLocalFile()).absolutePath(),
                 saveDirectory.path());
        QCOMPARE(QFileInfo(suggestedUrl.toLocalFile()).fileName(),
                 QDate::currentDate().toString(Qt::ISODate) + QStringLiteral("-untitled.md"));

        QSettings().setValue(QStringLiteral("file/lastSaveDirectory"),
                             saveDirectory.filePath(QStringLiteral("missing")));
        Backend fallbackDocument;
        QSignalSpy fallbackDialogSpy(&fallbackDocument, &Backend::saveDialogRequested);
        fallbackDocument.saveAsDialog();
        const QUrl fallbackUrl = fallbackDialogSpy.takeFirst().constFirst().toUrl();
        QCOMPARE(QFileInfo(fallbackUrl.toLocalFile()).absolutePath(), QDir::homePath());
    }

    void parsesTagSchemas() {
        QString error;
        int stateChars = 0;

        // The shipped default schema parses into the full question set.
        QFile defaultSchema(QFINDTESTDATA("../src/tag-schema.json"));
        QVERIFY(defaultSchema.open(QIODevice::ReadOnly));
        const QList<TagQuestion> questions =
            SystemOne::parseSchema(defaultSchema.readAll(), &error, &stateChars);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QCOMPARE(stateChars, 4000);
        QCOMPARE(questions.size(), 4);
        QSet<QString> ids;
        for (const TagQuestion &question : questions)
            ids.insert(question.id);
        QCOMPARE(ids, QSet<QString>({QStringLiteral("topic"), QStringLiteral("journal"),
                                     QStringLiteral("planning"), QStringLiteral("fiction")}));
        for (const TagQuestion &question : questions) {
            QCOMPARE(question.threshold, 0.90);
            QVERIFY(!question.instructions.isEmpty());
            if (question.id == QStringLiteral("topic")) {
                QCOMPARE(question.type, QStringLiteral("choice"));
                QCOMPARE(question.criteria.size(), 9);
                QCOMPARE(question.choiceTags.size(), 8);  // "other" stays silent
            } else {
                QCOMPARE(question.type, QStringLiteral("noul"));
                QCOMPARE(question.noulTag, question.id);
            }
        }

        // Malformed schemas are rejected loudly, naming the problem.
        QVERIFY(!SystemOne::parseSchema("not json", &error).isEmpty());
        QVERIFY(!SystemOne::parseSchema("{\"questions\":{}}", &error).isEmpty());
        QVERIFY(!SystemOne::parseSchema(
            "{\"questions\":{\"q\":{\"type\":\"choice\",\"instructions\":\"i\","
            "\"criteria\":{\"a\":\"ay\"},\"tags\":{\"b\":\"bee\"}}}}", &error).isEmpty());
        QVERIFY(!SystemOne::parseSchema(
            "{\"questions\":{\"q\":{\"type\":\"choice\",\"instructions\":\"i\","
            "\"criteria\":{\"a\":\"ay\"}}}}", &error).isEmpty());
        QVERIFY(!SystemOne::parseSchema(
            "{\"questions\":{\"q\":{\"type\":\"noul\",\"instructions\":\"i\"}}}}", &error)
            .isEmpty());
        QVERIFY(!SystemOne::parseSchema(
            "{\"questions\":{\"q\":{\"type\":\"score\",\"instructions\":\"i\",\"tag\":\"t\"}}}}",
            &error).isEmpty());

        // Entries carry their own thresholds and the state length is honored.
        const QList<TagQuestion> custom = SystemOne::parseSchema(QStringLiteral(
            "{\"stateChars\":250,\"questions\":{\"q\":{\"type\":\"choice\","
            "\"instructions\":\"pick\",\"criteria\":{\"a\":\"ay\",\"b\":\"bee\"},"
            "\"tags\":{\"a\":\"alpha\"},\"threshold\":0.8}}}").toUtf8(), &error, &stateChars);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QCOMPARE(stateChars, 250);
        QCOMPARE(custom.size(), 1);
        QCOMPARE(custom.first().threshold, 0.8);
    }

    void buildsDecisionRequests() {
        QString error;
        const QList<TagQuestion> questions = SystemOne::parseSchema(QStringLiteral(
            "{\"questions\":{"
            "\"topic\":{\"type\":\"choice\",\"instructions\":\"Which topic?\","
            "\"criteria\":{\"a\":\"ay\"},\"tags\":{\"a\":\"alpha\"}},"
            "\"sure\":{\"type\":\"noul\",\"instructions\":\"Is it sure?\",\"tag\":\"sure\"}}}")
                .toUtf8(), &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));

        const QJsonObject request = QJsonDocument::fromJson(SystemOne::buildRequestBody(
            QStringLiteral("state text"), questions, QStringLiteral("laya:latest"))).object();
        QCOMPARE(request.value(QStringLiteral("model")).toString(), QStringLiteral("laya:latest"));
        QCOMPARE(request.value(QStringLiteral("state")).toString(), QStringLiteral("state text"));
        const QJsonObject sent = request.value(QStringLiteral("questions")).toObject();
        const QJsonObject topic = sent.value(QStringLiteral("topic")).toObject();
        QCOMPARE(topic.value(QStringLiteral("type")).toString(), QStringLiteral("choice"));
        QCOMPARE(topic.value(QStringLiteral("instructions")).toString(),
                 QStringLiteral("Which topic?"));
        QCOMPARE(topic.value(QStringLiteral("criteria")).toObject().value(QStringLiteral("a"))
                      .toString(), QStringLiteral("ay"));
        QVERIFY(!topic.contains(QStringLiteral("tags")));  // emission rules stay client-side
        const QJsonObject sure = sent.value(QStringLiteral("sure")).toObject();
        QCOMPARE(sure.value(QStringLiteral("type")).toString(), QStringLiteral("noul"));
        QVERIFY(!sure.contains(QStringLiteral("criteria")));  // optional criteria omitted
    }

    void derivesTagsFromAnswers() {
        QString error;
        const QList<TagQuestion> questions = SystemOne::parseSchema(QStringLiteral(
            "{\"questions\":{"
            "\"topic\":{\"type\":\"choice\",\"instructions\":\"topic?\",\"criteria\":"
            "{\"a\":\"ay\",\"b\":\"bee\",\"other\":\"none\"},\"tags\":{\"a\":\"alpha\",\"b\":\"beta\"},"
            "\"threshold\":0.9},"
            "\"sure\":{\"type\":\"noul\",\"instructions\":\"sure?\",\"tag\":\"sure\",\"threshold\":0.9},"
            "\"strict\":{\"type\":\"noul\",\"instructions\":\"strict?\",\"tag\":\"strict\",\"threshold\":0.99},"
            "\"dupe\":{\"type\":\"choice\",\"instructions\":\"dupe?\",\"criteria\":{\"a\":\"ay\",\"c\":\"cee\"},"
            "\"tags\":{\"a\":\"alpha\"}}}}").toUtf8(), &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QCOMPARE(questions.size(), 4);

        // Only answers at or above each question's threshold become tags.
        QJsonObject answers = QJsonDocument::fromJson(QStringLiteral(
            "{\"topic\":{\"type\":\"choice\",\"choice\":\"a\","
            "\"probabilities\":{\"a\":0.93,\"b\":0.05,\"other\":0.02}},"
            "\"sure\":{\"type\":\"noul\",\"noul\":0.91},"
            "\"strict\":{\"type\":\"noul\",\"noul\":0.95},"
            "\"dupe\":{\"type\":\"choice\",\"choice\":\"a\","
            "\"probabilities\":{\"a\":0.99,\"c\":0.01}}}").toUtf8()).object();
        QCOMPARE(SystemOne::tagsFromAnswers(answers, questions),
                 QStringList({QStringLiteral("alpha"), QStringLiteral("sure")}));

        // Below the threshold: silence, even with a confident-looking argmax.
        answers = QJsonDocument::fromJson(QStringLiteral(
            "{\"topic\":{\"type\":\"choice\",\"choice\":\"b\","
            "\"probabilities\":{\"a\":0.05,\"b\":0.72,\"other\":0.23}},"
            "\"sure\":{\"type\":\"noul\",\"noul\":0.5},"
            "\"dupe\":{\"type\":\"choice\",\"choice\":\"c\","
            "\"probabilities\":{\"a\":0.01,\"c\":0.99}}}").toUtf8()).object();
        QVERIFY(SystemOne::tagsFromAnswers(answers, questions).isEmpty());

        // Missing per-option probabilities fall back to the answer's own
        // reported confidence, as Ollama-shaped answers do.
        answers = QJsonDocument::fromJson(QStringLiteral(
            "{\"topic\":{\"type\":\"choice\",\"choice\":\"a\",\"answer_confidence\":0.97},"
            "\"sure\":{\"type\":\"noul\",\"noul\":0.2},"
            "\"dupe\":{\"type\":\"choice\",\"choice\":\"a\",\"confidence\":0.99}}").toUtf8())
            .object();
        QCOMPARE(SystemOne::tagsFromAnswers(answers, questions),
                 QStringList({QStringLiteral("alpha")}));

        // An answer set that mentions nothing stays nothing.
        QVERIFY(SystemOne::tagsFromAnswers(QJsonObject(), questions).isEmpty());
    }

    void decidesTagsOverTheWire() {
        QString error;
        int stateChars = 0;
        QFile defaultSchema(QFINDTESTDATA("../src/tag-schema.json"));
        QVERIFY(defaultSchema.open(QIODevice::ReadOnly));
        const QList<TagQuestion> questions =
            SystemOne::parseSchema(defaultSchema.readAll(), &error, &stateChars);
        QVERIFY2(error.isEmpty(), qPrintable(error));

        FakeDecisionServer server([](const QJsonObject &) {
            return QJsonObject{{"answers", QJsonObject{
                {"topic", QJsonObject{{"type", "choice"}, {"choice", "food"},
                    {"probabilities", QJsonObject{{"food", 0.94}, {"tech", 0.02},
                     {"travel", 0.01}, {"work", 0.01}, {"personal", 0.005},
                     {"health", 0.005}, {"finance", 0.005}, {"learning", 0.005},
                     {"other", 0.01}}}}},
                {"journal", QJsonObject{{"type", "noul"}, {"noul", 0.97}}},
                {"planning", QJsonObject{{"type", "noul"}, {"noul", 0.05}}},
                {"fiction", QJsonObject{{"type", "noul"}, {"noul", 0.03}}}}}};
        });
        QVERIFY(server.port() > 0);

        DecisionServer config;
        config.endpoint = QUrl(QStringLiteral("http://127.0.0.1:%1/v1/systemone")
                                   .arg(server.port()));
        config.model = QStringLiteral("laya:latest");
        config.apiKey = QStringLiteral("secret");
        config.timeoutMs = 5000;

        const QStringList tags = SystemOne::decideTags(config, QStringLiteral("cooked pasta"),
                                                       questions, &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QCOMPARE(tags, QStringList({QStringLiteral("food"), QStringLiteral("journal")}));
        QCOMPARE(server.requestCount(), 1);
        QCOMPARE(server.lastRequestLine(), QByteArrayLiteral("POST /v1/systemone HTTP/1.1"));
        QVERIFY(server.lastHeaders().contains("Authorization: Bearer secret"));
        const QJsonObject request = QJsonDocument::fromJson(server.lastBody()).object();
        QCOMPARE(request.value(QStringLiteral("model")).toString(), QStringLiteral("laya:latest"));
        QCOMPARE(request.value(QStringLiteral("state")).toString(), QStringLiteral("cooked pasta"));
        QVERIFY(request.value(QStringLiteral("questions")).toObject().contains("topic"));

        // A server that takes too long trips the timeout instead of hanging.
        FakeDecisionServer silent(nullptr);
        QVERIFY(silent.port() > 0);
        DecisionServer hanging = config;
        hanging.endpoint = QUrl(QStringLiteral("http://127.0.0.1:%1/v1/systemone")
                                     .arg(silent.port()));
        hanging.timeoutMs = 150;
        QVERIFY(SystemOne::decideTags(hanging, QStringLiteral("cooked pasta"), questions, &error)
                    .isEmpty());
        QCOMPARE(error, QStringLiteral("decision server timed out"));

        // Nothing listening: an error, never tags.
        QTcpServer reserved;
        QVERIFY(reserved.listen(QHostAddress::LocalHost));
        const quint16 deadPort = reserved.serverPort();
        reserved.close();
        DecisionServer dead = config;
        dead.endpoint = QUrl(QStringLiteral("http://127.0.0.1:%1/v1/systemone").arg(deadPort));
        QVERIFY(SystemOne::decideTags(dead, QStringLiteral("cooked pasta"), questions, &error)
                    .isEmpty());
        QVERIFY(!error.isEmpty());

        // An HTTP failure is an error too, so no tag rides on a broken reply.
        FakeDecisionServer unavailable([](const QJsonObject &) { return QJsonObject(); },
                                        503);
        QVERIFY(unavailable.port() > 0);
        DecisionServer failing = config;
        failing.endpoint = QUrl(QStringLiteral("http://127.0.0.1:%1/v1/systemone")
                                    .arg(unavailable.port()));
        QVERIFY(SystemOne::decideTags(failing, QStringLiteral("cooked pasta"), questions, &error)
                    .isEmpty());
        QVERIFY(!error.isEmpty());
    }

    void mergesTagsIntoFrontMatter() {
        QStringList added;

        // An empty flow list becomes the decided tags.
        QCOMPARE(Backend::applyFrontMatterTags(
                     QStringLiteral("---\ndate: 2020-01-01\ntags: []\n---\n\nBody"),
                     {QStringLiteral("food")}, &added),
                 QStringLiteral("---\ndate: 2020-01-01\ntags: [food]\n---\n\nBody"));
        QCOMPARE(added, QStringList({QStringLiteral("food")}));

        // Existing tags are kept, matched case-insensitively, and appended to.
        QCOMPARE(Backend::applyFrontMatterTags(
                     QStringLiteral("---\ntags: [Food, \"two words\"]\n---\n\nBody"),
                     {QStringLiteral("food"), QStringLiteral("journal")}, &added),
                 QStringLiteral("---\ntags: [Food, two words, journal]\n---\n\nBody"));
        QCOMPARE(added, QStringList({QStringLiteral("journal")}));

        // Block-style lists keep their shape and indentation.
        QCOMPARE(Backend::applyFrontMatterTags(
                     QStringLiteral("---\ndate: 2020-03-04\ntags:\n  - one\n  - two\n"
                                    "title: T\n---\n\nBody"),
                     {QStringLiteral("three")}, &added),
                 QStringLiteral("---\ndate: 2020-03-04\ntags:\n  - one\n  - two\n  - three\n"
                                "title: T\n---\n\nBody"));

        // A lone scalar migrates into a flow list.
        QCOMPARE(Backend::applyFrontMatterTags(
                     QStringLiteral("---\ntags: one\n---\n\nBody"),
                     {QStringLiteral("two")}, &added),
                 QStringLiteral("---\ntags: [one, two]\n---\n\nBody"));

        // Front matter without a tags field gets one, before the delimiter.
        QCOMPARE(Backend::applyFrontMatterTags(
                     QStringLiteral("---\ndate: 2020-01-01\n---\n\nBody"),
                     {QStringLiteral("food")}, &added),
                 QStringLiteral("---\ndate: 2020-01-01\ntags: [food]\n---\n\nBody"));

        // Tricky scalars are quoted on the way in.
        QCOMPARE(Backend::applyFrontMatterTags(
                     QStringLiteral("---\ntags: []\n---\n"),
                     {QStringLiteral("true"), QStringLiteral("2026-10-12")}, &added),
                 QStringLiteral("---\ntags: [\"true\", \"2026-10-12\"]\n---\n"));

        // Nothing to add: the text is untouched.
        const QString settled = QStringLiteral("---\ntags: [food]\n---\n\nBody");
        QCOMPARE(Backend::applyFrontMatterTags(settled, {QStringLiteral("FOOD")}, &added),
                 settled);
        QVERIFY(added.isEmpty());

        // No front matter, no injection: plain files are never modified.
        const QString plain = QStringLiteral("# Just a post\n\nBody");
        QCOMPARE(Backend::applyFrontMatterTags(plain, {QStringLiteral("food")}, &added), plain);
        QVERIFY(added.isEmpty());
    }

    void tagsNewSavesThroughTheEditor() {
        const QString mainQmlPath = QFINDTESTDATA("../src/Main.qml");
        QVERIFY(!mainQmlPath.isEmpty());

        FakeDecisionServer server([](const QJsonObject &) {
            return QJsonObject{{"answers", QJsonObject{
                {"topic", QJsonObject{{"type", "choice"}, {"choice", "food"},
                    {"probabilities", QJsonObject{{"food", 0.95}, {"tech", 0.01},
                     {"travel", 0.01}, {"work", 0.01}, {"personal", 0.005},
                     {"health", 0.005}, {"finance", 0.005}, {"learning", 0.005},
                     {"other", 0.01}}}}},
                {"journal", QJsonObject{{"type", "noul"}, {"noul", 0.97}}},
                {"planning", QJsonObject{{"type", "noul"}, {"noul", 0.05}}},
                {"fiction", QJsonObject{{"type", "noul"}, {"noul", 0.03}}}}}};
        });
        QVERIFY(server.port() > 0);

        {
            QSettings settings;
            settings.setValue(QStringLiteral("tagging/enabled"), true);
            settings.setValue(QStringLiteral("tagging/endpoint"),
                              QStringLiteral("http://127.0.0.1:%1/v1/systemone")
                                  .arg(server.port()));
            settings.setValue(QStringLiteral("tagging/model"), QStringLiteral("fake-model"));
        }

        QTemporaryDir directory;
        QVERIFY(directory.isValid());

        Backend backend;
        QQmlEngine engine;
        engine.rootContext()->setContextProperty(QStringLiteral("backend"), &backend);
        QQmlComponent component(&engine, QUrl::fromLocalFile(mainQmlPath));
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        QScopedPointer<QObject> window(component.create());
        QVERIFY2(window, qPrintable(component.errorString()));
        QObject *editor = window->findChild<QObject *>(QStringLiteral("sourceEditor"));
        QVERIFY(editor);
        QVERIFY(editor->setProperty("text", QStringLiteral("# Tagged Post\n\nSome body")));

        // The first save asks once and writes the decisions into the file.
        const QString firstPath = directory.filePath(QStringLiteral("tagged.md"));
        backend.saveAs(QUrl::fromLocalFile(firstPath));
        QFile firstFile(firstPath);
        QVERIFY(firstFile.open(QIODevice::ReadOnly | QIODevice::Text));
        const QString firstContents = QString::fromUtf8(firstFile.readAll());
        QVERIFY2(firstContents.contains(QStringLiteral("tags: [food, journal]\n")),
                 qPrintable(firstContents));
        QCOMPARE(editor->property("text").toString(), firstContents);
        QVERIFY2(backend.status().contains(QStringLiteral("tags: food, journal")),
                 qPrintable(backend.status()));
        QCOMPARE(server.requestCount(), 1);
        const QJsonObject request = QJsonDocument::fromJson(server.lastBody()).object();
        QCOMPARE(request.value(QStringLiteral("model")).toString(), QStringLiteral("fake-model"));
        QCOMPARE(request.value(QStringLiteral("state")).toString(),
                 QStringLiteral("# Tagged Post\n\nSome body"));

        // A body change re-asks; tags already listed are not added twice.
        QString edited = firstContents;
        edited.replace(QStringLiteral("Some body"), QStringLiteral("More body"));
        QVERIFY(editor->setProperty("text", edited));
        const QString secondPath = directory.filePath(QStringLiteral("tagged-again.md"));
        backend.saveAs(QUrl::fromLocalFile(secondPath));
        QFile secondFile(secondPath);
        QVERIFY(secondFile.open(QIODevice::ReadOnly | QIODevice::Text));
        const QString secondContents = QString::fromUtf8(secondFile.readAll());
        QVERIFY(secondContents.contains(QStringLiteral("tags: [food, journal]\n")));
        QCOMPARE(editor->property("text").toString(), secondContents);
        QCOMPARE(server.requestCount(), 2);
        QVERIFY(!backend.status().contains(QStringLiteral("tags:")));

        // A front-matter-only edit leaves the body, so nothing is re-asked.
        QString retitled = secondContents;
        retitled.replace(QStringLiteral("title: Tagged Post"), QStringLiteral("title: Renamed"));
        QVERIFY(editor->setProperty("text", retitled));
        const QString thirdPath = directory.filePath(QStringLiteral("tagged-retitled.md"));
        backend.saveAs(QUrl::fromLocalFile(thirdPath));
        QCOMPARE(server.requestCount(), 2);

        // A disabled tagger leaves saves untouched.
        QSettings().setValue(QStringLiteral("tagging/enabled"), false);
        QString plainEdit = secondContents;
        plainEdit.replace(QStringLiteral("More body"), QStringLiteral("Plain body"));
        QVERIFY(editor->setProperty("text", plainEdit));
        const QString fourthPath = directory.filePath(QStringLiteral("tagged-plain.md"));
        backend.saveAs(QUrl::fromLocalFile(fourthPath));
        QFile fourthFile(fourthPath);
        QVERIFY(fourthFile.open(QIODevice::ReadOnly | QIODevice::Text));
        const QString fourthContents = QString::fromUtf8(fourthFile.readAll());
        QVERIFY(fourthContents.contains(QStringLiteral("Plain body")));
        QVERIFY(fourthContents.contains(QStringLiteral("tags: [food, journal]\n")));
        QCOMPARE(server.requestCount(), 2);
    }

private:
    QTemporaryDir m_settingsDirectory;
};

QTEST_MAIN(WriteCamTest)
#include "tst_writecam.moc"
