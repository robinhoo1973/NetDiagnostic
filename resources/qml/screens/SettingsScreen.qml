import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../widgets"

// ── Flutter SettingsScreen 1:1 — with AppBar ───────────────────────────
Item {
    id: page
    objectName: "settings"

    // AppBar (Flutter: Scaffold.appBar with "Settings" title)
    Rectangle {
        id: appBar
        anchors { left: parent.left; right: parent.right; top: parent.top }
        implicitHeight: 52; color: "#1A1A2E"
        border { width: 1; color: "#3A3A5A" }
        RowLayout {
            anchors { fill: parent; leftMargin: 16; rightMargin: 16 }
            AppIcon { name: "settings"; size: 20; color: Theme.cyan }
            Item { width: 10 }
            Label { text: "Settings"; font.family: "JetBrains Mono"; font.pixelSize: 15; font.weight: Font.DemiBold; color: Theme.textPrimary }
        }
    }

    Flickable {
        anchors { left: parent.left; right: parent.right; top: appBar.bottom; bottom: parent.bottom }
        clip: true
        contentHeight: setCol.implicitHeight

        ColumnLayout {
            id: setCol; width: parent.width - 48; x: 24; spacing: 0

            Item { Layout.preferredHeight: 24 }

            // ── Language Section ───────────────────────────────────────
            SectionHeader { icon: "🌐"; title: "Language / 语言" }
            Item { Layout.preferredHeight: 12 }
            Rectangle {
                Layout.fillWidth: true; implicitHeight: langCol.implicitHeight + 32; radius: 12
                color: Theme.bgCard; border { width: 1; color: "#2A2A4A" }
                ColumnLayout {
                    id: langCol
                    anchors { fill: parent; margins: 16 } spacing: 0
                    Label { text: "Interface Language"; font.family: "JetBrains Mono"; font.pixelSize: 13; font.weight: Font.Medium; color: Theme.textPrimary }
                    Item { Layout.preferredHeight: 4 }
                    Label { text: "Choose the display language for the application."; font.family: "JetBrains Mono"; font.pixelSize: 11; color: Theme.textSecondary }
                    Item { Layout.preferredHeight: 12 }
                    RowLayout { spacing: 8
                        LangBtn { Layout.fillWidth: true; label: "English"; code: "EN"; selected: true }
                        LangBtn { Layout.fillWidth: true; label: "中文"; code: "ZH"; selected: false }
                    }
                }
            }
            Item { Layout.preferredHeight: 32 }

            // ── SMTP Config Section ────────────────────────────────────
            SectionHeader { icon: "📧"; title: "Email (SMTP) Configuration" }
            Item { Layout.preferredHeight: 12 }
            Rectangle {
                Layout.fillWidth: true; implicitHeight: smtpCol.implicitHeight + 32; radius: 12
                color: Theme.bgCard; border { width: 1; color: "#2A2A4A" }
                ColumnLayout {
                    id: smtpCol
                    anchors { fill: parent; margins: 16 } spacing: 12
                    SmtpField { label: "SMTP Server"; placeholder: "smtp.example.com" }
                    SmtpField { label: "Port"; placeholder: "587" }
                    SmtpField { label: "Username"; placeholder: "user@example.com" }
                    SmtpField { label: "Password"; placeholder: "••••••••" }
                    SmtpField { label: "From Address"; placeholder: "noreply@example.com" }
                    // Info notice
                    Rectangle {
                        Layout.fillWidth: true; implicitHeight: noticeText.implicitHeight + 24; radius: 8
                        color: Qt.alpha(Theme.warnYellow, 0.08); border { width: 1; color: Qt.alpha(Theme.warnYellow, 0.2) }
                        RowLayout {
                            anchors { fill: parent; margins: 12 }
                            AppIcon { name: "warning"; size: 16; color: Theme.warnYellow }
                            Item { width: 10 }
                            Label {
                                id: noticeText; Layout.fillWidth: true
                                text: "SMTP configuration is a placeholder and will be implemented in a future update."
                                font.family: "JetBrains Mono"; font.pixelSize: 11; color: Qt.alpha(Theme.warnYellow, 0.8); wrapMode: Text.WordWrap; lineHeight: 1.4
                            }
                        }
                    }
                }
            }
            Item { Layout.preferredHeight: 32 }

            // ── About Section ──────────────────────────────────────────
            SectionHeader { icon: "ⓘ"; title: "About" }
            Item { Layout.preferredHeight: 12 }
            Rectangle {
                Layout.fillWidth: true; implicitHeight: aboutCol.implicitHeight + 32; radius: 12
                color: Theme.bgCard; border { width: 1; color: "#2A2A4A" }
                ColumnLayout {
                    id: aboutCol
                    anchors { fill: parent; margins: 16 } spacing: 0
                    // App icon + name
                    RowLayout {
                        Rectangle { implicitWidth: 48; implicitHeight: 48; radius: 12; color: Qt.alpha(Theme.accentBlue, 0.15)
                            AppIcon { anchors.centerIn: parent; name: "wifi"; size: 28; color: Theme.accentBlue } }
                        Item { width: 14 }
                        ColumnLayout { spacing: 2
                            Label { text: "NetDiagnostic"; font.family: "JetBrains Mono"; font.pixelSize: 18; font.weight: Font.Bold; color: Theme.textPrimary }
                            Label { text: "Version 1.0.0"; font.family: "JetBrains Mono"; font.pixelSize: 12; color: Theme.textSecondary }
                        }
                    }
                    Item { Layout.preferredHeight: 16 }
                    Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: "#2A2A4A" }
                    Item { Layout.preferredHeight: 12 }
                    Label { Layout.fillWidth: true; text: "A comprehensive cross-platform network diagnostic tool supporting Windows, macOS, Linux, iOS, and Android."
                        font.family: "JetBrains Mono"; font.pixelSize: 13; color: Theme.textSecondary; wrapMode: Text.WordWrap; lineHeight: 1.5 }
                    Item { Layout.preferredHeight: 16 }
                    AboutRow { aboutIcon: "💻"; aboutText: "Cross-platform (Windows, macOS, Linux, iOS, Android)" }
                    Item { Layout.preferredHeight: 8 }
                    AboutRow { aboutIcon: "⚡"; aboutText: "Real-time diagnostic engine" }
                    Item { Layout.preferredHeight: 8 }
                    AboutRow { aboutIcon: "📊"; aboutText: "Detailed reporting and export" }
                    Item { Layout.preferredHeight: 8 }
                    AboutRow { aboutIcon: "🌙"; aboutText: "Dark theme UI" }
                    Item { Layout.preferredHeight: 8 }
                    AboutRow { aboutIcon: "🖥"; aboutText: "Windows simulator mode" }
                }
            }
            Item { Layout.preferredHeight: 24 }
        }
    }

    // ── Subcomponents ──────────────────────────────────────────────────
    component SectionHeader: RowLayout {
        property string icon: ""; property string title: ""
        Rectangle { implicitWidth: 30; implicitHeight: 30; radius: 8; color: Qt.alpha(Theme.cyan, 0.1)
            Label { anchors.centerIn: parent; text: icon; font.pixelSize: 18; color: Theme.cyan } }
        Item { width: 12 }
        Label { text: title; font.family: "JetBrains Mono"; font.pixelSize: 16; font.weight: Font.DemiBold; color: Theme.textPrimary }
    }

    component LangBtn: Rectangle {
        property string label: ""; property bool selected: false; property string code: ""
        implicitHeight: 52; radius: 8
        color: selected ? Qt.alpha(Theme.accentBlue, 0.15) : "transparent"
        border { width: selected ? 1.5 : 1; color: selected ? Theme.accentBlue : "#3A3A5A" }
        ColumnLayout {
            anchors.centerIn: parent; spacing: 2
            Label { anchors.horizontalCenter: parent.horizontalCenter; text: label; font.family: "JetBrains Mono"; font.pixelSize: 14; font.weight: selected ? Font.DemiBold : Font.Medium; color: selected ? Theme.accentBlue : Theme.textPrimary }
            Label { anchors.horizontalCenter: parent.horizontalCenter; text: code; font.family: "JetBrains Mono"; font.pixelSize: 10; color: selected ? Qt.alpha(Theme.accentBlue, 0.6) : Theme.textSecondary }
        }
    }

    component SmtpField: ColumnLayout {
        property string label: ""; property string placeholder: ""
        Label { text: label; font.family: "JetBrains Mono"; font.pixelSize: 11; font.weight: Font.Medium; color: Theme.textSecondary }
        Item { Layout.preferredHeight: 4 }
        Rectangle {
            Layout.fillWidth: true; implicitHeight: 36; radius: 6
            color: Qt.alpha(Theme.bgDark, 0.6); border { width: 1; color: "#3A3A5A" }
            Label { anchors { fill: parent; leftMargin: 12; rightMargin: 12 } text: placeholder; font.family: "JetBrains Mono"; font.pixelSize: 12; color: Qt.alpha(Theme.textSecondary, 0.6); verticalAlignment: Text.AlignVCenter }
        }
    }

    component AboutRow: RowLayout {
        property string aboutIcon: ""; property string aboutText: ""
        Label { text: aboutIcon; font.pixelSize: 16; color: Qt.alpha(Theme.cyan, 0.7) }
        Item { width: 10 }
        Label { text: aboutText; font.family: "JetBrains Mono"; font.pixelSize: 12; color: Qt.alpha(Theme.textSecondary, 0.8) }
    }
}
