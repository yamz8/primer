import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Every shortcut, shown with ?.
Rectangle {
    id: overlay
    color: Qt.rgba(0, 0, 0, 0.72)
    signal closed()

    MouseArea {
        anchors.fill: parent
        onClicked: overlay.closed()
    }

    Rectangle {
        anchors.centerIn: parent
        width: Math.min(parent.width - 48, 560)
        height: Math.min(parent.height - 48, content.implicitHeight + 48)
        radius: theme.radius
        color: Palette.panel
        border.color: Palette.border

        MouseArea { anchors.fill: parent }

        ScrollView {
            anchors.fill: parent
            anchors.margins: 24
            contentWidth: availableWidth

            ColumnLayout {
                id: content
                width: parent.width
                spacing: 8

                Label {
                    text: qsTr("Keyboard")
                    font.pixelSize: 20
                    font.bold: true
                    Layout.bottomMargin: 8
                }
                Repeater {
                    model: [
                        ["Space", qsTr("Design pairs for the target")],
                        ["J  K  ↓  ↑", qsTr("Next and previous pair")],
                        ["Enter", qsTr("Pick or unpick the pair")],
                        ["C", qsTr("Switch between design and check")],
                        ["/", qsTr("Search the sequence, or type a position")],
                        ["S", qsTr("Settings")],
                        ["Ctrl+O", qsTr("Open a template or .primer file")],
                        ["Ctrl+V", qsTr("Paste a template")],
                        ["Ctrl+S", qsTr("Save")],
                        ["Ctrl+Shift+S", qsTr("Save as")],
                        ["Ctrl+E", qsTr("Export CSV, PDF, or SVG")],
                        ["Ctrl+Z", qsTr("Undo")],
                        ["Ctrl+Shift+Z", qsTr("Redo")],
                        ["Esc", qsTr("Close, or leave a field")],
                        ["?", qsTr("Show these keys")],
                        ["Q", qsTr("Quit")]
                    ]
                    delegate: RowLayout {
                        required property var modelData
                        Layout.fillWidth: true
                        spacing: 16
                        Rectangle {
                            Layout.preferredWidth: 130
                            implicitHeight: key.implicitHeight + 8
                            radius: Math.min(4, theme.radius)
                            color: Palette.raised
                            Label {
                                id: key
                                anchors.centerIn: parent
                                text: modelData[0]
                                font.family: Palette.mono
                                font.pixelSize: 13
                            }
                        }
                        Label {
                            Layout.fillWidth: true
                            text: modelData[1]
                            wrapMode: Text.Wrap
                            font.pixelSize: 14
                        }
                    }
                }
                Label {
                    Layout.topMargin: 12
                    Layout.fillWidth: true
                    text: qsTr("Drag across the sequence to set the target.")
                    color: Palette.muted
                    wrapMode: Text.Wrap
                }
            }
        }
    }
}
