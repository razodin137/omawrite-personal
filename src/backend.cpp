#include "backend.h"

#include <QClipboard>
#include <QCryptographicHash>
#include <QtConcurrent/QtConcurrentRun>
#include <QColor>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFont>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QMimeData>
#include <QProcess>
#include <QPrintDialog>
#include <QPrinter>
#include <QQuickTextDocument>
#include <QRegularExpression>
#include <QSaveFile>
#include <QScreen>
#include <QSettings>
#include <QStandardPaths>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLockFile>
#include <QTextBlock>
#include <QTextBlockFormat>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextLayout>
#include <QTextStream>
#include <QUrl>
#include <QVariantMap>
#include <QWindow>

#include <algorithm>
#include <iterator>
#include <utility>

#include "markdownhighlighter.h"
#include "systemone.h"
constexpr qreal typoraLineHeightPercent = 140;

static QTextBlockFormat lineHeightFormat() {
    QTextBlockFormat format;
    format.setLineHeight(typoraLineHeightPercent, QTextBlockFormat::ProportionalHeight);
    return format;
}
const QString lastSaveDirectorySetting = QStringLiteral("file/lastSaveDirectory");
const QString textZoomSetting = QStringLiteral("text/zoom");

// The user's own text size, multiplied onto the desktop's text scale.
// Browser-sized steps, with a wider range for small and large displays.
constexpr qreal textZoomSteps[] = {0.5, 0.67, 0.75, 0.9, 1.0, 1.1, 1.25, 1.5, 1.75, 2.0, 2.5, 3.0};
constexpr int textZoomStepCount = int(std::size(textZoomSteps));

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
    QString body = value.trimmed();
    if (body.startsWith(QLatin1Char('[')) && body.endsWith(QLatin1Char(']')))
        body = body.mid(1, body.size() - 2);
    QString item;
    bool quoted = false;
    QChar quote;
    for (const QChar c : body) {
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

// A shade of `from` carried part of the way towards `to`.
QColor blend(const QColor &from, const QColor &to, qreal amount) {
    return QColor::fromRgbF(from.redF() + (to.redF() - from.redF()) * amount,
                            from.greenF() + (to.greenF() - from.greenF()) * amount,
                            from.blueF() + (to.blueF() - from.blueF()) * amount);
}

}
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
    // A settings file truncated by a crash reads back as an empty value, and
    // both that and a NaN would clamp to the smallest size rather than fall
    // back to the default.
    bool zoomOk = false;
    const qreal storedZoom = QSettings().value(textZoomSetting, 1.0).toDouble(&zoomOk);
    m_textZoom = zoomOk && storedZoom > 0 ? qBound(textZoomSteps[0], storedZoom,
                                                   textZoomSteps[textZoomStepCount - 1])
                                          : 1.0;

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
    connect(&m_wordCountWatcher, &QFutureWatcher<int>::finished, this, [this] {
        if (!m_wordCountStale)
            setWordCount(m_wordCountWatcher.result());
        if (m_wordCountAgain) {
            m_wordCountAgain = false;
            refreshWordCount();
        }
    });
    connect(&m_tagWatcher, &QFutureWatcher<TagDecision>::finished,
            this, &Backend::finishTagRequest);
    m_recoveryTimer.setSingleShot(true);
    m_recoveryTimer.setInterval(750);
    connect(&m_recoveryTimer, &QTimer::timeout, this, &Backend::writeRecovery);
    m_autoSaveTimer.setSingleShot(true);
    m_autoSaveTimer.setInterval(500);
    connect(&m_autoSaveTimer, &QTimer::timeout, this, &Backend::autoSave);
    // Tools that replace a file by removing it and writing it again leave it
    // briefly missing, so let the burst settle before deciding what happened.
    m_fileCheckTimer.setSingleShot(true);
    m_fileCheckTimer.setInterval(100);
    connect(&m_fileCheckTimer, &QTimer::timeout, this, &Backend::checkWatchedFile);
    connect(&m_fileWatcher, &QFileSystemWatcher::fileChanged, &m_fileCheckTimer,
            qOverload<>(&QTimer::start));
    connect(&m_fileWatcher, &QFileSystemWatcher::directoryChanged, &m_fileCheckTimer,
            qOverload<>(&QTimer::start));

    loadOmarchyTheme();
    watchOmarchyTheme();
    // Switching themes touches several watched paths in a row, and the theme
    // folder can be briefly missing, so reload once the burst has settled.
    m_themeReloadTimer.setSingleShot(true);
    m_themeReloadTimer.setInterval(100);
    connect(&m_themeReloadTimer, &QTimer::timeout, this, [this]() {
        loadOmarchyTheme();
        watchOmarchyTheme();
    });
    connect(&m_themeWatcher, &QFileSystemWatcher::fileChanged, &m_themeReloadTimer,
            qOverload<>(&QTimer::start));
    connect(&m_themeWatcher, &QFileSystemWatcher::directoryChanged, &m_themeReloadTimer,
            qOverload<>(&QTimer::start));
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
    m_systemDarkMode = darkMode;
    loadOmarchyTheme();
}

void Backend::setDesktopTextScale(qreal textScale) {
    if (qFuzzyCompare(m_desktopTextScale, textScale))
        return;

    m_desktopTextScale = textScale;
    emit textScaleChanged();
}

void Backend::increaseTextSize() {
    setTextZoom(steppedTextZoom(1));
}

void Backend::decreaseTextSize() {
    setTextZoom(steppedTextZoom(-1));
}

void Backend::resetTextSize() {
    setTextZoom(1.0);
}

// The neighbouring rung, starting from whichever rung the current zoom sits
// closest to; the ends of the ladder hold.
qreal Backend::steppedTextZoom(int direction) const {
    int closest = 0;
    for (int step = 1; step < textZoomStepCount; ++step) {
        if (qAbs(textZoomSteps[step] - m_textZoom) < qAbs(textZoomSteps[closest] - m_textZoom))
            closest = step;
    }

    return textZoomSteps[qBound(0, closest + direction, textZoomStepCount - 1)];
}

void Backend::setTextZoom(qreal zoom) {
    // Report the size even when the ladder has run out, so a keypress that
    // changes nothing still says why.
    setStatus(QStringLiteral("Text size %1%").arg(qRound(zoom * 100)));
    if (qFuzzyCompare(m_textZoom, zoom))
        return;

    m_textZoom = zoom;
    QSettings().setValue(textZoomSetting, zoom);
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
    m_highlighter->setTheme(m_darkMode, m_themeBackground, m_themeForeground, m_themeAccent,
                            m_themeCodeBackground);
    m_highlighter->setCaret(m_caretPosition);
    updateAlternativeMarks();

    connect(this, &Backend::alternativesChanged, this, &Backend::updateAlternativeMarks);

    connect(m_document, &QTextDocument::contentsChange, this,
            [this](int position, int, int charsAdded) {
                if (m_formattingTypography || m_loading)
                    return;
                m_lastChangePos = position;
                m_lastChangeAdded = charsAdded;
            });

    applyDocumentTypography();
    restoreRecovery();
    if (m_history.isEmpty())
        resetHistory();
}

