import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// The checks that need both primers.
Rectangle {
    id: card
    property var pair: ({})
    readonly property bool ready: pair !== undefined && pair.forward !== undefined

    implicitHeight: column.implicitHeight + 28
    radius: theme.radius
    color: Palette.raised
    visible: ready

    ColumnLayout {
        id: column
        anchors.fill: parent
        anchors.margins: 14
        spacing: 8

        RowLayout {
            spacing: 8
            Label {
                text: qsTr("Pair")
                font.pixelSize: 15
                font.bold: true
            }
            StatusChip { status: card.ready ? pair.status : "pass" }
        }
        Repeater {
            model: card.ready ? pair.checks : []
            delegate: CheckRow {
                required property var modelData
                Layout.fillWidth: true
                status: modelData.status
                message: modelData.message
            }
        }
        StructureBlock {
            Layout.fillWidth: true
            Layout.topMargin: 4
            title: qsTr("Worst cross-dimer")
            structure: card.ready ? pair.crossDimer : ({})
        }
    }
}
