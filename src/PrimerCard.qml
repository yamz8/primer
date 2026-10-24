import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// One primer's numbers, checks, and worst structures.
Rectangle {
    id: card
    property var report: ({})
    property string title
    readonly property bool ready: report !== undefined && report.sequence !== undefined

    implicitHeight: column.implicitHeight + 28
    radius: theme.radius
    color: Colors.raised
    visible: ready

    ColumnLayout {
        id: column
        anchors.fill: parent
        anchors.margins: 14
        spacing: 8

        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            Label {
                text: card.title
                font.pixelSize: 15
                font.bold: true
            }
            StatusChip { status: card.ready ? report.status : "pass" }
            Item { Layout.fillWidth: true }
            Label {
                text: card.ready ? qsTr("Tm %1 °C").arg(Number(report.tm).toLocaleString(Qt.locale(), 'f', 1)) : ""
                font.pixelSize: 15
                font.bold: true
                color: theme.accent
            }
        }
        TextEdit {
            Layout.fillWidth: true
            text: card.ready ? "5′-" + report.sequence + "-3′" : ""
            readOnly: true
            selectByMouse: true
            wrapMode: TextEdit.WrapAnywhere
            font.family: Colors.mono
            font.pixelSize: 14
            color: Colors.text
            selectionColor: theme.accent
            selectedTextColor: theme.accentForeground
            LayoutMirroring.enabled: false
            horizontalAlignment: TextEdit.AlignLeft
        }
        Label {
            text: card.ready ? qsTr("%1 nt · GC %2% · 3′ end ΔG %3 kcal/mol")
                               .arg(report.length)
                               .arg(Number(report.gc).toLocaleString(Qt.locale(), 'f', 1))
                               .arg(Number(report.endStability).toLocaleString(Qt.locale(), 'f', 1)) : ""
            color: Colors.muted
            font.pixelSize: 12
        }
        Repeater {
            model: card.ready ? report.checks : []
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
            title: qsTr("Worst hairpin")
            structure: card.ready ? report.hairpin : ({})
        }
        StructureBlock {
            Layout.fillWidth: true
            title: qsTr("Worst self-dimer")
            structure: card.ready ? report.selfDimer : ({})
        }
    }
}