void Backend::updateAlternativeMarks() {
    if (!m_highlighter)
        return;

    QList<MarkdownHighlighter::Span> spans;
    const QList<WriteCamAlternative> &alternatives = m_writeCamState.alternatives();
    spans.reserve(alternatives.size());
    for (const WriteCamAlternative &alternative : alternatives) {
        if (alternative.length > 0)
            spans.append(MarkdownHighlighter::Span{alternative.start, alternative.length});
    }
    m_highlighter->setAlternativeSpans(spans);

    // The pointer can only tint a run that still exists; a flip or an edit
    // above it moves the span, and a deleted run lets go of the pointer.
    if (m_pointerActive && m_pointerAlternative >= alternatives.size()) {
        m_pointerActive = false;
        m_pointerAlternative = -1;
        emit pointerChanged();
    }
    if (m_pointerActive && m_pointerAlternative >= 0) {
        const WriteCamAlternative &alternative = alternatives.at(m_pointerAlternative);
        m_highlighter->setPointerSpan({alternative.start, alternative.length});
    } else {
        m_highlighter->setPointerSpan({0, 0});
    }
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
    if (!file.open(QIODevice::ReadOnly)) {
        setStatus(QStringLiteral("Could not open %1.").arg(targetName));
        return;
    }

    const QByteArray raw = file.readAll();
    // Text mode drops carriage returns, matching how the file is compared
    // and saved later.
    file.setTextModeEnabled(true);
    file.seek(0);
    const QByteArray contents = file.readAll();
    QString body;
    m_writeCamState = WriteCamState::fromFileText(QString::fromUtf8(contents), &body);
    loadDocumentText(body);
    clearRecovery();
    setKnownFileContents(contents, true);
    m_autoSaveSuspended = false;
    setFileUrl(url);
    watchCurrentFile();
    setModified(false);
    if (savesUnchanged(raw)) {
        setStatus(QStringLiteral("Opened %1").arg(fileName()));
    } else {
        // Saving writes UTF-8 with plain line endings, which would change
        // bytes this file holds. Leave that to an explicit save.
        suspendAutoSave();
        setStatus(QStringLiteral("Opened %1. Auto-save is off to keep its encoding and line endings.")
                      .arg(fileName()));
    }
}

void Backend::save() {
    if (!m_fileUrl.isValid() || m_fileUrl.isEmpty()) {
        saveAsDialog();
        return;
    }

    saveTo(m_fileUrl);
}

void Backend::saveAsDialog() {
    emit saveDialogRequested(suggestedSaveUrl());
}

void Backend::saveAs(const QUrl &url) {
    saveTo(url);
}

void Backend::discardRecovery() {
    clearRecovery();
    // Discarded changes must never reach the file, even if the open or close
    // that follows fails and leaves them in the editor. Opening a file or an
    // explicit save lifts this.
    suspendAutoSave();
}

// A file named on the command line need not exist yet: `writecam new.md`
// starts a blank document that the first save writes to that path. Opening
// from the file picker or reloading still requires the file to be there.
void Backend::openOrStartFile(const QUrl &requestedUrl) {
    // A relative path names a file in the directory WriteCam was started
    // from; a recovery draft must not resolve it against another one later.
    const QFileInfo info(requestedUrl.toLocalFile());
    const QUrl url = requestedUrl.isLocalFile()
        ? QUrl::fromLocalFile(info.absoluteFilePath()) : requestedUrl;
    if (m_modified) {
        // A recovered draft fills this window; opening over it would lose it.
        setStatus(QStringLiteral("Recovered unsaved changes; %1 was not opened.")
                      .arg(info.fileName()));
        return;
    }
    if (!url.isLocalFile() || info.exists() || !info.absoluteDir().exists()) {
        open(url);
        return;
    }

    loadDocumentText(QString());
    m_writeCamState = {};
    updateAlternativeMarks();
    resetHistory();
    clearRecovery();
    // Nothing has been read, so no text counts as saved, and auto-save holds
    // off until an explicit save creates the file (see autoSave()).
    setKnownFileContents(QByteArray(), false);
    setFileUrl(url);
    watchCurrentFile();
    setModified(false);
    setStatus(QStringLiteral("New file %1").arg(fileName()));
}

void Backend::reloadFromDisk() {
    if (m_fileUrl.isLocalFile())
        open(m_fileUrl);
}

void Backend::keepExternalVersion() {
    QFile file(m_fileUrl.toLocalFile());
    if (file.open(QIODevice::ReadOnly)) {
        const QByteArray raw = file.readAll();
        file.setTextModeEnabled(true);
        file.seek(0);
        setKnownFileContents(file.readAll(), true);
        // Keeping our text over an edited file is a deliberate choice to
        // overwrite it, so auto-save may resume, unless saving would change
        // the file's encoding or line endings. A removed file stays removed
        // until the writer saves it again.
        m_autoSaveSuspended = !savesUnchanged(raw);
    } else {
        setKnownFileContents(QByteArray(), false);
    }
    setModified(true);
    scheduleRecovery();
    scheduleAutoSave();
    watchCurrentFile();
    setStatus(QStringLiteral("Kept your version"));
}

