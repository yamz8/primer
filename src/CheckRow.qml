import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

RowLayout {
    property string status
    property string message
    spacing: 8

    Rectangle {
        Layout.alignment: Qt.AlignTop
        Layout.topMargin: 5
        width: 8
        height: 8
        radius: 4
        color: Colors.status(status)
    }
    Label {
        Layout.fillWidth: true
        text: message
        wrapMode: Text.Wrap
        color: status === "pass" ? Colors.muted : Colors.text
        font.pixelSize: 13
    }
}
