#include "backend.h"

#include <QClipboard>
#include <QCryptographicHash>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFontDatabase>
#include <QDesktopServices>
#include <QGuiApplication>
#include <QMimeData>
#include <QProcess>
#include <QPrintDialog>
#include <QPrinter>
#include <QQuickTextDocument>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSettings>
#include <QStandardPaths>
#include <QJsonObject>
#include <QLockFile>
#include <QSaveFile>
#include <QTextBlock>
#include <QTextBlockFormat>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextStream>
#include <QUrl>
#include <QVariantMap>
#include <QWindow>

#include <algorithm>

#include "markdownhighlighter.h"
#include "systemone.h"
constexpr qreal typoraLineHeightPercent = 140;
const QString lastSaveDirectorySetting = QStringLiteral("file/lastSaveDirectory");
const QString editorFontSetting = QStringLiteral("editor/font");
const QString taggingEnabledSetting = QStringLiteral("tagging/enabled");
const QString taggingEndpointSetting = QStringLiteral("tagging/endpoint");
const QString taggingModelSetting = QStringLiteral("tagging/model");
const QString taggingApiKeySetting = QStringLiteral("tagging/apiKey");
const QString taggingTimeoutSetting = QStringLiteral("tagging/timeoutMs");
const QString taggingSchemaPathSetting = QStringLiteral("tagging/schemaPath");
// Noto ships a separate family per script, which buries every other font in
// the picker. Offer only the core Latin families; the rest still serve as
// fallbacks for whatever scripts a document contains.
const QStringList keptNotoFamilies = {QStringLiteral("Noto Sans"),
                                      QStringLiteral("Noto Sans Mono"),
                                      QStringLiteral("Noto Serif")};

// Fontconfig's generic aliases just point at another listed family.
const QStringList genericFontAliases = {QStringLiteral("Monospace"), QStringLiteral("Mono"),
                                        QStringLiteral("Sans Serif"), QStringLiteral("Sans"),
                                        QStringLiteral("Serif"), QStringLiteral("System-ui")};

// Symbol fonts often claim Latin coverage because they map their glyphs onto
// ASCII, so they can only be told apart by name. D050000L is URW's Dingbats.
const QRegularExpression symbolFontName(
    QStringLiteral("symbol|dingbat|emoji|icon|awesome|^D050000L$"),
    QRegularExpression::CaseInsensitiveOption);

namespace {

// YAML front matter: a document that opens with a "---" line, holds its
// fields, and is closed by another "---" (or "...") line before the content.
struct FrontMatterRange {
    bool found = false;
    int contentStart = 0;
    int contentEnd = 0;
};

bool isFrontMatterDelimiter(const QString &line) {
    const QString trimmed = line.trimmed();
    return trimmed == QStringLiteral("---") || trimmed == QStringLiteral("...");
}

FrontMatterRange findFrontMatter(const QString &text) {
    FrontMatterRange range;
    int lineStart = 0;
    bool opened = false;
    while (lineStart <= text.size()) {
        const int lineEnd = text.indexOf(QLatin1Char('\n'), lineStart);
        const int end = lineEnd < 0 ? text.size() : lineEnd;
        const QString line = text.mid(lineStart, end - lineStart);
        if (!opened) {
            if (line.trimmed() != QStringLiteral("---"))
                return range;
            opened = true;
            range.contentStart = end + 1;
        } else if (isFrontMatterDelimiter(line)) {
            range.contentEnd = lineStart;
            range.found = true;
            return range;
        }
        if (lineEnd < 0)
            return range;
        lineStart = lineEnd + 1;
    }
    return range;
}

QString bodyAfterFrontMatter(const QString &text) {
    const FrontMatterRange frontMatter = findFrontMatter(text);
    if (!frontMatter.found)
        return text;
    QString body = text.mid(frontMatter.contentEnd);
    const int delimiterEnd = body.indexOf(QLatin1Char('\n'));
    body = delimiterEnd < 0 ? QString() : body.mid(delimiterEnd + 1);
    while (body.startsWith(QLatin1Char('\n')))
        body.remove(0, 1);
    return body;
}

QString stripYamlQuotes(QString value) {
    if (value.size() >= 2 && ((value.startsWith(QLatin1Char('"'))
                                    && value.endsWith(QLatin1Char('"')))
                               || (value.startsWith(QLatin1Char('\''))
                                   && value.endsWith(QLatin1Char('\'')))))
        value = value.mid(1, value.size() - 2);
    return value.trimmed();
}

// A flow YAML list like [a, "b, c", 'd'] into its scalar items.
QStringList flowListItems(const QString &value) {
    QStringList items;
    QString item;
    bool quoted = false;
    QChar quote;
    for (const QChar c : value) {
        if (quoted) {
            if (c == quote)
                quoted = false;
            else
                item += c;
        } else if (c == QLatin1Char('"') || c == QLatin1Char('\'')) {
            quoted = true;
            quote = c;
        } else if (c == QLatin1Char(',')) {
            items.append(stripYamlQuotes(item));
            item.clear();
        } else {
            item += c;
        }
    }
    if (!stripYamlQuotes(item).isEmpty())
        items.append(stripYamlQuotes(item));
    return items;
}

// The swappable decision server the tagger talks to. Laya's laya-serve,
// Ollama's decision models, and TypeSafe Jev all speak /v1/systemone.
DecisionServer loadDecisionServer() {
    QSettings settings;
    DecisionServer server;
    server.endpoint = QUrl(settings.value(
        taggingEndpointSetting,
        QStringLiteral("http://127.0.0.1:8000/v1/systemone")).toString());
    server.model = settings.value(
        taggingModelSetting, QStringLiteral("laya:latest")).toString();
    server.apiKey = settings.value(taggingApiKeySetting).toString();
    server.timeoutMs = settings.value(taggingTimeoutSetting, 6000).toInt();
    return server;
}

QString tagSchemaPath() {
    const QString configured = QSettings().value(taggingSchemaPathSetting).toString();
    if (!configured.isEmpty())
        return configured;
    return QDir(QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation))
        .filePath(QStringLiteral("tag-schema.json"));
}

}  // namespace

