pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import "DashboardFormat.js" as Format

Item {
    id: root

    required property var game
    required property var dashboard
    property var snapshot: ({})
    required property string redTeamName
    required property string blueTeamName
    property bool compact: false
    implicitHeight: header.height + seats.height + 3

    DashboardMatchHeader {
        id: header
        width: parent.width
        game: root.game
        dashboard: root.dashboard
        snapshot: root.snapshot
        redTeamName: root.redTeamName
        blueTeamName: root.blueTeamName
        compact: root.compact
        color: "transparent"
    }

    RowLayout {
        id: seats
        anchors.top: header.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: header.margin
        anchors.rightMargin: Math.max(37, header.margin)
        anchors.topMargin: 3
        height: root.compact ? 46 : 54
        spacing: 8
        TeamRoster {
            Layout.fillWidth: true
            Layout.fillHeight: true
            blue: false
        }
        Item { Layout.preferredWidth: header.clockWidth }
        TeamRoster {
            Layout.fillWidth: true
            Layout.fillHeight: true
            blue: true
        }
    }

    component TeamRoster: RowLayout {
        id: wing

        required property bool blue
        readonly property color accent: blue ? "#85c6f2" : "#fa9083"
        readonly property var robots: (blue ? root.dashboard.blueRobots : root.dashboard.redRobots) || []
        layoutDirection: wing.blue ? Qt.RightToLeft : Qt.LeftToRight
        spacing: 3

        Repeater {
            objectName: wing.blue ? "bluePilotRoster" : "redPilotRoster"
            model: wing.robots
            Rectangle {
                id: seat
                objectName: "pilotRosterCard"

                required property var modelData
                readonly property bool online: modelData.online !== false
                readonly property bool dead: modelData.alive === false
                    || Number(modelData.hp || 0) <= 0
                readonly property real superCap: Number(
                    modelData.energy?.superCapPercent ?? -1)
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredWidth: 1
                radius: 2
                border.width: modelData.selected ? 2 : 1
                border.color: modelData.selected ? "#c8f7e1"
                    : !online ? "#596268"
                    : dead ? "#8c5554" : Qt.alpha(wing.accent, 0.68)
                color: !online ? "#332b2f"
                    : dead ? "#382a2e" : "#e81b2b35"
                opacity: !online ? 0.62 : dead ? 0.78 : 1.0
                clip: true

                ColumnLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 6
                    anchors.rightMargin: 6
                    anchors.topMargin: 3
                    anchors.bottomMargin: 5
                    spacing: 1

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 4
                        Rectangle {
                            Layout.preferredWidth: 17
                            Layout.preferredHeight: 15
                            radius: 2
                            color: "#172731"
                            border.color: Qt.alpha(wing.accent, 0.62)
                            Text {
                                anchors.centerIn: parent
                                text: seat.modelData.number
                                color: wing.accent
                                font.pixelSize: 10
                                font.bold: true
                            }
                        }
                        Text {
                            Layout.fillWidth: true
                            text: seat.modelData.name || "机器人"
                            color: "#edf6fa"
                            font.pixelSize: 10
                            font.bold: true
                            elide: Text.ElideRight
                        }
                        Rectangle {
                            Layout.preferredWidth: 6
                            Layout.preferredHeight: 6
                            radius: 3
                            color: !seat.online ? "#858b8f"
                                : seat.dead ? "#ff8d83" : "#8fe2ae"
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 4
                        Text {
                            Layout.fillWidth: true
                            text: !seat.online ? "离线"
                                : seat.dead ? "战亡" : "在场"
                            color: !seat.online ? "#9aa1a5"
                                : seat.dead ? "#ff9b8e" : "#b8cbd7"
                            font.pixelSize: 9
                        }
                        Text {
                            horizontalAlignment: Text.AlignRight
                            text: Format.number(seat.modelData.hp)
                                + " / " + Format.number(seat.modelData.maxHp)
                            color: "#f4f8fb"
                            font.pixelSize: 10
                            font.bold: true
                        }
                        Text {
                            visible: seat.online && !seat.dead
                                && seat.superCap >= 0
                            text: "超电 " + Math.round(seat.superCap) + "%"
                            color: seat.superCap < 20 ? "#ff9b8e"
                                : seat.superCap < 50 ? "#f4cb87" : "#83d9bc"
                            font.pixelSize: 8
                        }
                    }
                }

                Rectangle {
                    anchors.left: parent.left
                    anchors.leftMargin: 2
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: 1
                    width: (parent.width - 4) * Format.ratio(seat.modelData.hp, seat.modelData.maxHp)
                    height: 3
                    radius: 1.5
                    color: wing.accent
                }
            }
        }
    }
}
