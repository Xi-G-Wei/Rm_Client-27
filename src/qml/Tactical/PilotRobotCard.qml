pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import ".." as Hud
import "DashboardFormat.js" as Format

Rectangle {
    id: root
    objectName: "pilotVehicleCard"

    required property var game
    readonly property var robot: game?.myRobot || ({})
    readonly property int number: Number(game?.robotId || 3) % 100
    readonly property bool blue: Number(game?.robotId || 3) >= 100
    readonly property bool heavy: number === 1
    readonly property bool installed: robot.heavy?.installedAt > 0
    readonly property bool hasTelemetry: Boolean(robot.isClientConnected)
    readonly property bool hasModuleData: (game?.moduleData?.length || 0) > 0
    readonly property var moduleStates: game?.moduleStates || []
    readonly property var buffs: game?.buffTimedData || ({})
    readonly property string chassisStatus: game?.chassisStatus || ""
    readonly property color accent: blue ? "#85c6f2" : "#fa9083"
    readonly property color muted: "#a4bac8"
    readonly property real unit: Math.max(0.72, Math.min(width / 495, height / 178))
    readonly property string stateText: !hasTelemetry ? "等待机器人数据"
        : game.currentHealth <= 0 ? "战亡"
        : robot.isOutOfCombat ? "已脱战"
        : game.outOfCombatStatus !== "已脱战" ? game.outOfCombatStatus : "交战中"
    readonly property color stateColor: !hasTelemetry ? muted : game.currentHealth <= 0 ? "#ffa094"
        : robot.isOutOfCombat ? "#a2dac5" : "#f4d49d"

    color: "#eb14222c"
    radius: 2
    border.width: 1
    border.color: Qt.tint("#526979", Qt.alpha(accent, 0.43))
    clip: true

    Canvas {
        id: face
        anchors.fill: parent
        anchors.margins: 1
        onWidthChanged: requestPaint()
        onHeightChanged: requestPaint()
        onPaint: {
            const ctx = getContext("2d")
            ctx.clearRect(0, 0, width, height)
            const surface = ctx.createLinearGradient(0, 0, width * .55, height)
            surface.addColorStop(0, Qt.alpha(root.accent, .17))
            surface.addColorStop(1, "#062d4957")
            ctx.fillStyle = surface
            ctx.beginPath()
            ctx.moveTo(0, 0)
            ctx.lineTo(width * .52, 0)
            ctx.lineTo(width * .27, height)
            ctx.lineTo(0, height)
            ctx.closePath()
            ctx.fill()
            ctx.strokeStyle = "#37515d"
            ctx.lineWidth = 1
            ctx.beginPath()
            for (let x = width - 66; x < width - 5; x += 6) {
                ctx.moveTo(x, height - 4)
                ctx.lineTo(x + 4, height - 8)
            }
            ctx.stroke()
        }
        Connections {
            target: root
            function onAccentChanged() { face.requestPaint() }
        }
    }

    Rectangle {
        x: 1
        y: 1
        width: 3 * root.unit
        height: parent.height - 2
        color: root.blue ? "#477fae" : "#b45851"
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.leftMargin: 13 * root.unit
        anchors.rightMargin: 12 * root.unit
        anchors.topMargin: 8 * root.unit
        anchors.bottomMargin: 9 * root.unit
        spacing: 5 * root.unit

        RowLayout {
            Layout.fillWidth: true
            spacing: 7 * root.unit
            Rectangle {
                Layout.preferredWidth: 24 * root.unit
                Layout.preferredHeight: 22 * root.unit
                color: "#1b2b35"
                border.color: Qt.alpha(root.accent, .68)
                Text {
                    anchors.centerIn: parent
                    text: root.number
                    color: root.accent
                    font.family: "Bahnschrift"
                    font.pixelSize: 15 * root.unit
                    font.bold: true
                }
            }
            Text {
                Layout.fillWidth: true
                text: root.heavy ? Format.heavyName(root.blue, root.installed)
                    : root.number === 6 ? "空中系统" : root.number === 7 ? "哨兵" : "步兵"
                textFormat: root.heavy ? Text.RichText : Text.PlainText
                color: "#eff5fa"
                font.pixelSize: 16 * root.unit
                font.bold: true
                elide: Text.ElideRight
            }
            Text {
                visible: root.chassisStatus.length > 0
                text: root.chassisStatus.startsWith("超限断电")
                    ? root.chassisStatus
                    : root.chassisStatus.indexOf(" W") >= 0
                        ? root.chassisStatus : "功率增益 " + root.chassisStatus
                color: root.chassisStatus.startsWith("超限断电") ? "#ffa094" : "#a9d1f3"
                font.pixelSize: 11 * root.unit
                font.bold: true
            }
            Text {
                visible: Number(root.game?.superCapEnergy ?? -1) >= 0
                text: "超电 " + Math.round(Number(root.game.superCapEnergy)) + "%"
                color: Number(root.game.superCapEnergy) < 20 ? "#ff9b8e"
                    : Number(root.game.superCapEnergy) < 50 ? "#f4cb87" : "#8fe2ae"
                font.pixelSize: 11 * root.unit
                font.bold: true
            }
            Text {
                visible: root.hasTelemetry && root.game.robotLevel > 0
                text: "等级 " + (root.game?.robotLevel || 0)
                color: root.muted
                font.pixelSize: 11 * root.unit
            }
            Text {
                visible: (root.game?.yellowCardCount || 0) > 0
                text: "黄牌 " + (root.game?.yellowCardCount || 0)
                color: "#ffd580"
                font.pixelSize: 11 * root.unit
            }
            Rectangle {
                Layout.preferredWidth: 5 * root.unit
                Layout.preferredHeight: 5 * root.unit
                radius: width / 2
                color: root.stateColor
            }
            Text {
                text: root.stateText
                color: root.stateColor
                font.pixelSize: 11 * root.unit
                font.bold: true
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 74 * root.unit
            spacing: 11 * root.unit
            Image {
                objectName: "pilotVehiclePortrait"
                Layout.preferredWidth: 108 * root.unit
                Layout.fillHeight: true
                source: Format.portrait(root.blue, root.number, root.installed)
                fillMode: Image.PreserveAspectFit
                smooth: true
            }
            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 6 * root.unit
                Meter {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 35 * root.unit
                    label: "血量"
                    value: (root.hasTelemetry ? Format.number(root.game.currentHealth) : "—")
                        + " / " + ((root.game?.maxHealth || 0) > 0 ? Format.number(root.game.maxHealth) : "—")
                    fraction: root.hasTelemetry ? Format.ratio(root.game.currentHealth, root.game.maxHealth) : 0
                    tint: root.accent
                    valueSize: 19 * root.unit
                }
                Meter {
                    visible: root.number !== 6
                    Layout.fillWidth: true
                    Layout.preferredHeight: 22 * root.unit
                    label: "缓冲能量"
                    value: root.hasTelemetry ? Format.number(root.game.bufferEnergy) + " J" : "—"
                    fraction: root.hasTelemetry ? Format.ratio(root.game.bufferEnergy, root.game.maxBufferEnergy) : 0
                    tint: "#97d3eb"
                    valueSize: 12 * root.unit
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.minimumHeight: 18 * root.unit
            visible: fieldEffects.defenseText.length + fieldEffects.coolingText.length
                + fieldEffects.preparationText.length > 0 || attackEffect.active || recoveryEffect.active
            spacing: 7 * root.unit
            Hud.FieldBuffStatus {
                id: fieldEffects
                buffs: root.buffs
                visualScale: root.unit
            }
            EffectText { id: attackEffect; buff: root.buffs.attack; label: "攻击"; tone: "#f2ab9a" }
            EffectText { id: recoveryEffect; buff: root.buffs.recovery; label: "回血"; tone: "#a2dac5" }
            Item { Layout.fillWidth: true }
        }

        Item { Layout.fillHeight: true }
        Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: "#46606d" }

        GridLayout {
            Layout.fillWidth: true
            columns: 5
            rowSpacing: 2 * root.unit
            columnSpacing: 5 * root.unit
            Repeater {
                model: ["机器人", "遥控器", "自定义", "图传", "RFID", "模块", "UWB", "底盘", "17mm", "42mm"]
                RowLayout {
                    id: moduleRow
                    required property int index
                    required property string modelData
                    Layout.preferredWidth: 1
                    Layout.fillWidth: true
                    spacing: 4 * root.unit
                    Rectangle {
                        Layout.preferredWidth: 5 * root.unit
                        Layout.preferredHeight: 5 * root.unit
                        radius: width / 2
                        color: !root.hasModuleData ? "#78919f"
                            : root.moduleStates[moduleRow.index] === 0 ? "#81d6b5" : "#eb877a"
                    }
                    Text {
                        Layout.fillWidth: true
                        text: moduleRow.modelData
                        elide: Text.ElideRight
                        color: "#d1e0e9"
                        font.pixelSize: 13 * root.unit
                    }
                }
            }
        }
    }

    component Meter: Rectangle {
        id: meter
        required property string label
        required property string value
        required property real fraction
        required property color tint
        required property real valueSize
        color: "#15242e"
        border.color: Qt.alpha(tint, .55)
        Rectangle {
            x: 1
            y: 1
            width: Math.max(0, parent.width - 2) * meter.fraction
            height: parent.height - 2
            gradient: Gradient {
                orientation: Gradient.Horizontal
                GradientStop { position: 0; color: Qt.alpha(meter.tint, .23) }
                GradientStop { position: 1; color: Qt.alpha(meter.tint, .50) }
            }
            Rectangle { anchors.right: parent.right; width: 1; height: parent.height; color: meter.tint }
        }
        Text {
            anchors.left: parent.left
            anchors.leftMargin: 8 * root.unit
            anchors.verticalCenter: parent.verticalCenter
            text: meter.label
            color: "#d5e4eb"
            font.pixelSize: 11 * root.unit
        }
        Text {
            anchors.right: parent.right
            anchors.rightMargin: 8 * root.unit
            anchors.verticalCenter: parent.verticalCenter
            text: meter.value
            color: "#f7fbfe"
            font.family: "Bahnschrift"
            font.pixelSize: meter.valueSize
            font.bold: true
        }
    }

    component EffectText: Text {
        required property var buff
        required property string label
        required property color tone
        Layout.alignment: Qt.AlignVCenter
        readonly property bool active: (buff?.level || 0) > 0 && (!(buff?.maxTime > 0) || buff?.leftTime > 0)
        visible: active
        text: label + (buff?.leftTime > 0 ? " " + buff.leftTime + "s" : "")
        font.pixelSize: 10 * root.unit
        color: tone
    }
}