QString Backend::normalizedLinkUrl(const QString &clipboardText) {
    QString candidate = clipboardText.trimmed();
    static const QRegularExpression lineBreakRe(QStringLiteral("[\\r\\n]"));
    const int lineBreak = candidate.indexOf(lineBreakRe);
    if (lineBreak >= 0)
        candidate = candidate.left(lineBreak).trimmed();

    if (candidate.isEmpty())
        return {};

    if (candidate.startsWith(QStringLiteral("www."), Qt::CaseInsensitive))
        candidate.prepend(QStringLiteral("https://"));

    static const QRegularExpression schemeRe(
        QStringLiteral("^[A-Za-z][A-Za-z0-9+.-]*:"));
    if (!schemeRe.match(candidate).hasMatch())
        return {};

    const QUrl url(candidate);
    if (!url.isValid() || url.scheme().isEmpty())
        return {};

    const QString scheme = url.scheme().toLower();
    const bool webUrl = scheme == QStringLiteral("http")
        || scheme == QStringLiteral("https")
        || scheme == QStringLiteral("ftp");
    if (webUrl && url.host().isEmpty())
        return {};

    if (!webUrl && scheme != QStringLiteral("mailto"))
        return {};

    return url.toString();
}

Backend::Backend(QObject *parent) : QObject(parent) {
    const QString savedFont = QSettings().value(editorFontSetting).toString();
    m_editorFont = !savedFont.isEmpty() && QFontDatabase::hasFamily(savedFont)
        ? savedFont
        : defaultEditorFont();

    const QString stateDirectory = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(stateDirectory);
    // Claim an orphaned snapshot before taking an empty slot. This ensures a
    // crash in window 2 is still recovered even if window 1 exited normally.
    for (int pass = 0; pass < 2 && !m_recoveryLock; ++pass) {
        for (int slot = 0; slot < 100; ++slot) {
            const QString base = QDir(stateDirectory).filePath(
                QStringLiteral("recovery-%1").arg(slot));
            const bool snapshotExists = QFileInfo::exists(base + QStringLiteral(".json"));
            if ((pass == 0) != snapshotExists)
                continue;
            auto lock = std::make_unique<QLockFile>(base + QStringLiteral(".lock"));
            if (lock->tryLock()) {
                m_recoveryPath = base + QStringLiteral(".json");
                m_recoveryLock = std::move(lock);
                break;
            }
        }
    }
    m_wordCountTimer.setSingleShot(true);
    m_wordCountTimer.setInterval(120);
    connect(&m_wordCountTimer, &QTimer::timeout, this, &Backend::refreshWordCount);
    m_recoveryTimer.setSingleShot(true);
    m_recoveryTimer.setInterval(750);
    connect(&m_recoveryTimer, &QTimer::timeout, this, &Backend::writeRecovery);
    connect(&m_fileWatcher, &QFileSystemWatcher::fileChanged, this,
            [this](const QString &path) {
                if (path != m_fileUrl.toLocalFile())
                    return;

                const bool deleted = !QFileInfo::exists(path);
                if (!deleted && m_hasKnownFileContents) {
                    QFile file(path);
                    if (file.open(QIODevice::ReadOnly)
                            && file.readAll() == m_lastKnownFileContents) {
                        // Atomic saves can replace the watched inode. Re-arm the
                        // watcher, but do not report our own save as an outside edit.
                        watchCurrentFile();
                        return;
                    }
                }

                emit externalChangeDetected(deleted, m_modified);
            });

    loadOmarchyTheme();
    watchOmarchyTheme();
    connect(&m_themeWatcher, &QFileSystemWatcher::fileChanged, this, [this]() {
        loadOmarchyTheme();
        watchOmarchyTheme();
    });
    connect(&m_themeWatcher, &QFileSystemWatcher::directoryChanged, this, [this]() {
        loadOmarchyTheme();
        watchOmarchyTheme();
    });
}

Backend::~Backend() = default;

void Backend::setParentWindow(QWindow *window) {
    m_parentWindow = window;
}

QString Backend::fileName() const {
    if (!m_fileUrl.isValid() || m_fileUrl.isEmpty())
        return QStringLiteral("Untitled.md");

    if (m_fileUrl.isLocalFile()) {
        const QFileInfo info(m_fileUrl.toLocalFile());
        if (!info.fileName().isEmpty())
            return info.fileName();
    }

    const QString name = m_fileUrl.fileName();
    return name.isEmpty() ? QStringLiteral("Untitled.md") : name;
}

void Backend::setDarkMode(bool darkMode) {
    if (m_darkMode == darkMode)
        return;

    m_darkMode = darkMode;
    loadOmarchyTheme();
    emit darkModeChanged();
}

void Backend::setTextScale(qreal textScale) {
    if (qFuzzyCompare(m_textScale, textScale))
        return;

    m_textScale = textScale;
    emit textScaleChanged();
}

void Backend::attachDocument(QObject *textDocument) {
    auto *quickDocument = qobject_cast<QQuickTextDocument *>(textDocument);
    if (!quickDocument || !quickDocument->textDocument()) {
        setStatus(QStringLiteral("Could not attach the Markdown renderer."));
        return;
    }

    if (m_highlighter)
        delete m_highlighter.data();

    m_document = quickDocument->textDocument();
    m_lastDocumentText = m_document->toPlainText();
    m_highlighter = new MarkdownHighlighter(m_document);
    m_highlighter->setDarkMode(m_darkMode);
    m_highlighter->setColors(m_themeBackground, m_themeForeground, m_themeAccent);

    connect(m_document, &QTextDocument::contentsChange, this,
            [this](int position, int, int charsAdded) {
                if (m_formattingTypography || m_loading)
                    return;
                m_lastChangePos = position;
                m_lastChangeAdded = charsAdded;
            });

    applyDocumentTypography();
    restoreRecovery();
}

void Backend::openDialog() {
    emit openDialogRequested();
}

