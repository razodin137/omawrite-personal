#pragma once

#include <QDate>
#include <QDateTime>
#include <QObject>
#include <QPointer>
#include <QByteArray>
#include <QFileSystemWatcher>
#include <QFutureWatcher>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>
#include <memory>

#include "writecamstate.h"

class MarkdownHighlighter;
class QFont;
class QTextDocument;
class QWindow;
class QLockFile;

class Backend : public QObject {
    Q_OBJECT
    Q_PROPERTY(QUrl fileUrl READ fileUrl NOTIFY fileUrlChanged)
    Q_PROPERTY(QString fileName READ fileName NOTIFY fileUrlChanged)
    Q_PROPERTY(bool modified READ modified NOTIFY modifiedChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(int wordCount READ wordCount NOTIFY wordCountChanged)
    Q_PROPERTY(bool darkMode READ darkMode WRITE setDarkMode NOTIFY darkModeChanged)
    Q_PROPERTY(qreal textScale READ textScale NOTIFY textScaleChanged)
    Q_PROPERTY(QString themeBackground READ themeBackground NOTIFY themeColorsChanged)
    Q_PROPERTY(QString themeForeground READ themeForeground NOTIFY themeColorsChanged)
    Q_PROPERTY(QString themeAccent READ themeAccent NOTIFY themeColorsChanged)
    Q_PROPERTY(QString themeSelection READ themeSelection NOTIFY themeColorsChanged)
    Q_PROPERTY(QString themeSelectionForeground READ themeSelectionForeground NOTIFY themeColorsChanged)
    Q_PROPERTY(QString editorFont READ editorFont WRITE setEditorFont NOTIFY editorFontChanged)
    Q_PROPERTY(QStringList availableFonts READ availableFonts CONSTANT)
    Q_PROPERTY(QVariantList alternatives READ alternativeList NOTIFY alternativesChanged)
    Q_PROPERTY(int activeAlternative READ activeAlternative NOTIFY alternativesChanged)
    Q_PROPERTY(QStringList activeAlternativeVersions READ activeAlternativeVersions
               NOTIFY alternativesChanged)
    Q_PROPERTY(int activeAlternativeShowing READ activeAlternativeShowing
               NOTIFY alternativesChanged)
    Q_PROPERTY(bool pointerActive READ pointerActive NOTIFY pointerChanged)
    Q_PROPERTY(int pointerAlternative READ pointerAlternative NOTIFY pointerChanged)

public:
    explicit Backend(QObject *parent = nullptr);
    ~Backend() override;

    void setParentWindow(QWindow *window);

    QUrl fileUrl() const { return m_fileUrl; }
    QString fileName() const;

    bool modified() const { return m_modified; }
    QString status() const { return m_status; }
    int wordCount() const { return m_wordCount; }
    bool darkMode() const { return m_darkMode; }
    void setDarkMode(bool darkMode);
    // What the desktop asks for, times what the user asked for on top of it.
    qreal textScale() const { return m_desktopTextScale * m_textZoom; }
    void setDesktopTextScale(qreal textScale);
    QString themeBackground() const { return m_themeBackground; }
    QString themeForeground() const { return m_themeForeground; }
    QString themeAccent() const { return m_themeAccent; }
    QString themeSelection() const { return m_themeSelection; }
    QString themeCodeBackground() const { return m_themeCodeBackground; }
    QString themeSelectionForeground() const { return m_themeSelectionForeground; }
    QString editorFont() const { return m_editorFont; }
    void setEditorFont(const QString &family);
    QStringList availableFonts() const;
    QVariantList alternativeList() const;
    QVariantList alternatives() const { return alternativeList(); }
    int activeAlternative() const { return m_activeAlternative; }
    QStringList activeAlternativeVersions() const;
    int activeAlternativeShowing() const;
    // The pointer owns the arrow keys while it rests on a run with
    // alternatives; any typing hands them back to the caret.
    bool pointerActive() const { return m_pointerActive; }
    int pointerAlternative() const { return m_pointerAlternative; }
    static QString defaultEditorFont();
    static QStringList selectableFontFamilies(const QStringList &families);
    static AlternativeRange alternativeRange(const QString &body, int start, int end)
    {
        return WriteCamState::alternativeRange(body, start, end);
    }
    static QPair<int, int> scopeForSelection(const QString &body, int start, int end)
    {
        return WriteCamState::scopeForSelection(body, start, end);
    }
    static int countWords(const QString &text);
    static QString normalizedLinkUrl(const QString &clipboardText);
    static QString suggestedFileName(const QString &text);
    static QString suggestedFileName(const QString &text, const QDate &today);
    static QString slugForTitle(const QString &title);
    static QString yamlScalar(const QString &value);
    static QString derivedTitle(const QString &text, const QString &fallback);
    static QString frontMatterText(const QString &title, const QDate &date);
    static QString applyFrontMatter(const QString &text, bool creatingNewFile, bool modified,
                                    const QString &fallbackTitle, const QDateTime &now);
    static QString applyFrontMatterTags(const QString &text, const QStringList &tags,
                                         QStringList *added = nullptr);
    static QString tomlValue(const QString &text);
    static QFont printFont(const QFont &editorFont, qreal screenDpi);

    Q_INVOKABLE void attachDocument(QObject *textDocument);
    Q_INVOKABLE void openDialog();
    Q_INVOKABLE void open(const QUrl &url);
    void openOrStartFile(const QUrl &url);
    Q_INVOKABLE void save();
    Q_INVOKABLE void saveAsDialog();
    Q_INVOKABLE void saveAs(const QUrl &url);
    Q_INVOKABLE void discardRecovery();
    Q_INVOKABLE void reloadFromDisk();
    Q_INVOKABLE void keepExternalVersion();
    Q_INVOKABLE void increaseTextSize();
    Q_INVOKABLE void decreaseTextSize();
    Q_INVOKABLE void resetTextSize();
    Q_INVOKABLE void holdAutoSave(bool held);
    Q_INVOKABLE void printDocument();
    Q_INVOKABLE void copyAsHtml();
    Q_INVOKABLE void newWindow();
    Q_INVOKABLE QString clipboardUrl() const;
    Q_INVOKABLE QString clipboardText() const;
    Q_INVOKABLE bool editorTextChanged();
    Q_INVOKABLE void setCaretPosition(int position);
    Q_INVOKABLE QString linkUrlAt(int position) const;
    Q_INVOKABLE QVariantMap emphasisSpanAt(int position, const QString &marker) const;
    Q_INVOKABLE int cursorPositionStep(int position, int direction) const;
    Q_INVOKABLE void setSearchHighlight(const QString &query, int currentMatchStart);
    Q_INVOKABLE QVariantList searchMatches(const QString &query) const;
    Q_INVOKABLE void replaceRange(int start, int end, const QString &text);
    Q_INVOKABLE bool makeAlternative();
    Q_INVOKABLE bool makeAlternative(int position);
    Q_INVOKABLE bool makeAlternative(int start, int end);
    Q_INVOKABLE bool addAlternative();
    Q_INVOKABLE bool addAlternative(int position);
    Q_INVOKABLE bool addAlternative(int start, int end);
    Q_INVOKABLE bool createAlternative();
    Q_INVOKABLE bool createAlternative(int position);
    Q_INVOKABLE bool createAlternative(int start, int end);
    Q_INVOKABLE bool newAlternative();
    Q_INVOKABLE bool newAlternative(int start, int end);
    Q_INVOKABLE bool makeAlternativeAt(int position);
    Q_INVOKABLE bool makeAlternativeAt(int start, int end);
    Q_INVOKABLE bool createAlternativeAt(int position);
    Q_INVOKABLE bool createAlternativeAt(int start, int end);
    Q_INVOKABLE QVariantMap alternativeAt(int position) const;
    Q_INVOKABLE bool hasAlternativeAt(int position) const;
    Q_INVOKABLE bool flipAlternative(int direction);
    Q_INVOKABLE bool flipAlternative(int position, int direction);
    Q_INVOKABLE bool flipAlternativeAt(int position, int direction);
    Q_INVOKABLE void pointAt(int position);
    Q_INVOKABLE void releasePointer();
    Q_INVOKABLE bool flipPointedAlternative(int direction);
    Q_INVOKABLE bool cycleAlternative(int position, int direction);
    Q_INVOKABLE bool nextAlternative(int position);
    Q_INVOKABLE bool previousAlternative(int position);
    Q_INVOKABLE bool nextAlternative();
    Q_INVOKABLE bool previousAlternative();
    Q_INVOKABLE bool showAlternative(int versionIndex);
    Q_INVOKABLE bool showAlternative(int alternativeIndex, int versionIndex);
    Q_INVOKABLE bool setAlternativeShowing(int versionIndex);
    Q_INVOKABLE bool setAlternativeShowing(int alternativeIndex, int versionIndex);
    Q_INVOKABLE bool showAlternativeVersion(int versionIndex);
    Q_INVOKABLE bool showAlternativeVersion(int alternativeIndex, int versionIndex);
    Q_INVOKABLE bool addAlternativeVersion(const QString &version);
    Q_INVOKABLE bool addAlternativeVersion(int alternativeIndex, const QString &version);
    Q_INVOKABLE bool setAlternativeVersion(int versionIndex, const QString &version);
    Q_INVOKABLE bool setAlternativeVersion(int alternativeIndex, int versionIndex,
                                           const QString &version);
    Q_INVOKABLE bool setAlternativeVersionText(int versionIndex, const QString &version);
    Q_INVOKABLE bool setAlternativeVersionText(int alternativeIndex, int versionIndex,
                                               const QString &version);
    Q_INVOKABLE bool renameAlternativeVersion(int versionIndex, const QString &version);
    Q_INVOKABLE bool renameAlternativeVersion(int alternativeIndex, int versionIndex,
                                              const QString &version);
    Q_INVOKABLE bool removeAlternativeVersion(int versionIndex);
    Q_INVOKABLE bool removeAlternativeVersion(int alternativeIndex, int versionIndex);
    Q_INVOKABLE bool setActiveAlternative(int index);
    Q_INVOKABLE bool activateAlternativeAt(int position);
    Q_INVOKABLE void undo();
    Q_INVOKABLE void redo();
    Q_INVOKABLE bool insideFence(int position) const;
    Q_INVOKABLE QVariantMap windowGeometry() const;
    Q_INVOKABLE void saveWindowGeometry(int x, int y, int width, int height, bool maximized);
    Q_INVOKABLE void documentFontChanged();

signals:
    void fileUrlChanged();
    void modifiedChanged();
    void statusChanged();
    void wordCountChanged();
    void darkModeChanged();
    void textScaleChanged();
    void themeColorsChanged();
    void editorFontChanged();
    void saveFailed();
    void openDialogRequested();
    void saveDialogRequested(const QUrl &suggestedUrl);
    void saveSucceeded();
    void externalChangeDetected(bool deleted, bool locallyModified);
    void tagsApplied(const QStringList &tags);
    void alternativesChanged();
    void alternativeFlipped(bool landedOnOriginal);
    void pointerChanged();

private:
    friend class WriteCamTest;

    // A decision from the tagger, collected on a worker thread so a slow or
    // missing decision server can never hold up the save that asked for it.
    struct TagDecision {
        QStringList tags;
        QString error;
    };

    void loadDocumentText(const QString &text);
    void updateFrontMatterOnSave(bool creatingNewFile, const QString &fallbackTitle,
                                 const QStringList &newTags);
    void tagDocument();
    void finishTagRequest();
    void replaceDocumentText(const QString &from, const QString &to);
    void updateDirtyStateForText(const QString &text);
    struct EditSnapshot {
        QString text;
        WriteCamState state;
        int position = 0;
        int anchor = 0;
    };
    EditSnapshot currentSnapshot() const;
    void resetHistory();
    void recordTextHistory(const QString &beforeText, const QString &afterText);
    void commitAlternativeChange(const EditSnapshot &before);
    void updateCurrentSnapshot();
    void refreshCurrentSnapshot();
    void restoreSnapshot(const EditSnapshot &snapshot);
    void applyShowingChoices(const QList<WriteCamAlternative> &wanted);
    void updateAlternativeMarks();
    void setPointerAlternative(int alternativeIndex);
    int alternativeIndexAt(int position) const;
    bool flipAlternativeByIndex(int alternativeIndex, int direction);
    qreal steppedTextZoom(int direction) const;
    void setTextZoom(qreal zoom);
    void setFileUrl(const QUrl &url);
    void setModified(bool modified);
    void setStatus(const QString &status);
    void saveTo(const QUrl &url);
    QUrl suggestedSaveUrl() const;
    QString currentDocumentText() const;
    void setWordCount(int words);
    void refreshWordCount();
    void scheduleWordCount();
    void applyDocumentTypography();
    void reapplyTypographyToChange();
    void scheduleRecovery();
    void writeRecovery();
    void scheduleAutoSave();
    void autoSave();
    void suspendAutoSave();
    void restoreRecovery();
    void clearRecovery();
    bool diskMatchesKnownContents() const;
    void reportExternalChange(bool deleted);
    static bool savesUnchanged(const QByteArray &raw);
    void setKnownFileContents(const QByteArray &contents, bool known);
    void unwatchFiles();
    void watchCurrentFile();
    void checkWatchedFile();
    void loadOmarchyTheme();
    void watchOmarchyTheme();
    QString m_lastTagBodyHash;
    qint64 m_tagSkipUntilMsecs = 0;
    QString m_lastTagNote;
    QString m_pendingTagState;
    QString m_pendingTagBodyHash;
    QUrl m_pendingTagUrl;
    bool m_tagRequestActive = false;
    bool m_tagAgain = false;
    QFutureWatcher<TagDecision> m_tagWatcher;

    QUrl m_fileUrl;
    bool m_modified = false;
    QString m_status;
    int m_wordCount = 0;
    bool m_darkMode = true;
    // The desktop's light/dark setting, used when the Omarchy theme names none.
    bool m_systemDarkMode = true;
    qreal m_desktopTextScale = 1.0;
    qreal m_textZoom = 1.0;
    bool m_loading = false;
    bool m_formattingTypography = false;
    int m_lastChangePos = 0;
    int m_lastChangeAdded = 0;
    QTimer m_wordCountTimer;
    // Counting a large document stalls typing, so counts run on a worker
    // thread. A load outdates the count in flight, and its result is dropped.
    QFutureWatcher<int> m_wordCountWatcher;
    bool m_wordCountStale = false;
    bool m_wordCountAgain = false;
    QTimer m_recoveryTimer;
    QTimer m_autoSaveTimer;
    QFileSystemWatcher m_fileWatcher;
    QTimer m_fileCheckTimer;
    bool m_fileMissing = false;
    QPointer<QTextDocument> m_document;
    QPointer<QWindow> m_parentWindow;
    QPointer<MarkdownHighlighter> m_highlighter;
    int m_caretPosition = 0;
    QString m_lastDocumentText;
    QByteArray m_lastKnownFileContents;
    QString m_lastKnownFileText;
    bool m_hasKnownFileContents = false;
    bool m_autoSaveSuspended = false;
    bool m_autoSaveHeld = false;
    QString m_recoveryPath;
    std::unique_ptr<QLockFile> m_recoveryLock;
    WriteCamState m_writeCamState;
    QList<EditSnapshot> m_history;
    int m_historyIndex = -1;
    bool m_usingAlternativeHistory = false;
    bool m_applyingAlternative = false;
    int m_activeAlternative = -1;
    int m_pointerAlternative = -1;
    bool m_pointerActive = false;

    QString m_themeBackground;
    QString m_themeForeground;
    QString m_themeAccent;
    QString m_themeSelection;
    QString m_themeCodeBackground;
    QString m_themeSelectionForeground;
    QString m_editorFont;
    QFileSystemWatcher m_themeWatcher;
    QTimer m_themeReloadTimer;
};
