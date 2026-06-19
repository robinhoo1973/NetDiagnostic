import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../widgets"

// ── Flutter ReportPreviewScreen 1:1 — with AppBar ─────────────────────
Item {
    id: page
    objectName: "report"
    property bool hasResults: appState.totalCompleted > 0

    // AppBar (Flutter: Scaffold.appBar with "Report Preview" title)
    Rectangle {
        id: appBar
        anchors { left: parent.left; right: parent.right; top: parent.top }
        implicitHeight: 52; color: "#1A1A2E"
        border { width: 1; color: "#3A3A5A" }
        RowLayout {
            anchors { fill: parent; leftMargin: 16; rightMargin: 16 }
            AppIcon { name: "report"; size: 20; color: Theme.cyan }
            Item { width: 10 }
            Label { text: "Report Preview"; font.family: "JetBrains Mono"; font.pixelSize: 15; font.weight: Font.DemiBold; color: Theme.textPrimary }
        }
    }

    // Centered content (Flutter: Center > Padding(40) > Column)
    Flickable {
        anchors { left: parent.left; right: parent.right; top: appBar.bottom; bottom: parent.bottom }
        clip: true
        contentHeight: reportCol.implicitHeight

        ColumnLayout {
            id: reportCol
            width: Math.min(500, parent.width - 80)
            anchors.centerIn: parent
            spacing: 0

            // Icon container (Flutter: 100x100, borderRadius 24, bg cyan8%, border cyan20%)
            Rectangle {
                Layout.preferredWidth: 100; Layout.preferredHeight: 100
                Layout.alignment: Qt.AlignHCenter
                radius: 24; color: Qt.alpha(Theme.cyan, 0.08)
                border { width: 1.5; color: Qt.alpha(Theme.cyan, 0.2) }
                AppIcon { anchors.centerIn: parent; name: "report"; size: 48; color: Qt.alpha(Theme.cyan, 0.6) }
            }
            Item { Layout.preferredHeight: 24 }

            // Title
            Label {
                Layout.alignment: Qt.AlignHCenter
                text: "Report Preview"
                font.family: "JetBrains Mono"; font.pixelSize: 22; font.weight: Font.DemiBold; color: Theme.textPrimary
            }
            Item { Layout.preferredHeight: 12 }

            // Subtitle
            Label {
                Layout.alignment: Qt.AlignHCenter
                text: "Report generation and preview will be\nimplemented in Phase 9."
                font.family: "JetBrains Mono"; font.pixelSize: 14; color: Qt.alpha(Theme.textSecondary, 0.6)
                horizontalAlignment: Text.AlignHCenter; lineHeight: 1.5
            }
            Item { Layout.preferredHeight: 24 }

            // Feature rows (Flutter: Icon 16px accentBlue 70% + SizedBox(10) + Text 13px textSecondary 70%)
            ColumnLayout { spacing: 10; Layout.alignment: Qt.AlignHCenter
                FeatureRow { featureIcon: "📄"; featureText: "Export to PDF format" }
                FeatureRow { featureIcon: "📤"; featureText: "Share reports via email" }
                FeatureRow { featureIcon: "🌐"; featureText: "HTML report with embedded charts" }
                FeatureRow { featureIcon: "⏱";  featureText: "Historical report comparison" }
            }
            Item { Layout.preferredHeight: 32 }

            // Status indicator (Flutter: padding h16 v10, borderRadius 8, conditional color)
            Rectangle {
                Layout.alignment: Qt.AlignHCenter
                implicitWidth: statusRow.implicitWidth + 32; implicitHeight: 40; radius: 8
                color: hasResults ? Qt.alpha(Theme.passGreen, 0.1) : Qt.alpha(Theme.warnYellow, 0.1)
                border { width: 1; color: hasResults ? Qt.alpha(Theme.passGreen, 0.3) : Qt.alpha(Theme.warnYellow, 0.3) }
                RowLayout {
                    id: statusRow
                    anchors.centerIn: parent
                    Label { text: hasResults ? "✓" : "ℹ"; font.pixelSize: 16; color: hasResults ? Theme.passGreen : Theme.warnYellow }
                    Item { width: 8 }
                    Label {
                        text: hasResults ? appState.totalCompleted + " results available" : "No diagnostic results"
                        font.family: "JetBrains Mono"; font.pixelSize: 12; color: hasResults ? Theme.passGreen : Theme.warnYellow
                    }
                }
            }
            Item { Layout.preferredHeight: 40 }
        }
    }

    component FeatureRow: RowLayout {
        property string featureIcon: ""; property string featureText: ""
        Label { text: featureIcon; font.pixelSize: 16; color: Qt.alpha(Theme.accentBlue, 0.7) }
        Item { width: 10 }
        Label { text: featureText; font.family: "JetBrains Mono"; font.pixelSize: 13; color: Qt.alpha(Theme.textSecondary, 0.7) }
    }
}