void Backend::open(const QUrl &url) {
    if (!url.isLocalFile()) {
        setStatus(QStringLiteral("Only local files can be opened."));
        return;
    }

    const QString targetName = QFileInfo(url.toLocalFile()).fileName();
    QFile file(url.toLocalFile());
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        setStatus(QStringLiteral("Could not open %1.").arg(targetName));
        return;
    }

    const QByteArray contents = file.readAll();
    loadDocumentText(QString::fromUtf8(contents));
    clearRecovery();
    m_lastKnownFileContents = contents;
    m_hasKnownFileContents = true;
    setFileUrl(url);
    watchCurrentFile();
    setModified(false);
    setStatus(QStringLiteral("Opened %1").arg(fileName()));
}

void Backend::save() {
    if (!m_fileUrl.isValid() || m_fileUrl.isEmpty()) {
        saveAsDialog();
        return;
    }

    saveTo(m_fileUrl);
}

void Backend::saveForClose() {
    if (!m_modified) {
        emit closeAfterSave();
        return;
    }

    m_closeAfterSave = true;
    save();
}

void Backend::saveAsDialog() {
    emit saveDialogRequested(suggestedSaveUrl());
}

void Backend::saveAs(const QUrl &url) {
    saveTo(url);
}

void Backend::fileDialogCanceled() {
    m_closeAfterSave = false;
}

void Backend::discardRecovery() {
    clearRecovery();
}

void Backend::reloadFromDisk() {
    if (m_fileUrl.isLocalFile())
        open(m_fileUrl);
}

void Backend::keepExternalVersion() {
    QFile file(m_fileUrl.toLocalFile());
    if (file.open(QIODevice::ReadOnly)) {
        m_lastKnownFileContents = file.readAll();
        m_hasKnownFileContents = true;
    } else {
        m_lastKnownFileContents.clear();
        m_hasKnownFileContents = false;
    }
    setModified(true);
    scheduleRecovery();
    watchCurrentFile();
    setStatus(QStringLiteral("Kept your version"));
}

void Backend::printDocument() {
    if (!m_document) {
        setStatus(QStringLiteral("There is no document to print."));
        return;
    }

    QPrinter printer(QPrinter::HighResolution);
    QPrintDialog dialog(&printer);
    dialog.setWindowTitle(QStringLiteral("Print %1").arg(fileName()));
    dialog.winId();
    if (dialog.windowHandle() && m_parentWindow)
        dialog.windowHandle()->setTransientParent(m_parentWindow);

    if (dialog.exec() == QDialog::Accepted) {
        QTextDocument rendered;
        rendered.setDefaultFont(m_document->defaultFont());
        rendered.setMarkdown(currentDocumentText());
        rendered.print(&printer);
    }
}

void Backend::newWindow() {
    const bool started = QProcess::startDetached(QCoreApplication::applicationFilePath(),
                                                 QStringList());
    if (!started)
        setStatus(QStringLiteral("Could not open a new window."));
}

QString Backend::clipboardUrl() const {
    const QClipboard *clipboard = QGuiApplication::clipboard();
    if (!clipboard)
        return {};

    const QMimeData *mimeData = clipboard->mimeData();
    if (!mimeData)
        return {};

    if (mimeData->hasUrls()) {
        const QList<QUrl> urls = mimeData->urls();
        for (const QUrl &url : urls) {
            const QString normalized = normalizedLinkUrl(url.toString());
            if (!normalized.isEmpty())
                return normalized;
        }
    }

    if (!mimeData->hasText())
        return {};

    return normalizedLinkUrl(mimeData->text());
}

QString Backend::clipboardText() const {
    const QClipboard *clipboard = QGuiApplication::clipboard();
    if (!clipboard)
        return {};

    const QMimeData *mimeData = clipboard->mimeData();
    return mimeData && mimeData->hasText() ? mimeData->text() : QString();
}

bool Backend::editorTextChanged() {
    if (m_loading || m_formattingTypography)
        return false;

    const QString text = currentDocumentText();
    if (text == m_lastDocumentText)
        return false;
    m_lastDocumentText = text;

    if (m_document) {
        const int blockCount = m_document->blockCount();
        if (blockCount > m_formattedBlockCount)
            reapplyTypographyToChange();
        m_formattedBlockCount = blockCount;
    }

    scheduleWordCount();
    setModified(true);
    setStatus(QStringLiteral("Unsaved"));
    scheduleRecovery();
    return true;
}

QVariantList Backend::hiddenRangesAt(int position) const {
    QVariantList ranges;
    if (!m_document)
        return ranges;

    const QTextBlock block =
        m_document->findBlock(qBound(0, position, m_document->characterCount() - 1));
    if (!block.isValid())
        return ranges;

    const int lineStart = block.position();
    QList<QPair<int, int>> spans;
    const QList<MarkdownHighlighter::InlineMarkup> markup =
        MarkdownHighlighter::inlineMarkup(block.text());
    for (const MarkdownHighlighter::InlineMarkup &item : markup) {
        for (const MarkdownHighlighter::Span &marker : item.markers) {
            spans.append({lineStart + marker.start,
                          lineStart + marker.start + marker.length});
        }
    }
    std::sort(spans.begin(), spans.end());

    for (const auto &span : spans) {
        ranges.append(QVariantMap{{QStringLiteral("start"), span.first},
                                  {QStringLiteral("end"), span.second}});
    }
    return ranges;
}

void Backend::setSearchHighlight(const QString &query, int currentMatchStart) {
    if (m_highlighter)
        m_highlighter->setSearch(query, currentMatchStart);
}

void Backend::openExternalUrl(const QUrl &url) {
    const QString scheme = url.scheme().toLower();
    if (scheme == QStringLiteral("http") || scheme == QStringLiteral("https")
            || scheme == QStringLiteral("mailto"))
        QDesktopServices::openUrl(url);
}

QVariantMap Backend::windowGeometry() const {
    QSettings settings;
    return {{QStringLiteral("x"), settings.value(QStringLiteral("window/x"), -1)},
            {QStringLiteral("y"), settings.value(QStringLiteral("window/y"), -1)},
            {QStringLiteral("width"), settings.value(QStringLiteral("window/width"), 1280)},
            {QStringLiteral("height"), settings.value(QStringLiteral("window/height"), 820)},
            {QStringLiteral("maximized"), settings.value(QStringLiteral("window/maximized"), false)}};
}