QFont Backend::printFont(const QFont &editorFont, qreal screenDpi) {
    // The editor sizes its font in pixels, which a printer takes as device dots.
    // Convert to points at the screen's DPI so the page matches the editor.
    QFont font = editorFont;
    if (font.pixelSize() <= 0)
        return font;

    const qreal dpi = screenDpi > 0.0 ? screenDpi : 96.0;
    font.setPointSizeF(font.pixelSize() * 72.0 / dpi);
    return font;
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
        const QScreen *screen = m_parentWindow ? m_parentWindow->screen()
                                               : QGuiApplication::primaryScreen();
        rendered.setDefaultFont(printFont(m_document->defaultFont(),
                                          screen ? screen->logicalDotsPerInchY() : 0.0));
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

void Backend::copyAsHtml() {
    QTextDocument rendered;
    rendered.setMarkdown(currentDocumentText());

    // Drop Qt's fixed fonts and margins from the body and blocks, so the pasted
    // text takes on the destination's own typeface and paragraph spacing.
    QString html = rendered.toHtml();
    static const QRegularExpression blockStyle(
        QStringLiteral("<(body|p|h[1-6]|ul|ol|li)\\s+style=\"[^\"]*\""));
    html.replace(blockStyle, QStringLiteral("<\\1"));

    auto *mimeData = new QMimeData;
    mimeData->setHtml(html);
    mimeData->setText(rendered.toPlainText());
    QGuiApplication::clipboard()->setMimeData(mimeData);
    setStatus(QStringLiteral("Copied as HTML."));
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

    const QString beforeText = m_lastDocumentText;
    const QString text = currentDocumentText();
    if (text == m_lastDocumentText)
        return false;
    m_lastDocumentText = text;

    reapplyTypographyToChange();

    if (m_applyingAlternative) {
        scheduleWordCount();
        updateDirtyStateForText(text);
        return true;
    }

    // Typing takes the arrow keys back for the caret, and the tint goes with
    // them. Moving the pointer again puts it back on whichever run it finds.
    if (m_pointerActive)
        releasePointer();

    m_writeCamState.reconcile(text);
    updateAlternativeMarks();
    recordTextHistory(beforeText, text);
    scheduleWordCount();
    updateDirtyStateForText(text);

    return true;
}

void Backend::updateDirtyStateForText(const QString &text) {
    const QString &baseline = m_lastKnownFileText;
    // An empty baseline means a pristine untitled document, but only when there
    // is no file behind it. Once there is one, an unknown baseline is unknown
    // rather than empty, and emptying the editor is a change like any other.
    const bool baselineKnown = m_hasKnownFileContents
        || !m_fileUrl.isValid() || m_fileUrl.isEmpty();

    if (baselineKnown && text == baseline) {
        setModified(false);
        clearRecovery();
        if (m_hasKnownFileContents)
            setStatus(QStringLiteral("Saved %1").arg(fileName()));
        else
            setStatus(QString());
    } else {
        setModified(true);
        setStatus(QStringLiteral("Unsaved"));
        scheduleRecovery();
        scheduleAutoSave();
    }
}

void Backend::setCaretPosition(int position) {
    m_caretPosition = position;
    if (m_highlighter)
        m_highlighter->setCaret(position);
}

// The emphasis span of the marker's kind that the caret sits in or against,
// in document positions, so a toggle with nothing selected can act on it.
QVariantMap Backend::emphasisSpanAt(int position, const QString &marker) const {
    if (!m_document)
        return {};

    const QTextBlock block =
        m_document->findBlock(qBound(0, position, m_document->characterCount() - 1));
    if (!block.isValid() || block.userState() >= MarkdownHighlighter::InsideFence
            || block.previous().userState() >= MarkdownHighlighter::InsideFence)
        return {};

    using Kind = MarkdownHighlighter::InlineKind;
    const Kind kind = marker == QStringLiteral("**") ? Kind::Bold
        : marker == QStringLiteral("~~")              ? Kind::Strikethrough
                                                      : Kind::Italic;
    const int offset = position - block.position();
    const QList<MarkdownHighlighter::InlineMarkup> markup =
        MarkdownHighlighter::inlineMarkup(block.text());
    for (const auto &item : markup) {
        const int start = item.markers[0].start;
        const int end = item.markers[1].start + item.markers[1].length;
        if (item.kind != kind || offset < start || offset > end)
            continue;
        // The visible text ends before the chain of nested emphasis closing
        // with it, as the italic "*" inside "***word***".
        const int contentEnd = item.content.start + item.content.length;
        int textEnd = contentEnd;
        for (bool moved = true; moved;) {
            moved = false;
            for (const auto &nested : markup) {
                const auto &closing = nested.markers[1];
                if (closing.start >= item.content.start && closing.length > 0
                        && closing.start + closing.length == textEnd) {
                    textEnd = closing.start;
                    moved = true;
                }
            }
        }
        const int base = block.position();
        return {{QStringLiteral("start"), base + start},
                {QStringLiteral("end"), base + end},
                {QStringLiteral("contentStart"), base + item.content.start},
                {QStringLiteral("contentEnd"), base + contentEnd},
                {QStringLiteral("textEnd"), base + textEnd}};
    }
    return {};
}

QString Backend::linkUrlAt(int position) const {
    if (!m_document)
        return {};

    const QTextBlock block =
        m_document->findBlock(qBound(0, position, m_document->characterCount() - 1));
    if (!block.isValid())
        return {};

    // Fenced code is literal, so nothing in it is a link.
    if (block.userState() >= MarkdownHighlighter::InsideFence
            || block.previous().userState() >= MarkdownHighlighter::InsideFence)
        return {};

    return MarkdownHighlighter::linkUrlAt(block.text(), position - block.position());
}

int Backend::cursorPositionStep(int position, int direction) const
{
    if (!m_document || direction == 0)
        return position;

    const int documentLength = m_document->characterCount() - 1;
    position = qBound(0, position, documentLength);

    if (direction > 0) {
        if (position >= documentLength)
            return documentLength;
    } else if (position <= 0) {
        return 0;
    }

    const QTextBlock block = m_document->findBlock(position);
    if (!block.isValid())
        return position;

    const int blockStart = block.position();
    const int blockTextLength = block.text().size();
    // A block's text excludes its paragraph separator, so this is the last
    // cursor position inside the block, just before the separator.
    const int blockEnd = blockStart + blockTextLength;

    if (direction > 0) {
        // The separator is the next valid cursor position past a block; the
        // layout only knows the block's own characters.
        if (position >= blockEnd)
            return position + 1;
    } else if (position <= blockStart) {
        return position - 1;
    }

    // A fresh layout over the block's own text is always in sync with the
    // document, unlike the document's display layout, whose text() cannot be
    // checked for staleness. Building lines is unnecessary for cursor
    // stepping and dominates the cost, so the layout is left unformatted.
    // QTextLayout steps by whole grapheme clusters, so surrogate pairs, skin
    // tone modifiers and ZWJ sequences are crossed in a single move.
    QTextLayout layout(block.text());
    const int local = position - blockStart;
    const int stepped = direction > 0
        ? layout.nextCursorPosition(local, QTextLayout::SkipCharacters)
        : layout.previousCursorPosition(local, QTextLayout::SkipCharacters);
    return blockStart + qBound(0, stepped, blockTextLength);
}

void Backend::setSearchHighlight(const QString &query, int currentMatchStart) {
    if (m_highlighter)
        m_highlighter->setSearch(query, currentMatchStart);
}

// Found by the highlighter's own matcher, so the offsets agree with what it
// paints, diacritics and all.
QVariantList Backend::searchMatches(const QString &query) const {
    QVariantList matches;
    if (query.isEmpty())
        return matches;
    for (const int start : MarkdownHighlighter::findMatches(currentDocumentText(), query))
        matches.append(start);
    return matches;
}

// One edit command, so a single undo takes the whole replacement back.
void Backend::replaceRange(int start, int end, const QString &text) {
    if (!m_document)
        return;
    const int last = m_document->characterCount() - 1;
    QTextCursor cursor(m_document);
    cursor.beginEditBlock();
    cursor.setPosition(qBound(0, start, last));
    cursor.setPosition(qBound(0, end, last), QTextCursor::KeepAnchor);
    cursor.insertText(text);
    cursor.endEditBlock();
}

Backend::EditSnapshot Backend::currentSnapshot() const {
    EditSnapshot snapshot;
    snapshot.text = currentDocumentText();
    snapshot.state = m_writeCamState;
    snapshot.state.reconcile(snapshot.text);
    if (m_document) {
        const QTextCursor cursor(m_document);
        snapshot.position = cursor.position();
        snapshot.anchor = cursor.anchor();
    }
    return snapshot;
}

void Backend::resetHistory() {
    m_history.clear();
    m_historyIndex = -1;
    m_usingAlternativeHistory = false;
    m_applyingAlternative = false;
    m_activeAlternative = -1;
    const bool hadPointer = m_pointerActive;
    m_pointerAlternative = -1;
    m_pointerActive = false;
    if (hadPointer) {
        if (m_highlighter)
            m_highlighter->setPointerSpan({0, 0});
        emit pointerChanged();
    }
    if (!m_document)
        return;
    m_history.append(currentSnapshot());
    m_historyIndex = 0;
}

void Backend::recordTextHistory(const QString &beforeText, const QString &afterText) {
    if (m_loading || m_applyingAlternative || beforeText == afterText)
        return;
    if (m_history.isEmpty() || m_historyIndex < 0) {
        EditSnapshot before = currentSnapshot();
        before.text = beforeText;
        m_history.append(before);
        m_historyIndex = 0;
    }

    m_history.erase(m_history.begin() + m_historyIndex + 1, m_history.end());
    EditSnapshot after = currentSnapshot();
    after.text = afterText;
    m_history.append(after);
    m_historyIndex = int(m_history.size()) - 1;
}

void Backend::commitAlternativeChange(const EditSnapshot &before) {
    if (m_history.isEmpty() || m_historyIndex < 0) {
        m_history.append(before);
        m_historyIndex = 0;
    } else if (m_history.at(m_historyIndex).text != before.text) {
        m_history[m_historyIndex] = before;
    }

    m_history.erase(m_history.begin() + m_historyIndex + 1, m_history.end());
    m_history.append(currentSnapshot());
    m_historyIndex = int(m_history.size()) - 1;
    m_usingAlternativeHistory = true;
}

void Backend::updateCurrentSnapshot() {
    if (m_history.isEmpty() || m_historyIndex < 0) {
        resetHistory();
        m_usingAlternativeHistory = true;
        return;
    }

    m_history.erase(m_history.begin() + m_historyIndex + 1, m_history.end());
    m_history[m_historyIndex] = currentSnapshot();
    m_usingAlternativeHistory = true;
}

void Backend::refreshCurrentSnapshot() {
    if (m_history.isEmpty() || m_historyIndex < 0) {
        resetHistory();
        return;
    }
    m_history[m_historyIndex] = currentSnapshot();
}

void Backend::restoreSnapshot(const EditSnapshot &snapshot) {
    if (!m_document)
        return;

    m_loading = true;
    const bool undoEnabled = m_document->isUndoRedoEnabled();
    m_document->setUndoRedoEnabled(false);
    m_document->setPlainText(snapshot.text);
    m_writeCamState = snapshot.state;
    m_writeCamState.reconcile(snapshot.text);
    m_lastDocumentText = snapshot.text;
    applyDocumentTypography();
    m_document->setUndoRedoEnabled(undoEnabled);
    m_loading = false;

    const int last = qMax(0, m_document->characterCount() - 1);
    QTextCursor cursor(m_document);
    cursor.setPosition(qBound(0, snapshot.position, last));
    if (snapshot.anchor != snapshot.position)
        cursor.setPosition(qBound(0, snapshot.anchor, last), QTextCursor::KeepAnchor);
    m_activeAlternative = qBound(-1, m_activeAlternative,
                                 int(m_writeCamState.alternatives().size()) - 1);

    m_wordCountTimer.stop();
    m_wordCountStale = true;
    m_wordCountAgain = false;
    setWordCount(countWords(snapshot.text));
    updateDirtyStateForText(snapshot.text);
    emit alternativesChanged();
}

void Backend::applyShowingChoices(const QList<WriteCamAlternative> &wanted) {
    if (!m_document || wanted.isEmpty())
        return;

    struct Replacement {
        int start = 0;
        int length = 0;
        QString wording;
        int showing = 0;
    };
    QList<Replacement> replacements;
    QList<WriteCamAlternative> &alternatives = m_writeCamState.alternatives();
    QList<bool> used(alternatives.size(), false);
    for (const WriteCamAlternative &choice : wanted) {
        for (int index = 0; index < alternatives.size(); ++index) {
            if (used.at(index) || alternatives.at(index).versions != choice.versions)
                continue;
            used[index] = true;
            const WriteCamAlternative &alternative = alternatives.at(index);
            if (choice.showing >= 0 && choice.showing < choice.versions.size()
                    && choice.showing != alternative.showing) {
                replacements.append(Replacement{
                    alternative.start,
                    alternative.length,
                    choice.versions.at(choice.showing),
                    choice.showing,
                });
            }
            break;
        }
    }
    if (replacements.isEmpty())
        return;

    std::sort(replacements.begin(), replacements.end(),
              [](const Replacement &left, const Replacement &right) {
                  return left.start > right.start;
              });

    m_loading = true;
    for (const Replacement &replacement : std::as_const(replacements))
        replaceRange(replacement.start, replacement.start + replacement.length,
                     replacement.wording);
    m_loading = false;

    m_lastDocumentText = currentDocumentText();
    m_writeCamState.reconcile(m_lastDocumentText);
    m_wordCountTimer.stop();
    m_wordCountStale = true;
    m_wordCountAgain = false;
    setWordCount(countWords(m_lastDocumentText));
    updateDirtyStateForText(m_lastDocumentText);
    emit alternativesChanged();
}

void Backend::undo() {
    if (!m_document)
        return;
    if (!m_usingAlternativeHistory) {
        m_document->undo();
        return;
    }
    if (m_historyIndex <= 0)
        return;
    const QList<WriteCamAlternative> choices = m_writeCamState.alternatives();
    --m_historyIndex;
    restoreSnapshot(m_history.at(m_historyIndex));
    applyShowingChoices(choices);
    m_history[m_historyIndex] = currentSnapshot();
}

void Backend::redo() {
    if (!m_document)
        return;
    if (!m_usingAlternativeHistory) {
        m_document->redo();
        return;
    }
    if (m_historyIndex + 1 >= int(m_history.size()))
        return;
    const QList<WriteCamAlternative> choices = m_writeCamState.alternatives();
    ++m_historyIndex;
    restoreSnapshot(m_history.at(m_historyIndex));
    applyShowingChoices(choices);
    m_history[m_historyIndex] = currentSnapshot();
}

QVariantList Backend::alternativeList() const {
    QVariantList rows;
    const QList<WriteCamAlternative> &alternatives = m_writeCamState.alternatives();
    for (int index = 0; index < alternatives.size(); ++index) {
        const WriteCamAlternative &alternative = alternatives.at(index);
        rows.append(QVariantMap{
            {QStringLiteral("index"), index},
            {QStringLiteral("start"), alternative.start},
            {QStringLiteral("length"), alternative.length},
            {QStringLiteral("scope"), WriteCamState::scopeName(alternative.scope)},
            {QStringLiteral("versions"), alternative.versions},
            {QStringLiteral("showing"), alternative.showing},
            {QStringLiteral("active"), index == m_activeAlternative},
        });
    }
    return rows;
}

int Backend::alternativeIndexAt(int position) const {
    const QList<WriteCamAlternative> &alternatives = m_writeCamState.alternatives();
    for (int index = 0; index < alternatives.size(); ++index) {
        const WriteCamAlternative &alternative = alternatives.at(index);
        if (position >= alternative.start
                && position < alternative.start + alternative.length)
            return index;
    }
    return -1;
}

QVariantMap Backend::alternativeAt(int position) const {
    const int index = alternativeIndexAt(position);
    if (index < 0)
        return {};
    return alternativeList().at(index).toMap();
}

bool Backend::hasAlternativeAt(int position) const {
    return alternativeIndexAt(position) >= 0;
}

bool Backend::makeAlternative() {
    return makeAlternative(m_caretPosition, m_caretPosition);
}

bool Backend::makeAlternative(int position) {
    return makeAlternative(position, position);
}

bool Backend::makeAlternative(int start, int end) {
    if (!m_document)
        return false;

    const QString text = currentDocumentText();
    const AlternativeRange range = WriteCamState::alternativeRange(text, start, end);
    if (range.length <= 0)
        return false;

    const QString wording = text.mid(range.start, range.length);
    if (wording.trimmed().isEmpty())
        return false;

    const EditSnapshot before = currentSnapshot();

    // Groups do not nest. A larger group swallows the smaller ones it covers,
    // and a selection inside an existing group simply reaches for that group.
    QList<WriteCamAlternative> &alternatives = m_writeCamState.alternatives();
    for (int index = alternatives.size() - 1; index >= 0; --index) {
        const WriteCamAlternative &alternative = alternatives.at(index);
        const int alternativeEnd = alternative.start + alternative.length;
        const int rangeEnd = range.start + range.length;
        const bool contains = alternative.start <= range.start && alternativeEnd >= rangeEnd;
        if (contains)
            return false;
        if (alternative.start < rangeEnd && alternativeEnd > range.start) {
            if (alternative.start >= range.start && alternativeEnd <= rangeEnd)
                alternatives.removeAt(index);
            else
                return false;
        }
    }

    WriteCamAlternative alternative;
    alternative.start = range.start;
    alternative.length = range.length;
    alternative.versions = {wording};
    alternative.showing = 0;
    alternative.scope = range.scope;
    alternatives.append(alternative);
    std::sort(alternatives.begin(), alternatives.end(),
              [](const WriteCamAlternative &left, const WriteCamAlternative &right) {
                  return left.start < right.start;
              });
    m_activeAlternative = int(alternatives.size()) - 1;
    for (int index = 0; index < alternatives.size(); ++index) {
        if (alternatives.at(index).start == range.start)
            m_activeAlternative = index;
    }

    commitAlternativeChange(before);
    emit alternativesChanged();
    return true;
}

bool Backend::addAlternative() {
    return makeAlternative();
}

bool Backend::addAlternative(int position) {
    return makeAlternative(position);
}

bool Backend::addAlternative(int start, int end) {
    return makeAlternative(start, end);
}

bool Backend::createAlternative() {
    return makeAlternative();
}

bool Backend::createAlternative(int position) {
    return makeAlternative(position);
}

bool Backend::createAlternative(int start, int end) {
    return makeAlternative(start, end);
}

bool Backend::newAlternative() {
    return makeAlternative();
}

bool Backend::newAlternative(int start, int end) {
    return makeAlternative(start, end);
}

bool Backend::makeAlternativeAt(int position) {
    return makeAlternative(position);
}

bool Backend::makeAlternativeAt(int start, int end) {
    return makeAlternative(start, end);
}

bool Backend::createAlternativeAt(int position) {
    return makeAlternative(position);
}

bool Backend::createAlternativeAt(int start, int end) {
    return makeAlternative(start, end);
}

bool Backend::flipAlternativeAt(int position, int direction) {
    return flipAlternativeByIndex(alternativeIndexAt(position), direction);
}

bool Backend::cycleAlternative(int position, int direction) {
    return flipAlternativeAt(position, direction);
}

bool Backend::flipAlternativeByIndex(int alternativeIndex, int direction) {
    if (!m_document || direction == 0)
        return false;
    QList<WriteCamAlternative> &alternatives = m_writeCamState.alternatives();
    if (alternativeIndex < 0 || alternativeIndex >= alternatives.size())
        return false;
    WriteCamAlternative &alternative = alternatives[alternativeIndex];
    if (alternative.versions.size() < 2)
        return false;

    const int count = int(alternative.versions.size());
    const int showing = (alternative.showing + direction % count + count) % count;
    if (showing == alternative.showing)
        return false;

    const QString wording = alternative.versions.at(showing);
    const int start = alternative.start;
    m_applyingAlternative = true;
    replaceRange(start, start + alternative.length, wording);
    m_applyingAlternative = false;
    m_lastDocumentText = currentDocumentText();

    alternative.start = start;
    alternative.length = wording.size();
    alternative.showing = showing;
    m_activeAlternative = alternativeIndex;
    updateCurrentSnapshot();
    emit alternativesChanged();
    return true;
}

bool Backend::flipAlternative(int direction) {
    int index = m_activeAlternative;
    if (index < 0)
        index = alternativeIndexAt(m_caretPosition);
    return flipAlternativeByIndex(index, direction);
}

void Backend::setPointerAlternative(int alternativeIndex) {
    const QList<WriteCamAlternative> &alternatives = m_writeCamState.alternatives();
    const bool active = alternativeIndex >= 0 && alternativeIndex < alternatives.size();
    const int index = active ? alternativeIndex : -1;
    if (index == m_pointerAlternative && active == m_pointerActive)
        return;

    m_pointerAlternative = index;
    m_pointerActive = active;
    if (m_highlighter) {
        if (active) {
            const WriteCamAlternative &alternative = alternatives.at(index);
            m_highlighter->setPointerSpan({alternative.start, alternative.length});
        } else {
            m_highlighter->setPointerSpan({0, 0});
        }
    }
    emit pointerChanged();
}

void Backend::pointAt(int position) {
    setPointerAlternative(alternativeIndexAt(position));
}

void Backend::releasePointer() {
    setPointerAlternative(-1);
}

bool Backend::flipPointedAlternative(int direction) {
    if (!m_pointerActive || m_pointerAlternative < 0 || direction == 0)
        return false;
    return flipAlternativeByIndex(m_pointerAlternative, direction);
}

bool Backend::flipAlternative(int position, int direction) {
    return flipAlternativeByIndex(alternativeIndexAt(position), direction);
}

bool Backend::nextAlternative(int position) {
    return flipAlternative(position, 1);
}

bool Backend::previousAlternative(int position) {
    return flipAlternative(position, -1);
}

bool Backend::nextAlternative() {
    return flipAlternative(1);
}

bool Backend::previousAlternative() {
    return flipAlternative(-1);
}

bool Backend::showAlternative(int versionIndex) {
    return showAlternativeVersion(versionIndex);
}

bool Backend::showAlternative(int alternativeIndex, int versionIndex) {
    return showAlternativeVersion(alternativeIndex, versionIndex);
}

bool Backend::setAlternativeShowing(int versionIndex) {
    return showAlternativeVersion(versionIndex);
}

bool Backend::setAlternativeShowing(int alternativeIndex, int versionIndex) {
    return showAlternativeVersion(alternativeIndex, versionIndex);
}

bool Backend::showAlternativeVersion(int versionIndex) {
    int index = m_activeAlternative;
    if (index < 0)
        index = alternativeIndexAt(m_caretPosition);
    return showAlternativeVersion(index, versionIndex);
}

bool Backend::showAlternativeVersion(int alternativeIndex, int versionIndex) {
    if (!m_document)
        return false;
    QList<WriteCamAlternative> &alternatives = m_writeCamState.alternatives();
    if (alternativeIndex < 0 || alternativeIndex >= alternatives.size())
        return false;
    WriteCamAlternative &alternative = alternatives[alternativeIndex];
    if (versionIndex < 0 || versionIndex >= alternative.versions.size())
        return false;
    if (versionIndex == alternative.showing)
        return false;

    const QString wording = alternative.versions.at(versionIndex);
    const int start = alternative.start;
    m_applyingAlternative = true;
    replaceRange(start, start + alternative.length, wording);
    m_applyingAlternative = false;
    m_lastDocumentText = currentDocumentText();

    alternative.start = start;
    alternative.length = wording.size();
    alternative.showing = versionIndex;
    m_activeAlternative = alternativeIndex;
    updateCurrentSnapshot();
    emit alternativesChanged();
    return true;
}

bool Backend::addAlternativeVersion(const QString &version) {
    int index = m_activeAlternative;
    if (index < 0)
        index = alternativeIndexAt(m_caretPosition);
    return addAlternativeVersion(index, version);
}

bool Backend::addAlternativeVersion(int alternativeIndex, const QString &version) {
    if (version.isEmpty())
        return false;
    QList<WriteCamAlternative> &alternatives = m_writeCamState.alternatives();
    if (alternativeIndex < 0 || alternativeIndex >= alternatives.size())
        return false;
    if (alternatives.at(alternativeIndex).versions.contains(version))
        return false;

    const EditSnapshot before = currentSnapshot();
    alternatives[alternativeIndex].versions.append(version);
    m_activeAlternative = alternativeIndex;
    commitAlternativeChange(before);
    emit alternativesChanged();
    return true;
}

bool Backend::removeAlternativeVersion(int versionIndex) {
    int index = m_activeAlternative;
    if (index < 0)
        index = alternativeIndexAt(m_caretPosition);
    return removeAlternativeVersion(index, versionIndex);
}

bool Backend::removeAlternativeVersion(int alternativeIndex, int versionIndex) {
    if (!m_document)
        return false;
    QList<WriteCamAlternative> &alternatives = m_writeCamState.alternatives();
    if (alternativeIndex < 0 || alternativeIndex >= alternatives.size())
        return false;
    WriteCamAlternative &alternative = alternatives[alternativeIndex];
    if (alternative.versions.size() <= 1
            || versionIndex < 0 || versionIndex >= alternative.versions.size())
        return false;

    const EditSnapshot before = currentSnapshot();
    const bool wasShowing = versionIndex == alternative.showing;
    alternative.versions.removeAt(versionIndex);
    if (alternative.showing > versionIndex)
        --alternative.showing;
    alternative.showing = qBound(0, alternative.showing,
                                 int(alternative.versions.size()) - 1);

    if (wasShowing) {
        const QString wording = alternative.versions.at(alternative.showing);
        const int start = alternative.start;
        m_applyingAlternative = true;
        replaceRange(start, start + alternative.length, wording);
        m_applyingAlternative = false;
        m_lastDocumentText = currentDocumentText();
        alternative.start = start;
        alternative.length = wording.size();
    }

    m_activeAlternative = alternativeIndex;
    commitAlternativeChange(before);
    emit alternativesChanged();
    return true;
}

// A block's state says what follows its line, so a fence line itself is only
// code on its inner side: past an opening fence's backticks or tildes, or anywhere
// before the end of a closing one.
bool Backend::insideFence(int position) const {
    if (!m_document)
        return false;
    const QTextBlock block =
        m_document->findBlock(qBound(0, position, m_document->characterCount() - 1));
    if (!block.isValid())
        return false;
    const bool opensOrContinues = block.userState() >= MarkdownHighlighter::InsideFence;
    const bool afterOpening = block.previous().userState() >= MarkdownHighlighter::InsideFence;
    const int offset = position - block.position();
    if (opensOrContinues && !afterOpening) {
        static const QRegularExpression fenceMark(QStringLiteral("[`~]"));
        return offset >= block.text().indexOf(fenceMark) + 3;
    }
    if (afterOpening && !opensOrContinues)
        return offset < block.text().length();
    return opensOrContinues;
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
    m_wordCountStale = true;
    m_wordCountAgain = false;
    setWordCount(countWords(text));
    resetHistory();
    emit alternativesChanged();
}

void Backend::setFileUrl(const QUrl &url) {
    if (m_fileUrl == url)
        return;

    m_fileUrl = url;
    emit fileUrlChanged();
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
        setStatus(QStringLiteral("Only local files can be saved."));
        emit saveFailed();
        return;
    }

    const QString targetName = QFileInfo(url.toLocalFile()).fileName();
    QSaveFile file(url.toLocalFile());
    // Without a temporary file beside it, as in a folder only the file itself
    // may be written in, write the file in place rather than fail.
    file.setDirectWriteFallback(true);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        setStatus(QStringLiteral("Could not save %1.").arg(targetName));
        emit saveFailed();
        return;
    }

    // A document saved for the first time gets front matter; one that already
    // tracks `updated` gets the save's timestamp appended to its log. The
    // buffer is updated too, so the editor always matches the file on disk.
    // Tags are decided after the file is written, off the save's path.
    const bool creatingNewFile = !m_fileUrl.isValid() || m_fileUrl.isEmpty();
    const bool documentChanged = creatingNewFile || m_modified;
    m_lastTagNote.clear();
    updateFrontMatterOnSave(creatingNewFile, QFileInfo(targetName).completeBaseName(), {});

    // The editor holds the piece itself; the alternatives and ghosts follow it
    // in the one trailing comment, and nothing is appended when there are none.
    const QString body = currentDocumentText();
    m_writeCamState.reconcile(body);
    updateAlternativeMarks();
    const QByteArray contents = WriteCamState::withComment(body, m_writeCamState).toUtf8();
    file.write(contents);

    // QSaveFile commits by replacing the target. Stop watching the old inode
    // before that replacement so our own write is not classified as external.
    unwatchFiles();

    // commit() flushes, fsyncs, and atomically renames the temp file into place,
    // returning false (and leaving the original untouched) on any write error.
    if (!file.commit()) {
        watchCurrentFile();
        setStatus(QStringLiteral("Could not write %1.").arg(targetName));
        emit saveFailed();
        return;
    }

    setKnownFileContents(contents, true);
    m_autoSaveSuspended = false;
    m_autoSaveTimer.stop();
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

    if (documentChanged)
        tagDocument();
}

