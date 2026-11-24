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
        height: root.compact ? 19 : 22
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

                required property var modelData
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredWidth: 1
                border.color: modelData.selected ? wing.accent : "#526b7b"
                color: "#e81b2b35"
                clip: true

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 5
                    anchors.rightMargin: 5
                    spacing: 3
                    Text {
                        text: seat.modelData.number
                        color: wing.accent
                        font.pixelSize: 11
                        font.bold: true
                    }
                    Text {
                        Layout.fillWidth: true
                        horizontalAlignment: Text.AlignRight
                        text: Format.number(seat.modelData.hp)
                        color: "#edf6fa"
                        font.pixelSize: 11
                        elide: Text.ElideRight
                    }
                }

                Rectangle {
                    anchors.left: parent.left
                    anchors.leftMargin: 1
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: 1
                    width: (parent.width - 2) * Format.ratio(seat.modelData.hp, seat.modelData.maxHp)
                    height: 2
                    color: wing.accent
                }
            }
        }
    }
}