void Backend::saveWindowGeometry(int x, int y, int width, int height, bool maximized) {
    QSettings settings;
    if (!maximized) {
        settings.setValue(QStringLiteral("window/x"), x);
        settings.setValue(QStringLiteral("window/y"), y);
        settings.setValue(QStringLiteral("window/width"), width);
        settings.setValue(QStringLiteral("window/height"), height);
    }
    settings.setValue(QStringLiteral("window/maximized"), maximized);
}

QString Backend::defaultEditorFont() {
    return QStringLiteral("IBM Plex Mono");
}

void Backend::setEditorFont(const QString &family) {
    if (family.isEmpty() || family == m_editorFont)
        return;

    m_editorFont = family;
    QSettings().setValue(editorFontSetting, family);
    emit editorFontChanged();
}

QStringList Backend::availableFonts() const {
    // Leave out icon, emoji, and other symbol fonts, along with anything that
    // cannot set the Latin text the editor is written in.
    QStringList families;
    for (const QString &family : QFontDatabase::families(QFontDatabase::Latin)) {
        if (QFontDatabase::isPrivateFamily(family)
                || QFontDatabase::writingSystems(family).contains(QFontDatabase::Symbol))
            continue;
        families.append(family);
    }
    return selectableFontFamilies(families);
}

QStringList Backend::selectableFontFamilies(const QStringList &families) {
    QStringList selectable;
    static const QRegularExpression foundrySuffix(QStringLiteral("\\s*\\[[^\\]]*\\]$"));
    for (QString family : families) {
        // The same family from two foundries is listed as "Name [Foundry]".
        family.remove(foundrySuffix);
        if (family.startsWith(QStringLiteral("Noto ")) && !keptNotoFamilies.contains(family))
            continue;
        if (genericFontAliases.contains(family, Qt::CaseInsensitive)
                || family.contains(symbolFontName))
            continue;
        if (family == defaultEditorFont() || selectable.contains(family, Qt::CaseInsensitive))
            continue;
        selectable.append(family);
    }
    std::sort(selectable.begin(), selectable.end(), [](const QString &a, const QString &b) {
        return a.compare(b, Qt::CaseInsensitive) < 0;
    });
    selectable.prepend(defaultEditorFont());
    return selectable;
}

void Backend::documentFontChanged() {
    if (m_highlighter)
        m_highlighter->refreshFont();
}

void Backend::loadDocumentText(const QString &text) {
    if (!m_document) {
        setStatus(QStringLiteral("Could not attach the Markdown renderer."));
        return;
    }

    m_loading = true;
    m_document->setPlainText(text);
    m_lastDocumentText = text;
    m_loading = false;

    applyDocumentTypography();
    m_wordCountTimer.stop();
    setWordCount(countWords(text));
}

void Backend::setFileUrl(const QUrl &url) {
    if (m_fileUrl == url)
        return;

    m_fileUrl = url;
    emit fileUrlChanged();
    watchCurrentFile();
}

void Backend::setModified(bool modified) {
    if (m_modified == modified)
        return;

    m_modified = modified;
    emit modifiedChanged();
}

void Backend::setStatus(const QString &status) {
    if (m_status == status)
        return;

    m_status = status;
    emit statusChanged();
}

void Backend::saveTo(const QUrl &url) {
    if (!url.isLocalFile()) {
        m_closeAfterSave = false;
        setStatus(QStringLiteral("Only local files can be saved."));
        return;
    }

    const QString targetName = QFileInfo(url.toLocalFile()).fileName();
    QSaveFile file(url.toLocalFile());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        m_closeAfterSave = false;
        setStatus(QStringLiteral("Could not save %1.").arg(targetName));
        return;
    }

    // A document saved for the first time gets front matter; one that already
    // tracks `updated` gets the save's timestamp appended to its log. The
    // buffer is updated too, so the editor always matches the file on disk.
    // Decisions are collected first so the tags land in the file the moment
    // it is written; a missing decision server never blocks the save.
    const bool creatingNewFile = !m_fileUrl.isValid() || m_fileUrl.isEmpty();
    const QStringList decidedTags = decideSaveTags(creatingNewFile);
    updateFrontMatterOnSave(creatingNewFile, QFileInfo(targetName).completeBaseName(),
                            decidedTags);

    const QByteArray contents = currentDocumentText().toUtf8();
    file.write(contents);

    // QSaveFile commits by replacing the target. Stop watching the old inode
    // before that replacement so our own write is not classified as external.
    const QStringList watched = m_fileWatcher.files();
    if (!watched.isEmpty())
        m_fileWatcher.removePaths(watched);

    // commit() flushes, fsyncs, and atomically renames the temp file into place,
    // returning false (and leaving the original untouched) on any write error.
    if (!file.commit()) {
        watchCurrentFile();
        m_closeAfterSave = false;
        setStatus(QStringLiteral("Could not write %1.").arg(targetName));
        return;
    }

    const bool shouldClose = m_closeAfterSave;
    m_closeAfterSave = false;
    m_lastKnownFileContents = contents;
    m_hasKnownFileContents = true;
    setFileUrl(url);
    watchCurrentFile();
    QSettings().setValue(lastSaveDirectorySetting,
                         QFileInfo(url.toLocalFile()).absolutePath());
    setModified(false);
    QString status = QStringLiteral("Saved %1").arg(fileName());
    if (!m_lastTagNote.isEmpty())
        status += QStringLiteral(" · %1").arg(m_lastTagNote);
    setStatus(status);
    clearRecovery();
    emit saveSucceeded();

    if (shouldClose)
        emit closeAfterSave();
}

void Backend::scheduleRecovery() {
    m_recoveryTimer.start();
}

QString Backend::recoveryPath() const {
    return m_recoveryPath;
}

void Backend::writeRecovery() {
    if (!m_modified)
        return;
    const QString path = recoveryPath();
    if (path.isEmpty())
        return;
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return;
    const QJsonObject recovery{{QStringLiteral("fileUrl"), m_fileUrl.toString()},
                               {QStringLiteral("text"), currentDocumentText()}};
    file.write(QJsonDocument(recovery).toJson(QJsonDocument::Compact));
    file.commit();
}

