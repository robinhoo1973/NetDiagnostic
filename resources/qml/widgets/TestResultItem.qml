import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// ── Flutter TestResultItem — 2-line collapsed + detail dialog ───────
Rectangle {
    id: root
    property var resultData: null
    property bool isPending: resultData ? (resultData.isPending === true) : true
    property bool isRunning: resultData ? (resultData.isRunning === true) : false

    implicitHeight: isPending ? 30 : (resultData && resultData.summary ? 52 : 38)
    color: "transparent"

    function statusIcon(s) {
        switch(s) { case 0: return "✓"; case 1: return "⚠"; case 2: return "✗"; case 3: return "⊖"; case 4: return "✗"; default: return "ⓘ" }
    }
    function statusColor(s) {
        switch(s) { case 0: return Theme.passGreen; case 1: return Theme.warnYellow; case 2: return Theme.failRed; case 3: return Theme.skipGray; case 4: return Theme.failRed; default: return Theme.accentBlue }
    }
    function fmtDur(ms) {
        var m = ms || 0
        if (m >= 1000) return (m/1000).toFixed(1) + "s"
        return m + "ms"
    }

    // ── Pending row ──────────────────────────────────────────────────
    RowLayout {
        anchors { left: parent.left; right: parent.right; verticalCenter: parent.verticalCenter; leftMargin: 4; rightMargin: 4 }
        visible: isPending; spacing: 8

        Item {
            width: 18; height: 18
            Label {
                anchors.centerIn: parent
                text: isRunning ? "⟳" : "⊖"
                font.pixelSize: isRunning ? 14 : 12
                color: isRunning ? Theme.cyan : Qt.alpha(Theme.textSecondary, 0.3)
                RotationAnimation on rotation { running: isRunning; from: 0; to: 360; duration: 1000; loops: Animation.Infinite }
            }
        }
        Label {
            Layout.fillWidth: true
            text: resultData ? (resultData.displayName || "?") : "?"
            font.family: "JetBrains Mono"; font.pixelSize: 12
            color: Qt.alpha(Theme.textSecondary, 0.5)
            elide: Text.ElideRight
        }
        Label {
            visible: isRunning
            text: "Running..."
            font.family: "JetBrains Mono"; font.pixelSize: 10; font.italic: true; color: Theme.cyan
        }
    }

    // ── Result collapsed (2-line) ────────────────────────────────────
    ColumnLayout {
        anchors { fill: parent; leftMargin: 6; rightMargin: 4; topMargin: 4; bottomMargin: 4 }
        visible: !isPending; spacing: 1

        RowLayout {
            Layout.fillWidth: true; spacing: 6
            Label {
                Layout.alignment: Qt.AlignTop
                Layout.topMargin: 1
                text: resultData ? statusIcon(resultData.status) : "○"
                font.pixelSize: 16
                color: resultData ? statusColor(resultData.status) : Theme.textSecondary
            }
            Label {
                Layout.fillWidth: true
                text: resultData ? (resultData.displayName || "?") : "?"
                font.family: "JetBrains Mono"; font.pixelSize: 12; font.weight: Font.Medium
                color: resultData ? statusColor(resultData.status) : Theme.textSecondary
                elide: Text.ElideRight
            }
            Rectangle {
                implicitWidth: durText.implicitWidth + 10; implicitHeight: 18; radius: 4
                color: "#2A2A4A"
                visible: resultData && (resultData.durationMs || 0) > 0
                Label {
                    id: durText; anchors.centerIn: parent
                    text: resultData ? fmtDur(resultData.durationMs) : ""
                    font.family: "JetBrains Mono"; font.pixelSize: 10; color: Theme.textSecondary
                }
            }
        }

        Label {
            Layout.fillWidth: true; Layout.leftMargin: 22
            visible: resultData && resultData.summary
            text: resultData ? (resultData.summary || "") : ""
            font.family: "JetBrains Mono"; font.pixelSize: 10; color: Qt.alpha(Theme.textSecondary, 0.7)
            elide: Text.ElideRight; maximumLineCount: 1
        }
    }

    // ── Click → detail popup ────────────────────────────────────────
    MouseArea {
        anchors.fill: parent
        enabled: !isPending
        onClicked: {
            if (root.resultData && root.resultData.isDone) {
                detailPopup.currentResult = root.resultData
                detailPopup.open()
            }
        }
    }

    // ── Detail Popup (declared directly — no createObject needed) ───
    Popup {
        id: detailPopup
        property var currentResult: null
        modal: true
        closePolicy: Popup.CloseOnEscape
        width: Math.min(380, parent ? parent.width * 0.94 : 380)
        height: Math.min(450, parent ? parent.height * 0.88 : 450)
        x: parent ? (parent.width - width) / 2 : 0
        y: parent ? (parent.height - height) / 2 : 0
        padding: 0

        onCurrentResultChanged: {
            if (!currentResult) return
            headerTitle.text = currentResult.displayName || ""
            statusLabel.text = ["Pass","Warning","Fail","Skipped","Error","Info"][currentResult.status] || "Unknown"
            durLabel.text = root.fmtDur(currentResult.durationMs || 0)
            summaryText.text = currentResult.summary || ""
            sumSection.visible = currentResult.summary && currentResult.summary !== ""
            propsSection.visible = currentResult.properties && currentResult.properties.length > 0
            outputSection.visible = currentResult.details && currentResult.details !== ""
            outputArea.text = currentResult.details || ""
        }

            background: Rectangle {
                radius: 12
                color: "#1E1E2E"
                border { width: 1; color: "#3A3A5A" }
            }

            ColumnLayout {
                anchors.fill: parent
                spacing: 0

                // Header
                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: 52
                    color: "transparent"
                    border { width: 1; color: "#3A3A5A" }
                    RowLayout {
                        anchors { fill: parent; leftMargin: 16; rightMargin: 16 }
                        AppIcon { name: "report"; size: 20; color: Theme.cyan }
                        Item { width: 10 }
                        Label {
                            id: headerTitle
                            Layout.fillWidth: true
                            text: ""
                            font.family: "JetBrains Mono"; font.pixelSize: 16; font.weight: Font.DemiBold; color: Theme.textPrimary
                            elide: Text.ElideRight
                        }
                        Label {
                            text: "✕"; font.pixelSize: 18; color: Theme.textSecondary
                            MouseArea {
                                anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                                onClicked: detailPopup.close()
                            }
                        }
                    }
                }

                // Scrollable body
                ScrollView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
                    ScrollBar.vertical.policy: ScrollBar.AsNeeded

                    ColumnLayout {
                        width: parent.width - 32
                        x: 16
                        spacing: 12
                        Item { Layout.preferredHeight: 12 }

                        // Status + Duration
                        RowLayout {
                            Label { text: "Status:"; font.family: "JetBrains Mono"; font.pixelSize: 11; font.weight: Font.DemiBold; color: Theme.textSecondary }
                            Item { width: 8 }
                            Label {
                                id: statusLabel
                                text: ""
                                font.family: "JetBrains Mono"; font.pixelSize: 13; font.weight: Font.DemiBold; color: Theme.textSecondary
                            }
                            Item { width: 20 }
                            Label { text: "Duration:"; font.family: "JetBrains Mono"; font.pixelSize: 11; font.weight: Font.DemiBold; color: Theme.textSecondary }
                            Item { width: 8 }
                            Label {
                                id: durLabel
                                text: ""
                                font.family: "JetBrains Mono"; font.pixelSize: 13; color: Theme.textPrimary
                            }
                        }

                        // Summary
                        ColumnLayout {
                            id: sumSection
                            visible: false
                            spacing: 4
                            Label { text: "Summary"; font.family: "JetBrains Mono"; font.pixelSize: 11; font.weight: Font.DemiBold; color: Theme.textSecondary }
                            Rectangle {
                                Layout.fillWidth: true; implicitHeight: summaryText.implicitHeight + 20; radius: 6; color: "#252538"
                            Label {
                                id: summaryText
                                anchors { fill: parent; margins: 10 }
                                text: ""
                                font.family: "JetBrains Mono"; font.pixelSize: 12; color: Theme.textPrimary; wrapMode: Text.WordWrap
                            }
                            }
                        }

                        // Properties table
                        ColumnLayout {
                            id: propsSection
                            visible: false
                            spacing: 4
                            Label { text: "Properties"; font.family: "JetBrains Mono"; font.pixelSize: 11; font.weight: Font.DemiBold; color: Theme.textSecondary }
                            Repeater {
                                model: detailPopup.currentResult ? (detailPopup.currentResult.properties || []) : []
                                delegate: RowLayout {
                                    Label {
                                        text: (modelData.label || "?") + ":"
                                        font.family: "JetBrains Mono"; font.pixelSize: 11; font.weight: Font.DemiBold; color: Theme.textSecondary
                                        Layout.preferredWidth: 140
                                    }
                                    Label {
                                        Layout.fillWidth: true
                                        text: modelData.value || ""
                                        font.family: "JetBrains Mono"; font.pixelSize: 11; color: Theme.textPrimary
                                        wrapMode: Text.WordWrap
                                    }
                                }
                            }
                        }

                        // Raw output
                        ColumnLayout {
                            id: outputSection
                            visible: false
                            spacing: 4
                            Label { text: "Output"; font.family: "JetBrains Mono"; font.pixelSize: 11; font.weight: Font.DemiBold; color: Theme.textSecondary }
                            Rectangle {
                                Layout.fillWidth: true; implicitHeight: Math.min(200, outputArea.implicitHeight + 20)
                                radius: 6; color: "#252538"
                                clip: true
                                Flickable {
                                    id: outputFlick
                                    anchors.fill: parent; anchors.margins: 10
                                    contentWidth: outputArea.width
                                    contentHeight: outputArea.implicitHeight
                                TextArea {
                                    id: outputArea
                                    width: outputFlick.width
                                    text: ""
                                    font.family: "JetBrains Mono"; font.pixelSize: 11
                                        color: Qt.alpha(Theme.textSecondary, 0.8)
                                        readOnly: true
                                        wrapMode: Text.WordWrap
                                        background: Item {}
                                    }
                                }
                            }
                        }
                        Item { Layout.preferredHeight: 12 }
                    }
                }

                // Footer
                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: 44
                    color: "transparent"
                    border { width: 1; color: "#3A3A5A" }
                    RowLayout {
                        anchors { fill: parent; leftMargin: 16; rightMargin: 16 }
                        Item { Layout.fillWidth: true }
                        Button {
                            text: "Close"
                            font.family: "JetBrains Mono"; font.pixelSize: 12
                            flat: true
                            onClicked: detailPopup.close()
                        }
                    }
                }
            }
        }
    }
}
