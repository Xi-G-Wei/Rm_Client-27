pragma ComponentBehavior: Bound

import QtQuick 2.15
import QtQuick.Layouts
import "DashboardFormat.js" as Format

Rectangle {
    id: root

    required property var robot
    required property bool blue
    required property color sideSurface
    readonly property color accent: blue ? "#8ac8fb" : "#f69589"
    readonly property bool aerial: robot.number === 6
    readonly property bool isHeavy: robot.number === 1
    readonly property bool showsHeavyForm: isHeavy && robot.heavy !== undefined
    readonly property bool splitHeavyHeader: showsHeavyForm && width < 200
    readonly property DashboardHeavyEffects heavyMotion: heavyEffect.item as DashboardHeavyEffects
    readonly property bool heavyHero: Boolean(heavyMotion?.hero)
    readonly property real heavyHeroMix: heavyMotion?.heroMix || 0
    objectName: isHeavy ? (blue ? "blueHeavyCard" : "redHeavyCard") : ""
    readonly property bool reviving: robot.respawnLeft !== undefined
    readonly property bool invincible: robot.invincibleLeft > 0
    readonly property color fillColor: invincible ? "#ffd580" : reviving ? "#8fe2ae" : accent
    readonly property real progress: invincible ? Format.ratio(robot.invincibleLeft, robot.invincibleTotal)
        : reviving ? Format.ratio(robot.respawnLeft, robot.respawnTotal) : Format.ratio(robot.hp, robot.maxHp)
    readonly property string posture: ["", "攻", "守", "移"][robot.sentry?.mode || 0] || ""
    readonly property real superCapPercent: Number(robot.energy?.superCapPercent ?? -1)
    readonly property real chassisPowerW: Number(robot.energy?.chassisPowerW ?? -1)
    readonly property real chassisPowerLimitW: Number(robot.energy?.chassisPowerLimitW ?? -1)
    readonly property bool offline: robot.online === false
    readonly property bool dead: !root.reviving && Number(robot.hp ?? -1) === 0
    readonly property bool overpowered: chassisPowerW >= 0
        && chassisPowerLimitW > 0 && chassisPowerW > chassisPowerLimitW
    readonly property bool lowSuperCap: superCapPercent >= 0 && superCapPercent < 20
    readonly property string driveTypeLabel: robot.chassis?.driveTypeLabel || ""
    readonly property string chassisModeLabel: robot.chassis?.modeLabel || ""
    readonly property string wheelLegModeLabel: robot.wheelLeg?.telemetryAvailable
        ? String(robot.wheelLeg?.modeLabel || "") : ""
    readonly property int fontSize: height < 65 ? 12 : 14
    readonly property var currentEffects: robot.effects || []
    readonly property int visibleEffects: Math.min(currentEffects.length,
        (splitHeavyHeader && height < 90) || (height < 78 && (posture || invincible)) ? 1 : 2)

    radius: 2
    clip: true
    color: root.offline ? "#32292d"
        : root.invincible ? "#3e3020"
        : Qt.tint("#19262f", Qt.alpha(root.sideSurface, 0.18))
    border.width: root.overpowered || root.lowSuperCap || root.offline ? 2 : 1
    border.color: root.offline ? "#8a9399"
        : root.overpowered ? "#ff786c"
        : root.lowSuperCap ? "#ffc267"
        : Qt.tint("#506674", Qt.alpha(root.fillColor, root.robot.selected ? 0.72 : 0.37))

    Canvas {
        id: pattern
        anchors.fill: parent
        anchors.margins: 1
        onWidthChanged: requestPaint()
        onHeightChanged: requestPaint()
        onPaint: {
            const ctx = getContext("2d")
            ctx.clearRect(0, 0, width, height)
            ctx.beginPath()
            ctx.moveTo(0, 0)
            ctx.lineTo(width * 0.37, 0)
            ctx.lineTo(width * 0.18, height)
            ctx.lineTo(0, height)
            ctx.closePath()
            ctx.fillStyle = "#223541"
            ctx.fill()
            ctx.beginPath()
            ctx.moveTo(width * 0.85, 0)
            ctx.lineTo(width * 0.66, height)
            ctx.lineWidth = 1.5
            ctx.strokeStyle = "#304450"
            ctx.stroke()
            ctx.beginPath()
            for (let x = width * 0.73; x < width - 10; x += 6) {
                ctx.moveTo(x, height - 6)
                ctx.lineTo(x + 6, height - 12)
            }
            ctx.lineWidth = 0.6
            ctx.strokeStyle = "#57707c"
            ctx.stroke()
        }
        visible: !root.invincible
    }

    Rectangle {
        x: 1
        y: 1
        width: (parent.width - 2) * root.progress
        height: parent.height - 2
        gradient: Gradient {
            GradientStop { position: 0; color: Qt.alpha(root.fillColor, 0.45) }
            GradientStop { position: 0.48; color: Qt.alpha(root.fillColor, 0.29) }
            GradientStop { position: 1; color: Qt.alpha(root.fillColor, 0.22) }
        }
        Behavior on width { NumberAnimation { duration: 160 } }
        Rectangle {
            anchors.right: parent.right
            width: 1
            height: parent.height
            color: Qt.alpha(root.fillColor, 0.65)
        }
    }

    Rectangle {
        anchors.left: root.blue ? undefined : parent.left
        anchors.right: root.blue ? parent.right : undefined
        width: 4
        height: parent.height
        color: root.invincible ? "#cf9548" : root.blue ? "#477fae" : "#b45851"
    }

    Item {
        id: portrait
        x: 12
        anchors.verticalCenter: parent.verticalCenter
        anchors.verticalCenterOffset: root.showsHeavyForm ? 8 : 0
        width: Math.max(41, Math.min(parent.width * 0.29, 68))
        height: Math.max(0, Math.min(parent.height - (root.showsHeavyForm ? 25 : 14), 56))

        Image {
            anchors.fill: parent
            source: Format.portrait(root.blue, root.robot.number, root.showsHeavyForm ? false : undefined)
            sourceSize.width: width * 2
            sourceSize.height: height * 2
            opacity: 1 - root.heavyHeroMix
            fillMode: Image.PreserveAspectFit
            smooth: true
        }
        Image {
            anchors.fill: parent
            source: root.showsHeavyForm ? Format.portrait(root.blue, 1, true) : ""
            sourceSize.width: width * 2
            sourceSize.height: height * 2
            opacity: root.heavyHeroMix
            fillMode: Image.PreserveAspectFit
            smooth: true
        }
    }
    Rectangle {
        x: 8
        y: 4
        width: 17
        height: 16
        radius: 2
        color: "#1d2734"
        border.color: root.robot.selected ? "#b8f4da" : Qt.alpha(root.accent, 0.58)
        Text { anchors.centerIn: parent; text: root.robot.number; color: root.accent; font.pixelSize: 10; font.bold: true }
    }

    RowLayout {
        id: heavyHeader
        visible: root.showsHeavyForm
        anchors.left: parent.left
        anchors.leftMargin: 29
        anchors.right: parent.right
        anchors.rightMargin: root.blue ? 9 : 7
        anchors.top: parent.top
        anchors.topMargin: 4
        spacing: 4

        Text {
            objectName: "heavyCardTitle"
            Layout.fillWidth: true
            text: Format.heavyName(root.blue, root.heavyHero)
            textFormat: Text.RichText
            color: "#f0f4f7"
            font.pixelSize: root.width < 191 ? 11 : root.fontSize
            font.weight: Font.DemiBold
        }
        HealthReading { visible: !root.splitHeavyHeader }
    }

    ColumnLayout {
        anchors.left: portrait.right
        anchors.leftMargin: 6
        anchors.right: parent.right
        anchors.rightMargin: root.blue ? 9 : 7
        anchors.verticalCenter: root.showsHeavyForm ? undefined : parent.verticalCenter
        anchors.top: root.showsHeavyForm ? heavyHeader.bottom : undefined
        anchors.topMargin: root.splitHeavyHeader ? 2
            : root.showsHeavyForm ? Math.max(3, Math.min(15, root.height * 0.11)) : 0
        spacing: root.height < 78 ? 1 : 4

        HealthReading {
            visible: root.splitHeavyHeader
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignRight
        }

        RowLayout {
            visible: !root.showsHeavyForm
            Layout.fillWidth: true
            spacing: 2
            Text {
                Layout.fillWidth: true
                text: root.robot.name
                color: "#f0f4f7"
                font.pixelSize: root.fontSize
                font.weight: Font.DemiBold
                elide: Text.ElideRight
            }
            HealthReading {}
        }

        RowLayout {
            Layout.fillWidth: true
            visible: root.showsHeavyForm || root.posture !== "" || root.visibleEffects === 0
            spacing: 4
            DashboardGlyph {
                visible: root.reviving
                kind: "revive"
                tint: root.fillColor
                Layout.preferredWidth: 12
                Layout.preferredHeight: 12
            }
            Text {
                Layout.fillWidth: true
                text: root.offline ? "× 数据中断"
                    : root.dead ? "◇ 战亡"
                    : root.overpowered ? "⚡ 超功率"
                    : root.lowSuperCap ? "⚡ 超电低"
                    : root.posture !== "" ? "○ " + root.posture
                        + (root.robot.sentry.powered ? "  强化" : root.robot.sentry.weakened ? "  弱化" : "")
                        + (root.robot.sentry.cooldown > 0 ? "  ≈" + root.robot.sentry.cooldown + "s" : "")
                        : root.aerial ? "— 单机状态未上报"
                        : root.invincible ? "护盾保护"
                        : root.reviving ? "复活读条"
                        : root.showsHeavyForm ? "◇ " + (root.heavyMotion?.transforming ? "变形中" : Format.heavyStatus(root.robot.heavy))
                        : root.robot.hp !== undefined ? "○ 在场" : "— 待上报"
                color: root.offline ? "#aab3b8"
                    : root.overpowered ? "#ff8b80"
                    : root.lowSuperCap ? "#ffd08d"
                    : root.robot.hp === 0 && !root.reviving ? "#ffb09e"
                    : root.reviving || root.invincible ? root.fillColor
                    : root.heavyHero ? Format.heavyGold()
                    : root.robot.heavy?.allowed === true ? Format.heavyAccent(root.blue) : "#ccdae4"
                font.pixelSize: root.fontSize - 1
                elide: Text.ElideRight
            }
        }

        Repeater {
            model: root.visibleEffects
            DashboardEffectBadge {
                required property int index
                effect: root.currentEffects[index]
                Layout.fillWidth: true
                Layout.preferredHeight: root.height < 78 ? 12 : 15
                labelSize: root.height < 78 ? 9 : 10
            }
        }

        RowLayout {
            Layout.fillWidth: true
            visible: !root.aerial
            spacing: 7
            Meter {
                Layout.fillWidth: true
                label: "弹"
                value: root.robot.ammo ?? "—"
                fraction: Format.ratio(root.robot.ammo, root.robot.ammoPeak)
                accent: root.robot.ammo === undefined ? "#9eafbd" : "#a5dacb"
            }
            Meter {
                Layout.fillWidth: true
                visible: root.robot.heat !== undefined
                label: root.robot.heatPenalty ? "热罚" : "热"
                value: visible ? Math.round(root.robot.heat / root.robot.maxHeat * 100) + "%" : ""
                fraction: Format.ratio(root.robot.heat, root.robot.maxHeat)
                accent: root.robot.heatPenalty ? "#ff997f" : "#ffd08d"
            }
        }

        RowLayout {
            Layout.fillWidth: true
            visible: root.superCapPercent >= 0
                || root.chassisPowerW >= 0
                || root.driveTypeLabel.length > 0
            spacing: 6
            Text {
                visible: root.superCapPercent >= 0
                text: "超电 " + Math.round(root.superCapPercent) + "%"
                color: root.superCapPercent < 20 ? "#ff9b8e"
                    : root.superCapPercent < 50 ? "#f4cb87" : "#8fe2ae"
                font.pixelSize: Math.max(9, root.fontSize - 3)
                font.bold: true
            }
            Text {
                Layout.fillWidth: true
                visible: root.chassisPowerW >= 0
                text: "功率 " + Math.round(root.chassisPowerW) + " W"
                    + (root.chassisPowerLimitW >= 0
                        ? " / " + Math.round(root.chassisPowerLimitW) : "")
                color: root.chassisPowerLimitW >= 0
                    && root.chassisPowerW > root.chassisPowerLimitW
                    ? "#ff9b8e" : "#b8cbd7"
                font.pixelSize: Math.max(9, root.fontSize - 3)
                elide: Text.ElideRight
            }
            Text {
                visible: (root.driveTypeLabel + root.wheelLegModeLabel).length > 0
                text: root.driveTypeLabel
                    + (root.wheelLegModeLabel.length > 0
                        ? " · " + root.wheelLegModeLabel
                        : root.chassisModeLabel.length > 0
                            ? " · " + root.chassisModeLabel : "")
                color: "#a9c6d8"
                font.pixelSize: Math.max(9, root.fontSize - 3)
                elide: Text.ElideRight
            }
        }
    }

    Loader {
        id: heavyEffect
        anchors.fill: parent
        active: root.showsHeavyForm
        z: 10
        sourceComponent: DashboardHeavyEffects {
            objectName: "heavyCardEffects"
            teamColor: Format.heavyAccent(root.blue)
            gold: Format.heavyGold()
            allowed: root.robot.heavy?.allowed === true
            installedAt: root.robot.heavy?.installedAt || 0
            portraitBounds: Qt.rect(portrait.x, portrait.y, portrait.width, portrait.height)
        }
    }

    component HealthReading: Text {
        text: root.invincible ? "无敌 " + root.robot.invincibleLeft + "s"
            : root.reviving ? String(root.robot.respawnLeft)
            : "<b>" + (root.robot.hp ?? "—") + "</b> <span style='font-size:11px'>/ " + (root.robot.maxHp ?? "—") + "</span>"
        textFormat: Text.RichText
        color: root.invincible || root.reviving ? root.fillColor : "#f4f8fe"
        font.family: "Bahnschrift"
        font.pixelSize: root.fontSize
        font.bold: true
    }

    component Meter: ColumnLayout {
        id: meter
        property string label
        property string value
        property real fraction: 0
        property color accent
        property int barHeight: 2
        spacing: 0

        RowLayout {
            Layout.fillWidth: true
            spacing: 1
            Text {
                Layout.fillWidth: true
                text: meter.label
                color: "#d1dce4"
                font.pixelSize: root.fontSize - 2
                elide: Text.ElideRight
            }
            Text {
                text: meter.value
                color: meter.accent
                font.pixelSize: root.fontSize - 2
                font.weight: Font.DemiBold
            }
        }
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: meter.barHeight
            color: meter.value === "—" ? "transparent" : "#4b616b"
            Canvas {
                anchors.fill: parent
                visible: meter.value === "—"
                onWidthChanged: requestPaint()
                onPaint: {
                    const ctx = getContext("2d")
                    ctx.clearRect(0, 0, width, height)
                    ctx.fillStyle = "#4e6878"
                    for (let x = 0; x < width; x += 6)
                        ctx.fillRect(x, 0, 3, height)
                }
            }
            Rectangle { width: parent.width * meter.fraction; height: parent.height; color: meter.accent }
        }
    }
}