void Backend::restoreRecovery() {
    QFile file(recoveryPath());
    if (!file.open(QIODevice::ReadOnly))
        return;
    const QJsonDocument json = QJsonDocument::fromJson(file.readAll());
    if (!json.isObject() || !json.object().contains(QStringLiteral("text")))
        return;
    const QJsonObject recovery = json.object();
    loadDocumentText(recovery.value(QStringLiteral("text")).toString());
    const QUrl recoveredUrl(recovery.value(QStringLiteral("fileUrl")).toString());
    QFile diskFile(recoveredUrl.toLocalFile());
    if (recoveredUrl.isLocalFile() && diskFile.open(QIODevice::ReadOnly)) {
        m_lastKnownFileContents = diskFile.readAll();
        m_hasKnownFileContents = true;
    } else {
        m_lastKnownFileContents.clear();
        m_hasKnownFileContents = false;
    }
    setFileUrl(recoveredUrl);
    setModified(true);
    setStatus(QStringLiteral("Recovered unsaved changes"));
}

void Backend::clearRecovery() {
    m_recoveryTimer.stop();
    QFile::remove(recoveryPath());
}

void Backend::watchCurrentFile() {
    const QStringList watched = m_fileWatcher.files();
    if (!watched.isEmpty())
        m_fileWatcher.removePaths(watched);
    if (m_fileUrl.isLocalFile() && QFileInfo::exists(m_fileUrl.toLocalFile()))
        m_fileWatcher.addPath(m_fileUrl.toLocalFile());
}

void Backend::loadOmarchyTheme() {
    m_themeBackground = m_darkMode ? QStringLiteral("#101010") : QStringLiteral("#ffffff");
    m_themeForeground = m_darkMode ? QStringLiteral("#eeeeee") : QStringLiteral("#222324");
    m_themeAccent = m_darkMode ? QStringLiteral("#5584aa") : QStringLiteral("#2077b2");
    m_themeSelection = m_darkMode ? QStringLiteral("#186a9a") : QStringLiteral("#2077b2");

    const QString colorsPath = QDir::homePath()
        + QStringLiteral("/.local/state/omarchy/current/theme/colors.toml");
    QString themeMode;
    QFile file(colorsPath);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&file);
        while (!in.atEnd()) {
            const QString line = in.readLine().trimmed();
            if (line.isEmpty() || line.startsWith(QLatin1Char('#')))
                continue;

            const int equals = line.indexOf(QLatin1Char('='));
            if (equals < 0)
                continue;

            const QString key = line.left(equals).trimmed();
            QString value = line.mid(equals + 1).trimmed();
            if (value.size() >= 2
                    && ((value.front() == QLatin1Char('"') && value.back() == QLatin1Char('"'))
                        || (value.front() == QLatin1Char('\'') && value.back() == QLatin1Char('\''))))
                value = value.mid(1, value.size() - 2);

            if (key == QStringLiteral("mode"))
                themeMode = value;
            else if (key == QStringLiteral("background"))
                m_themeBackground = value;
            else if (key == QStringLiteral("foreground"))
                m_themeForeground = value;
            else if (key == QStringLiteral("accent"))
                m_themeAccent = value;
            else if (key == QStringLiteral("selection"))
                m_themeSelection = value;
        }
    }

    bool themeModeKnown = false;
    bool themeIsDark = m_darkMode;
    if (themeMode == QStringLiteral("dark")) {
        themeIsDark = true;
        themeModeKnown = true;
    } else if (themeMode == QStringLiteral("light")) {
        themeIsDark = false;
        themeModeKnown = true;
    } else {
        const QColor background(m_themeBackground);
        if (background.isValid()) {
            const double luminance = 0.299 * background.redF()
                + 0.587 * background.greenF() + 0.114 * background.blueF();
            themeIsDark = luminance < 0.5;
            themeModeKnown = true;
        }
    }
    if (themeModeKnown && themeIsDark != m_darkMode) {
        m_darkMode = themeIsDark;
        emit darkModeChanged();
    }

    if (m_highlighter) {
        m_highlighter->setDarkMode(m_darkMode);
        m_highlighter->setColors(m_themeBackground, m_themeForeground, m_themeAccent);
    }

    emit themeColorsChanged();
}

void Backend::watchOmarchyTheme() {
    const QStringList watched = m_themeWatcher.files() + m_themeWatcher.directories();
    if (!watched.isEmpty())
        m_themeWatcher.removePaths(watched);

    const QString currentDir = QDir::homePath()
        + QStringLiteral("/.local/state/omarchy/current");
    const QString themeDir = currentDir + QStringLiteral("/theme");
    const QString colorsPath = themeDir + QStringLiteral("/colors.toml");

    if (QDir(currentDir).exists())
        m_themeWatcher.addPath(currentDir);
    if (QDir(themeDir).exists())
        m_themeWatcher.addPath(themeDir);
    if (QFile::exists(colorsPath))
        m_themeWatcher.addPath(colorsPath);
}

QUrl Backend::suggestedSaveUrl() const {
    if (m_fileUrl.isLocalFile())
        return m_fileUrl;

    const QString savedDirectory = QSettings().value(lastSaveDirectorySetting).toString();
    const QDir directory = savedDirectory.isEmpty() || !QDir(savedDirectory).exists()
        ? QDir::home()
        : QDir(savedDirectory);
    return QUrl::fromLocalFile(
        directory.filePath(suggestedFileName(currentDocumentText())));
}

QString Backend::currentDocumentText() const {
    return m_document ? m_document->toPlainText() : QString();
}

int Backend::countWords(const QString &text) {
    static const QRegularExpression wordRe(
        QStringLiteral("[\\p{L}\\p{N}]+(?:['-][\\p{L}\\p{N}]+)*"));
    int count = 0;
    QRegularExpressionMatchIterator it = wordRe.globalMatch(text);
    while (it.hasNext()) {
        it.next();
        ++count;
    }
    return count;
}

QString Backend::suggestedFileName(const QString &text) {
    return suggestedFileName(text, QDate::currentDate());
}

