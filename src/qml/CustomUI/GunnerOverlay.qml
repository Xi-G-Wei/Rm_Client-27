import QtQuick 2.15

Item {
    id: root
    anchors.fill: parent

    // gameData 由 MainWindow 注入 QML 上下文，统一在叠加层入口适配。
    // qmllint disable unqualified
    readonly property var gameDataContext: typeof gameData !== "undefined" ? gameData : null
    // qmllint enable unqualified
    property var matchUiSource: null
    readonly property bool matchUiActive: Boolean(root.matchUiSource?.active)
    property var customData: root.matchUiActive
        ? root.matchUiSource.myRobotCustomData
        : root.gameDataContext ? root.gameDataContext.myRobotCustomData : ({})
    property bool uiEnabled: root.matchUiActive
        || Boolean(root.gameDataContext?.customUIEnabled)
    readonly property bool currentTheme: root.matchUiActive
        ? Boolean(root.matchUiSource.uses2027Protocol)
        : Boolean(root.gameDataContext?.uses2027Protocol)
    visible: root.uiEnabled

    FrictionWheelStatus {
        id: frictionPanel
        anchors.left: parent.left
        anchors.leftMargin: 15
        anchors.top: parent.top
        anchors.topMargin: 8
        customData: root.customData
        currentTheme: root.currentTheme
    }

    ChassisModePanel {
        id: chassisPanel
        anchors.right: parent.right
        anchors.rightMargin: 15
        anchors.top: parent.top
        anchors.topMargin: 8
        customData: root.customData
        currentTheme: root.currentTheme
    }

    SuperCapacitorBar {
        id: capPanel
        visible: !root.currentTheme && root.customData.superCapEnergy > 0
        anchors.right: parent.right
        anchors.rightMargin: 15
        anchors.verticalCenter: parent.verticalCenter
        customData: root.customData
    }

}
