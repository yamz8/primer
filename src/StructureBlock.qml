import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// A hairpin or dimer, drawn as aligned base pairs.
ColumnLayout {
    id: block
    property string title
    property var structure: ({})
    readonly property bool ready: structure !== undefined && structure !== null && structure.drawing !== undefined
    visible: ready
    spacing: 4

    Label {
        text: ready ? qsTr("%1 · ΔG %2 kcal/mol").arg(title).arg(Number(structure.dG).toLocaleString(Qt.locale(), 'f', 1)) : ""
        color: Palette.muted
        font.pixelSize: 12
    }
    Rectangle {
        Layout.fillWidth: true
        implicitHeight: drawing.implicitHeight + 16
        radius: Math.min(4, theme.radius)
        color: Palette.window
        clip: true
        // Base pairs read left to right in every language.
        LayoutMirroring.enabled: false
        LayoutMirroring.childrenInherit: true

        Flickable {
            anchors.fill: parent
            anchors.margins: 8
            contentWidth: drawing.implicitWidth
            contentHeight: drawing.implicitHeight
            flickableDirection: Flickable.HorizontalFlick
            boundsBehavior: Flickable.StopAtBounds
            TextEdit {
                id: drawing
                text: block.ready ? block.structure.drawing : ""
                readOnly: true
                selectByMouse: true
                font.family: Palette.mono
                font.pixelSize: 12
                color: Palette.text
                selectionColor: theme.accent
                selectedTextColor: theme.accentForeground
            }
        }
    }
}