QString Backend::suggestedFileName(const QString &text, const QDate &today) {
    QString name;
    const FrontMatterRange frontMatter = findFrontMatter(text);
    if (frontMatter.found) {
        const int closingNewline = text.indexOf(QLatin1Char('\n'), frontMatter.contentEnd);
        QString after = closingNewline < 0 ? QString() : text.mid(closingNewline + 1);
        while (after.startsWith(QLatin1Char('\n')))
            after.remove(0, 1);
        name = after.section(QLatin1Char('\n'), 0, 0).trimmed();
    } else {
        name = text.section(QLatin1Char('\n'), 0, 0).trimmed();
    }
    name.replace(QRegularExpression(QStringLiteral("[/\\x00-\\x1f\\x7f]")),
                 QStringLiteral("-"));
    name.remove(QRegularExpression(QStringLiteral("^#{1,6}\\s+")));
    if (name.endsWith(QStringLiteral(".md"), Qt::CaseInsensitive))
        name.chop(3);

    // The day's date plus a hyphenated slug of the title, unless the title
    // already carries a date of its own.
    QString slug = name.toLower();
    static const QRegularExpression nonSlugChars(QStringLiteral("[^\\p{L}\\p{N}]+"));
    slug.replace(nonSlugChars, QStringLiteral("-"));
    static const QRegularExpression edgeHyphens(QStringLiteral("^-+|-+$"));
    slug.remove(edgeHyphens);
    slug = slug.left(120);
    if (slug.isEmpty())
        slug = QStringLiteral("untitled");
    static const QRegularExpression alreadyDated(QStringLiteral("^\\d{4}-\\d{2}-\\d{2}(?:-|$)"));
    if (!alreadyDated.match(slug).hasMatch())
        slug.prepend(today.toString(Qt::ISODate) + QLatin1Char('-'));
    if (!slug.endsWith(QStringLiteral(".md")))
        slug += QStringLiteral(".md");
    return slug;
}

QString Backend::yamlScalar(const QString &value) {
    // Plain YAML scalars cannot start with punctuation, carry ": " or a " #"
    // comment, or collide with another YAML type such as a bool or a date.
    static const QRegularExpression plainSafe(
        QStringLiteral("^[A-Za-z0-9](?:[A-Za-z0-9 ._/@()+'?!&-]*)$"));
    static const QRegularExpression anotherType(
        QStringLiteral("^(?:true|false|null|yes|no|on|off|~|"
                       "[+-]?(?:\\d+(?:\\.\\d*)?|\\.\\d+)(?:[eE][+-]?\\d+)?|"
                       "\\d{4}-\\d{1,2}(?:-\\d{1,2})?.*)$"),
        QRegularExpression::CaseInsensitiveOption);
    if (plainSafe.match(value).hasMatch() && !anotherType.match(value).hasMatch())
        return value;

    QString escaped = value;
    escaped.replace(QLatin1Char('\\'), QStringLiteral("\\\\"));
    escaped.replace(QLatin1Char('"'), QStringLiteral("\\\""));
    return QLatin1Char('"') + escaped + QLatin1Char('"');
}

QString Backend::derivedTitle(const QString &text, const QString &fallback) {
    static const QRegularExpression headingRe(QStringLiteral("^(#{1,6})\\s+(.+?)\\s*$"));
    const QStringList lines = text.split(QLatin1Char('\n'));
    for (const QString &rawLine : lines) {
        const QString line = rawLine.trimmed();
        if (line.isEmpty() || isFrontMatterDelimiter(line))
            continue;
        const QRegularExpressionMatch heading = headingRe.match(line);
        return heading.hasMatch() ? heading.captured(2).trimmed() : line;
    }
    return fallback.isEmpty() ? QStringLiteral("Untitled") : fallback;
}

QString Backend::frontMatterText(const QString &title, const QDate &date) {
    return QStringLiteral("---\n")
        + QStringLiteral("date: ") + date.toString(Qt::ISODate) + QStringLiteral("\n")
        + QStringLiteral("updated: []\n")
        + QStringLiteral("title: ") + yamlScalar(title) + QStringLiteral("\n")
        + QStringLiteral("draft: true\n")
        + QStringLiteral("categories: []\n")
        + QStringLiteral("tags: []\n")
        + QStringLiteral("---\n");
}

QString Backend::applyFrontMatter(const QString &text, bool creatingNewFile, bool modified,
                                  const QString &fallbackTitle, const QDateTime &now) {
    const FrontMatterRange frontMatter = findFrontMatter(text);
    if (!frontMatter.found) {
        if (!creatingNewFile)
            return text;

        const QString block = frontMatterText(derivedTitle(text, fallbackTitle), now.date());
        QString body = text;
        while (body.startsWith(QLatin1Char('\n')))
            body.remove(0, 1);
        return body.isEmpty() ? block : block + QLatin1Char('\n') + body;
    }

    if (!modified)
        return text;

    // `updated` keeps a log of every save's timestamp. A scalar value (or an
    // empty field) becomes the first entry, front matter that never tracked
    // one has the log started after its `date`, and every other field is the
    // writer's own and is left exactly as written.
    static const QRegularExpression updatedKeyRe(
        QStringLiteral("^(\\s*)updated\\s*:\\s*(.*?)\\s*$"));
    static const QRegularExpression dateKeyRe(QStringLiteral("^(\\s*)date\\s*:"));
    static const QRegularExpression itemRe(QStringLiteral("^(\\s*)-\\s"));

    const QString stamp = now.toString(QStringLiteral("yyyy-MM-dd HH:mm"));
    int dateLineEnd = -1;
    QString dateIndent;

    int keyStart = frontMatter.contentStart;
    while (keyStart < frontMatter.contentEnd) {
        int keyEnd = text.indexOf(QLatin1Char('\n'), keyStart);
        if (keyEnd < 0 || keyEnd > frontMatter.contentEnd)
            keyEnd = frontMatter.contentEnd;
        const QString line = text.mid(keyStart, keyEnd - keyStart);
        const QRegularExpressionMatch key = updatedKeyRe.match(line);
        if (!key.hasMatch()) {
            if (dateLineEnd < 0) {
                const QRegularExpressionMatch dateKey = dateKeyRe.match(line);
                if (dateKey.hasMatch()) {
                    dateIndent = dateKey.captured(1);
                    dateLineEnd = keyEnd;
                }
            }
            keyStart = keyEnd + 1;
            continue;
        }

        const QString keyIndent = key.captured(1);
        const QString inlineValue = key.captured(2);
        const bool emptyList = inlineValue.isEmpty()
            || inlineValue == QStringLiteral("[]");

        // Collect the block sequence that follows the key, if there is one.
        int itemsStart = keyEnd + 1;
        int itemsEnd = keyEnd;
        QString firstItemIndent;
        QString lastEntry;
        int scan = itemsStart;
        while (scan < frontMatter.contentEnd) {
            int lineEnd = text.indexOf(QLatin1Char('\n'), scan);
            if (lineEnd < 0 || lineEnd > frontMatter.contentEnd)
                lineEnd = frontMatter.contentEnd;
            const QString line = text.mid(scan, lineEnd - scan);
            const QRegularExpressionMatch item = itemRe.match(line);
            if (!item.hasMatch())
                break;
            if (firstItemIndent.isNull())
                firstItemIndent = item.captured(1);
            lastEntry = line.mid(item.capturedLength(0)).trimmed();
            itemsEnd = lineEnd;
            scan = lineEnd + 1;
        }

        if (lastEntry.isNull() && !emptyList)
            lastEntry = inlineValue;
        if (lastEntry == stamp)
            return text;  // this save's minute is already logged

        const QString itemIndent = !firstItemIndent.isNull()
            ? firstItemIndent
            : keyIndent + QStringLiteral("  ");
        QString replacement = keyIndent + QStringLiteral("updated:\n");
        if (!emptyList)
            replacement += itemIndent + QStringLiteral("- ") + inlineValue + QStringLiteral("\n");
        if (itemsEnd > keyEnd)
            replacement += text.mid(itemsStart, itemsEnd - itemsStart) + QStringLiteral("\n");
        replacement += itemIndent + QStringLiteral("- ") + stamp;

        return text.left(keyStart) + replacement + text.mid(itemsEnd);
    }

    // No `updated` field anywhere: start the log after the `date` line, or at
    // the top of the front matter when the writer keeps no date either.
    const QString indent = dateLineEnd >= 0 ? dateIndent : QString();
    const int insertAt = dateLineEnd >= 0 ? dateLineEnd + 1 : frontMatter.contentStart;
    return text.left(insertAt)
        + indent + QStringLiteral("updated:\n")
        + indent + QStringLiteral("  - ") + stamp + QStringLiteral("\n")
        + text.mid(insertAt);
}