void Backend::scheduleRecovery() {
    m_recoveryTimer.start();
}

void Backend::scheduleAutoSave() {
    // Only auto-save documents with a file name. Untitled drafts are left to
    // manual save so auto-save never silently opens a file picker or creates a
    // file the writer did not name; crash recovery still covers them. A named
    // file that does not exist yet waits for an explicit save (see autoSave()).
    if (m_fileUrl.isLocalFile() && !m_autoSaveSuspended && !m_autoSaveHeld)
        m_autoSaveTimer.start();
}

void Backend::holdAutoSave(bool held) {
    m_autoSaveHeld = held;
    if (held)
        m_autoSaveTimer.stop();
    else if (m_modified)
        scheduleAutoSave();
}

void Backend::autoSave() {
    if (!m_modified || !m_fileUrl.isLocalFile() || m_autoSaveSuspended || m_autoSaveHeld)
        return;

    // Never write over a version we have not seen. The watcher normally
    // catches outside edits first, but it can miss some (or fire late), so
    // check the file itself right before replacing it.
    const bool exists = QFileInfo::exists(m_fileUrl.toLocalFile());
    if (!m_hasKnownFileContents) {
        // A removed file, a new one not yet saved, or a recovered draft whose
        // file was gone waits for an explicit save rather than being created.
        suspendAutoSave();
        if (exists)
            emit externalChangeDetected(false, m_modified);
        return;
    }
    if (!diskMatchesKnownContents()) {
        reportExternalChange(!exists);
        return;
    }

    saveTo(m_fileUrl);
}

