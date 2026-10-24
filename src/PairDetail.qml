import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// The report for a pair, or for a single primer in check mode.
ColumnLayout {
    id: detail
    property var pair: ({})
    property var primer: ({})
    property string heading
    readonly property bool hasPair: pair !== undefined && pair.forward !== undefined
    readonly property bool hasPrimer: primer !== undefined && primer.sequence !== undefined
    spacing: 12

    RowLayout {
        Layout.fillWidth: true
        visible: detail.hasPair || detail.hasPrimer
        Label {
            text: detail.heading
            font.pixelSize: 18
            font.bold: true
            Layout.fillWidth: true
            elide: Text.ElideRight
        }
        Label {
            visible: detail.hasPair && pair.score > 0
            text: detail.hasPair ? qsTr("Score %1").arg(pair.score) : ""
            color: Palette.muted
            font.pixelSize: 13
        }
    }
    Label {
        visible: detail.hasPair && pair.start >= 0
        text: detail.hasPair ? qsTr("%1 bp product, %2..%3").arg(pair.productSize).arg(pair.start + 1).arg(pair.end + 1) : ""
        font.pixelSize: 14
        color: theme.accent
    }
    PrimerCard {
        Layout.fillWidth: true
        title: qsTr("Primer")
        report: detail.hasPrimer ? detail.primer : ({})
    }
    PrimerCard {
        Layout.fillWidth: true
        title: qsTr("Forward")
        report: detail.hasPair ? pair.forward : ({})
    }
    PrimerCard {
        Layout.fillWidth: true
        title: qsTr("Reverse")
        report: detail.hasPair ? pair.reverse : ({})
    }
    PairCard {
        Layout.fillWidth: true
        pair: detail.hasPair ? detail.pair : ({})
    }
    Label {
        Layout.fillWidth: true
        visible: detail.hasPair || detail.hasPrimer
        text: qsTr("These are predictions. Confirm primers in the lab before relying on them.")
        wrapMode: Text.Wrap
        color: Palette.muted
        font.pixelSize: 12
        font.italic: true
    }
}
