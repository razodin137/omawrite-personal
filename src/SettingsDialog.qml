pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: root
    objectName: "settingsDialog"

    property bool darkMode: true
    property color textColor: darkMode ? "#d0d0d0" : "#42464c"
    property color strongTextColor: darkMode ? "#eeeeee" : "#222324"
    property color mutedColor: darkMode ? "#909191" : "#aeb1b5"
    property color activeButtonColor: "#428bca"
    property real textScale: 1
    property int containerWidth: 1280
    property int containerHeight: 820

    readonly property color fieldBackground: darkMode ? "#202020" : "#f6f6f6"
    readonly property color fieldBorder: darkMode ? "#424242" : "#c8c8c8"

    title: "Settings"
    modal: true
    focus: true
    closePolicy: Popup.CloseOnEscape
    implicitWidth: Math.round(620 * textScale)
    implicitHeight: header.implicitHeight + footer.implicitHeight
                    + topPadding + bottomPadding + settingsColumn.implicitHeight
    width: Math.max(0, Math.min(implicitWidth, containerWidth - 32))
    height: Math.max(0, Math.min(implicitHeight, containerHeight - 32))
    x: Math.round((containerWidth - width) / 2)
    y: Math.round((containerHeight - height) / 2)
    padding: 24

    function formatIndex(value) {
        for (var i = 0; i < backend.aiWireFormats.length; ++i) {
            if (backend.aiWireFormats[i].value === value)
                return i;
        }
        return 0;
    }

    function loadSettings() {
        addressField.text = backend.aiAddress;
        modelField.text = backend.aiModel;
        // The stored key never comes back to the interface; leaving this
        // field blank keeps whatever the native side already holds.
        keyField.text = "";
        wireFormatBox.currentIndex = formatIndex(backend.aiWireFormat);
        resultLabel.text = "";
    }

    onAboutToShow: loadSettings()
    onOpened: addressField.forceActiveFocus()
    onClosed: keyField.text = ""

    Overlay.modal: Rectangle { color: Qt.rgba(0, 0, 0, 0.65) }

    background: Rectangle {
        color: root.darkMode ? "#181818" : "#fbfbfb"
        border.color: root.darkMode ? "#343434" : "#d8d8d8"
        border.width: 1
        radius: 0
    }

    header: ColumnLayout {
        spacing: 0
        Label {
            Layout.fillWidth: true
            Layout.leftMargin: 24
            Layout.rightMargin: 24
            Layout.topMargin: 20
            Layout.bottomMargin: 8
            text: root.title
            color: root.strongTextColor
            font.family: "IBM Plex Mono"
            font.pixelSize: Math.round(15 * root.textScale)
            font.bold: true
            wrapMode: Text.Wrap
        }
    }

    contentItem: ColumnLayout {
        id: settingsColumn
        spacing: 10

        Label {
            Layout.fillWidth: true
            Layout.bottomMargin: 6
            text: "One language model answers every AI request. Leave a field empty to "
                  + "use its WRITECAM_AI_* environment variable."
            color: root.textColor
            opacity: 0.75
            font.family: "IBM Plex Mono"
            font.pixelSize: Math.round(11 * root.textScale)
            wrapMode: Text.Wrap
        }

        Label {
            text: "Address"
            color: root.strongTextColor
            font.family: "IBM Plex Mono"
            font.pixelSize: Math.round(12 * root.textScale)
            font.bold: true
        }
        TextField {
            id: addressField
            objectName: "aiAddressField"
            Layout.fillWidth: true
            selectByMouse: true
            color: root.textColor
            placeholderText: "https://api.example.com/v1/chat/completions"
            placeholderTextColor: root.mutedColor
            selectionColor: root.activeButtonColor
            selectedTextColor: "#ffffff"
            font.family: "IBM Plex Mono"
            font.pixelSize: Math.round(12 * root.textScale)
            background: Rectangle {
                implicitHeight: Math.round(34 * root.textScale)
                color: root.fieldBackground
                border.color: addressField.activeFocus ? root.activeButtonColor : root.fieldBorder
                radius: 0
            }
        }

        Label {
            text: "Wire format"
            color: root.strongTextColor
            font.family: "IBM Plex Mono"
            font.pixelSize: Math.round(12 * root.textScale)
            font.bold: true
        }
        ComboBox {
            id: wireFormatBox
            objectName: "aiWireFormatBox"
            Layout.fillWidth: true
            Layout.preferredHeight: Math.round(34 * root.textScale)
            model: backend.aiWireFormats
            textRole: "label"
            font.family: "IBM Plex Mono"
            font.pixelSize: Math.round(12 * root.textScale)
            background: Rectangle {
                color: root.fieldBackground
                border.color: wireFormatBox.activeFocus
                    ? root.activeButtonColor : root.fieldBorder
                radius: 0
            }
            contentItem: Label {
                leftPadding: 12
                rightPadding: wireFormatBox.indicator.width + 8
                text: wireFormatBox.displayText
                color: root.textColor
                verticalAlignment: Text.AlignVCenter
                elide: Text.ElideRight
                font.family: "IBM Plex Mono"
                font.pixelSize: Math.round(12 * root.textScale)
            }
            delegate: ItemDelegate {
                id: formatDelegate
                width: wireFormatBox.width
                implicitHeight: Math.round(30 * root.textScale)
                contentItem: Label {
                    leftPadding: 12
                    text: modelData.label
                    color: root.textColor
                    verticalAlignment: Text.AlignVCenter
                    font.family: "IBM Plex Mono"
                    font.pixelSize: Math.round(12 * root.textScale)
                }
                background: Rectangle {
                    color: formatDelegate.hovered
                        ? (root.darkMode ? "#242424" : "#eeeeee")
                        : (root.darkMode ? "#202020" : "#f6f6f6")
                }
            }
        }

        Label {
            text: "Model"
            color: root.strongTextColor
            font.family: "IBM Plex Mono"
            font.pixelSize: Math.round(12 * root.textScale)
            font.bold: true
        }
        TextField {
            id: modelField
            objectName: "aiModelField"
            Layout.fillWidth: true
            selectByMouse: true
            color: root.textColor
            placeholderText: "gpt-4o-mini"
            placeholderTextColor: root.mutedColor
            selectionColor: root.activeButtonColor
            selectedTextColor: "#ffffff"
            font.family: "IBM Plex Mono"
            font.pixelSize: Math.round(12 * root.textScale)
            background: Rectangle {
                implicitHeight: Math.round(34 * root.textScale)
                color: root.fieldBackground
                border.color: modelField.activeFocus ? root.activeButtonColor : root.fieldBorder
                radius: 0
            }
        }

        Label {
            text: "Key"
            color: root.strongTextColor
            font.family: "IBM Plex Mono"
            font.pixelSize: Math.round(12 * root.textScale)
            font.bold: true
        }
        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            TextField {
                id: keyField
                objectName: "aiKeyField"
                Layout.fillWidth: true
                selectByMouse: true
                echoMode: TextInput.Password
                color: root.textColor
                placeholderText: backend.aiKeyAvailable
                    ? "A key is set — leave blank to keep it"
                    : "No key set"
                placeholderTextColor: root.mutedColor
                selectionColor: root.activeButtonColor
                selectedTextColor: "#ffffff"
                font.family: "IBM Plex Mono"
                font.pixelSize: Math.round(12 * root.textScale)
                background: Rectangle {
                    implicitHeight: Math.round(34 * root.textScale)
                    color: root.fieldBackground
                    border.color: keyField.activeFocus
                        ? root.activeButtonColor : root.fieldBorder
                    radius: 0
                }
            }

            SquareDialogButton {
                objectName: "forgetKeyButton"
                text: "Forget"
                visible: backend.aiKeyAvailable
                darkMode: root.darkMode
                textScale: root.textScale
                activeColor: root.activeButtonColor
                onClicked: {
                    backend.clearAiKey();
                    keyField.text = "";
                    resultLabel.text = "The stored key was forgotten.";
                    resultLabel.color = root.textColor;
                }
            }
        }

        Label {
            id: resultLabel
            objectName: "aiTestResult"
            Layout.fillWidth: true
            Layout.topMargin: 4
            color: root.textColor
            font.family: "IBM Plex Mono"
            font.pixelSize: Math.round(11 * root.textScale)
            wrapMode: Text.Wrap
        }
    }

    footer: Item {
        implicitHeight: Math.round(34 * root.textScale) + 20

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 24
            anchors.rightMargin: 24
            spacing: 8

            SquareDialogButton {
                id: testButton
                objectName: "aiTestButton"
                text: backend.aiTesting ? "Testing…" : "Test connection"
                enabled: !backend.aiTesting
                darkMode: root.darkMode
                textScale: root.textScale
                activeColor: root.activeButtonColor
                onClicked: {
                    resultLabel.text = "Connecting…";
                    resultLabel.color = root.mutedColor;
                    backend.testAiConnection(addressField.text,
                        backend.aiWireFormats[wireFormatBox.currentIndex].value,
                        modelField.text, keyField.text);
                }
            }

            Item { Layout.fillWidth: true }

            SquareDialogButton {
                objectName: "aiCancelButton"
                text: "Cancel"
                darkMode: root.darkMode
                textScale: root.textScale
                activeColor: root.activeButtonColor
                onClicked: root.close()
            }

            SquareDialogButton {
                id: saveButton
                objectName: "aiSaveButton"
                text: "Save"
                primary: true
                darkMode: root.darkMode
                textScale: root.textScale
                activeColor: root.activeButtonColor
                onClicked: {
                    backend.saveAiSettings(addressField.text,
                        backend.aiWireFormats[wireFormatBox.currentIndex].value,
                        modelField.text, keyField.text);
                    root.close();
                }
            }
        }
    }

    Connections {
        target: backend
        function onAiTestFinished(success, message) {
            resultLabel.text = success ? "Connection works." : message;
            resultLabel.color = success ? root.activeButtonColor : "#d9534f";
        }
    }
}
