import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

// One numeric setting, committed when editing finishes.
RowLayout {
    id: row
    property string key
    property string label
    property string unit
    property int decimals: 0
    spacing: 8

    function shown() {
        translator.language  // re-render in the new language's number format
        const value = backend.settings[key]
        return value === undefined ? "" : Number(value).toLocaleString(Qt.locale(), 'f', decimals)
    }

    Label {
        Layout.fillWidth: true
        text: row.label
        elide: Text.ElideRight
        font.pixelSize: 13
    }
    TextField {
        id: field
        Layout.preferredWidth: 84
        Material.roundedScale: theme.radius
        Material.containerStyle: Material.Outlined
        topPadding: 6
        bottomPadding: 6
        horizontalAlignment: TextInput.AlignRight
        font.pixelSize: 13
        text: row.shown()
        inputMethodHints: Qt.ImhFormattedNumbersOnly
        selectByMouse: true
        onEditingFinished: {
            let value = Number.fromLocaleString(Qt.locale(), text.trim())
            if (isNaN(value))
                value = parseFloat(text.replace(",", "."))
            if (isNaN(value) || !backend.setSetting(row.key, value))
                text = row.shown()
        }
        Connections {
            target: backend
            function onSettingsChanged() { field.text = row.shown() }
        }
    }
    Label {
        Layout.preferredWidth: 34
        text: row.unit
        color: Palette.muted
        font.pixelSize: 12
    }
}
