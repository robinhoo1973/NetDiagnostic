import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../widgets"

ApplicationWindow {
    id: page
    objectName: "simulator"
    visible: true
    color: "#2D2D2D"
    property int currentTab: 1  // default: Diagnostics

    Component.onCompleted: {
        showFullScreen()
        // Fallback: force size
        width = Screen.width
        height = Screen.height
        x = 0; y = 0
    }

    FontLoader { id: jbMono; source: "qrc:/fonts/JetBrainsMono-Regular.ttf" }
    FontLoader { id: jbMonoBold; source: "qrc:/fonts/JetBrainsMono-Bold.ttf" }

    property var devices: [
        { id: "iphone_15_pro",  name: "iPhone 15 Pro",    type:"phone", os:"ios",     w:393, h:852, bezel:4, island:true,  radius:47 },
        { id: "iphone_se",      name: "iPhone SE",        type:"phone", os:"ios",     w:375, h:667, bezel:8, island:false, radius:20 },
        { id: "pixel_8_pro",    name: "Pixel 8 Pro",      type:"phone", os:"android", w:412, h:915, bezel:4, island:false, radius:28 },
        { id: "generic_desktop",name: "Generic Desktop",   type:"desktop",os:"linux",  w:800, h:500, bezel:0, island:false, radius:8  }
    ]
    property int currentDevice: 0
    property bool portrait: true
    onPortraitChanged: { scaledFrame.s = body.calcScale() }
    onCurrentDeviceChanged: { scaledFrame.s = body.calcScale() }

    function cur() { return devices[currentDevice] }
    function osColor(os) {
        if (os === "windows") return "#00ADEF"
        if (os === "ios") return "#007AFF"
        if (os === "linux") return "#FCC624"
        if (os === "android") return "#3DDC84"
        return "#888"
    }

    // ── Top bar: AppBar + NavBar ──────────────────────────────────────
    ColumnLayout {
        id: topArea
        anchors { top: parent.top; left: parent.left; right: parent.right }
        spacing: 0; z: 10

        // AppBar row
        Rectangle {
            Layout.fillWidth: true; implicitHeight: 48; color: "#1A1A2E"
            RowLayout {
                anchors { fill: parent; leftMargin: 16; rightMargin: 12 }
                Label { text: "NetDiagnostic Simulator"; font.family: "JetBrains Mono"; font.pixelSize: 16; font.weight: Font.DemiBold; color: "white" }
                Item { Layout.fillWidth: true }
                // Device selector
                Rectangle {
                    implicitWidth: 160; implicitHeight: 34; radius: 4
                    color: Qt.alpha("white", 0.08); border { width: 1; color: Qt.alpha("white", 0.15) }
                    RowLayout {
                        anchors { fill: parent; leftMargin: 10; rightMargin: 8 }
                        Label { Layout.fillWidth: true; text: cur().name; font.family: "JetBrains Mono"; font.pixelSize: 12; color: "white" }
                        Label { text: "▾"; font.pixelSize: 14; color: Qt.alpha("white", 0.6) }
                    }
                    MouseArea { anchors.fill: parent; onClicked: devicePopup.open() }
                }
                Item { width: 8 }
                // Orientation toggle
                Rectangle {
                    implicitWidth: 34; implicitHeight: 34; radius: 4; color: "transparent"
                    border { width: 1; color: Qt.alpha("white", 0.2) }
                    Label { anchors.centerIn: parent; text: portrait ? "↻" : "↕"; font.pixelSize: 18; color: Qt.alpha("white",0.7) }
                    MouseArea { anchors.fill: parent; onClicked: portrait = !portrait }
                }
                Item { width: 12 }
                // OS badge
                Rectangle {
                    implicitWidth: 44; implicitHeight: 26; radius: 4
                    color: Qt.alpha(osColor(cur().os), 0.15); border { width: 1; color: Qt.alpha(osColor(cur().os), 0.3) }
                    Label { anchors.centerIn: parent; text: cur().os.substring(0,1).toUpperCase(); font.family: "JetBrains Mono"; font.pixelSize: 13; font.weight: Font.DemiBold; color: "white" }
                }
            }
        }

        // Navigation bar — same as production version
        Rectangle {
            Layout.fillWidth: true; implicitHeight: 40; color: "#1A1A2E"
            border { color: "#3A3A5A"; width: 1 }
            RowLayout {
                anchors { fill: parent; leftMargin: 16; rightMargin: 16 }
                Repeater {
                    model: [
                        { label: "Dashboard",   icon: "dashboard" },
                        { label: "Diagnostics", icon: "diagnostics" },
                        { label: "Config",      icon: "config" },
                        { label: "Report",      icon: "report" },
                        { label: "Settings",    icon: "settings" }
                    ]
                    delegate: ItemDelegate {
                        id: navBtn
                        implicitWidth: 90; implicitHeight: 36
                        background: Rectangle {
                            color: navBtn.active ? Qt.alpha(Theme.cyan, 0.12) : "transparent"
                            radius: 8
                        }
                        property bool active: index === page.currentTab
                        contentItem: RowLayout {
                            anchors.centerIn: parent; spacing: 4
                            AppIcon { name: modelData.icon; size: 14; color: navBtn.active ? Theme.cyan : Qt.alpha(Theme.textSecondary, 0.5) }
                            Label { text: modelData.label; font.family: "JetBrains Mono"; font.pixelSize: 10; color: navBtn.active ? Theme.cyan : Qt.alpha(Theme.textSecondary, 0.7) }
                        }
                        onClicked: page.currentTab = index
                    }
                }
            }
        }
    }

    Popup {
        id: devicePopup
        y: topArea.height + 2; x: page.width - 300
        width: 260; height: Math.min(240, devices.length * 46 + 8); padding: 6
        background: Rectangle { radius: 10; color: "#1E1E2E"; border { width: 1; color: "#3A3A5A" } }
        ListView {
            anchors.fill: parent; clip: true; model: devices
            delegate: ItemDelegate {
                width: ListView.view.width; implicitHeight: 44
                contentItem: RowLayout { spacing: 8
                    Label { text: modelData.name; font.family: "JetBrains Mono"; font.pixelSize: 13; color: "white"; Layout.fillWidth: true }
                    Rectangle {
                        implicitWidth: 32; implicitHeight: 20; radius: 4
                        color: Qt.alpha(osColor(modelData.os), 0.15)
                        Label { anchors.centerIn: parent; text: modelData.os.substring(0,1).toUpperCase(); font.family: "JetBrains Mono"; font.pixelSize: 10; color: "white" }
                    }
                }
                background: Rectangle { color: index === currentDevice ? Qt.alpha("#0078D4", 0.2) : "transparent"; radius: 6 }
                onClicked: { currentDevice = index; devicePopup.close() }
            }
        }
    }

    // ── Body ──────────────────────────────────────────────────────────
    Item {
        id: body
        anchors { top: topArea.bottom; left: parent.left; right: parent.right; bottom: parent.bottom }

        function calcScale() {
            var d = cur()
            var sw = portrait ? d.w : d.h
            var sh = portrait ? d.h : d.w
            var tw = sw + d.bezel * 2 + 16
            var th = sh + d.bezel * 2 + 16 + (d.island ? 30 : 0)
            return Math.max(0.1, Math.min(3.0, Math.min((width-8)/tw, (height-8)/th)))
        }

        Item {
            id: scaledFrame
            anchors.centerIn: parent
            property real s: {
                // Explicit dependencies force re-evaluation on change
                var _p = portrait; var _d = cur(); var _w = body.width; var _h = body.height
                return body.calcScale()
            }
            width: frame.width * s
            height: frame.height * s

            // Device Frame
            Rectangle {
                id: frame
                property var d: cur()
                property real sw: portrait ? d.w : d.h
                property real sh: portrait ? d.h : d.w
                property real bh: d.bezel
                property real extra: d.island ? 30 : 0

                width: sw + bh * 2
                height: sh + bh * 2 + extra
                radius: d.radius + bh
                color: d.bezel > 0 ? "#0A0A0A" : "transparent"
                border { width: d.bezel > 0 ? 0 : 1; color: "#333" }
                clip: true
                layer.enabled: true
                layer.samples: 4

                 // Screen with page switching via currentTab
                    Rectangle {
                        anchors.centerIn: parent
                        anchors.verticalCenterOffset: d.island ? -frame.extra / 2 : 0
                        width: frame.sw; height: frame.sh; radius: frame.d.radius
                        color: "#1E1E2E"
                        clip: true
                        layer.enabled: true
                        layer.samples: 4

                        // Status bar — rounded top corners to match device frame
                        Rectangle {
                            anchors { top: parent.top; left: parent.left; right: parent.right }
                            height: 24; color: "#1A1A2E"
                            radius: frame.d.radius
                            z: 10
                            RowLayout {
                                anchors { fill: parent; leftMargin: 16; rightMargin: 12 }
                                Label { text: "9:41"; font.family: "JetBrains Mono"; font.pixelSize: 10; color: "#A0A0B8" }
                                Item { Layout.fillWidth: true }
                                Label { text: "●●●●○"; font.pixelSize: 8; color: "#4ADE80" }
                            }
                        }

                        // Page stack — switches based on currentTab
                        StackView {
                            id: simStack
                            anchors { top: parent.top; topMargin: 24; left: parent.left; right: parent.right; bottom: parent.bottom }
                            clip: true
                            Component.onCompleted: simStack.push("../screens/DiagnosticScreen.qml")

                            Connections {
                                target: page
                                function onCurrentTabChanged() {
                                    var url = ""
                                    switch (page.currentTab) {
                                        case 0: url = "../screens/DashboardScreen.qml"; break
                                        case 1: url = "../screens/DiagnosticScreen.qml"; break
                                        case 2: url = "../screens/ConfigScreen.qml"; break
                                        case 3: url = "../screens/ReportScreen.qml"; break
                                        case 4: url = "../screens/SettingsScreen.qml"; break
                                    }
                                    if (url !== "") {
                                        simStack.clear()
                                        simStack.push(url)
                                    }
                                }
                            }
                        }
                    }

                // Dynamic Island
                Rectangle {
                    visible: cur().island && portrait
                    anchors { horizontalCenter: parent.horizontalCenter; top: parent.top; topMargin: frame.bh + 8 }
                    width: 126; height: 36; radius: 18; color: "#000000"
                }
            }
        }
    }

}
