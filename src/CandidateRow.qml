import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// One ranked pair in the candidate list.
Rectangle {
    id: row
    required property var modelData
    required property int index
    property bool current: false
    signal clicked()
    signal doubleClicked()

    height: 34
    color: current ? Qt.rgba(Qt.color(theme.accent).r, Qt.color(theme.accent).g, Qt.color(theme.accent).b, 0.16)
                   : mouse.containsMouse ? Colors.raised : "transparent"
    radius: Math.min(4, theme.radius)

    function tm(v) { return Number(v).toLocaleString(Qt.locale(), 'f', 1) }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 12
        anchors.rightMargin: 12
        spacing: 14

        Label {
            Layout.preferredWidth: 26
            text: row.index + 1
            color: Colors.muted
            horizontalAlignment: Text.AlignRight
        }
        Rectangle {
            width: 8
            height: 8
            radius: 4
            color: Colors.status(row.modelData.status)
        }
        Label {
            Layout.preferredWidth: 34
            text: row.modelData.score
            font.bold: true
            horizontalAlignment: Text.AlignRight
        }
        Label {
            Layout.preferredWidth: 64
            text: qsTr("%1 bp").arg(row.modelData.productSize)
            horizontalAlignment: Text.AlignRight
        }
        Label {
            Layout.fillWidth: true
            Layout.preferredWidth: 220
            text: row.modelData.forward.sequence
            font.family: Colors.mono
            elide: Text.ElideRight
        }
        Label {
            Layout.preferredWidth: 44
            text: row.tm(row.modelData.forward.tm)
            color: Colors.muted
        }
        Label {
            Layout.fillWidth: true
            Layout.preferredWidth: 220
            text: row.modelData.reverse.sequence
            font.family: Colors.mono
            elide: Text.ElideRight
        }
        Label {
            Layout.preferredWidth: 44
            text: row.tm(row.modelData.reverse.tm)
            color: Colors.muted
        }
        Label {
            Layout.preferredWidth: 18
            text: row.modelData.picked ? "✓" : ""
            color: theme.accent
            font.bold: true
        }
    }

    MouseArea {
        id: mouse
        anchors.fill: parent
        hoverEnabled: true
        onClicked: row.clicked()
        onDoubleClicked: row.doubleClicked()
    }
}
