import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// ── Flutter TestResultItem — inline expandable, explicit height ───────
Item {
    id: root
    property var itemData: ({})
    property bool _expanded: false
    property real _contentHeight: 28

    // Explicit height avoids QML implicitHeight circular layout deps
    height: Math.max(28, itemData.isPending ? 28 : _contentHeight)
    clip: true

    on_ContentHeightChanged: { height = Math.max(28, _contentHeight) }

    // ── Pending item ──────────────────────────────────────────────────
    RowLayout {
        id: pendingRow
        anchors { left: parent.left; right: parent.right; top: parent.top; topMargin: 4 }
        visible: itemData.isPending; height: 24; spacing: 8

        Label {
            text: itemData.isRunning ? "⟳" : "⊖"; font.pixelSize: 12
            color: itemData.isRunning ? "#00BCD4" : "#555555"
            RotationAnimation on rotation { running: itemData.isRunning; from:0; to:360; duration:1000; loops:Animation.Infinite }
        }
        Label {
            text: itemData.displayName || ("#" + itemData.testId)
            font.family: "JetBrains Mono"; font.pixelSize: 12; color: "#666666"
            Layout.fillWidth: true; elide: Text.ElideRight
        }
        Label {
            visible: itemData.isRunning; text: "Running..."
            font.family:"JetBrains Mono"; font.pixelSize:10; font.italic:true; color:"#00BCD4"
        }
    }

    // ── Collapsed view ────────────────────────────────────────────────
    RowLayout {
        id: collapsedRow
        anchors { left: parent.left; right: parent.right; top: parent.top; topMargin: 4 }
        visible: !itemData.isPending; height: 24; spacing: 8

        Label {
            text: { var s=itemData.status; if(s===0)return"✓"; if(s===2)return"✗"; if(s===1)return"⚠"; return"⊖" }
            font.pixelSize: 12
            color: { var s=itemData.status; if(s===0)return"#4ADE80"; if(s===2)return"#EF4444"; if(s===1)return"#FACC15"; return"#888" }
        }
        Label {
            text: itemData.displayName || ("#" + itemData.testId)
            font.family: "JetBrains Mono"; font.pixelSize: 12; font.weight: Font.Medium
            color: { var s=itemData.status; return(s===0)?"#E0E0E0":(s===2?"#EF4444":"#A0A0B8") }
            Layout.fillWidth: true; elide: Text.ElideRight
        }
        Rectangle {
            visible: (itemData.durationMs||0)>0; implicitWidth:durText.implicitWidth+12; implicitHeight:20; radius:4
            color: "#2A2A4A"
            Label { id:durText; anchors.centerIn:parent; text:_fmtDur(itemData.durationMs||0); font.family:"JetBrains Mono"; font.pixelSize:10; color:"#A0A0B8" }
        }
        Label { text: _expanded?"▼":"▶"; font.pixelSize:8; color:"#A0A0B8" }
    }

    // ── Expanded details ──────────────────────────────────────────────
    ColumnLayout {
        id: expandedRow
        anchors { left: parent.left; right: parent.right; top: collapsedRow.bottom; topMargin: 4; leftMargin: 20 }
        visible: !itemData.isPending; spacing: 4

        Label {
            visible: itemData.summary||""; text: itemData.summary||""
            font.family:"JetBrains Mono"; font.pixelSize:10; color:"#A0A0B8"
            wrapMode:Text.WordWrap; Layout.fillWidth:true; maximumLineCount:3
        }

        Repeater {
            model: { var p = itemData.properties; return p || [] }
            delegate: RowLayout {
                spacing: 4
                Rectangle { implicitWidth:6; implicitHeight:6; radius:3; color:"#0078D4"; Layout.alignment:Qt.AlignTop; Layout.topMargin:6 }
                Label { text:(modelData["label"]||"?")+":"; font.family:"JetBrains Mono"; font.pixelSize:10; font.weight:Font.DemiBold; color:"#A0A0B8"; Layout.preferredWidth:100 }
                Label { text:modelData["value"]||""; font.family:"JetBrains Mono"; font.pixelSize:10; color:"#E0E0E0"; Layout.fillWidth:true; wrapMode:Text.WordWrap }
            }
        }

        Rectangle {
            visible: itemData.details||""; Layout.fillWidth:true; implicitHeight:Math.min(200,outArea.implicitHeight+16)
            radius:6; color:"#1A1A2E"; clip:true
            Flickable {
                anchors { fill: parent; margins: 8 }
                contentWidth: width
                contentHeight: outArea.implicitHeight
                TextArea {
                    id: outArea
                    width: parent.width
                    text: itemData.details || ""
                    font.family: "JetBrains Mono"; font.pixelSize: 9
                    color: Qt.alpha("#A0A0B8", 0.8)
                    readOnly: true; wrapMode: Text.WordWrap
                    background: Item {}
                }
            }
        }

        Component.onCompleted: {
            root._contentHeight = collapsedRow.height + implicitHeight + 12
        }
    }

    // ── Click ─────────────────────────────────────────────────────────
    MouseArea {
        anchors.fill: parent
        enabled: !itemData.isPending
        onClicked: {
            _expanded = !_expanded
            if (_expanded) _contentHeight = collapsedRow.height + expandedRow.implicitHeight + 12
            else _contentHeight = 28
        }
    }

    function _fmtDur(ms) {
        if (ms<1000) return ms+"ms"
        if (ms<60000) return (ms/1000).toFixed(1)+"s"
        var m=Math.floor(ms/60000); var s=Math.floor((ms%60000)/1000)
        return m+"m"+s+"s"
    }
}
