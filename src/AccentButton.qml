import QtQuick
import QtQuick.Controls

// The main action in a place, filled with the theme accent.
Button {
    id: button
    font.bold: true
    contentItem: Label {
        text: button.text
        font: button.font
        color: button.enabled ? theme.accentForeground : Colors.muted
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }
    background: Rectangle {
        implicitWidth: 96
        implicitHeight: 38
        radius: Math.min(6, theme.radius)
        color: !button.enabled ? Colors.raised : button.down ? Qt.darker(theme.accent, 1.2)
             : button.hovered ? Qt.lighter(theme.accent, 1.1) : theme.accent
    }
}
