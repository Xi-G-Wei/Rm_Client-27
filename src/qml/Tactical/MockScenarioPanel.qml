import QtQuick 2.15

Rectangle {
    id: root

    property var source: null
    readonly property bool active: Boolean(root.source && root.source.loaded)
    width: 760
    height: 38
    radius: 3
    color: "#e5152530"
    border.width: 1
    border.color: "#6689a3b4"
    visible: root.active

    Row {
        anchors.fill: parent
        anchors.leftMargin: 7
        anchors.rightMargin: 7
        spacing: 5

        Text {
            width: 72
            anchors.verticalCenter: parent.verticalCenter
            text: "本地模拟"
            color: "#8fe2ae"
            font.pixelSize: 11
            font.bold: true
        }

        Grid {
            anchors.verticalCenter: parent.verticalCenter
            columns: 4
            spacing: 4

            SimButton {
                label: "基础"
                active: root.active
                    && root.source.scenarioId === "match_3v3_basic"
                onTriggered: root.source.selectScenario("match_3v3_basic")
            }
            SimButton {
                label: "受伤"
                active: root.active
                    && root.source.scenarioId === "match_3v3_damage"
                onTriggered: root.source.selectScenario("match_3v3_damage")
            }
            SimButton {
                label: "离线"
                active: root.active
                    && root.source.scenarioId === "match_3v3_offline"
                onTriggered: root.source.selectScenario("match_3v3_offline")
            }
            SimButton {
                label: "结算"
                active: root.active
                    && root.source.scenarioId === "match_3v3_settlement"
                onTriggered: root.source.selectScenario("match_3v3_settlement")
            }
        }

        Rectangle {
            width: 1
            height: parent.height - 10
            anchors.verticalCenter: parent.verticalCenter
            color: "#55718291"
        }

        SimButton {
            anchors.verticalCenter: parent.verticalCenter
            label: root.active && root.source.paused ? "继续" : "暂停"
            active: root.active && root.source.paused
            onTriggered: root.source.togglePause()
        }
        SimButton {
            anchors.verticalCenter: parent.verticalCenter
            label: "重置"
            onTriggered: {
                root.source.reset()
                root.source.start()
            }
        }

        Rectangle {
            width: 1
            height: parent.height - 10
            anchors.verticalCenter: parent.verticalCenter
            color: "#55718291"
        }

        SimButton {
            anchors.verticalCenter: parent.verticalCenter
            label: "0.5×"
            active: root.active
                && Math.abs(root.source.playbackRate - 0.5) < 0.01
            onTriggered: root.source.setPlaybackRate(0.5)
        }
        SimButton {
            anchors.verticalCenter: parent.verticalCenter
            label: "1×"
            active: root.active
                && Math.abs(root.source.playbackRate - 1.0) < 0.01
            onTriggered: root.source.setPlaybackRate(1.0)
        }
        SimButton {
            anchors.verticalCenter: parent.verticalCenter
            label: "2×"
            active: root.active
                && Math.abs(root.source.playbackRate - 2.0) < 0.01
            onTriggered: root.source.setPlaybackRate(2.0)
        }
    }

    component SimButton: Rectangle {
        id: button
        property string label: ""
        property bool active: false
        signal triggered()
        width: Math.max(40, buttonText.implicitWidth + 14)
        height: 24
        radius: 2
        color: button.active ? "#3f6372" : "#263843"
        border.width: 1
        border.color: button.active ? "#8fe2ae" : "#526b7b"

        Text {
            id: buttonText
            anchors.centerIn: parent
            text: button.label
            color: button.active ? "#d9fff0" : "#c3d4df"
            font.pixelSize: 11
            font.bold: button.active
        }

        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            onClicked: button.triggered()
        }
    }
}