void Backend::suspendAutoSave() {
    // Held until the writer reloads, keeps their version, or saves explicitly.
    m_autoSaveTimer.stop();
    m_autoSaveSuspended = true;
}

// Whether the file still holds what we last read or wrote, read as open()
// reads it so carriage returns compare equal.
bool Backend::diskMatchesKnownContents() const {
    if (!m_hasKnownFileContents)
        return false;
    QFile file(m_fileUrl.toLocalFile());
    return file.open(QIODevice::ReadOnly | QIODevice::Text)
        && file.readAll() == m_lastKnownFileContents;
}

void Backend::reportExternalChange(bool deleted) {
    // Whatever is on disk now, it is not what we last read, so the baseline is
    // unknown until the writer picks a version. A deletion counts: the old
    // text is not saved anywhere either.
    setKnownFileContents(QByteArray(), false);
    suspendAutoSave();
    emit externalChangeDetected(deleted, m_modified);
}

// Saving writes UTF-8 with plain line endings; anything else on disk would be
// rewritten by it.
bool Backend::savesUnchanged(const QByteArray &raw) {
    return !raw.contains('\r') && QString::fromUtf8(raw).toUtf8() == raw;
}

void Backend::setKnownFileContents(const QByteArray &contents, bool known) {
    m_lastKnownFileContents = contents;
    m_hasKnownFileContents = known;
    // Every caller reads in text mode or writes plain line endings, so there
    // are no carriage returns to strip. The baseline is the piece itself, so
    // the trailing writecam comment never counts as an unsaved edit.
    m_lastKnownFileText = known
        ? WriteCamState::bodyOf(QString::fromUtf8(contents)) : QString();
}