// Merges decided tags into the front matter's `tags` field, keeping
// whatever the writer already listed and the style it was written in.
QString Backend::applyFrontMatterTags(const QString &text, const QStringList &tags,
                                      QStringList *added) {
    if (added)
        added->clear();
    const FrontMatterRange frontMatter = findFrontMatter(text);
    if (!frontMatter.found || tags.isEmpty())
        return text;

    static const QRegularExpression tagsKeyRe(
        QStringLiteral("^(\\s*)tags\\s*:\\s*(.*?)\\s*$"));
    static const QRegularExpression itemRe(QStringLiteral("^(\\s*)-\\s"));

    int keyStart = frontMatter.contentStart;
    while (keyStart < frontMatter.contentEnd) {
        int keyEnd = text.indexOf(QLatin1Char('\n'), keyStart);
        if (keyEnd < 0 || keyEnd > frontMatter.contentEnd)
            keyEnd = frontMatter.contentEnd;
        const QString line = text.mid(keyStart, keyEnd - keyStart);
        const QRegularExpressionMatch key = tagsKeyRe.match(line);
        if (!key.hasMatch()) {
            keyStart = keyEnd + 1;
            continue;
        }

        const QString keyIndent = key.captured(1);
        const QString inlineValue = key.captured(2);

        // Collect the block sequence that follows the key, if there is one.
        int itemsStart = keyEnd + 1;
        int itemsEnd = keyEnd;
        QString firstItemIndent;
        QStringList existing;
        int scan = itemsStart;
        while (scan < frontMatter.contentEnd) {
            int lineEnd = text.indexOf(QLatin1Char('\n'), scan);
            if (lineEnd < 0 || lineEnd > frontMatter.contentEnd)
                lineEnd = frontMatter.contentEnd;
            const QString itemLine = text.mid(scan, lineEnd - scan);
            const QRegularExpressionMatch item = itemRe.match(itemLine);
            if (!item.hasMatch())
                break;
            if (firstItemIndent.isNull())
                firstItemIndent = item.captured(1);
            existing.append(stripYamlQuotes(itemLine.mid(item.capturedLength(0))));
            itemsEnd = lineEnd;
            scan = lineEnd + 1;
        }

        // Without a block, an inline value is a flow list, a scalar, or empty.
        if (itemsEnd == keyEnd) {
            if (inlineValue.startsWith(QLatin1Char('[')))
                existing.append(flowListItems(inlineValue));
            else if (!inlineValue.isEmpty())
                existing.append(stripYamlQuotes(inlineValue));
        }

        QStringList missing;
        for (const QString &tag : tags)
            if (!existing.contains(tag, Qt::CaseInsensitive)
                    && !missing.contains(tag, Qt::CaseInsensitive))
                missing.append(tag);
        if (added)
            *added = missing;
        if (missing.isEmpty())
            return text;

        if (itemsEnd > keyEnd) {
            // Block style: keep the writer's items and append the new ones.
            QStringList lines;
            lines << line << text.mid(itemsStart, itemsEnd - itemsStart);
            const QString itemIndent = firstItemIndent.isNull()
                ? keyIndent + QStringLiteral("  ") : firstItemIndent;
            for (const QString &tag : missing)
                lines << itemIndent + QStringLiteral("- ") + yamlScalar(tag);
            return text.left(keyStart) + lines.join(QLatin1Char('\n'))
                + text.mid(itemsEnd);
        }

        // Flow style: one inline list, quoting anything that needs it.
        QStringList rendered;
        for (const QString &tag : existing + missing)
            rendered << yamlScalar(tag);
        const QString replacement = keyIndent + QStringLiteral("tags: [")
            + rendered.join(QStringLiteral(", ")) + QLatin1Char(']');
        return text.left(keyStart) + replacement + text.mid(keyEnd);
    }

    // No `tags` field anywhere: open one at the bottom of the front matter.
    QStringList rendered;
    for (const QString &tag : tags)
        rendered << yamlScalar(tag);
    if (added)
        *added = tags;
    return text.left(frontMatter.contentEnd)
        + QStringLiteral("tags: [") + rendered.join(QStringLiteral(", "))
        + QLatin1Char(']') + QLatin1Char('\n') + text.mid(frontMatter.contentEnd);
}

