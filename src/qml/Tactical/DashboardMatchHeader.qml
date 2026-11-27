pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts

Rectangle {
    id: root
    objectName: "dashboardMatchHeader"

    required property var game
    required property var dashboard
    property var snapshot: ({})
    required property string redTeamName
    required property string blueTeamName
    property bool compact: false
    readonly property bool bases: Boolean(game?.hasBases)
    readonly property color redAccent: "#fa9083"
    readonly property color blueAccent: "#85c6f2"
    readonly property int side: Number(dashboard.leadingSide || 0)
    readonly property int margin: Math.max(12, Math.min(26, Math.round(width * 0.0115)))
    readonly property int clockHeight: compact ? 64 : 76
    readonly property int clockWidth: Math.max(220, Math.round((width - 2 * margin - 28) * 0.45 / 2.45))
    readonly property int advantageHeight: 16
    readonly property int contentHeight: clockHeight + 3 + advantageHeight
        + (compact ? 6 : 10)
    implicitHeight: Math.max(bases ? (compact ? 84 : 106) : 0,
                             contentHeight)
    height: implicitHeight
    readonly property bool hasSnapshot: snapshot && snapshot.timeRemainingSec !== undefined
    readonly property int remainingSeconds: hasSnapshot
        ? Number(snapshot.timeRemainingSec || 0)
        : Number(game?.remainingTime || 0)
    readonly property int redTeamScore: hasSnapshot
        ? Number(snapshot.redTeam?.score || 0)
        : Number(game?.redScore || 0)
    readonly property int blueTeamScore: hasSnapshot
        ? Number(snapshot.blueTeam?.score || 0)
        : Number(game?.blueScore || 0)
    readonly property int roundNumber: hasSnapshot
        ? Number(snapshot.round || 1)
        : Number(game?.currentRound || 0)
    readonly property string phaseText: (hasSnapshot ? snapshot.isPaused : game?.is_paused)
        ? "比赛暂停" : hasSnapshot
            ? String(snapshot.phaseLabel || game?.gamePhase || "等待开赛")
            : String(game?.gamePhase || "等待开赛")

    readonly property string clock: {
        const t = Math.max(0, root.remainingSeconds)
        return Math.floor(t / 60).toString().padStart(2, "0") + ":" + (t % 60).toString().padStart(2, "0")
    }

    function structureStats(isBlue) {
        const key = isBlue ? "blue" : "red"
        return {
            baseHp: root.dashboard[key + "BaseHp"],
            baseStatus: root.dashboard[key + "BaseStatus"],
            outpostHp: root.dashboard[key + "OutpostHp"],
            outpostOwnerRed: root.game?.territoryStatus?.[key + "OutpostOwnerRed"],
            outpostControl: root.game?.territoryStatus?.[key + "OutpostControl"]
        }
    }

    color: "#47101820"
    Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: "#596774" }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: root.margin
        anchors.rightMargin: root.margin
        anchors.topMargin: root.compact ? 3 : 6
        anchors.bottomMargin: 4
        spacing: root.compact ? 10 : 14
        DashboardHealthWing {
            Layout.fillWidth: true
            Layout.preferredWidth: 1
            Layout.fillHeight: true
            blue: false
            objectName: "redDashboardHealth"
            compact: root.compact
            teamName: root.redTeamName
            structures: root.bases
            stats: root.structureStats(false)
        }
        ColumnLayout {
            Layout.minimumWidth: root.clockWidth
            Layout.preferredWidth: root.clockWidth
            Layout.maximumWidth: root.clockWidth
            Layout.alignment: Qt.AlignVCenter
            spacing: 3
            Item {
                objectName: "dashboardClockWell"
                Layout.fillWidth: true
                Layout.minimumHeight: root.clockHeight
                Layout.preferredHeight: root.clockHeight
                Layout.maximumHeight: root.clockHeight
                Canvas {
                    anchors.fill: parent
                    onWidthChanged: requestPaint()
                    onHeightChanged: requestPaint()
                    onPaint: {
                        const ctx = getContext("2d")
                        ctx.clearRect(0, 0, width, height)
                        ctx.beginPath()
                        ctx.moveTo(width * .05, .5)
                        ctx.lineTo(width * .95, .5)
                        ctx.lineTo(width - .5, height * .18)
                        ctx.lineTo(width * .95, height * .78)
                        ctx.lineTo(width * .90, height - .5)
                        ctx.lineTo(width * .10, height - .5)
                        ctx.lineTo(width * .05, height * .78)
                        ctx.lineTo(.5, height * .18)
                        ctx.closePath()
                        const edge = ctx.createLinearGradient(0, 0, 0, height)
                        edge.addColorStop(0, "#30383c")
                        edge.addColorStop(1, "#20272a")
                        ctx.fillStyle = edge
                        ctx.fill()
                        ctx.strokeStyle = "#526263"
                        ctx.stroke()
                        const face = ctx.createLinearGradient(0, 0, 0, height)
                        face.addColorStop(0, "#303b3f")
                        face.addColorStop(.48, "#192124")
                        face.addColorStop(1, "#243033")
                        ctx.fillStyle = face
                        ctx.fillRect(width * .25, 1, width * .50, height - 2)
                    }
                }
                Text {
                    objectName: "dashboardRedScore"
                    x: 4
                    width: parent.width * 0.22
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.verticalCenterOffset: 4
                    horizontalAlignment: Text.AlignHCenter
                    text: root.redTeamScore
                    color: root.redAccent
                    font.family: "Bahnschrift"
                    font.pixelSize: root.compact ? 30 : 36
                    font.bold: true
                }
                Text {
                    width: parent.width * 0.52
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.top: parent.top
                    anchors.topMargin: 3
                    horizontalAlignment: Text.AlignHCenter
                    text: (root.roundNumber > 0 ? "第 " + root.roundNumber + " 局 · " : "")
                          + root.phaseText
                    color: "#bce1d3"
                    font.pixelSize: 10
                    elide: Text.ElideRight
                }
                Text {
                    objectName: "dashboardMatchClock"
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.verticalCenterOffset: 5
                    horizontalAlignment: Text.AlignHCenter
                    text: root.clock
                    color: "#fbfcff"
                    font.family: "Bahnschrift"
                    font.pixelSize: root.compact ? 24
                        : Math.max(32, Math.min(38, Math.round(root.width * 0.022)))
                    font.weight: Font.ExtraBold
                    verticalAlignment: Text.AlignVCenter
                }
                Text {
                    objectName: "dashboardBlueScore"
                    anchors.right: parent.right
                    anchors.rightMargin: 4
                    width: parent.width * 0.22
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.verticalCenterOffset: 4
                    horizontalAlignment: Text.AlignHCenter
                    text: root.blueTeamScore
                    color: root.blueAccent
                    font.family: "Bahnschrift"
                    font.pixelSize: root.compact ? 30 : 36
                    font.bold: true
                }
            }
            Rectangle {
                visible: !root.hasSnapshot
                Layout.alignment: Qt.AlignHCenter
                Layout.preferredWidth: Math.min(root.clockWidth, advantage.implicitWidth + 24)
                Layout.minimumHeight: root.advantageHeight
                Layout.preferredHeight: root.advantageHeight
                Layout.maximumHeight: root.advantageHeight
                color: "#6611202c"
                Rectangle { width: 1; height: parent.height; color: "#70657786" }
                Rectangle { anchors.right: parent.right; width: 1; height: parent.height; color: "#70657786" }
                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 7
                    anchors.rightMargin: 6
                    spacing: 5
                    Rectangle { Layout.preferredWidth: 5; Layout.preferredHeight: 5; radius: 3; color: advantage.color }
                    Text {
                        id: advantage
                        objectName: "commandAdvantageLabel"
                        Layout.fillWidth: true
                        text: (root.dashboard.leadText || "等待裁判数据")
                              + (root.dashboard.leadReason ? "  " + root.dashboard.leadReason : "")
                        font.pixelSize: 11
                        font.weight: Font.DemiBold
                        color: root.side === 1 ? root.redAccent : root.side === 2 ? root.blueAccent : "#e2e9ec"
                        elide: Text.ElideRight
                    }
                }
            }
        }
        DashboardHealthWing {
            Layout.fillWidth: true
            Layout.preferredWidth: 1
            Layout.fillHeight: true
            blue: true
            objectName: "blueDashboardHealth"
            compact: root.compact
            teamName: root.blueTeamName
            structures: root.bases
            stats: root.structureStats(true)
        }
    }
}