void Backend::writeRecovery() {
    if (!m_modified)
        return;
    const QString path = m_recoveryPath;
    if (path.isEmpty())
        return;
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return;
    const QString text = currentDocumentText();
    WriteCamState state = m_writeCamState;
    state.reconcile(text);
    QJsonObject recovery{{QStringLiteral("fileUrl"), m_fileUrl.toString()},
                         {QStringLiteral("text"), text}};
    if (!state.isEmpty())
        recovery.insert(QStringLiteral("writecam"), state.toJson());
    file.write(QJsonDocument(recovery).toJson(QJsonDocument::Compact));
    file.commit();
}

void Backend::restoreRecovery() {
    QFile file(m_recoveryPath);
    if (!file.open(QIODevice::ReadOnly))
        return;
    const QJsonDocument json = QJsonDocument::fromJson(file.readAll());
    if (!json.isObject() || !json.object().contains(QStringLiteral("text"))) {
        // Unreadable now means unreadable every launch; do not keep claiming it.
        file.remove();
        return;
    }
    const QJsonObject recovery = json.object();
    const QString text = recovery.value(QStringLiteral("text")).toString();
    m_writeCamState = WriteCamState::fromJson(
        recovery.value(QStringLiteral("writecam")).toObject(), text);
    loadDocumentText(text);
    const QUrl recoveredUrl(recovery.value(QStringLiteral("fileUrl")).toString());
    QFile diskFile(recoveredUrl.toLocalFile());
    if (recoveredUrl.isLocalFile() && diskFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        setKnownFileContents(diskFile.readAll(), true);
    } else {
        setKnownFileContents(QByteArray(), false);
    }
    setFileUrl(recoveredUrl);
    // The file may have moved on since this draft was written, and the draft
    // alone cannot tell. Leave overwriting it to an explicit save.
    m_autoSaveSuspended = recoveredUrl.isLocalFile();
    watchCurrentFile();
    setModified(true);
    setStatus(QStringLiteral("Recovered unsaved changes"));
}

void Backend::clearRecovery() {
    m_recoveryTimer.stop();
    QFile::remove(m_recoveryPath);
}

void Backend::unwatchFiles() {
    m_fileCheckTimer.stop();
    const QStringList watched = m_fileWatcher.files() + m_fileWatcher.directories();
    if (!watched.isEmpty())
        m_fileWatcher.removePaths(watched);
}

// A missing file cannot be watched, so watch its folder instead to notice it
// coming back.
void Backend::watchCurrentFile() {
    unwatchFiles();
    if (!m_fileUrl.isLocalFile())
        return;
    const QFileInfo info(m_fileUrl.toLocalFile());
    m_fileMissing = !info.exists();
    if (info.exists())
        m_fileWatcher.addPath(info.filePath());
    else if (info.dir().exists())
        m_fileWatcher.addPath(info.dir().path());
}

void Backend::checkWatchedFile() {
    // Atomic saves replace the watched inode, which drops it from the
    // watcher, so re-arm it either way. Our own save is not an outside edit.
    const bool wasMissing = m_fileMissing;
    watchCurrentFile();
    if (diskMatchesKnownContents())
        return;
    // A file that was already missing has been reported, or was never there.
    // One that turns up again is an outside edit like any other, even while
    // the writer is still looking at the removal.
    if (m_fileMissing && wasMissing)
        return;
    reportExternalChange(m_fileMissing);
}

