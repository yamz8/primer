import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

// Design limits and reaction conditions. Changes redesign right away.
Drawer {
    id: drawer
    edge: Qt.RightEdge
    width: 380
    Material.roundedScale: theme.radius

    background: Rectangle {
        color: Colors.panel
        Rectangle { width: 1; height: parent.height; color: Colors.border }
    }

    ScrollView {
        anchors.fill: parent
        contentWidth: availableWidth

        ColumnLayout {
            width: parent.width
            spacing: 6

            component Heading: Label {
                Layout.topMargin: 14
                font.pixelSize: 12
                font.bold: true
                font.capitalization: Font.AllUppercase
                color: Colors.muted
            }

            Item { Layout.preferredHeight: 4 }
            RowLayout {
                Layout.leftMargin: 20
                Layout.rightMargin: 20
                Label {
                    Layout.fillWidth: true
                    text: qsTr("Settings")
                    font.pixelSize: 18
                    font.bold: true
                }
                Button {
                    text: qsTr("Defaults")
                    flat: true
                    onClicked: backend.resetSettings()
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.leftMargin: 20
                Layout.rightMargin: 20
                spacing: 4

                Heading { text: qsTr("Primers") }
                SettingField { Layout.fillWidth: true; key: "lengthMin"; label: qsTr("Shortest"); unit: "nt" }
                SettingField { Layout.fillWidth: true; key: "lengthOpt"; label: qsTr("Best length"); unit: "nt" }
                SettingField { Layout.fillWidth: true; key: "lengthMax"; label: qsTr("Longest"); unit: "nt" }
                SettingField { Layout.fillWidth: true; key: "tmMin"; label: qsTr("Lowest Tm"); unit: "°C"; decimals: 1 }
                SettingField { Layout.fillWidth: true; key: "tmOpt"; label: qsTr("Best Tm"); unit: "°C"; decimals: 1 }
                SettingField { Layout.fillWidth: true; key: "tmMax"; label: qsTr("Highest Tm"); unit: "°C"; decimals: 1 }
                SettingField { Layout.fillWidth: true; key: "gcMin"; label: qsTr("Lowest GC"); unit: "%" }
                SettingField { Layout.fillWidth: true; key: "gcMax"; label: qsTr("Highest GC"); unit: "%" }
                SettingField { Layout.fillWidth: true; key: "gcClamp"; label: qsTr("GC clamp (3′ G/C bases)"); unit: "" }

                Heading { text: qsTr("Pairs") }
                SettingField { Layout.fillWidth: true; key: "tmDifference"; label: qsTr("Largest Tm difference"); unit: "°C"; decimals: 1 }
                SettingField { Layout.fillWidth: true; key: "productMin"; label: qsTr("Smallest product"); unit: "bp" }
                SettingField { Layout.fillWidth: true; key: "productMax"; label: qsTr("Largest product"); unit: "bp" }

                Heading { text: qsTr("Reaction") }
                SettingField { Layout.fillWidth: true; key: "sodium"; label: qsTr("Na⁺ and K⁺"); unit: "mM"; decimals: 1 }
                SettingField { Layout.fillWidth: true; key: "magnesium"; label: qsTr("Mg²⁺"); unit: "mM"; decimals: 2 }
                SettingField { Layout.fillWidth: true; key: "dntp"; label: qsTr("dNTPs, total"); unit: "mM"; decimals: 2 }
                SettingField { Layout.fillWidth: true; key: "primer"; label: qsTr("Each primer"); unit: "nM" }

                Label {
                    Layout.fillWidth: true
                    Layout.topMargin: 14
                    Layout.bottomMargin: 20
                    text: qsTr("Tm uses SantaLucia (1998) nearest-neighbor values with Mg²⁺ folded into a sodium equivalent. Structures are scored at 37 °C.")
                    wrapMode: Text.Wrap
                    color: Colors.muted
                    font.pixelSize: 12
                }
            }
        }
    }
}
