import QtQuick
import QtQuick.Controls

Item {
    id: panel
    objectName: "alternativesPanel"

    // The versions of the alternative the caret or pointer has chosen. The
    // empty row after them adds a version, and a row emptied by the writer
    // removes its version unless it is the only one left.
    property var versions: []
    property int showing: -1
    property bool darkMode: true
    property real textScale: 1
    property color textColor: "#d0d0d0"
    property color mutedColor: "#909191"
    property color accentColor: "#428bca"

    readonly property int count: versions ? versions.length : 0
    readonly property int rowCount: count + 1
    property int currentIndex: count

    signal versionChosen(int index)
    signal versionEdited(int index, string text)
    signal versionAdded(string text)
    signal versionRemoved(int index)

    function scaledSize(pixels) {
        return Math.max(1, Math.round(pixels * textScale));
    }

    function clampIndex(index) {
        return Math.max(0, Math.min(count, index));
    }

    function focusRow(index) {
        currentIndex = clampIndex(index);
        var field = emptyField;
        var row = null;
        if (currentIndex < count) {
            row = versionRepeater.itemAt(currentIndex);
            for (var child = 0; row && child < row.children.length; ++child) {
                if (row.children[child].objectName === "alternativeVersionField") {
                    field = row.children[child];
                    break;
                }
            }
        }
        if (!field)
            return;
        field.forceActiveFocus();
        if (field.selectAll)
            field.selectAll();
    }

    // Up and down walk the versions and the empty row, stopping at either end.
    function moveCurrent(direction) {
        if (direction !== 0)
            focusRow(currentIndex + direction);
    }

    function addVersion(text) {
        var wording = String(text).trim();
        if (wording.length === 0)
            return false;
        versionAdded(wording);
        return true;
    }

    function removeVersion(index) {
        if (index < 0 || index >= count || count <= 1)
            return false;
        versionRemoved(index);
        currentIndex = Math.min(index, count - 2);
        return true;
    }

    function chooseVersion(index) {
        if (index < 0 || index >= count)
            return false;
        currentIndex = index;
        versionChosen(index);
        return true;
    }

    function editVersion(index, text) {
        if (index < 0 || index >= count)
            return false;
        var wording = String(text).trim();
        if (wording.length === 0)
            return removeVersion(index);
        if (wording === versions[index])
            return false;
        versionEdited(index, wording);
        return true;
    }

    onVersionsChanged: {
        if (currentIndex > count)
            currentIndex = count;
    }

    Rectangle {
        anchors.fill: parent
        radius: 9
        color: panel.darkMode ? "#22221f" : "#fffef2"
        border.width: 1
        border.color: panel.mutedColor
        opacity: 0.97
    }

    Column {
        id: content
        anchors.fill: parent
        anchors.margins: panel.scaledSize(10)
        spacing: panel.scaledSize(6)

        Label {
            id: panelTitle
            text: "Versions"
            color: panel.mutedColor
            font.family: "IBM Plex Mono"
            font.pixelSize: panel.scaledSize(11)
        }

        Rectangle {
            id: titleDivider
            width: parent.width
            height: 1
            color: panel.mutedColor
            opacity: 0.3
        }

        Flickable {
            id: rowsFlick
            width: parent.width
            height: Math.max(0, parent.height - content.spacing * 2
                             - panelTitle.height - titleDivider.height)
            contentHeight: rowsColumn.height
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

            Column {
                id: rowsColumn
                width: rowsFlick.width

                Repeater {
                    id: versionRepeater
                    objectName: "alternativeVersionRepeater"
                    model: panel.versions

                    delegate: Item {
                        id: versionRow
                        objectName: "alternativeVersionRow"
                        required property string modelData
                        required property int index

                        readonly property int versionIndex: index
                        readonly property bool showingVersion: index === panel.showing

                        width: rowsColumn.width
                        height: panel.scaledSize(32)

                        Rectangle {
                            anchors.fill: parent
                            radius: 5
                            color: panel.accentColor
                            opacity: versionRow.index === panel.currentIndex ? 0.16
                                     : (versionRow.showingVersion ? 0.08 : 0)
                        }

                        Label {
                            id: showingMark
                            objectName: "alternativeShowingMark"
                            anchors.left: parent.left
                            anchors.verticalCenter: parent.verticalCenter
                            width: panel.scaledSize(18)
                            text: versionRow.showingVersion ? "✓" : ""
                            color: panel.accentColor
                            font.family: "IBM Plex Mono"
                            font.pixelSize: panel.scaledSize(14)
                            horizontalAlignment: Text.AlignHCenter
                        }

                        TextInput {
                            id: versionField
                            objectName: "alternativeVersionField"
                            anchors.left: showingMark.right
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            height: parent.height
                            verticalAlignment: TextInput.AlignVCenter
                            text: versionRow.modelData
                            color: panel.textColor
                            selectionColor: panel.accentColor
                            selectedTextColor: panel.darkMode ? "#1c1c1a" : "#ffffff"
                            selectByMouse: true
                            activeFocusOnTab: true
                            clip: true
                            font.family: "IBM Plex Mono"
                            font.pixelSize: panel.scaledSize(13)

                            onActiveFocusChanged: {
                                if (activeFocus)
                                    panel.currentIndex = versionRow.index;
                            }
                            onEditingFinished: {
                                var previousText = versionRow.modelData;
                                if (!panel.editVersion(versionRow.index, text))
                                    text = previousText;
                            }
                            Keys.onUpPressed: function(event) {
                                panel.moveCurrent(-1);
                                event.accepted = true;
                            }
                            Keys.onDownPressed: function(event) {
                                panel.moveCurrent(1);
                                event.accepted = true;
                            }
                        }
                    }
                }

                Item {
                    id: emptyRow
                    objectName: "alternativeEmptyRow"
                    width: rowsColumn.width
                    height: panel.scaledSize(32)

                    Rectangle {
                        anchors.fill: parent
                        radius: 5
                        color: panel.accentColor
                        opacity: panel.currentIndex === panel.count ? 0.12 : 0
                    }

                    Label {
                        anchors.left: parent.left
                        anchors.verticalCenter: parent.verticalCenter
                        width: panel.scaledSize(18)
                        text: "+"
                        color: panel.mutedColor
                        font.family: "IBM Plex Mono"
                        font.pixelSize: panel.scaledSize(14)
                        horizontalAlignment: Text.AlignHCenter
                    }

                    TextInput {
                        id: emptyField
                        objectName: "alternativeVersionInput"
                        anchors.left: parent.left
                        anchors.leftMargin: panel.scaledSize(18)
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        height: parent.height
                        verticalAlignment: TextInput.AlignVCenter
                        color: panel.textColor
                        selectionColor: panel.accentColor
                        selectedTextColor: panel.darkMode ? "#1c1c1a" : "#ffffff"
                        selectByMouse: true
                        activeFocusOnTab: true
                        clip: true
                        font.family: "IBM Plex Mono"
                        font.pixelSize: panel.scaledSize(13)

                        Label {
                            anchors.fill: parent
                            verticalAlignment: Text.AlignVCenter
                            text: "Add a version"
                            visible: emptyField.text.length === 0
                            color: panel.mutedColor
                            font: emptyField.font
                        }

                        onActiveFocusChanged: {
                            if (activeFocus)
                                panel.currentIndex = panel.count;
                        }
                        onEditingFinished: {
                            if (panel.addVersion(text)) {
                                text = "";
                                Qt.callLater(function() { panel.currentIndex = panel.count; });
                            }
                        }
                        Keys.onUpPressed: function(event) {
                            panel.moveCurrent(-1);
                            event.accepted = true;
                        }
                        Keys.onDownPressed: function(event) {
                            panel.moveCurrent(1);
                            event.accepted = true;
                        }
                    }
                }
            }
        }
    }
}
