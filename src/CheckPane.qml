import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

// Check mode: the user's own primers, with or without a template.
Rectangle {
    id: pane
    implicitHeight: column.implicitHeight + 28
    color: Colors.panel

    function focusForward() { forward.forceActiveFocus() }
    function update() { backend.setCheckPrimers(forward.text, reverse.text) }

    ColumnLayout {
        id: column
        anchors.fill: parent
        anchors.margins: 14
        spacing: 10

        RowLayout {
            Layout.fillWidth: true
            spacing: 12
            TextField {
                id: forward
                Layout.fillWidth: true
                Material.roundedScale: theme.radius
                placeholderText: qsTr("Forward primer, 5′→3′")
                text: backend.checkForward
                font.family: Colors.mono
                font.pixelSize: 15
                selectByMouse: true
                onTextEdited: pane.update()
                Keys.onTabPressed: reverse.forceActiveFocus()
            }
            TextField {
                id: reverse
                Layout.fillWidth: true
                Material.roundedScale: theme.radius
                placeholderText: qsTr("Reverse primer, 5′→3′ (optional)")
                text: backend.checkReverse
                font.family: Colors.mono
                font.pixelSize: 15
                selectByMouse: true
                onTextEdited: pane.update()
            }
        }
        RowLayout {
            Layout.fillWidth: true
            CheckBox {
                text: backend.length > 0 ? qsTr("Find binding sites on %1").arg(backend.name) : qsTr("No template open: binding sites are skipped")
                enabled: backend.length > 0
                checked: backend.checkUsesTemplate && backend.length > 0
                onToggled: backend.checkUsesTemplate = checked
            }
            Item { Layout.fillWidth: true }
            Label {
                visible: backend.checkReport.kind === "error"
                text: backend.checkReport.kind === "error" ? backend.checkReport.message : ""
                color: Colors.fail
            }
        }
    }
}
