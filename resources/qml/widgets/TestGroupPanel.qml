import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// ── Flutter TestGroupPanel — card with ExpansionTile-like behavior ─────
Item {
    id: root
    property int groupIndex: 0
    property string label: ""
    property var stats: ({pass:0,warn:0,fail:0,skip:0,total:0,enabled:0})
    property bool isRunning: appState.runStatus === 1
    property int completed: 0
    property int enabledCount: 0
    
    // Flutter ExpansionTile behavior: initiallyExpanded when running or has results,
    // but user can always toggle. _userToggled tracks whether user manually set state.
    property bool _userToggled: false
    property bool _userWantsExpanded: true  // actual user preference
    property bool expanded: _userToggled ? _userWantsExpanded : (isRunning || completed > 0 || enabledCount > 0)
    
    // Local results list — updated on each test completion so Repeater re-renders
    property var resultsModel: []

    implicitHeight: Math.max(52, cardColumn.implicitHeight + 12)

    function refreshStats() {
        stats = appState.groupStats(groupIndex)
        completed = stats.total || 0
        enabledCount = stats.enabled || 0
        resultsModel = appState.allTestsForGroup(groupIndex)
    }
    Component.onCompleted: {
        label = appState.groupLabels[groupIndex] || ""
        refreshStats()
    }
    Connections {
        target: appState
        function onTestCompleted() { refreshStats() }
        function onProgressChanged() { refreshStats() }
        function onResultsReset() {
            stats = {pass:0,warn:0,fail:0,skip:0,total:0,enabled:0}
            completed = 0
            enabledCount = 0
            _userToggled = false
            _userWantsExpanded = true
        }
        // Auto-expand when run starts, reset toggle state
        function onRunStatusChanged() {
            if (appState.runStatus === 1) {
                // Running started: auto-expand, reset user toggle
                _userToggled = false
                _userWantsExpanded = true
            }
            if (appState.runStatus === 2 || appState.runStatus === 3)
                refreshStats()
        }
    }

    // Border card (Flutter: Card + ExpansionTile)
    Rectangle {
        anchors.fill: parent; radius: 10; color: Theme.bgCard
        border { width: 1; color: isRunning ? Qt.alpha(Theme.cyan, 0.4) : "#2A2A4A" }
    }

    ColumnLayout {
        id: cardColumn
        anchors { fill: parent; leftMargin: 12; rightMargin: 12; topMargin: 8; bottomMargin: 8 }
        spacing: 4

        // ── Header row (Flutter: ExpansionTile title) ──────────────────
        MouseArea {
            Layout.fillWidth: true
            implicitHeight: headerRow.implicitHeight + 8
            cursorShape: Qt.PointingHandCursor
            onClicked: {
                _userToggled = true
                _userWantsExpanded = !_userWantsExpanded
            }

            RowLayout {
                id: headerRow
                anchors { fill: parent; topMargin: 4; bottomMargin: 4 }
                spacing: 0

                // Group indicator bar (Flutter: 3x24, cyan/accentBlue, rounded)
                Rectangle {
                    Layout.preferredWidth: 3; implicitHeight: 24; radius: 2
                    color: isRunning ? Theme.cyan : Theme.accentBlue
                    Layout.alignment: Qt.AlignVCenter
                }
                Item { Layout.preferredWidth: 10 }

                // Group name + running indicator
                ColumnLayout {
                    Layout.fillWidth: true; spacing: 1
                    Label {
                        text: label
                        font.family: "JetBrains Mono"; font.pixelSize: 13; font.weight: Font.DemiBold; color: Theme.textPrimary
                    }
                    Label {
                        visible: isRunning
                        text: "Running: " + (appState.currentTestLabel || "") + "..."
                        font.family: "JetBrains Mono"; font.pixelSize: 10; font.italic: true; color: Theme.cyan
                        elide: Text.ElideRight
                    }
                }

                // Progress text (Flutter: completed/enabled)
                Label {
                    visible: isRunning || completed > 0
                    text: completed + "/" + enabledCount
                    font.family: "JetBrains Mono"; font.pixelSize: 11; font.weight: Font.Medium; color: Theme.textSecondary
                }
                Item { Layout.preferredWidth: 8 }

                // Status badges
                Row { spacing: 4
                    Badge { count: stats.pass; accent: Theme.passGreen }
                    Badge { count: stats.warn; accent: Theme.warnYellow }
                    Badge { count: stats.fail; accent: Theme.failRed }
                    Badge { count: stats.skip; accent: Theme.skipGray }
                }

                // Expand/collapse chevron
                Item { Layout.preferredWidth: 4 }
                Label {
                    text: expanded ? "▾" : "▸"
                    font.pixelSize: 12; color: Theme.textSecondary
                }
            }
        }

        // ── Collapsible body ───────────────────────────────────────────
        ColumnLayout {
            visible: expanded; spacing: 4

            // Progress bar (Flutter: LinearProgressIndicator with rounded corners)
            Rectangle {
                visible: isRunning || completed > 0
                Layout.fillWidth: true; implicitHeight: 3; radius: 2; color: "#2A2A4A"
                Rectangle {
                    height: 3; radius: 2
                    width: parent.width * (enabledCount > 0 ? completed / enabledCount : 0)
                    color: isRunning ? Theme.cyan : Theme.passGreen
                }
            }
            Item { Layout.preferredHeight: 4; visible: isRunning || completed > 0 }

            // Divider
            Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: "#2A2A4A"; visible: completed > 0 }

            // Results list (completed + pending items) — Flutter TreeView hierarchy
            ColumnLayout {
                spacing: 0
                visible: resultsModel.length > 0
                Repeater {
                    model: resultsModel
                    delegate: Item {
                        width: cardColumn.width - 16
                        implicitHeight: testItem.implicitHeight
                        // TreeView connector line area + item
                        RowLayout {
                            anchors.fill: parent; spacing: 0
                            // TreeView indent with visible connector lines
                            Item {
                                Layout.preferredWidth: 20; Layout.fillHeight: true
                                // Vertical connector line — thicker for visibility
                                Rectangle {
                                    anchors { left: parent.left; leftMargin: 6; top: parent.top; bottom: parent.bottom }
                                    width: 1.5; color: "#3A3A5A"
                                }
                                // Horizontal connector stub
                                Rectangle {
                                    anchors { left: parent.left; leftMargin: 6; verticalCenter: parent.verticalCenter }
                                    width: 8; height: 1.5; color: "#3A3A5A"
                                }
                            }
                            TestResultItem {
                                id: testItem
                                resultData: modelData
                                Layout.fillWidth: true
                            }
                        }
                    }
                }
            }
        }
    }

    component Badge: Rectangle {
        property int count: 0; property color accent: Theme.passGreen
        visible: count > 0
        width: 22; height: 16; radius: 4; color: Qt.alpha(accent, 0.15)
        Label { anchors.centerIn: parent; text: count; font.family: "JetBrains Mono"; font.pixelSize: 10; font.weight: Font.Bold; color: accent }
    }
}
