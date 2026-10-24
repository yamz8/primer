pragma Singleton
import QtQuick

// The window's own colors; the accent comes from the Omarchy theme.
QtObject {
    readonly property color window: "#0e0e10"
    readonly property color panel: "#151518"
    readonly property color raised: "#1c1c21"
    readonly property color border: "#2a2a31"
    readonly property color text: "#e6e6e9"
    readonly property color muted: "#8b8b96"
    readonly property color pass: "#7fc77a"
    readonly property color warn: "#e5b567"
    readonly property color fail: "#f0717f"
    readonly property string mono: "monospace"

    function status(s) {
        return s === "pass" ? pass : s === "warn" ? warn : fail
    }
}
