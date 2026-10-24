import QtQuick
import QtQuick.Controls

Rectangle {
    property string status
    implicitWidth: chipLabel.implicitWidth + 12
    implicitHeight: chipLabel.implicitHeight + 4
    radius: Math.min(4, theme.radius)
    color: "transparent"
    border.color: Colors.status(status)

    Label {
        id: chipLabel
        anchors.centerIn: parent
        text: status === "pass" ? qsTr("Pass") : status === "warn" ? qsTr("Warn") : qsTr("Fail")
        color: Colors.status(status)
        font.pixelSize: 11
        font.bold: true
    }
}
