import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// ── Flutter SummaryCards — 2x2 grid with icons (matching Flutter) ─────
ColumnLayout {
    id: summaryRoot
    spacing: 0
    property int pass: 0; property int warn: 0; property int fail: 0; property int skip: 0

    // Header: "Summary" + "Total: N" (Flutter: fontSize 11/10, w600, letterSpacing 0.6)
    RowLayout {
        Label { Layout.fillWidth: true; text: "Summary"; font.family: "JetBrains Mono"; font.pixelSize: 11; font.weight: Font.DemiBold; color: Theme.textSecondary }
        Label { text: "Total: " + (pass+warn+fail+skip); font.family: "JetBrains Mono"; font.pixelSize: 10; color: Theme.textSecondary }
    }
    Item { Layout.preferredHeight: 8 }

    // Row 1: Pass + Warning
    RowLayout { spacing: 6
        SummaryCard { Layout.fillWidth: true; accent: Theme.passGreen;  label: "Pass";    count: summaryRoot.pass }
        SummaryCard { Layout.fillWidth: true; accent: Theme.warnYellow; label: "Warning"; count: summaryRoot.warn }
    }
    Item { Layout.preferredHeight: 6 }
    // Row 2: Fail + Skipped
    RowLayout { spacing: 6
        SummaryCard { Layout.fillWidth: true; accent: Theme.failRed;  label: "Fail";    count: summaryRoot.fail }
        SummaryCard { Layout.fillWidth: true; accent: Theme.skipGray; label: "Skipped"; count: summaryRoot.skip }
    }

    Connections {
        target: appState
        function onProgressChanged() { refresh() }
        function onTestCompleted() { refresh() }
        function onResultsReset() { pass=warn=fail=skip=0 }
    }
    Component.onCompleted: refresh()
    function refresh() {
        pass=0; warn=0; fail=0; skip=0
        for (var g=0; g<appState.groupLabels.length; g++) {
            var s = appState.groupStats(g)
            pass += s.pass; warn += s.warn; fail += s.fail; skip += s.skip
        }
    }

    // Flutter _SummaryCard: padding h10 v10, borderRadius 8, icon 20px + count 20/w700 + label 9/w500
    component SummaryCard: Rectangle {
        property color accent: Theme.passGreen
        property string label: ""
        property int count: 0
        implicitHeight: 46; radius: 8
        color: Qt.alpha(accent, 0.08)
        border { width: 1; color: Qt.alpha(accent, 0.25) }

        RowLayout {
            anchors { fill: parent; leftMargin: 10; rightMargin: 10 }
            // Flutter: Icon(icon, size: 20, color: color) — use AppIcon with matching names
            AppIcon {
                name: label === "Pass" ? "check" : (label === "Warning" ? "warning" : (label === "Fail" ? "error" : (label === "Skipped" ? "circle" : "info")))
                size: 20; color: accent
            }
            Item { width: 8 }
            ColumnLayout { spacing: 1
                Label { text: count; font.family: "JetBrains Mono"; font.pixelSize: 20; font.weight: Font.Bold; color: accent }
                Label { text: label; font.family: "JetBrains Mono"; font.pixelSize: 9; font.weight: Font.Medium; color: Qt.alpha(Theme.textSecondary, 0.8) }
            }
        }
    }
}