// Asks the configured decision server for tags covering the document's
// body. Runs at most once per distinct body: unchanged text keeps the tags
// it already carries, so manual tag edits survive later saves.
QStringList Backend::decideSaveTags(bool creatingNewFile) {
    m_lastTagNote.clear();

    if (!QSettings().value(taggingEnabledSetting, true).toBool())
        return {};

    const QString text = currentDocumentText();
    if (!creatingNewFile && !findFrontMatter(text).found)
        return {};  // plain files are never modified, so there is nowhere to write
    if (!m_modified && !creatingNewFile)
        return {};

    const QString body = bodyAfterFrontMatter(text).trimmed();
    if (body.isEmpty())
        return {};

    QString error;
    int stateChars = 4000;
    const QList<TagQuestion> questions = SystemOne::loadSchema(tagSchemaPath(), &error,
                                                                &stateChars);
    if (questions.isEmpty()) {
        m_lastTagNote = error;
        return {};
    }

    const QString state = body.left(stateChars);
    const QString stateHash = QString::fromLatin1(QCryptographicHash::hash(
        state.toUtf8(), QCryptographicHash::Sha256).toHex());
    if (stateHash == m_lastTagBodyHash)
        return {};  // this body already carries its decisions
    if (QDateTime::currentMSecsSinceEpoch() < m_tagSkipUntilMsecs)
        return {};  // a server that just failed gets a couple of minutes' rest

    const QStringList tags = SystemOne::decideTags(loadDecisionServer(), state, questions,
                                                   &error);
    if (!error.isEmpty()) {
        m_tagSkipUntilMsecs = QDateTime::currentMSecsSinceEpoch() + 120000;
        m_lastTagNote = QStringLiteral("tagger unreachable");
        return {};
    }
    m_lastTagBodyHash = stateHash;
    return tags;
}

void Backend::updateFrontMatterOnSave(bool creatingNewFile, const QString &fallbackTitle,
                                      const QStringList &newTags) {
    if (!m_document)
        return;

    const QString text = m_document->toPlainText();
    QString updated = applyFrontMatter(text, creatingNewFile, m_modified, fallbackTitle,
                                        QDateTime::currentDateTime());
    if (!newTags.isEmpty()) {
        QStringList addedTags;
        updated = applyFrontMatterTags(updated, newTags, &addedTags);
        if (!addedTags.isEmpty())
            m_lastTagNote = QStringLiteral("tags: %1")
                                .arg(addedTags.join(QStringLiteral(", ")));
    }
    if (updated == text)
        return;

    // The change always sits at the head of the document; locate it with a
    // common prefix/suffix so it is applied in the buffer as one edit too.
    int prefix = 0;
    while (prefix < text.size() && prefix < updated.size()
           && text.at(prefix) == updated.at(prefix))
        ++prefix;
    int suffix = 0;
    while (suffix < text.size() - prefix && suffix < updated.size() - prefix
           && text.at(text.size() - 1 - suffix) == updated.at(updated.size() - 1 - suffix))
        ++suffix;
    const QString replacement = updated.mid(prefix, updated.size() - prefix - suffix);

    // m_loading keeps the insertion from being reported as a fresh user edit
    // that would flip the just-saved document back to "Unsaved".
    m_loading = true;
    QTextCursor cursor(m_document);
    cursor.setPosition(prefix);
    cursor.setPosition(qMin(text.size() - suffix, m_document->characterCount() - 1),
                       QTextCursor::KeepAnchor);
    cursor.insertText(replacement);
    m_loading = false;

    m_lastDocumentText = m_document->toPlainText();
    m_lastChangePos = prefix;
    m_lastChangeAdded = replacement.size();
    reapplyTypographyToChange();
    m_formattedBlockCount = m_document->blockCount();
    m_wordCountTimer.stop();
    setWordCount(countWords(m_lastDocumentText));
}

void Backend::setWordCount(int words) {
    if (m_wordCount == words)
        return;

    m_wordCount = words;
    emit wordCountChanged();
}

void Backend::refreshWordCount() {
    setWordCount(countWords(currentDocumentText()));
}

void Backend::scheduleWordCount() {
    m_wordCountTimer.start();
}

void Backend::applyDocumentTypography() {
    if (!m_document)
        return;

    QTextBlockFormat blockFormat;
    blockFormat.setLineHeight(typoraLineHeightPercent, QTextBlockFormat::ProportionalHeight);

    // A full pass is only used for freshly loaded/attached documents, so it is
    // safe to drop undo history here (re-enabling clears the stack anyway).
    const bool undoEnabled = m_document->isUndoRedoEnabled();
    m_document->setUndoRedoEnabled(false);

    m_formattingTypography = true;
    QTextCursor cursor(m_document);
    cursor.select(QTextCursor::Document);
    cursor.mergeBlockFormat(blockFormat);
    m_formattingTypography = false;

    m_document->setUndoRedoEnabled(undoEnabled);

    m_formattedBlockCount = m_document->blockCount();
}

void Backend::reapplyTypographyToChange() {
    if (!m_document)
        return;

    QTextBlockFormat blockFormat;
    blockFormat.setLineHeight(typoraLineHeightPercent, QTextBlockFormat::ProportionalHeight);

    // Format only the block(s) touched by the last edit instead of the whole
    // document, and fold the change into the preceding edit command so a single
    // undo reverts both the text and its formatting.
    const int maxPos = m_document->characterCount() - 1;
    const int start = qBound(0, m_lastChangePos, maxPos);
    const int end = qBound(start, m_lastChangePos + m_lastChangeAdded, maxPos);

    m_formattingTypography = true;
    QTextCursor cursor(m_document);
    cursor.joinPreviousEditBlock();
    cursor.setPosition(start);
    cursor.setPosition(end, QTextCursor::KeepAnchor);
    cursor.mergeBlockFormat(blockFormat);
    cursor.endEditBlock();
    m_formattingTypography = false;
}
