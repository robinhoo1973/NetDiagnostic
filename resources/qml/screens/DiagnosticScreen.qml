import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../widgets"

// ── Flutter DiagnosticMainScreen 1:1 ───────────────────────────────────
Item {
    id: page
    objectName: "diagnostic"
    readonly property bool wide: width >= 600

    // Filter groups: only show those with enabled tests or results
    // Depends on totalCompleted + runStatus to trigger re-evaluation
    property var visibleGroups: {
        var _force = appState.totalCompleted + appState.runStatus
        var g = []
        for (var i = 0; i < appState.groupLabels.length; i++) {
            var s = appState.groupStats(i)
            if (s.enabled > 0 || s.total > 0) g.push(i)
        }
        return g
    }

    // ── Wide: Row[ Sidebar(260) | Divider(1) | Content(flex) ] ─────
    RowLayout {
        anchors.fill: parent; spacing: 0
        visible: page.wide

        Rectangle {
            Layout.preferredWidth: 260; Layout.fillHeight: true
            color: Theme.bgSidebar; clip: true
            SidebarContent { anchors.fill: parent; compact: false }
        }
        Rectangle { Layout.preferredWidth: 1; Layout.fillHeight: true; color: "#3A3A5A" }
        ContentArea { Layout.fillWidth: true; Layout.fillHeight: true }
    }

    // ── Narrow: Column[ Sidebar(flex) | Divider | Content(flex) ] ──
    ColumnLayout {
        anchors.fill: parent; spacing: 0
        visible: !page.wide

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: appState.totalCompleted > 0 ? 0.29 * page.height : 0.5 * page.height
            color: Theme.bgSidebar; clip: true
            Flickable {
                anchors.fill: parent; contentHeight: sidebarCol.implicitHeight
                boundsBehavior: Flickable.StopAtBounds
                ColumnLayout { id: sidebarCol; width: parent.width
                    SidebarContent { width: parent.width; compact: appState.totalCompleted > 0 }
                }
            }
        }
        Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: "#3A3A5A" }
        ContentArea { Layout.fillWidth: true; Layout.fillHeight: true }
    }

    // ═══════════════════ SIDEBAR ═══════════════════
    component SidebarContent: ColumnLayout {
        property bool compact: false
        spacing: 0

        // Header
        Rectangle {
            Layout.fillWidth: true; implicitHeight: 49
            color: "transparent"
            border { width: 1; color: "#3A3A5A" }
            RowLayout {
                anchors { left: parent.left; right: parent.right; verticalCenter: parent.verticalCenter; leftMargin: 16; rightMargin: 16 }
                AppIcon { name: "wifi"; size: 20; color: Theme.cyan }
                Item { width: 10 }
                Label { text: "NetDiagnostic"; font.family: "JetBrains Mono"; font.pixelSize: 16; font.weight: Font.Bold; color: Theme.textPrimary }
            }
        }

        // Target input + Run
        Item { Layout.preferredHeight: 12 }
        TargetInputPanel { Layout.fillWidth: true; Layout.leftMargin: 12; Layout.rightMargin: 12 }

        // Layer checkboxes (not in compact) — matches Flutter _buildLayerCheckboxes
        Item { Layout.preferredHeight: 8; visible: !compact }
        ColumnLayout {
            visible: !compact; spacing: 2
            Layout.leftMargin: 12; Layout.rightMargin: 12
            Label { text: "Diagnosis Group"; font.family: "JetBrains Mono"; font.pixelSize: 11; font.weight: Font.DemiBold; color: Theme.textSecondary }
            Item { Layout.preferredHeight: 6 }
            Repeater {
                model: appState.groupLabels.length
                delegate: Rectangle {
                    property int gIdx: index
                    readonly property bool canEnable:
                        appState.runStatus === 1 ? false :
                        (gIdx === 3) ? (appState.target !== "") :
                        (gIdx === 4) ? (appState.target.indexOf("://") >= 0) :
                        true
                    Layout.fillWidth: true; implicitHeight: 32; radius: 6
                    color: appState.isGroupAllEnabled(gIdx) ? Qt.alpha(Theme.accentBlue, 0.12) : "transparent"
                    RowLayout {
                        anchors { fill: parent; leftMargin: 8; rightMargin: 8 }
                        CheckBox {
                            id: gCb
                            Layout.preferredWidth: 18; Layout.preferredHeight: 18
                            checkState: appState.isGroupAllEnabled(gIdx) ? Qt.Checked :
                                        appState.isGroupAnyEnabled(gIdx) ? Qt.PartiallyChecked : Qt.Unchecked
                            enabled: canEnable
                            onToggled: if (canEnable) appState.setGroupEnabled(gIdx, checkState === Qt.Checked)
                        }
                        Item { width: 8 }
                        Label {
                            Layout.fillWidth: true
                            text: appState.groupLabels[gIdx] || ""
                            font.family: "JetBrains Mono"; font.pixelSize: 12
                            font.weight: appState.isGroupAllEnabled(gIdx) ? Font.DemiBold : Font.Normal
                            color: canEnable ? (appState.isGroupAllEnabled(gIdx) ? Theme.textPrimary : Theme.textSecondary)
                                            : Qt.alpha(Theme.textSecondary, 0.35)
                        }
                    }
                }
            }
        }

        // Port scan — Flutter order: checkboxes → SizedBox(8) → PortScanConfig
        Item { Layout.preferredHeight: 8; visible: !compact }
        PortScanConfig { Layout.fillWidth: true; Layout.leftMargin: 12; Layout.rightMargin: 12; visible: !compact }

        // Divider — Flutter: Divider(color:#3A3A5A, height:20) between portscan and analysis
        Rectangle {
            Layout.fillWidth: true; Layout.preferredHeight: 20; color: "transparent"
            visible: !compact
            Rectangle { anchors.centerIn: parent; width: parent.width - 24; height: 1; color: "#3A3A5A" }
        }

        // Target Analysis (Flutter: after divider, only when target non-empty)
        TargetAnalysisPanel {
            Layout.fillWidth: true
            Layout.leftMargin: 12; Layout.rightMargin: 12
            visible: !compact && appState.target !== ""
            target: appState.target
        }

        // Spacer — Flutter: Spacer() pushes SummaryCards to bottom (wide only, not compact)
        Item {
            Layout.fillHeight: true
            visible: !compact
        }

        // Summary cards — Flutter: Container(top border) always visible at bottom
        Rectangle {
            Layout.fillWidth: true; implicitHeight: summaryCards.implicitHeight + 24
            color: "transparent"
            border { width: 1; color: "#3A3A5A" }
            SummaryCards {
                id: summaryCards
                anchors { fill: parent; margins: 12; topMargin: 8; bottomMargin: 16 }
            }
        }
    }

    // ═══════════════════ CONTENT ═══════════════════
    component ContentArea: ColumnLayout {
        spacing: 0

        // Header bar — Flutter: Container color #1A1A2E, padding h16 v10, no border
        Rectangle {
            Layout.fillWidth: true; implicitHeight: 41
            color: "#1A1A2E"
            RowLayout {
                anchors { fill: parent; leftMargin: 16; rightMargin: 16 }
                AppIcon { name: "diagnostics"; size: 18; color: Theme.cyan }
                Item { width: 8 }
                Label {
                    text: appState.runStatus === 1 ? "Running Diagnostics..." :
                          appState.runStatus === 2 ? "Diagnostic Complete" :
                          appState.runStatus === 3 ? "Cancelled" :
                          appState.runStatus === 4 ? "Error" : "Results"
                    font.family: "JetBrains Mono"; font.pixelSize: 15; font.weight: Font.DemiBold; color: Theme.textPrimary
                }
                Item { Layout.fillWidth: true }
                Button {
                    visible: appState.runStatus >= 2 && appState.totalCompleted > 0
                    implicitHeight: 32
                    text: "Reset"
                    font.family: "JetBrains Mono"; font.pixelSize: 12; font.weight: Font.Normal
                    flat: true
                    background: Rectangle { radius: 6; color: "transparent"; border { width: 1; color: "#5A5A7A" } }
                    contentItem: Label { text: "↻ Reset"; font: parent.font; color: Theme.textSecondary; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                    onClicked: appState.reset()
                }
            }
        }

        // Results body
        Item {
            Layout.fillWidth: true; Layout.fillHeight: true
            // Idle state (Flutter: "Enter a target and press Run")
            Column {
                anchors.centerIn: parent; spacing: 16
                visible: appState.runStatus === 0 && appState.totalCompleted === 0
                AppIcon { anchors.horizontalCenter: parent.horizontalCenter; name: "wifi"; size: 80; color: Qt.alpha(Theme.textSecondary, 0.2) }
                Label { anchors.horizontalCenter: parent.horizontalCenter; text: "Enter a target and press Run"; font.family: "JetBrains Mono"; font.pixelSize: 15; font.weight: Font.Medium; color: Qt.alpha(Theme.textSecondary, 0.6) }
                Label { anchors.horizontalCenter: parent.horizontalCenter; text: "Analyze your network with a single click"; font.family: "JetBrains Mono"; font.pixelSize: 12; color: Qt.alpha(Theme.textSecondary, 0.4) }
            }
            // Tree view results — Flickable + Repeater avoids ListView delegate bugs
            Flickable {
                id: treeFlick
                anchors { fill: parent; topMargin: 4; bottomMargin: 8 }
                visible: appState.totalCompleted > 0 || appState.runStatus === 1
                clip: true
                contentWidth: width
                contentHeight: treeCol.implicitHeight + 8
                boundsBehavior: Flickable.StopAtBounds
                Column {
                    id: treeCol
                    width: parent.width - 8
                    anchors.horizontalCenter: parent.horizontalCenter
                    spacing: 2
                    Repeater {
                        model: visibleGroups
                        delegate: TestGroupPanel {
                            groupIndex: modelData
                            width: treeCol.width
                        }
                    }
                }
            }
        }

        // Bottom bar — LiveProgress or IdleReady
        Rectangle {
            Layout.fillWidth: true; implicitHeight: 40
            color: "#1A1A2E"
            border { width: 1; color: "#3A3A5A" }
            LiveProgressPanel {
                anchors { fill: parent; leftMargin: 16; rightMargin: 16; topMargin: 10; bottomMargin: 12 }
                visible: appState.runStatus >= 1 && appState.runStatus <= 4
            }
            RowLayout {
                anchors { fill: parent; leftMargin: 16; rightMargin: 16 }
                visible: appState.runStatus === 0
                AppIcon { name: "circle"; size: 14; color: Theme.textSecondary }
                Item { width: 8 }
                Label { text: "Ready"; font.family: "JetBrains Mono"; font.pixelSize: 12; font.weight: Font.DemiBold; color: Theme.textSecondary }
            }
        }
    }
}