// A quoted TOML value ends at its closing quote; whatever trails it is an inline comment.
QString Backend::tomlValue(const QString &text) {
    const QString value = text.trimmed();
    if (value.isEmpty())
        return value;

    const QChar quote = value.front();
    if (quote != QLatin1Char('"') && quote != QLatin1Char('\''))
        return value;

    const int close = value.indexOf(quote, 1);
    return close < 0 ? value : value.mid(1, close - 1);
}

void Backend::loadOmarchyTheme() {
    const QString colorsPath = QDir::homePath()
        + QStringLiteral("/.local/state/omarchy/current/theme/colors.toml");
    QString themeMode;
    QString background;
    QString foreground;
    QString accent;
    QString selection;
    QString selectionBackground;
    QString selectionForeground;
    QString brightForeground;
    QString lighterBackground;
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
            const QString value = tomlValue(line.mid(equals + 1));

            if (key == QStringLiteral("mode"))
                themeMode = value;
            else if (key == QStringLiteral("background"))
                background = value;
            else if (key == QStringLiteral("foreground"))
                foreground = value;
            else if (key == QStringLiteral("accent"))
                accent = value;
            else if (key == QStringLiteral("selection"))
                selection = value;
            else if (key == QStringLiteral("selection_background"))
                selectionBackground = value;
            else if (key == QStringLiteral("selection_foreground"))
                selectionForeground = value;
            else if (key == QStringLiteral("bright_foreground"))
                brightForeground = value;
            else if (key == QStringLiteral("lighter_background"))
                lighterBackground = value;
        }
    }

    const bool wasDark = m_darkMode;
    const QStringList previousColors{m_themeBackground, m_themeForeground, m_themeAccent,
                                     m_themeSelection, m_themeSelectionForeground,
                                     m_themeCodeBackground};

    bool themeIsDark = m_systemDarkMode;
    if (themeMode == QStringLiteral("dark")) {
        themeIsDark = true;
    } else if (themeMode == QStringLiteral("light")) {
        themeIsDark = false;
    } else {
        const QColor parsedBackground(background);
        if (parsedBackground.isValid()) {
            const double luminance = 0.299 * parsedBackground.redF()
                + 0.587 * parsedBackground.greenF() + 0.114 * parsedBackground.blueF();
            themeIsDark = luminance < 0.5;
        }
    }
    m_darkMode = themeIsDark;

    // The defaults follow the mode the theme resolved to, so a colour we cannot
    // read costs that one colour rather than the contrast of the whole editor.
    m_themeBackground = m_darkMode ? QStringLiteral("#101010") : QStringLiteral("#ffffff");
    m_themeForeground = m_darkMode ? QStringLiteral("#eeeeee") : QStringLiteral("#222324");
    m_themeAccent = m_darkMode ? QStringLiteral("#5584aa") : QStringLiteral("#2077b2");
    m_themeSelection = m_darkMode ? QStringLiteral("#186a9a") : QStringLiteral("#2077b2");

    // An unparseable colour reaches QML as black, so keep the default instead.
    // Later calls win, so each fallback is applied before what it falls back from.
    const auto applyColor = [](QString &target, const QString &value) {
        if (QColor(value).isValid())
            target = value;
    };
    applyColor(m_themeBackground, background);
    applyColor(m_themeForeground, foreground);
    applyColor(m_themeAccent, accent);
    applyColor(m_themeSelection, selectionBackground);
    applyColor(m_themeSelection, selection);
    m_themeSelectionForeground = m_themeForeground;
    applyColor(m_themeSelectionForeground, brightForeground);
    applyColor(m_themeSelectionForeground, selectionForeground);

    // Omarchy themes name a lighter background for panels like the one code
    // sits on, so use the shade the theme chose. Not every theme sets the key,
    // and a few set it to the page background, which would leave code with no
    // panel at all: mix a shade of the page towards the text for those. Mixing,
    // unlike lightening, still moves on a pure black background.
    const QColor page(m_themeBackground);
    const QColor lighter(lighterBackground);
    m_themeCodeBackground = (lighter.isValid() && lighter != page
                                 ? lighter
                                 : blend(page, QColor(m_themeForeground), 0.06)).name();

    if (m_highlighter) {
        m_highlighter->setTheme(m_darkMode, m_themeBackground, m_themeForeground,
                                m_themeAccent, m_themeCodeBackground);
    }

    if (m_darkMode != wasDark)
        emit darkModeChanged();
    const QStringList colors{m_themeBackground, m_themeForeground, m_themeAccent,
                             m_themeSelection, m_themeSelectionForeground,
                             m_themeCodeBackground};
    if (colors != previousColors)
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
    // Only letters and numbers: PCRE2 reads \p{Han} as Script_Extensions, which
    // puts 。 、 「 and the CJK radicals in Han and would count each as a word.
    // One per thread, as loading a file counts here while a worker may too.
    thread_local const QRegularExpression cjkRe(
        QStringLiteral("(?=[\\p{L}\\p{N}])[\\p{Han}\\p{Hiragana}\\p{Katakana}]"));
    thread_local const QRegularExpression wordRe(
        QStringLiteral("[\\p{L}\\p{N}]+(?:['-][\\p{L}\\p{N}]+)*"));

    // Count unspaced Han and kana characters individually, then replace them
    // so they remain boundaries between surrounding space-delimited words.
    int count = 0;

    QRegularExpressionMatchIterator cjkIt = cjkRe.globalMatch(text);
    while (cjkIt.hasNext()) {
        cjkIt.next();
        ++count;
    }

    QString nonCjkText = text;
    nonCjkText.replace(cjkRe, QStringLiteral(" "));

    QRegularExpressionMatchIterator it = wordRe.globalMatch(nonCjkText);
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
    QString slug = slugForTitle(name);
    static const QRegularExpression alreadyDated(QStringLiteral("^\\d{4}-\\d{2}-\\d{2}(?:-|$)"));
    if (!alreadyDated.match(slug).hasMatch())
        slug.prepend(today.toString(Qt::ISODate) + QLatin1Char('-'));
    if (!slug.endsWith(QStringLiteral(".md")))
        slug += QStringLiteral(".md");
    return slug;
}

