/****************************************************************************
** Meta object code from reading C++ file 'backend.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.12.0)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../src/backend.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'backend.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 69
#error "This file was generated using the moc from 6.12.0. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

#ifndef Q_CONSTINIT
#define Q_CONSTINIT
#endif

QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
QT_WARNING_DISABLE_GCC("-Wuseless-cast")
namespace {
struct qt_meta_tag_ZN7BackendE_t {};
} // unnamed namespace

template <> constexpr inline auto Backend::qt_create_metaobjectdata<qt_meta_tag_ZN7BackendE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "Backend",
        "fileUrlChanged",
        "",
        "modifiedChanged",
        "statusChanged",
        "wordCountChanged",
        "darkModeChanged",
        "textScaleChanged",
        "themeColorsChanged",
        "editorFontChanged",
        "saveFailed",
        "openDialogRequested",
        "saveDialogRequested",
        "QUrl",
        "suggestedUrl",
        "saveSucceeded",
        "externalChangeDetected",
        "deleted",
        "locallyModified",
        "tagsApplied",
        "tags",
        "alternativesChanged",
        "alternativeFlipped",
        "landedOnOriginal",
        "pointerChanged",
        "aiSettingsChanged",
        "aiTestingChanged",
        "aiTestFinished",
        "success",
        "message",
        "attachDocument",
        "textDocument",
        "openDialog",
        "open",
        "url",
        "save",
        "saveAsDialog",
        "saveAs",
        "discardRecovery",
        "reloadFromDisk",
        "keepExternalVersion",
        "increaseTextSize",
        "decreaseTextSize",
        "resetTextSize",
        "holdAutoSave",
        "held",
        "printDocument",
        "copyAsHtml",
        "newWindow",
        "saveAiSettings",
        "address",
        "wireFormat",
        "model",
        "key",
        "clearAiKey",
        "testAiConnection",
        "clipboardUrl",
        "clipboardText",
        "editorTextChanged",
        "setCaretPosition",
        "position",
        "linkUrlAt",
        "emphasisSpanAt",
        "QVariantMap",
        "marker",
        "cursorPositionStep",
        "direction",
        "setSearchHighlight",
        "query",
        "currentMatchStart",
        "searchMatches",
        "QVariantList",
        "replaceRange",
        "start",
        "end",
        "text",
        "makeAlternative",
        "addAlternative",
        "createAlternative",
        "newAlternative",
        "makeAlternativeAt",
        "createAlternativeAt",
        "alternativeAt",
        "hasAlternativeAt",
        "flipAlternative",
        "flipAlternativeAt",
        "pointAt",
        "releasePointer",
        "flipPointedAlternative",
        "cycleAlternative",
        "nextAlternative",
        "previousAlternative",
        "showAlternative",
        "versionIndex",
        "alternativeIndex",
        "setAlternativeShowing",
        "showAlternativeVersion",
        "addAlternativeVersion",
        "version",
        "setAlternativeVersion",
        "setAlternativeVersionText",
        "renameAlternativeVersion",
        "removeAlternativeVersion",
        "setActiveAlternative",
        "index",
        "activateAlternativeAt",
        "undo",
        "redo",
        "insideFence",
        "windowGeometry",
        "saveWindowGeometry",
        "x",
        "y",
        "width",
        "height",
        "maximized",
        "documentFontChanged",
        "fileUrl",
        "fileName",
        "modified",
        "status",
        "wordCount",
        "darkMode",
        "textScale",
        "themeBackground",
        "themeForeground",
        "themeAccent",
        "themeSelection",
        "themeSelectionForeground",
        "editorFont",
        "availableFonts",
        "aiAddress",
        "aiWireFormat",
        "aiModel",
        "aiKeyAvailable",
        "aiTesting",
        "aiWireFormats",
        "alternatives",
        "activeAlternative",
        "activeAlternativeVersions",
        "activeAlternativeShowing",
        "pointerActive",
        "pointerAlternative"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'fileUrlChanged'
        QtMocHelpers::SignalData<void()>(1, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'modifiedChanged'
        QtMocHelpers::SignalData<void()>(3, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'statusChanged'
        QtMocHelpers::SignalData<void()>(4, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'wordCountChanged'
        QtMocHelpers::SignalData<void()>(5, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'darkModeChanged'
        QtMocHelpers::SignalData<void()>(6, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'textScaleChanged'
        QtMocHelpers::SignalData<void()>(7, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'themeColorsChanged'
        QtMocHelpers::SignalData<void()>(8, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'editorFontChanged'
        QtMocHelpers::SignalData<void()>(9, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'saveFailed'
        QtMocHelpers::SignalData<void()>(10, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'openDialogRequested'
        QtMocHelpers::SignalData<void()>(11, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'saveDialogRequested'
        QtMocHelpers::SignalData<void(const QUrl &)>(12, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 13, 14 },
        }}),
        // Signal 'saveSucceeded'
        QtMocHelpers::SignalData<void()>(15, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'externalChangeDetected'
        QtMocHelpers::SignalData<void(bool, bool)>(16, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Bool, 17 }, { QMetaType::Bool, 18 },
        }}),
        // Signal 'tagsApplied'
        QtMocHelpers::SignalData<void(const QStringList &)>(19, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QStringList, 20 },
        }}),
        // Signal 'alternativesChanged'
        QtMocHelpers::SignalData<void()>(21, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'alternativeFlipped'
        QtMocHelpers::SignalData<void(bool)>(22, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Bool, 23 },
        }}),
        // Signal 'pointerChanged'
        QtMocHelpers::SignalData<void()>(24, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'aiSettingsChanged'
        QtMocHelpers::SignalData<void()>(25, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'aiTestingChanged'
        QtMocHelpers::SignalData<void()>(26, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'aiTestFinished'
        QtMocHelpers::SignalData<void(bool, const QString &)>(27, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Bool, 28 }, { QMetaType::QString, 29 },
        }}),
        // Method 'attachDocument'
        QtMocHelpers::MethodData<void(QObject *)>(30, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QObjectStar, 31 },
        }}),
        // Method 'openDialog'
        QtMocHelpers::MethodData<void()>(32, 2, QMC::AccessPublic, QMetaType::Void),
        // Method 'open'
        QtMocHelpers::MethodData<void(const QUrl &)>(33, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 13, 34 },
        }}),
        // Method 'save'
        QtMocHelpers::MethodData<void()>(35, 2, QMC::AccessPublic, QMetaType::Void),
        // Method 'saveAsDialog'
        QtMocHelpers::MethodData<void()>(36, 2, QMC::AccessPublic, QMetaType::Void),
        // Method 'saveAs'
        QtMocHelpers::MethodData<void(const QUrl &)>(37, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 13, 34 },
        }}),
        // Method 'discardRecovery'
        QtMocHelpers::MethodData<void()>(38, 2, QMC::AccessPublic, QMetaType::Void),
        // Method 'reloadFromDisk'
        QtMocHelpers::MethodData<void()>(39, 2, QMC::AccessPublic, QMetaType::Void),
        // Method 'keepExternalVersion'
        QtMocHelpers::MethodData<void()>(40, 2, QMC::AccessPublic, QMetaType::Void),
        // Method 'increaseTextSize'
        QtMocHelpers::MethodData<void()>(41, 2, QMC::AccessPublic, QMetaType::Void),
        // Method 'decreaseTextSize'
        QtMocHelpers::MethodData<void()>(42, 2, QMC::AccessPublic, QMetaType::Void),
        // Method 'resetTextSize'
        QtMocHelpers::MethodData<void()>(43, 2, QMC::AccessPublic, QMetaType::Void),
        // Method 'holdAutoSave'
        QtMocHelpers::MethodData<void(bool)>(44, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Bool, 45 },
        }}),
        // Method 'printDocument'
        QtMocHelpers::MethodData<void()>(46, 2, QMC::AccessPublic, QMetaType::Void),
        // Method 'copyAsHtml'
        QtMocHelpers::MethodData<void()>(47, 2, QMC::AccessPublic, QMetaType::Void),
        // Method 'newWindow'
        QtMocHelpers::MethodData<void()>(48, 2, QMC::AccessPublic, QMetaType::Void),
        // Method 'saveAiSettings'
        QtMocHelpers::MethodData<void(const QString &, const QString &, const QString &, const QString &)>(49, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 50 }, { QMetaType::QString, 51 }, { QMetaType::QString, 52 }, { QMetaType::QString, 53 },
        }}),
        // Method 'clearAiKey'
        QtMocHelpers::MethodData<void()>(54, 2, QMC::AccessPublic, QMetaType::Void),
        // Method 'testAiConnection'
        QtMocHelpers::MethodData<void(const QString &, const QString &, const QString &, const QString &)>(55, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 50 }, { QMetaType::QString, 51 }, { QMetaType::QString, 52 }, { QMetaType::QString, 53 },
        }}),
        // Method 'clipboardUrl'
        QtMocHelpers::MethodData<QString() const>(56, 2, QMC::AccessPublic, QMetaType::QString),
        // Method 'clipboardText'
        QtMocHelpers::MethodData<QString() const>(57, 2, QMC::AccessPublic, QMetaType::QString),
        // Method 'editorTextChanged'
        QtMocHelpers::MethodData<bool()>(58, 2, QMC::AccessPublic, QMetaType::Bool),
        // Method 'setCaretPosition'
        QtMocHelpers::MethodData<void(int)>(59, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 60 },
        }}),
        // Method 'linkUrlAt'
        QtMocHelpers::MethodData<QString(int) const>(61, 2, QMC::AccessPublic, QMetaType::QString, {{
            { QMetaType::Int, 60 },
        }}),
        // Method 'emphasisSpanAt'
        QtMocHelpers::MethodData<QVariantMap(int, const QString &) const>(62, 2, QMC::AccessPublic, 0x80000000 | 63, {{
            { QMetaType::Int, 60 }, { QMetaType::QString, 64 },
        }}),
        // Method 'cursorPositionStep'
        QtMocHelpers::MethodData<int(int, int) const>(65, 2, QMC::AccessPublic, QMetaType::Int, {{
            { QMetaType::Int, 60 }, { QMetaType::Int, 66 },
        }}),
        // Method 'setSearchHighlight'
        QtMocHelpers::MethodData<void(const QString &, int)>(67, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 68 }, { QMetaType::Int, 69 },
        }}),
        // Method 'searchMatches'
        QtMocHelpers::MethodData<QVariantList(const QString &) const>(70, 2, QMC::AccessPublic, 0x80000000 | 71, {{
            { QMetaType::QString, 68 },
        }}),
        // Method 'replaceRange'
        QtMocHelpers::MethodData<void(int, int, const QString &)>(72, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 73 }, { QMetaType::Int, 74 }, { QMetaType::QString, 75 },
        }}),
        // Method 'makeAlternative'
        QtMocHelpers::MethodData<bool()>(76, 2, QMC::AccessPublic, QMetaType::Bool),
        // Method 'makeAlternative'
        QtMocHelpers::MethodData<bool(int)>(76, 2, QMC::AccessPublic, QMetaType::Bool, {{
            { QMetaType::Int, 60 },
        }}),
        // Method 'makeAlternative'
        QtMocHelpers::MethodData<bool(int, int)>(76, 2, QMC::AccessPublic, QMetaType::Bool, {{
            { QMetaType::Int, 73 }, { QMetaType::Int, 74 },
        }}),
        // Method 'addAlternative'
        QtMocHelpers::MethodData<bool()>(77, 2, QMC::AccessPublic, QMetaType::Bool),
        // Method 'addAlternative'
        QtMocHelpers::MethodData<bool(int)>(77, 2, QMC::AccessPublic, QMetaType::Bool, {{
            { QMetaType::Int, 60 },
        }}),
        // Method 'addAlternative'
        QtMocHelpers::MethodData<bool(int, int)>(77, 2, QMC::AccessPublic, QMetaType::Bool, {{
            { QMetaType::Int, 73 }, { QMetaType::Int, 74 },
        }}),
        // Method 'createAlternative'
        QtMocHelpers::MethodData<bool()>(78, 2, QMC::AccessPublic, QMetaType::Bool),
        // Method 'createAlternative'
        QtMocHelpers::MethodData<bool(int)>(78, 2, QMC::AccessPublic, QMetaType::Bool, {{
            { QMetaType::Int, 60 },
        }}),
        // Method 'createAlternative'
        QtMocHelpers::MethodData<bool(int, int)>(78, 2, QMC::AccessPublic, QMetaType::Bool, {{
            { QMetaType::Int, 73 }, { QMetaType::Int, 74 },
        }}),
        // Method 'newAlternative'
        QtMocHelpers::MethodData<bool()>(79, 2, QMC::AccessPublic, QMetaType::Bool),
        // Method 'newAlternative'
        QtMocHelpers::MethodData<bool(int, int)>(79, 2, QMC::AccessPublic, QMetaType::Bool, {{
            { QMetaType::Int, 73 }, { QMetaType::Int, 74 },
        }}),
        // Method 'makeAlternativeAt'
        QtMocHelpers::MethodData<bool(int)>(80, 2, QMC::AccessPublic, QMetaType::Bool, {{
            { QMetaType::Int, 60 },
        }}),
        // Method 'makeAlternativeAt'
        QtMocHelpers::MethodData<bool(int, int)>(80, 2, QMC::AccessPublic, QMetaType::Bool, {{
            { QMetaType::Int, 73 }, { QMetaType::Int, 74 },
        }}),
        // Method 'createAlternativeAt'
        QtMocHelpers::MethodData<bool(int)>(81, 2, QMC::AccessPublic, QMetaType::Bool, {{
            { QMetaType::Int, 60 },
        }}),
        // Method 'createAlternativeAt'
        QtMocHelpers::MethodData<bool(int, int)>(81, 2, QMC::AccessPublic, QMetaType::Bool, {{
            { QMetaType::Int, 73 }, { QMetaType::Int, 74 },
        }}),
        // Method 'alternativeAt'
        QtMocHelpers::MethodData<QVariantMap(int) const>(82, 2, QMC::AccessPublic, 0x80000000 | 63, {{
            { QMetaType::Int, 60 },
        }}),
        // Method 'hasAlternativeAt'
        QtMocHelpers::MethodData<bool(int) const>(83, 2, QMC::AccessPublic, QMetaType::Bool, {{
            { QMetaType::Int, 60 },
        }}),
        // Method 'flipAlternative'
        QtMocHelpers::MethodData<bool(int)>(84, 2, QMC::AccessPublic, QMetaType::Bool, {{
            { QMetaType::Int, 66 },
        }}),
        // Method 'flipAlternative'
        QtMocHelpers::MethodData<bool(int, int)>(84, 2, QMC::AccessPublic, QMetaType::Bool, {{
            { QMetaType::Int, 60 }, { QMetaType::Int, 66 },
        }}),
        // Method 'flipAlternativeAt'
        QtMocHelpers::MethodData<bool(int, int)>(85, 2, QMC::AccessPublic, QMetaType::Bool, {{
            { QMetaType::Int, 60 }, { QMetaType::Int, 66 },
        }}),
        // Method 'pointAt'
        QtMocHelpers::MethodData<void(int)>(86, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 60 },
        }}),
        // Method 'releasePointer'
        QtMocHelpers::MethodData<void()>(87, 2, QMC::AccessPublic, QMetaType::Void),
        // Method 'flipPointedAlternative'
        QtMocHelpers::MethodData<bool(int)>(88, 2, QMC::AccessPublic, QMetaType::Bool, {{
            { QMetaType::Int, 66 },
        }}),
        // Method 'cycleAlternative'
        QtMocHelpers::MethodData<bool(int, int)>(89, 2, QMC::AccessPublic, QMetaType::Bool, {{
            { QMetaType::Int, 60 }, { QMetaType::Int, 66 },
        }}),
        // Method 'nextAlternative'
        QtMocHelpers::MethodData<bool(int)>(90, 2, QMC::AccessPublic, QMetaType::Bool, {{
            { QMetaType::Int, 60 },
        }}),
        // Method 'previousAlternative'
        QtMocHelpers::MethodData<bool(int)>(91, 2, QMC::AccessPublic, QMetaType::Bool, {{
            { QMetaType::Int, 60 },
        }}),
        // Method 'nextAlternative'
        QtMocHelpers::MethodData<bool()>(90, 2, QMC::AccessPublic, QMetaType::Bool),
        // Method 'previousAlternative'
        QtMocHelpers::MethodData<bool()>(91, 2, QMC::AccessPublic, QMetaType::Bool),
        // Method 'showAlternative'
        QtMocHelpers::MethodData<bool(int)>(92, 2, QMC::AccessPublic, QMetaType::Bool, {{
            { QMetaType::Int, 93 },
        }}),
        // Method 'showAlternative'
        QtMocHelpers::MethodData<bool(int, int)>(92, 2, QMC::AccessPublic, QMetaType::Bool, {{
            { QMetaType::Int, 94 }, { QMetaType::Int, 93 },
        }}),
        // Method 'setAlternativeShowing'
        QtMocHelpers::MethodData<bool(int)>(95, 2, QMC::AccessPublic, QMetaType::Bool, {{
            { QMetaType::Int, 93 },
        }}),
        // Method 'setAlternativeShowing'
        QtMocHelpers::MethodData<bool(int, int)>(95, 2, QMC::AccessPublic, QMetaType::Bool, {{
            { QMetaType::Int, 94 }, { QMetaType::Int, 93 },
        }}),
        // Method 'showAlternativeVersion'
        QtMocHelpers::MethodData<bool(int)>(96, 2, QMC::AccessPublic, QMetaType::Bool, {{
            { QMetaType::Int, 93 },
        }}),
        // Method 'showAlternativeVersion'
        QtMocHelpers::MethodData<bool(int, int)>(96, 2, QMC::AccessPublic, QMetaType::Bool, {{
            { QMetaType::Int, 94 }, { QMetaType::Int, 93 },
        }}),
        // Method 'addAlternativeVersion'
        QtMocHelpers::MethodData<bool(const QString &)>(97, 2, QMC::AccessPublic, QMetaType::Bool, {{
            { QMetaType::QString, 98 },
        }}),
        // Method 'addAlternativeVersion'
        QtMocHelpers::MethodData<bool(int, const QString &)>(97, 2, QMC::AccessPublic, QMetaType::Bool, {{
            { QMetaType::Int, 94 }, { QMetaType::QString, 98 },
        }}),
        // Method 'setAlternativeVersion'
        QtMocHelpers::MethodData<bool(int, const QString &)>(99, 2, QMC::AccessPublic, QMetaType::Bool, {{
            { QMetaType::Int, 93 }, { QMetaType::QString, 98 },
        }}),
        // Method 'setAlternativeVersion'
        QtMocHelpers::MethodData<bool(int, int, const QString &)>(99, 2, QMC::AccessPublic, QMetaType::Bool, {{
            { QMetaType::Int, 94 }, { QMetaType::Int, 93 }, { QMetaType::QString, 98 },
        }}),
        // Method 'setAlternativeVersionText'
        QtMocHelpers::MethodData<bool(int, const QString &)>(100, 2, QMC::AccessPublic, QMetaType::Bool, {{
            { QMetaType::Int, 93 }, { QMetaType::QString, 98 },
        }}),
        // Method 'setAlternativeVersionText'
        QtMocHelpers::MethodData<bool(int, int, const QString &)>(100, 2, QMC::AccessPublic, QMetaType::Bool, {{
            { QMetaType::Int, 94 }, { QMetaType::Int, 93 }, { QMetaType::QString, 98 },
        }}),
        // Method 'renameAlternativeVersion'
        QtMocHelpers::MethodData<bool(int, const QString &)>(101, 2, QMC::AccessPublic, QMetaType::Bool, {{
            { QMetaType::Int, 93 }, { QMetaType::QString, 98 },
        }}),
        // Method 'renameAlternativeVersion'
        QtMocHelpers::MethodData<bool(int, int, const QString &)>(101, 2, QMC::AccessPublic, QMetaType::Bool, {{
            { QMetaType::Int, 94 }, { QMetaType::Int, 93 }, { QMetaType::QString, 98 },
        }}),
        // Method 'removeAlternativeVersion'
        QtMocHelpers::MethodData<bool(int)>(102, 2, QMC::AccessPublic, QMetaType::Bool, {{
            { QMetaType::Int, 93 },
        }}),
        // Method 'removeAlternativeVersion'
        QtMocHelpers::MethodData<bool(int, int)>(102, 2, QMC::AccessPublic, QMetaType::Bool, {{
            { QMetaType::Int, 94 }, { QMetaType::Int, 93 },
        }}),
        // Method 'setActiveAlternative'
        QtMocHelpers::MethodData<bool(int)>(103, 2, QMC::AccessPublic, QMetaType::Bool, {{
            { QMetaType::Int, 104 },
        }}),
        // Method 'activateAlternativeAt'
        QtMocHelpers::MethodData<bool(int)>(105, 2, QMC::AccessPublic, QMetaType::Bool, {{
            { QMetaType::Int, 60 },
        }}),
        // Method 'undo'
        QtMocHelpers::MethodData<void()>(106, 2, QMC::AccessPublic, QMetaType::Void),
        // Method 'redo'
        QtMocHelpers::MethodData<void()>(107, 2, QMC::AccessPublic, QMetaType::Void),
        // Method 'insideFence'
        QtMocHelpers::MethodData<bool(int) const>(108, 2, QMC::AccessPublic, QMetaType::Bool, {{
            { QMetaType::Int, 60 },
        }}),
        // Method 'windowGeometry'
        QtMocHelpers::MethodData<QVariantMap() const>(109, 2, QMC::AccessPublic, 0x80000000 | 63),
        // Method 'saveWindowGeometry'
        QtMocHelpers::MethodData<void(int, int, int, int, bool)>(110, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 111 }, { QMetaType::Int, 112 }, { QMetaType::Int, 113 }, { QMetaType::Int, 114 },
            { QMetaType::Bool, 115 },
        }}),
        // Method 'documentFontChanged'
        QtMocHelpers::MethodData<void()>(116, 2, QMC::AccessPublic, QMetaType::Void),
    };
    QtMocHelpers::UintData qt_properties {
        // property 'fileUrl'
        QtMocHelpers::PropertyData<QUrl>(117, 0x80000000 | 13, QMC::DefaultPropertyFlags | QMC::EnumOrFlag, 0),
        // property 'fileName'
        QtMocHelpers::PropertyData<QString>(118, QMetaType::QString, QMC::DefaultPropertyFlags, 0),
        // property 'modified'
        QtMocHelpers::PropertyData<bool>(119, QMetaType::Bool, QMC::DefaultPropertyFlags, 1),
        // property 'status'
        QtMocHelpers::PropertyData<QString>(120, QMetaType::QString, QMC::DefaultPropertyFlags, 2),
        // property 'wordCount'
        QtMocHelpers::PropertyData<int>(121, QMetaType::Int, QMC::DefaultPropertyFlags, 3),
        // property 'darkMode'
        QtMocHelpers::PropertyData<bool>(122, QMetaType::Bool, QMC::DefaultPropertyFlags | QMC::Writable | QMC::StdCppSet, 4),
        // property 'textScale'
        QtMocHelpers::PropertyData<qreal>(123, QMetaType::QReal, QMC::DefaultPropertyFlags, 5),
        // property 'themeBackground'
        QtMocHelpers::PropertyData<QString>(124, QMetaType::QString, QMC::DefaultPropertyFlags, 6),
        // property 'themeForeground'
        QtMocHelpers::PropertyData<QString>(125, QMetaType::QString, QMC::DefaultPropertyFlags, 6),
        // property 'themeAccent'
        QtMocHelpers::PropertyData<QString>(126, QMetaType::QString, QMC::DefaultPropertyFlags, 6),
        // property 'themeSelection'
        QtMocHelpers::PropertyData<QString>(127, QMetaType::QString, QMC::DefaultPropertyFlags, 6),
        // property 'themeSelectionForeground'
        QtMocHelpers::PropertyData<QString>(128, QMetaType::QString, QMC::DefaultPropertyFlags, 6),
        // property 'editorFont'
        QtMocHelpers::PropertyData<QString>(129, QMetaType::QString, QMC::DefaultPropertyFlags | QMC::Writable | QMC::StdCppSet, 7),
        // property 'availableFonts'
        QtMocHelpers::PropertyData<QStringList>(130, QMetaType::QStringList, QMC::DefaultPropertyFlags | QMC::Constant),
        // property 'aiAddress'
        QtMocHelpers::PropertyData<QString>(131, QMetaType::QString, QMC::DefaultPropertyFlags, 17),
        // property 'aiWireFormat'
        QtMocHelpers::PropertyData<QString>(132, QMetaType::QString, QMC::DefaultPropertyFlags, 17),
        // property 'aiModel'
        QtMocHelpers::PropertyData<QString>(133, QMetaType::QString, QMC::DefaultPropertyFlags, 17),
        // property 'aiKeyAvailable'
        QtMocHelpers::PropertyData<bool>(134, QMetaType::Bool, QMC::DefaultPropertyFlags, 17),
        // property 'aiTesting'
        QtMocHelpers::PropertyData<bool>(135, QMetaType::Bool, QMC::DefaultPropertyFlags, 18),
        // property 'aiWireFormats'
        QtMocHelpers::PropertyData<QVariantList>(136, 0x80000000 | 71, QMC::DefaultPropertyFlags | QMC::EnumOrFlag | QMC::Constant),
        // property 'alternatives'
        QtMocHelpers::PropertyData<QVariantList>(137, 0x80000000 | 71, QMC::DefaultPropertyFlags | QMC::EnumOrFlag, 14),
        // property 'activeAlternative'
        QtMocHelpers::PropertyData<int>(138, QMetaType::Int, QMC::DefaultPropertyFlags, 14),
        // property 'activeAlternativeVersions'
        QtMocHelpers::PropertyData<QStringList>(139, QMetaType::QStringList, QMC::DefaultPropertyFlags, 14),
        // property 'activeAlternativeShowing'
        QtMocHelpers::PropertyData<int>(140, QMetaType::Int, QMC::DefaultPropertyFlags, 14),
        // property 'pointerActive'
        QtMocHelpers::PropertyData<bool>(141, QMetaType::Bool, QMC::DefaultPropertyFlags, 16),
        // property 'pointerAlternative'
        QtMocHelpers::PropertyData<int>(142, QMetaType::Int, QMC::DefaultPropertyFlags, 16),
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<Backend, qt_meta_tag_ZN7BackendE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject Backend::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN7BackendE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN7BackendE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN7BackendE_t>.metaTypes,
    nullptr
} };

void Backend::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<Backend *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->fileUrlChanged(); break;
        case 1: _t->modifiedChanged(); break;
        case 2: _t->statusChanged(); break;
        case 3: _t->wordCountChanged(); break;
        case 4: _t->darkModeChanged(); break;
        case 5: _t->textScaleChanged(); break;
        case 6: _t->themeColorsChanged(); break;
        case 7: _t->editorFontChanged(); break;
        case 8: _t->saveFailed(); break;
        case 9: _t->openDialogRequested(); break;
        case 10: _t->saveDialogRequested((*reinterpret_cast<std::add_pointer_t<QUrl>>(_a[1]))); break;
        case 11: _t->saveSucceeded(); break;
        case 12: _t->externalChangeDetected((*reinterpret_cast<std::add_pointer_t<bool>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<bool>>(_a[2]))); break;
        case 13: _t->tagsApplied((*reinterpret_cast<std::add_pointer_t<QStringList>>(_a[1]))); break;
        case 14: _t->alternativesChanged(); break;
        case 15: _t->alternativeFlipped((*reinterpret_cast<std::add_pointer_t<bool>>(_a[1]))); break;
        case 16: _t->pointerChanged(); break;
        case 17: _t->aiSettingsChanged(); break;
        case 18: _t->aiTestingChanged(); break;
        case 19: _t->aiTestFinished((*reinterpret_cast<std::add_pointer_t<bool>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2]))); break;
        case 20: _t->attachDocument((*reinterpret_cast<std::add_pointer_t<QObject*>>(_a[1]))); break;
        case 21: _t->openDialog(); break;
        case 22: _t->open((*reinterpret_cast<std::add_pointer_t<QUrl>>(_a[1]))); break;
        case 23: _t->save(); break;
        case 24: _t->saveAsDialog(); break;
        case 25: _t->saveAs((*reinterpret_cast<std::add_pointer_t<QUrl>>(_a[1]))); break;
        case 26: _t->discardRecovery(); break;
        case 27: _t->reloadFromDisk(); break;
        case 28: _t->keepExternalVersion(); break;
        case 29: _t->increaseTextSize(); break;
        case 30: _t->decreaseTextSize(); break;
        case 31: _t->resetTextSize(); break;
        case 32: _t->holdAutoSave((*reinterpret_cast<std::add_pointer_t<bool>>(_a[1]))); break;
        case 33: _t->printDocument(); break;
        case 34: _t->copyAsHtml(); break;
        case 35: _t->newWindow(); break;
        case 36: _t->saveAiSettings((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[3])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[4]))); break;
        case 37: _t->clearAiKey(); break;
        case 38: _t->testAiConnection((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[3])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[4]))); break;
        case 39: { QString _r = _t->clipboardUrl();
            if (_a[0]) *reinterpret_cast<QString*>(_a[0]) = std::move(_r); }  break;
        case 40: { QString _r = _t->clipboardText();
            if (_a[0]) *reinterpret_cast<QString*>(_a[0]) = std::move(_r); }  break;
        case 41: { bool _r = _t->editorTextChanged();
            if (_a[0]) *reinterpret_cast<bool*>(_a[0]) = std::move(_r); }  break;
        case 42: _t->setCaretPosition((*reinterpret_cast<std::add_pointer_t<int>>(_a[1]))); break;
        case 43: { QString _r = _t->linkUrlAt((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])));
            if (_a[0]) *reinterpret_cast<QString*>(_a[0]) = std::move(_r); }  break;
        case 44: { QVariantMap _r = _t->emphasisSpanAt((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2])));
            if (_a[0]) *reinterpret_cast<QVariantMap*>(_a[0]) = std::move(_r); }  break;
        case 45: { int _r = _t->cursorPositionStep((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[2])));
            if (_a[0]) *reinterpret_cast<int*>(_a[0]) = std::move(_r); }  break;
        case 46: _t->setSearchHighlight((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[2]))); break;
        case 47: { QVariantList _r = _t->searchMatches((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])));
            if (_a[0]) *reinterpret_cast<QVariantList*>(_a[0]) = std::move(_r); }  break;
        case 48: _t->replaceRange((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[3]))); break;
        case 49: { bool _r = _t->makeAlternative();
            if (_a[0]) *reinterpret_cast<bool*>(_a[0]) = std::move(_r); }  break;
        case 50: { bool _r = _t->makeAlternative((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])));
            if (_a[0]) *reinterpret_cast<bool*>(_a[0]) = std::move(_r); }  break;
        case 51: { bool _r = _t->makeAlternative((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[2])));
            if (_a[0]) *reinterpret_cast<bool*>(_a[0]) = std::move(_r); }  break;
        case 52: { bool _r = _t->addAlternative();
            if (_a[0]) *reinterpret_cast<bool*>(_a[0]) = std::move(_r); }  break;
        case 53: { bool _r = _t->addAlternative((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])));
            if (_a[0]) *reinterpret_cast<bool*>(_a[0]) = std::move(_r); }  break;
        case 54: { bool _r = _t->addAlternative((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[2])));
            if (_a[0]) *reinterpret_cast<bool*>(_a[0]) = std::move(_r); }  break;
        case 55: { bool _r = _t->createAlternative();
            if (_a[0]) *reinterpret_cast<bool*>(_a[0]) = std::move(_r); }  break;
        case 56: { bool _r = _t->createAlternative((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])));
            if (_a[0]) *reinterpret_cast<bool*>(_a[0]) = std::move(_r); }  break;
        case 57: { bool _r = _t->createAlternative((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[2])));
            if (_a[0]) *reinterpret_cast<bool*>(_a[0]) = std::move(_r); }  break;
        case 58: { bool _r = _t->newAlternative();
            if (_a[0]) *reinterpret_cast<bool*>(_a[0]) = std::move(_r); }  break;
        case 59: { bool _r = _t->newAlternative((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[2])));
            if (_a[0]) *reinterpret_cast<bool*>(_a[0]) = std::move(_r); }  break;
        case 60: { bool _r = _t->makeAlternativeAt((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])));
            if (_a[0]) *reinterpret_cast<bool*>(_a[0]) = std::move(_r); }  break;
        case 61: { bool _r = _t->makeAlternativeAt((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[2])));
            if (_a[0]) *reinterpret_cast<bool*>(_a[0]) = std::move(_r); }  break;
        case 62: { bool _r = _t->createAlternativeAt((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])));
            if (_a[0]) *reinterpret_cast<bool*>(_a[0]) = std::move(_r); }  break;
        case 63: { bool _r = _t->createAlternativeAt((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[2])));
            if (_a[0]) *reinterpret_cast<bool*>(_a[0]) = std::move(_r); }  break;
        case 64: { QVariantMap _r = _t->alternativeAt((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])));
            if (_a[0]) *reinterpret_cast<QVariantMap*>(_a[0]) = std::move(_r); }  break;
        case 65: { bool _r = _t->hasAlternativeAt((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])));
            if (_a[0]) *reinterpret_cast<bool*>(_a[0]) = std::move(_r); }  break;
        case 66: { bool _r = _t->flipAlternative((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])));
            if (_a[0]) *reinterpret_cast<bool*>(_a[0]) = std::move(_r); }  break;
        case 67: { bool _r = _t->flipAlternative((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[2])));
            if (_a[0]) *reinterpret_cast<bool*>(_a[0]) = std::move(_r); }  break;
        case 68: { bool _r = _t->flipAlternativeAt((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[2])));
            if (_a[0]) *reinterpret_cast<bool*>(_a[0]) = std::move(_r); }  break;
        case 69: _t->pointAt((*reinterpret_cast<std::add_pointer_t<int>>(_a[1]))); break;
        case 70: _t->releasePointer(); break;
        case 71: { bool _r = _t->flipPointedAlternative((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])));
            if (_a[0]) *reinterpret_cast<bool*>(_a[0]) = std::move(_r); }  break;
        case 72: { bool _r = _t->cycleAlternative((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[2])));
            if (_a[0]) *reinterpret_cast<bool*>(_a[0]) = std::move(_r); }  break;
        case 73: { bool _r = _t->nextAlternative((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])));
            if (_a[0]) *reinterpret_cast<bool*>(_a[0]) = std::move(_r); }  break;
        case 74: { bool _r = _t->previousAlternative((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])));
            if (_a[0]) *reinterpret_cast<bool*>(_a[0]) = std::move(_r); }  break;
        case 75: { bool _r = _t->nextAlternative();
            if (_a[0]) *reinterpret_cast<bool*>(_a[0]) = std::move(_r); }  break;
        case 76: { bool _r = _t->previousAlternative();
            if (_a[0]) *reinterpret_cast<bool*>(_a[0]) = std::move(_r); }  break;
        case 77: { bool _r = _t->showAlternative((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])));
            if (_a[0]) *reinterpret_cast<bool*>(_a[0]) = std::move(_r); }  break;
        case 78: { bool _r = _t->showAlternative((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[2])));
            if (_a[0]) *reinterpret_cast<bool*>(_a[0]) = std::move(_r); }  break;
        case 79: { bool _r = _t->setAlternativeShowing((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])));
            if (_a[0]) *reinterpret_cast<bool*>(_a[0]) = std::move(_r); }  break;
        case 80: { bool _r = _t->setAlternativeShowing((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[2])));
            if (_a[0]) *reinterpret_cast<bool*>(_a[0]) = std::move(_r); }  break;
        case 81: { bool _r = _t->showAlternativeVersion((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])));
            if (_a[0]) *reinterpret_cast<bool*>(_a[0]) = std::move(_r); }  break;
        case 82: { bool _r = _t->showAlternativeVersion((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[2])));
            if (_a[0]) *reinterpret_cast<bool*>(_a[0]) = std::move(_r); }  break;
        case 83: { bool _r = _t->addAlternativeVersion((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])));
            if (_a[0]) *reinterpret_cast<bool*>(_a[0]) = std::move(_r); }  break;
        case 84: { bool _r = _t->addAlternativeVersion((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2])));
            if (_a[0]) *reinterpret_cast<bool*>(_a[0]) = std::move(_r); }  break;
        case 85: { bool _r = _t->setAlternativeVersion((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2])));
            if (_a[0]) *reinterpret_cast<bool*>(_a[0]) = std::move(_r); }  break;
        case 86: { bool _r = _t->setAlternativeVersion((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[3])));
            if (_a[0]) *reinterpret_cast<bool*>(_a[0]) = std::move(_r); }  break;
        case 87: { bool _r = _t->setAlternativeVersionText((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2])));
            if (_a[0]) *reinterpret_cast<bool*>(_a[0]) = std::move(_r); }  break;
        case 88: { bool _r = _t->setAlternativeVersionText((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[3])));
            if (_a[0]) *reinterpret_cast<bool*>(_a[0]) = std::move(_r); }  break;
        case 89: { bool _r = _t->renameAlternativeVersion((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2])));
            if (_a[0]) *reinterpret_cast<bool*>(_a[0]) = std::move(_r); }  break;
        case 90: { bool _r = _t->renameAlternativeVersion((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[3])));
            if (_a[0]) *reinterpret_cast<bool*>(_a[0]) = std::move(_r); }  break;
        case 91: { bool _r = _t->removeAlternativeVersion((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])));
            if (_a[0]) *reinterpret_cast<bool*>(_a[0]) = std::move(_r); }  break;
        case 92: { bool _r = _t->removeAlternativeVersion((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[2])));
            if (_a[0]) *reinterpret_cast<bool*>(_a[0]) = std::move(_r); }  break;
        case 93: { bool _r = _t->setActiveAlternative((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])));
            if (_a[0]) *reinterpret_cast<bool*>(_a[0]) = std::move(_r); }  break;
        case 94: { bool _r = _t->activateAlternativeAt((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])));
            if (_a[0]) *reinterpret_cast<bool*>(_a[0]) = std::move(_r); }  break;
        case 95: _t->undo(); break;
        case 96: _t->redo(); break;
        case 97: { bool _r = _t->insideFence((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])));
            if (_a[0]) *reinterpret_cast<bool*>(_a[0]) = std::move(_r); }  break;
        case 98: { QVariantMap _r = _t->windowGeometry();
            if (_a[0]) *reinterpret_cast<QVariantMap*>(_a[0]) = std::move(_r); }  break;
        case 99: _t->saveWindowGeometry((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[3])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[4])),(*reinterpret_cast<std::add_pointer_t<bool>>(_a[5]))); break;
        case 100: _t->documentFontChanged(); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (Backend::*)()>(_a, &Backend::fileUrlChanged, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (Backend::*)()>(_a, &Backend::modifiedChanged, 1))
            return;
        if (QtMocHelpers::indexOfMethod<void (Backend::*)()>(_a, &Backend::statusChanged, 2))
            return;
        if (QtMocHelpers::indexOfMethod<void (Backend::*)()>(_a, &Backend::wordCountChanged, 3))
            return;
        if (QtMocHelpers::indexOfMethod<void (Backend::*)()>(_a, &Backend::darkModeChanged, 4))
            return;
        if (QtMocHelpers::indexOfMethod<void (Backend::*)()>(_a, &Backend::textScaleChanged, 5))
            return;
        if (QtMocHelpers::indexOfMethod<void (Backend::*)()>(_a, &Backend::themeColorsChanged, 6))
            return;
        if (QtMocHelpers::indexOfMethod<void (Backend::*)()>(_a, &Backend::editorFontChanged, 7))
            return;
        if (QtMocHelpers::indexOfMethod<void (Backend::*)()>(_a, &Backend::saveFailed, 8))
            return;
        if (QtMocHelpers::indexOfMethod<void (Backend::*)()>(_a, &Backend::openDialogRequested, 9))
            return;
        if (QtMocHelpers::indexOfMethod<void (Backend::*)(const QUrl & )>(_a, &Backend::saveDialogRequested, 10))
            return;
        if (QtMocHelpers::indexOfMethod<void (Backend::*)()>(_a, &Backend::saveSucceeded, 11))
            return;
        if (QtMocHelpers::indexOfMethod<void (Backend::*)(bool , bool )>(_a, &Backend::externalChangeDetected, 12))
            return;
        if (QtMocHelpers::indexOfMethod<void (Backend::*)(const QStringList & )>(_a, &Backend::tagsApplied, 13))
            return;
        if (QtMocHelpers::indexOfMethod<void (Backend::*)()>(_a, &Backend::alternativesChanged, 14))
            return;
        if (QtMocHelpers::indexOfMethod<void (Backend::*)(bool )>(_a, &Backend::alternativeFlipped, 15))
            return;
        if (QtMocHelpers::indexOfMethod<void (Backend::*)()>(_a, &Backend::pointerChanged, 16))
            return;
        if (QtMocHelpers::indexOfMethod<void (Backend::*)()>(_a, &Backend::aiSettingsChanged, 17))
            return;
        if (QtMocHelpers::indexOfMethod<void (Backend::*)()>(_a, &Backend::aiTestingChanged, 18))
            return;
        if (QtMocHelpers::indexOfMethod<void (Backend::*)(bool , const QString & )>(_a, &Backend::aiTestFinished, 19))
            return;
    }
    if (_c == QMetaObject::ReadProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 0: *reinterpret_cast<QUrl*>(_v) = _t->fileUrl(); break;
        case 1: *reinterpret_cast<QString*>(_v) = _t->fileName(); break;
        case 2: *reinterpret_cast<bool*>(_v) = _t->modified(); break;
        case 3: *reinterpret_cast<QString*>(_v) = _t->status(); break;
        case 4: *reinterpret_cast<int*>(_v) = _t->wordCount(); break;
        case 5: *reinterpret_cast<bool*>(_v) = _t->darkMode(); break;
        case 6: *reinterpret_cast<qreal*>(_v) = _t->textScale(); break;
        case 7: *reinterpret_cast<QString*>(_v) = _t->themeBackground(); break;
        case 8: *reinterpret_cast<QString*>(_v) = _t->themeForeground(); break;
        case 9: *reinterpret_cast<QString*>(_v) = _t->themeAccent(); break;
        case 10: *reinterpret_cast<QString*>(_v) = _t->themeSelection(); break;
        case 11: *reinterpret_cast<QString*>(_v) = _t->themeSelectionForeground(); break;
        case 12: *reinterpret_cast<QString*>(_v) = _t->editorFont(); break;
        case 13: *reinterpret_cast<QStringList*>(_v) = _t->availableFonts(); break;
        case 14: *reinterpret_cast<QString*>(_v) = _t->aiAddress(); break;
        case 15: *reinterpret_cast<QString*>(_v) = _t->aiWireFormat(); break;
        case 16: *reinterpret_cast<QString*>(_v) = _t->aiModel(); break;
        case 17: *reinterpret_cast<bool*>(_v) = _t->aiKeyAvailable(); break;
        case 18: *reinterpret_cast<bool*>(_v) = _t->aiTesting(); break;
        case 19: *reinterpret_cast<QVariantList*>(_v) = _t->aiWireFormats(); break;
        case 20: *reinterpret_cast<QVariantList*>(_v) = _t->alternativeList(); break;
        case 21: *reinterpret_cast<int*>(_v) = _t->activeAlternative(); break;
        case 22: *reinterpret_cast<QStringList*>(_v) = _t->activeAlternativeVersions(); break;
        case 23: *reinterpret_cast<int*>(_v) = _t->activeAlternativeShowing(); break;
        case 24: *reinterpret_cast<bool*>(_v) = _t->pointerActive(); break;
        case 25: *reinterpret_cast<int*>(_v) = _t->pointerAlternative(); break;
        default: break;
        }
    }
    if (_c == QMetaObject::WriteProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 5: _t->setDarkMode(*reinterpret_cast<bool*>(_v)); break;
        case 12: _t->setEditorFont(*reinterpret_cast<QString*>(_v)); break;
        default: break;
        }
    }
}

const QMetaObject *Backend::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *Backend::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN7BackendE_t>.strings))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int Backend::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 101)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 101;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 101)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 101;
    }
    if (_c == QMetaObject::ReadProperty || _c == QMetaObject::WriteProperty
            || _c == QMetaObject::ResetProperty || _c == QMetaObject::BindableProperty
            || _c == QMetaObject::RegisterPropertyMetaType) {
        qt_static_metacall(this, _c, _id, _a);
        _id -= 26;
    }
    return _id;
}

// SIGNAL 0
void Backend::fileUrlChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 0, nullptr);
}

// SIGNAL 1
void Backend::modifiedChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 1, nullptr);
}

// SIGNAL 2
void Backend::statusChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 2, nullptr);
}

// SIGNAL 3
void Backend::wordCountChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 3, nullptr);
}

// SIGNAL 4
void Backend::darkModeChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 4, nullptr);
}

// SIGNAL 5
void Backend::textScaleChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 5, nullptr);
}

// SIGNAL 6
void Backend::themeColorsChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 6, nullptr);
}

// SIGNAL 7
void Backend::editorFontChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 7, nullptr);
}

// SIGNAL 8
void Backend::saveFailed()
{
    QMetaObject::activate(this, &staticMetaObject, 8, nullptr);
}

// SIGNAL 9
void Backend::openDialogRequested()
{
    QMetaObject::activate(this, &staticMetaObject, 9, nullptr);
}

// SIGNAL 10
void Backend::saveDialogRequested(const QUrl & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 10, nullptr, _t1);
}

// SIGNAL 11
void Backend::saveSucceeded()
{
    QMetaObject::activate(this, &staticMetaObject, 11, nullptr);
}

// SIGNAL 12
void Backend::externalChangeDetected(bool _t1, bool _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 12, nullptr, _t1, _t2);
}

// SIGNAL 13
void Backend::tagsApplied(const QStringList & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 13, nullptr, _t1);
}

// SIGNAL 14
void Backend::alternativesChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 14, nullptr);
}

// SIGNAL 15
void Backend::alternativeFlipped(bool _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 15, nullptr, _t1);
}

// SIGNAL 16
void Backend::pointerChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 16, nullptr);
}

// SIGNAL 17
void Backend::aiSettingsChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 17, nullptr);
}

// SIGNAL 18
void Backend::aiTestingChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 18, nullptr);
}

// SIGNAL 19
void Backend::aiTestFinished(bool _t1, const QString & _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 19, nullptr, _t1, _t2);
}
QT_WARNING_POP
