#include <QtTest>
#include <QFont>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickStyle>

#include "backend.h"
#include "markdownhighlighter.h"

class WriteCamTest : public QObject {
    Q_OBJECT

private slots:
    void initTestCase() {
        QVERIFY(m_settingsDirectory.isValid());
        QQuickStyle::setStyle(QStringLiteral("Material"));
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope,
                           m_settingsDirectory.path());
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

private:
    QTemporaryDir m_settingsDirectory;
};

QTEST_MAIN(WriteCamTest)
#include "tst_writecam.moc"