QString Backend::slugForTitle(const QString &title) {
    QString slug = title.toLower();
    static const QRegularExpression nonSlugChars(QStringLiteral("[^\\p{L}\\p{N}]+"));
    slug.replace(nonSlugChars, QStringLiteral("-"));
    static const QRegularExpression edgeHyphens(QStringLiteral("^-+|-+$"));
    slug.remove(edgeHyphens);
    slug = slug.left(120);
    if (slug.isEmpty())
        slug = QStringLiteral("untitled");
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
        + QStringLiteral("slug: ") + yamlScalar(slugForTitle(title)) + QStringLiteral("\n")
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

// Asks the configured decision server about the body that was just saved.
// The request runs on a worker thread, so the save has already completed and
// a slow or missing server never holds it up. Runs at most once per distinct
// body: unchanged text keeps the tags it already carries, so manual tag edits
// survive later saves.
void Backend::tagDocument() {
    if (!QSettings().value(taggingEnabledSetting, false).toBool())
        return;

    const QString text = currentDocumentText();
    if (!findFrontMatter(text).found)
        return;  // plain files are never modified, so there is nowhere to write

    const QString body = bodyAfterFrontMatter(text).trimmed();
    if (body.isEmpty())
        return;

    QString error;
    int stateChars = 4000;
    const QList<TagQuestion> questions = SystemOne::loadSchema(tagSchemaPath(), &error,
                                                                &stateChars);
    if (questions.isEmpty()) {
        m_lastTagNote = error;
        if (m_status.startsWith(QStringLiteral("Saved ")))
            setStatus(m_status + QStringLiteral(" · ") + m_lastTagNote);
        return;
    }

    const QString state = body.left(stateChars);
    const QString stateHash = QString::fromLatin1(QCryptographicHash::hash(
        state.toUtf8(), QCryptographicHash::Sha256).toHex());
    if (stateHash == m_lastTagBodyHash)
        return;  // this body already carries its decisions
    if (QDateTime::currentMSecsSinceEpoch() < m_tagSkipUntilMsecs)
        return;  // a server that just failed gets a couple of minutes' rest
    if (m_tagRequestActive) {
        if (stateHash != m_pendingTagBodyHash)
            m_tagAgain = true;  // ask again once the request in flight finishes
        return;
    }

    const DecisionServer server = loadDecisionServer();
    m_pendingTagState = state;
    m_pendingTagBodyHash = stateHash;
    m_pendingTagUrl = m_fileUrl;
    m_tagRequestActive = true;
    m_tagWatcher.setFuture(QtConcurrent::run([server, state, questions]() {
        TagDecision decision;
        decision.tags = SystemOne::decideTags(server, state, questions, &decision.error);
        return decision;
    }));
}

void Backend::finishTagRequest() {
    m_tagRequestActive = false;
    const TagDecision decision = m_tagWatcher.result();
    const auto askAgainIfNeeded = [this]() {
        if (!m_tagAgain)
            return;
        m_tagAgain = false;
        if (!m_modified)
            tagDocument();
    };

    if (!QSettings().value(taggingEnabledSetting, false).toBool()) {
        askAgainIfNeeded();
        return;
    }

    if (!decision.error.isEmpty()) {
        m_tagSkipUntilMsecs = QDateTime::currentMSecsSinceEpoch() + 120000;
        m_lastTagNote = QStringLiteral("tagger unreachable");
        if (m_status.startsWith(QStringLiteral("Saved ")))
            setStatus(m_status + QStringLiteral(" · ") + m_lastTagNote);
        askAgainIfNeeded();
        return;
    }

    // A decision for a document that moved on is dropped; the next save asks
    // again with whatever the writer is looking at now.
    if (!m_document || m_fileUrl != m_pendingTagUrl || m_modified
            || !diskMatchesKnownContents()) {
        askAgainIfNeeded();
        return;
    }

    const QString text = currentDocumentText();
    const QString body = bodyAfterFrontMatter(text).trimmed();
    if (!body.startsWith(m_pendingTagState)) {
        m_tagAgain = true;
        askAgainIfNeeded();
        return;
    }

    if (decision.tags.isEmpty()) {
        m_lastTagBodyHash = m_pendingTagBodyHash;
        askAgainIfNeeded();
        return;
    }

    QStringList added;
    const QString updated = applyFrontMatterTags(text, decision.tags, &added);
    if (added.isEmpty()) {
        m_lastTagBodyHash = m_pendingTagBodyHash;
        askAgainIfNeeded();
        return;
    }

    replaceDocumentText(text, updated);
    m_lastTagBodyHash = m_pendingTagBodyHash;

    // Write the tags without another `updated` log entry: this is the same
    // save finishing its optional extra, not a new revision. Keep the
    // alternatives comment that the save itself wrote.
    const QString savedBody = currentDocumentText();
    m_writeCamState.reconcile(savedBody);
    updateAlternativeMarks();
    const QByteArray contents = WriteCamState::withComment(savedBody, m_writeCamState).toUtf8();
    QSaveFile file(m_fileUrl.toLocalFile());
    file.setDirectWriteFallback(true);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        file.write(contents);
        unwatchFiles();
        if (file.commit()) {
            setKnownFileContents(contents, true);
            watchCurrentFile();
        } else {
            watchCurrentFile();
            setModified(true);
            scheduleAutoSave();
        }
    } else {
        setModified(true);
        scheduleAutoSave();
    }

    m_lastTagNote = QStringLiteral("tags: %1").arg(added.join(QStringLiteral(", ")));
    if (m_status.startsWith(QStringLiteral("Saved ")))
        setStatus(m_status + QStringLiteral(" · ") + m_lastTagNote);
    emit tagsApplied(added);
    askAgainIfNeeded();
}

// Replaces the document buffer with `to`, applying only the changed span so
// the writer's caret and scroll position stay put.
void Backend::replaceDocumentText(const QString &from, const QString &to) {
    if (!m_document || from == to)
        return;

    int prefix = 0;
    while (prefix < from.size() && prefix < to.size() && from.at(prefix) == to.at(prefix))
        ++prefix;
    int suffix = 0;
    while (suffix < from.size() - prefix && suffix < to.size() - prefix
           && from.at(from.size() - 1 - suffix) == to.at(to.size() - 1 - suffix))
        ++suffix;
    const QString replacement = to.mid(prefix, to.size() - prefix - suffix);

    // m_loading keeps the insertion from being reported as a fresh user edit
    // that would flip the just-saved document back to "Unsaved".
    m_loading = true;
    QTextCursor cursor(m_document);
    cursor.setPosition(prefix);
    cursor.setPosition(qMin(from.size() - suffix, m_document->characterCount() - 1),
                       QTextCursor::KeepAnchor);
    cursor.insertText(replacement);
    m_loading = false;

    m_lastDocumentText = m_document->toPlainText();
    m_lastChangePos = prefix;
    m_lastChangeAdded = replacement.size();
    reapplyTypographyToChange();
    m_wordCountTimer.stop();
    setWordCount(countWords(m_lastDocumentText));
    refreshCurrentSnapshot();
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

    replaceDocumentText(text, updated);
}

void Backend::setWordCount(int words) {
    if (m_wordCount == words)
        return;

    m_wordCount = words;
    emit wordCountChanged();
}

void Backend::refreshWordCount() {
    // One count at a time; edits made while it runs get one more after it.
    if (m_wordCountWatcher.isRunning()) {
        m_wordCountAgain = true;
        return;
    }
    // editorTextChanged keeps m_lastDocumentText current, and handing the
    // worker that shared copy saves reading the whole document out again.
    m_wordCountStale = false;
    m_wordCountWatcher.setFuture(QtConcurrent::run(&Backend::countWords, m_lastDocumentText));
}

void Backend::scheduleWordCount() {
    m_wordCountTimer.start();
}

void Backend::applyDocumentTypography() {
    if (!m_document)
        return;

    // A full pass is only used for freshly loaded/attached documents, so it is
    // safe to drop undo history here (re-enabling clears the stack anyway).
    const bool undoEnabled = m_document->isUndoRedoEnabled();
    m_document->setUndoRedoEnabled(false);

    m_formattingTypography = true;
    QTextCursor cursor(m_document);
    cursor.select(QTextCursor::Document);
    cursor.mergeBlockFormat(lineHeightFormat());
    m_formattingTypography = false;

    m_document->setUndoRedoEnabled(undoEnabled);
}

void Backend::reapplyTypographyToChange() {
    if (!m_document)
        return;

    // Format only the block(s) touched by the last edit instead of the whole
    // document, and fold the change into the preceding edit command so a single
    // undo reverts both the text and its formatting.
    const int maxPos = m_document->characterCount() - 1;
    const int start = qBound(0, m_lastChangePos, maxPos);
    const int end = qBound(start, m_lastChangePos + m_lastChangeAdded, maxPos);

    // Blocks that already have it are left alone: an undo brings their format
    // back with the text, and merging anyway would push a command that clears
    // the redo stack.
    bool needed = false;
    for (QTextBlock block = m_document->findBlock(start);
         block.isValid() && block.position() <= end; block = block.next()) {
        const QTextBlockFormat format = block.blockFormat();
        if (format.lineHeightType() != QTextBlockFormat::ProportionalHeight
                || format.lineHeight() != typoraLineHeightPercent) {
            needed = true;
            break;
        }
    }
    if (!needed)
        return;

    m_formattingTypography = true;
    QTextCursor cursor(m_document);
    cursor.joinPreviousEditBlock();
    cursor.setPosition(start);
    cursor.setPosition(end, QTextCursor::KeepAnchor);
    cursor.mergeBlockFormat(lineHeightFormat());
    cursor.endEditBlock();
    m_formattingTypography = false;
}
