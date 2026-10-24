import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import Primer

ApplicationWindow {
    id: win
    width: 1360
    height: 860
    minimumWidth: 980
    minimumHeight: 620
    visible: true
    title: (backend.length > 0 ? qsTr("%1 — Primer").arg(backend.name) : "Primer") + (backend.dirty ? " •" : "")
    color: Colors.window

    Material.theme: Material.Dark
    Material.accent: theme.accent
    Material.roundedScale: theme.radius

    property bool helpVisible: false
    property bool searchVisible: false
    property bool quitting: false
    readonly property bool designMode: backend.mode === "design"
    // Single-key shortcuts stand down while a text field has the keyboard.
    readonly property bool typing: activeFocusItem !== null && activeFocusItem.cursorPosition !== undefined
    readonly property bool blocked: typing || helpVisible || confirm.visible || pasteDialog.visible || settings.opened

    function num(value, decimals) {
        translator.language
        return Number(value).toLocaleString(Qt.locale(), 'f', decimals)
    }

    function leaveField() {
        sequenceArea.forceActiveFocus()
    }

    // Runs the action now, or after asking when unsaved work would be lost.
    function guard(question, action) {
        if (!backend.dirty)
            action()
        else
            confirm.ask(question, action)
    }

    function quit() {
        guard(qsTr("Save your changes before quitting?"), function () {
            win.quitting = true
            Qt.quit()
        })
    }

    onClosing: function (close) {
        if (backend.dirty && !quitting) {
            close.accepted = false
            quit()
        }
    }

    Shortcut { sequence: "?"; enabled: !win.typing; context: Qt.ApplicationShortcut; onActivated: win.helpVisible = !win.helpVisible }
    Shortcut { sequence: "Q"; enabled: !win.blocked; context: Qt.ApplicationShortcut; onActivated: win.quit() }
    Shortcut {
        sequence: "Space"
        enabled: !win.blocked
        context: Qt.ApplicationShortcut
        onActivated: win.designMode ? backend.design() : checkPane.focusForward()
    }
    Shortcut { sequences: ["J", "Down"]; enabled: !win.blocked && win.designMode; context: Qt.ApplicationShortcut; onActivated: backend.move(1) }
    Shortcut { sequences: ["K", "Up"]; enabled: !win.blocked && win.designMode; context: Qt.ApplicationShortcut; onActivated: backend.move(-1) }
    Shortcut { sequences: ["Return", "Enter"]; enabled: !win.blocked && win.designMode; context: Qt.ApplicationShortcut; onActivated: backend.togglePick() }
    Shortcut {
        sequence: "C"
        enabled: !win.blocked
        context: Qt.ApplicationShortcut
        onActivated: backend.mode = win.designMode ? "check" : "design"
    }
    Shortcut {
        sequence: "/"
        enabled: !win.blocked && backend.length > 0
        context: Qt.ApplicationShortcut
        onActivated: {
            win.searchVisible = true
            searchField.forceActiveFocus()
            searchField.selectAll()
        }
    }
    Shortcut { sequence: "S"; enabled: !win.blocked; context: Qt.ApplicationShortcut; onActivated: settings.open() }
    Shortcut { sequence: "Ctrl+O"; context: Qt.ApplicationShortcut; onActivated: win.guard(qsTr("Save your changes before opening another file?"), backend.openFile) }
    Shortcut {
        sequence: "Ctrl+V"
        enabled: !win.blocked
        context: Qt.ApplicationShortcut
        onActivated: win.guard(qsTr("Save your changes before replacing the template?"), backend.pasteClipboard)
    }
    Shortcut { sequence: "Ctrl+S"; context: Qt.ApplicationShortcut; onActivated: backend.save() }
    Shortcut { sequence: "Ctrl+Shift+S"; context: Qt.ApplicationShortcut; onActivated: backend.saveAs() }
    Shortcut { sequence: "Ctrl+E"; context: Qt.ApplicationShortcut; onActivated: backend.exportFile() }
    Shortcut { sequences: [StandardKey.Undo]; enabled: !win.typing; context: Qt.ApplicationShortcut; onActivated: backend.undo() }
    Shortcut { sequences: ["Ctrl+Shift+Z", StandardKey.Redo]; enabled: !win.typing; context: Qt.ApplicationShortcut; onActivated: backend.redo() }
    Shortcut {
        sequence: "Escape"
        context: Qt.ApplicationShortcut
        onActivated: {
            if (win.helpVisible)
                win.helpVisible = false
            else if (settings.opened)
                settings.close()
            else if (win.searchVisible) {
                win.searchVisible = false
                backend.search("")
                win.leaveField()
            } else if (backend.error !== "")
                backend.dismissError()
            else
                win.leaveField()
        }
    }

    Connections {
        target: backend
        function onReveal(index) { sequence.ensureVisible(index) }
        function onSaveCancelled() { confirm.pendingAction = null }
        function onSaved() {
            if (confirm.pendingAction) {
                const action = confirm.pendingAction
                confirm.pendingAction = null
                action()
            }
        }
    }

    // Right-to-left languages mirror the window; the Translator sets the
    // application's layout direction.
    Item {
        anchors.fill: parent
        LayoutMirroring.enabled: Qt.application.layoutDirection === Qt.RightToLeft
        LayoutMirroring.childrenInherit: true

        ColumnLayout {
            anchors.fill: parent
            spacing: 0

            // Title bar: the template, the mode, and the file actions.
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 54
                color: Colors.panel

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 18
                    anchors.rightMargin: 10
                    spacing: 14

                    Label {
                        text: "Primer"
                        font.pixelSize: 17
                        font.bold: true
                    }
                    Label {
                        Layout.fillWidth: true
                        text: backend.length > 0 ? qsTr("%1 · %2 bp").arg(backend.name).arg(win.num(backend.length, 0)) : qsTr("No template")
                        color: Colors.muted
                        elide: Text.ElideRight
                        font.pixelSize: 14
                    }
                    TabBar {
                        id: modeBar
                        currentIndex: win.designMode ? 0 : 1
                        background: null
                        TabButton {
                            text: qsTr("Design")
                            width: implicitWidth + 12
                            onClicked: backend.mode = "design"
                        }
                        TabButton {
                            text: qsTr("Check")
                            width: implicitWidth + 12
                            onClicked: backend.mode = "check"
                        }
                    }
                    ToolSeparator {}
                    ToolButton { text: qsTr("Open"); onClicked: win.guard(qsTr("Save your changes before opening another file?"), backend.openFile) }
                    ToolButton { text: qsTr("Paste"); onClicked: win.guard(qsTr("Save your changes before replacing the template?"), pasteDialog.open) }
                    ToolButton { text: qsTr("Save"); onClicked: backend.save() }
                    ToolButton { text: qsTr("Export"); onClicked: backend.exportFile() }
                    ToolButton { text: qsTr("Settings"); onClicked: settings.open() }
                    ToolButton { text: "?"; onClicked: win.helpVisible = true }
                }
                Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: Colors.border }
            }

            // Errors stay until dismissed or fixed.
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: visible ? errorLabel.implicitHeight + 16 : 0
                visible: backend.error !== ""
                color: Qt.rgba(0.94, 0.44, 0.5, 0.16)

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 18
                    anchors.rightMargin: 10
                    Label {
                        id: errorLabel
                        Layout.fillWidth: true
                        text: backend.error
                        color: Colors.fail
                        wrapMode: Text.Wrap
                    }
                    ToolButton { text: "✕"; onClicked: backend.dismissError() }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 0

                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    spacing: 0

                    // Design: the target. Check: the user's primers.
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 58
                        visible: win.designMode && backend.length > 0
                        color: Colors.panel

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 18
                            anchors.rightMargin: 14
                            spacing: 12

                            Label { text: qsTr("Target"); font.bold: true }
                            TextField {
                                id: regionField
                                Layout.preferredWidth: 150
                                Material.roundedScale: theme.radius
                                Material.containerStyle: Material.Outlined
                                topPadding: 6
                                bottomPadding: 6
                                text: backend.regionText
                                placeholderText: qsTr("start..end")
                                font.family: Colors.mono
                                selectByMouse: true
                                onAccepted: {
                                    if (backend.setRegionText(text))
                                        win.leaveField()
                                }
                                Connections {
                                    target: backend
                                    function onRegionChanged() { regionField.text = backend.regionText }
                                }
                            }
                            Label {
                                Layout.fillWidth: true
                                text: backend.regionStart >= 0
                                      ? qsTr("%1 bp selected. Products %2–%3 bp.").arg(backend.regionEnd - backend.regionStart + 1)
                                            .arg(win.num(backend.settings.productMin, 0)).arg(win.num(backend.settings.productMax, 0))
                                      : qsTr("Drag across the sequence, or type start..end")
                                color: Colors.muted
                                elide: Text.ElideRight
                            }
                            AccentButton {
                                text: qsTr("Design  ␣")
                                enabled: backend.regionStart >= 0
                                onClicked: backend.design()
                            }
                        }
                    }
                    CheckPane {
                        id: checkPane
                        Layout.fillWidth: true
                        visible: !win.designMode
                    }
                    Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: Colors.border }

                    // The template.
                    FocusScope {
                        id: sequenceArea
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true

                        SequenceView {
                            id: sequence
                            anchors.fill: parent
                            anchors.rightMargin: 12
                            visible: backend.length > 0
                            // DNA reads left to right in every language.
                            LayoutMirroring.enabled: false
                            sequence: backend.sequence
                            selectionStart: backend.regionStart
                            selectionEnd: backend.regionEnd
                            marks: backend.marks
                            accent: theme.accent
                            accentForeground: theme.accentForeground
                            textColor: Colors.text
                            mutedColor: Colors.muted
                            onSelectionEdited: function (start, end) {
                                win.leaveField()
                                backend.setRegion(start, end)
                            }
                        }
                        ScrollBar {
                            id: sequenceScroll
                            anchors.right: parent.right
                            anchors.top: parent.top
                            anchors.bottom: parent.bottom
                            visible: backend.length > 0
                            orientation: Qt.Vertical
                            policy: sequence.contentHeight > sequence.height ? ScrollBar.AlwaysOn : ScrollBar.AlwaysOff
                            size: sequence.contentHeight > 0 ? Math.min(1, sequence.height / sequence.contentHeight) : 1
                            position: sequence.contentHeight > 0 ? sequence.contentY / sequence.contentHeight : 0
                            onPositionChanged: if (pressed) sequence.contentY = position * sequence.contentHeight
                        }

                        // No template yet: paste or open one.
                        ColumnLayout {
                            anchors.centerIn: parent
                            width: Math.min(parent.width - 64, 640)
                            visible: backend.length === 0
                            spacing: 14

                            Label {
                                Layout.fillWidth: true
                                text: win.designMode ? qsTr("Paste a template sequence, or open a FASTA or GenBank file")
                                                     : qsTr("Check mode needs no template. Open one to find binding sites and the product.")
                                font.pixelSize: 18
                                wrapMode: Text.Wrap
                                horizontalAlignment: Text.AlignHCenter
                            }
                            ScrollView {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 180
                                TextArea {
                                    id: emptyPaste
                                    Material.roundedScale: theme.radius
                                    Material.containerStyle: Material.Outlined
                                    placeholderText: qsTr("ACGT…, or a whole FASTA or GenBank record")
                                    font.family: Colors.mono
                                    wrapMode: TextEdit.WrapAnywhere
                                    selectByMouse: true
                                }
                            }
                            RowLayout {
                                Layout.alignment: Qt.AlignHCenter
                                spacing: 10
                                AccentButton {
                                    text: qsTr("Use this sequence")
                                    enabled: emptyPaste.text.trim().length > 0
                                    onClicked: if (backend.loadText(emptyPaste.text, "")) emptyPaste.text = ""
                                }
                                Button { text: qsTr("Open file…"); flat: true; onClicked: backend.openFile() }
                                Button { text: qsTr("Paste from clipboard"); flat: true; onClicked: backend.pasteClipboard() }
                            }
                        }

                        // Search: a sequence (either strand) or a position.
                        Rectangle {
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.bottom: parent.bottom
                            anchors.margins: 12
                            height: 52
                            visible: win.searchVisible
                            radius: theme.radius
                            color: Colors.raised
                            border.color: Colors.border

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 14
                                anchors.rightMargin: 8
                                spacing: 10
                                Label { text: "/"; color: Colors.muted; font.family: Colors.mono; font.pixelSize: 16 }
                                TextField {
                                    id: searchField
                                    Layout.fillWidth: true
                                    Material.containerStyle: Material.Filled
                                    background: null
                                    placeholderText: qsTr("Sequence or position")
                                    font.family: Colors.mono
                                    selectByMouse: true
                                    onTextEdited: backend.search(text)
                                    Keys.onReturnPressed: function (event) { backend.nextMatch(event.modifiers & Qt.ShiftModifier ? -1 : 1) }
                                    Keys.onEnterPressed: function (event) { backend.nextMatch(event.modifiers & Qt.ShiftModifier ? -1 : 1) }
                                }
                                Label {
                                    text: backend.searchQuery === "" ? "" : backend.matchCount === 0 ? qsTr("No match")
                                          : qsTr("%1 of %2").arg(backend.matchIndex + 1).arg(backend.matchCount)
                                    color: backend.matchCount === 0 ? Colors.fail : Colors.muted
                                }
                            }
                        }
                    }

                    // Ranked candidates.
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 1
                        visible: win.designMode && backend.designed
                        color: Colors.border
                    }
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 250
                        visible: win.designMode && backend.designed
                        color: Colors.panel

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.topMargin: 10
                            spacing: 4

                            RowLayout {
                                Layout.fillWidth: true
                                Layout.leftMargin: 18
                                Layout.rightMargin: 18
                                Label {
                                    text: qsTr("%n pairs", "", backend.pairs.length)
                                    font.bold: true
                                }
                                Label {
                                    Layout.fillWidth: true
                                    text: backend.pickedCount > 0 ? qsTr("%n picked for export", "", backend.pickedCount)
                                                                  : qsTr("Enter picks a pair for export")
                                    color: Colors.muted
                                    horizontalAlignment: Text.AlignRight
                                }
                            }
                            ListView {
                                id: candidates
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                Layout.leftMargin: 6
                                Layout.rightMargin: 6
                                clip: true
                                model: backend.pairs
                                currentIndex: backend.currentIndex
                                highlightFollowsCurrentItem: false
                                boundsBehavior: Flickable.StopAtBounds
                                ScrollBar.vertical: ScrollBar {}
                                onCurrentIndexChanged: Qt.callLater(positionViewAtIndex, currentIndex, ListView.Contain)
                                delegate: CandidateRow {
                                    width: candidates.width - 12
                                    current: index === backend.currentIndex
                                    onClicked: backend.currentIndex = index
                                    onDoubleClicked: backend.togglePick(index)
                                }
                            }
                        }
                    }
                }

                Rectangle { Layout.fillHeight: true; Layout.preferredWidth: 1; color: Colors.border }

                // The report for the current pair, or for the checked primers.
                Rectangle {
                    Layout.preferredWidth: 480
                    Layout.fillHeight: true
                    color: Colors.panel

                    ScrollView {
                        id: reportScroll
                        anchors.fill: parent
                        contentWidth: availableWidth

                        PairDetail {
                            width: reportScroll.availableWidth - 36
                            x: 18
                            y: 18
                            heading: win.designMode ? (backend.currentIndex >= 0 ? qsTr("Pair %1").arg(backend.currentIndex + 1) : "")
                                                    : qsTr("Check")
                            pair: win.designMode ? backend.current
                                                 : (backend.checkReport.kind === "pair" ? backend.checkReport.pair : ({}))
                            primer: !win.designMode && backend.checkReport.kind === "primer" ? backend.checkReport.primer : ({})
                        }
                    }
                    Label {
                        anchors.centerIn: parent
                        width: parent.width - 64
                        visible: win.designMode ? backend.currentIndex < 0 : backend.checkReport.kind !== "pair" && backend.checkReport.kind !== "primer"
                        text: win.designMode ? (backend.length === 0 ? qsTr("Open a template to design primers.")
                                                : backend.regionStart < 0 ? qsTr("Select a target, then press Space.")
                                                : backend.designed ? qsTr("No pair to show.") : qsTr("Press Space to design pairs."))
                                             : qsTr("Type a primer to check it. Add a reverse primer to check the pair.")
                        color: Colors.muted
                        wrapMode: Text.Wrap
                        horizontalAlignment: Text.AlignHCenter
                    }
                }
            }

            // Status line.
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 28
                color: Colors.panel
                Rectangle { width: parent.width; height: 1; color: Colors.border }
                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 18
                    anchors.rightMargin: 18
                    Label {
                        Layout.fillWidth: true
                        text: backend.message
                        color: Colors.muted
                        font.pixelSize: 12
                        elide: Text.ElideRight
                    }
                    Label {
                        text: qsTr("Predictions — confirm in the lab · ? for keys")
                        color: Colors.muted
                        font.pixelSize: 12
                    }
                }
            }
        }

        HelpOverlay {
            anchors.fill: parent
            visible: win.helpVisible
            onClosed: win.helpVisible = false
        }
    }

    SettingsPanel {
        id: settings
        height: win.height
    }

    // Replacing the template from pasted text.
    Dialog {
        id: pasteDialog
        anchors.centerIn: parent
        width: Math.min(win.width - 80, 720)
        modal: true
        title: qsTr("Paste a template")
        onOpened: pasteArea.forceActiveFocus()

        ColumnLayout {
            anchors.fill: parent
            spacing: 12
            ScrollView {
                Layout.fillWidth: true
                Layout.preferredHeight: 260
                TextArea {
                    id: pasteArea
                    font.family: Colors.mono
                    wrapMode: TextEdit.WrapAnywhere
                    placeholderText: qsTr("ACGT…, or a whole FASTA or GenBank record")
                    selectByMouse: true
                }
            }
        }
        footer: DialogButtonBox {
            AccentButton {
                text: qsTr("Use this sequence")
                DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
            }
            Button {
                text: qsTr("Cancel")
                flat: true
                DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
            }
        }
        onAccepted: {
            if (backend.loadText(pasteArea.text, ""))
                pasteArea.text = ""
        }
    }

    // Asks before unsaved work is lost: save first, discard, or stay.
    Dialog {
        id: confirm
        property var action: null
        property var pendingAction: null
        property string question
        anchors.centerIn: parent
        width: Math.min(win.width - 80, 460)
        modal: true
        title: qsTr("Unsaved changes")

        function ask(text, then) {
            question = text
            action = then
            open()
        }

        Label {
            width: parent.width
            text: confirm.question
            wrapMode: Text.Wrap
        }
        footer: DialogButtonBox {
            AccentButton {
                text: qsTr("Save")
                onClicked: {
                    confirm.pendingAction = confirm.action
                    confirm.close()
                    backend.save()
                }
            }
            Button {
                text: qsTr("Discard")
                flat: true
                onClicked: {
                    const action = confirm.action
                    confirm.close()
                    backend.dismissError()
                    action()
                }
            }
            Button {
                text: qsTr("Cancel")
                flat: true
                onClicked: confirm.close()
            }
        }
    }
}
