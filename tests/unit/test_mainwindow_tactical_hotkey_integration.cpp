#include "core/GameData.h"
#include "ui/MainWindow.h"
#include "widgets/GameResultWidget.h"
#include "widgets/HeroVideoWidget.h"
#include "widgets/VideoBackgroundWidget.h"
#include "simulator/MockMatchSource.h"

#include <QApplication>
#include <QDir>
#include <QFontMetricsF>
#include <QImage>
#include <QKeyEvent>
#include <QLineEdit>
#include <QQuickItem>
#include <QQuickWidget>
#include <QScopeGuard>
#include <QSignalSpy>
#include <QToolTip>
#include <QWidget>
#include <QtTest>
#include <algorithm>
#include <cmath>
#include <string>

#include "robomaster.pb.h"

namespace {

int childStackIndex(QWidget *parent, QObject *child) {
  if (!parent || !child) {
    return -1;
  }
  return parent->children().indexOf(child);
}

QQuickItem *findVisualItem(QQuickItem *root, const QString &name) {
  if (root->objectName() == name)
    return root;
  for (auto *child : root->childItems()) {
    if (auto *found = findVisualItem(child, name))
      return found;
  }
  return nullptr;
}

} // namespace

class TestMainWindowTacticalHotkeyIntegration : public QObject {
  Q_OBJECT

private slots:
  void testDuelMockStartupAndSwitching() {
    qputenv("RM_MATCH_PROFILE", "rmul1v1");
    qputenv("RM_UI_FIRST_MOCK", "1");
    qputenv("RM_START_VIEW", "video");
    const auto restore = qScopeGuard([] {
      qputenv("RM_MATCH_PROFILE", "legacy2026");
      qunsetenv("RM_UI_FIRST_MOCK");
      qunsetenv("RM_START_VIEW");
    });
    RM::MainWindow window;
    window.resize(1280, 720);
    auto *data = window.findChild<GameData *>();
    auto *mock = window.findChild<RM::MockMatchSource *>();
    QVERIFY(data && mock);
    QCOMPARE(data->matchProfile(), RM::MatchProfile::Rmul1v1);
    QCOMPARE(data->currentRobotId(), 3);
    QCOMPARE(mock->matchSession()->matchSnapshot().profile,
             QStringLiteral("rmul1v1"));
    auto *vehicle = window.findChild<QQuickWidget *>("pilotVehiclePanel");
    QVERIFY(vehicle && vehicle->rootObject());
    auto *card = findVisualItem(vehicle->rootObject(), "pilotVehicleCard");
    QVERIFY(card);
    QCOMPARE(card->property("number").toInt(), 3);
    QVERIFY(mock->selectScenario("match_3v3_basic"));
    QCOMPARE(data->matchProfile(), RM::MatchProfile::Rmul3v3);
    QVERIFY(QMetaObject::invokeMethod(&window, "onMatchProfileRequested",
        Q_ARG(int, static_cast<int>(RM::MatchProfile::Rmul1v1))));
    QCOMPARE(data->matchProfile(), RM::MatchProfile::Rmul1v1);
    QCOMPARE(mock->scenarioId(), QStringLiteral("match_1v1_basic"));
  }

  void initTestCase() {
    qputenv("RM_DISABLE_AUDIO", "1");
    qputenv("RM_MATCH_PROFILE", "legacy2026");
    qunsetenv("RM_DEVTOOLS");
    qunsetenv("RM_S1_ENGINE_HOST");
    Q_INIT_RESOURCE(qml);
    Q_INIT_RESOURCE(resources);
    Q_INIT_RESOURCE(mock);
  }

  void cleanupTestCase() {
    qunsetenv("RM_MATCH_PROFILE");
    Q_CLEANUP_RESOURCE(mock);
    Q_CLEANUP_RESOURCE(resources);
    Q_CLEANUP_RESOURCE(qml);
  }

  void testTacticalMapStartsLargeAndMIsScoped() {
    // MainWindow 使用零延时 single-shot 延后启动服务。保持本测试同步执行，
    // 这样只覆盖 UI 路由，不启动网络、模拟器和视频服务。
    RM::MainWindow window;
    QWidget *clientTarget = window.centralWidget();
    QVERIFY(clientTarget);
    auto *tacticalPage =
        window.findChild<QQuickWidget *>(QStringLiteral("tacticalPageWidget"));
    QVERIFY(tacticalPage);
    QVERIFY(tacticalPage->rootObject());
    auto *gameData = window.findChild<GameData *>();
    auto *heroVideo = window.findChild<RM::HeroVideoWidget *>();
    QVERIFY(gameData);
    QVERIFY(heroVideo);
    QVERIFY(tacticalPage->rootObject()
                ->property("cameraRefreshEnabled")
                .toBool());
    QVERIFY(window.tacticalLargeMapMode());
    QVERIFY(window.tacticalLargeMapRendered());

    QSignalSpy modeChangedSpy(&window,
                              &RM::MainWindow::tacticalLargeMapModeChanged);
    QVERIFY(modeChangedSpy.isValid());

    QKeyEvent shortcutOverride(QEvent::ShortcutOverride, Qt::Key_M,
                               Qt::NoModifier);
    QApplication::sendEvent(clientTarget, &shortcutOverride);
    QVERIFY(shortcutOverride.isAccepted());
    QVERIFY(window.tacticalLargeMapMode());
    QCOMPARE(modeChangedSpy.count(), 0);

    QWidget unrelatedTopLevel;
    QKeyEvent unrelatedWindowM(QEvent::KeyPress, Qt::Key_M, Qt::NoModifier);
    QApplication::sendEvent(&unrelatedTopLevel, &unrelatedWindowM);
    QVERIFY(window.tacticalLargeMapMode());
    QCOMPARE(modeChangedSpy.count(), 0);

    // 现有飞镖模拟会依次经过 GameData 的飞镖信号和真实敌方命中共用的
    // 处理路径；触发后应退出启动时的大地图，切到操作/视频页。
    QKeyEvent dartHit(QEvent::KeyPress, Qt::Key_D,
                      Qt::ControlModifier | Qt::ShiftModifier);
    QApplication::sendEvent(&window, &dartHit);
    QVERIFY(dartHit.isAccepted());
    QVERIFY(!window.tacticalLargeMapMode());
    QVERIFY(!tacticalPage->rootObject()
                 ->property("cameraRefreshEnabled")
                 .toBool());
    QVERIFY(!window.tacticalLargeMapRendered());
    QCOMPARE(modeChangedSpy.count(), 1);

    // 操作页沿用原有 M 键行为，但不能改动战术页新增状态。
    QKeyEvent operationScreenM(QEvent::KeyPress, Qt::Key_M, Qt::NoModifier);
    QApplication::sendEvent(clientTarget, &operationScreenM);
    QVERIFY(operationScreenM.isAccepted());
    QVERIFY(!window.tacticalLargeMapMode());
    QCOMPARE(modeChangedSpy.count(), 1);

    QKeyEvent enterTacticalMode(QEvent::KeyPress, Qt::Key_T,
                                Qt::ControlModifier);
    QApplication::sendEvent(clientTarget, &enterTacticalMode);
    QVERIFY(enterTacticalMode.isAccepted());
    QVERIFY(tacticalPage->rootObject()
                ->property("cameraRefreshEnabled")
                .toBool());
    QVERIFY(!window.tacticalLargeMapMode());
    QCOMPARE(modeChangedSpy.count(), 1);

    QKeyEvent tacticalMAgain(QEvent::KeyPress, Qt::Key_M, Qt::NoModifier);
    QApplication::sendEvent(clientTarget, &tacticalMAgain);
    QVERIFY(tacticalMAgain.isAccepted());
    QVERIFY(window.tacticalLargeMapMode());
    QCOMPARE(modeChangedSpy.count(), 2);

    // QML 战术页显示期间，从工业相机机器人切换到 R3 后，不能重新显示
    // 先前非战术页面遗留的原生视频帧。
    heroVideo->setCurrentRobotId(6);
    heroVideo->setFrame(QImage(16, 16, QImage::Format_RGB32));
    heroVideo->hide();
    gameData->setCurrentRobotId(3);
    gameData->robotDataUpdated(3);
    QVERIFY(heroVideo->isHidden());

    QKeyEvent leaveTacticalMode(QEvent::KeyPress, Qt::Key_T,
                                Qt::ControlModifier);
    QApplication::sendEvent(clientTarget, &leaveTacticalMode);
    QVERIFY(leaveTacticalMode.isAccepted());
    QVERIFY(!tacticalPage->rootObject()
                 ->property("cameraRefreshEnabled")
                 .toBool());
    QVERIFY(heroVideo->isHidden());
    QVERIFY(!window.tacticalLargeMapMode());
    QCOMPARE(modeChangedSpy.count(), 3);
  }

  void testDartOcclusionReturnsToMapOnlyTacticalScreen() {
    RM::MainWindow window;
    QWidget *clientTarget = window.centralWidget();
    QVERIFY(clientTarget);

    auto *tacticalPage =
        window.findChild<QQuickWidget *>(QStringLiteral("tacticalPageWidget"));
    auto *gameData = window.findChild<GameData *>();
    QVERIFY(tacticalPage);
    QVERIFY(gameData);

    QKeyEvent dartHit(QEvent::KeyPress, Qt::Key_D,
                      Qt::ControlModifier | Qt::ShiftModifier);
    QApplication::sendEvent(&window, &dartHit);
    QVERIFY(dartHit.isAccepted());
    QVERIFY(!window.tacticalLargeMapMode());
    QVERIFY(tacticalPage->isHidden());

    // 生产策略把命中当秒计入遮挡，因此固定 10 秒的遮挡需经过
    // 11 次协议倒计时更新才归零。
    gameData->simulateGameTimeElapse(11);

    QVERIFY(!tacticalPage->isHidden());
    QVERIFY(window.tacticalLargeMapMode());
    QVERIFY(window.tacticalLargeMapRendered());

    // 自动恢复后，M 键仍可打开原有战术指令页。
    QKeyEvent showCommandScreen(QEvent::KeyPress, Qt::Key_M, Qt::NoModifier);
    QApplication::sendEvent(clientTarget, &showCommandScreen);
    QVERIFY(showCommandScreen.isAccepted());
    QVERIFY(!window.tacticalLargeMapMode());
  }

  void testTacticalLargeMapSitsBetweenPhasePopupAndTimedEventPopup() {
    RM::MainWindow window;
    window.resize(1920, 1080);
    window.show();
    QTest::qWait(50);

    QWidget *clientTarget = window.centralWidget();
    QVERIFY(clientTarget);

    auto *gameData = window.findChild<GameData *>();
    QVERIFY(gameData);

    auto *tacticalPage =
        window.findChild<QQuickWidget *>(QStringLiteral("tacticalPageWidget"));
    auto *officialPopup = window.findChild<QQuickWidget *>(
        QStringLiteral("officialEventPopupWidget"));
    auto *timedPopup = window.findChild<QQuickWidget *>(
        QStringLiteral("tacticalTimedEventPopup0"));
    auto *popupOverlay =
        window.findChild<QQuickWidget *>(QStringLiteral("popupOverlayWidget"));
    QVERIFY(tacticalPage);
    QVERIFY(officialPopup);
    QVERIFY(timedPopup);
    QVERIFY(popupOverlay);

    auto sendKeyPress = [&](int key, Qt::KeyboardModifiers modifiers = Qt::NoModifier) {
      QKeyEvent event(QEvent::KeyPress, key, modifiers);
      QApplication::sendEvent(clientTarget, &event);
      QVERIFY(event.isAccepted());
    };

    sendKeyPress(Qt::Key_M);
    QTRY_VERIFY(!tacticalPage->isHidden());

    const int officialDisplayToken =
        officialPopup->property("displayToken").toInt();
    sendKeyPress(Qt::Key_F9, Qt::ControlModifier | Qt::ShiftModifier);
    QTRY_VERIFY(officialPopup->property("displayToken").toInt() >
                officialDisplayToken);
    // 官方事件面板当前按产品策略保持隐藏，调试键只刷新事件内容。
    QVERIFY(officialPopup->isHidden());

    sendKeyPress(Qt::Key_F8, Qt::ControlModifier | Qt::ShiftModifier);
    QTRY_VERIFY(!timedPopup->isHidden());

    QTRY_VERIFY(childStackIndex(clientTarget, timedPopup) >
                childStackIndex(clientTarget, tacticalPage));

    sendKeyPress(Qt::Key_M);
    QTRY_VERIFY(window.tacticalLargeMapMode());
    QTRY_VERIFY(window.tacticalLargeMapRendered());
    QTRY_VERIFY(childStackIndex(clientTarget, tacticalPage) >
                childStackIndex(clientTarget, timedPopup));

    robomaster::GameStatus status;
    status.set_current_stage(1);
    status.set_stage_countdown_sec(30);
    status.set_stage_elapsed_sec(0);
    status.set_red_score(0);
    status.set_blue_score(0);
    status.set_current_round(1);
    status.set_total_rounds(1);
    status.set_is_paused(false);
    gameData->updateGameStatus(status);

    QTRY_VERIFY(!popupOverlay->isHidden());
    QTRY_VERIFY(childStackIndex(clientTarget, popupOverlay) >
                childStackIndex(clientTarget, tacticalPage));
    QTRY_VERIFY(childStackIndex(clientTarget, tacticalPage) >
                childStackIndex(clientTarget, timedPopup));
  }

  void test2027DashboardOnTheMainWindow() {
    qunsetenv("RM_START_VIEW");
    qputenv("RM_MATCH_PROFILE", "rmuc2027");
    qputenv("RM_CLIENT_ROBOT_ID", "3");
    const auto resetEnvironment = qScopeGuard([] {
      qputenv("RM_MATCH_PROFILE", "legacy2026");
      qunsetenv("RM_CLIENT_ROBOT_ID");
    });
    RM::MainWindow window;
    window.resize(1536, 960);
    window.show();
    auto *game = window.findChild<GameData *>();
    auto *screen = window.findChild<QQuickWidget *>("tacticalPageWidget");
    QVERIFY(game && screen && screen->rootObject());
    auto *page = screen->rootObject();
    game->setMatchProfile(RM::MatchProfile::Rmuc2027);
    QTRY_COMPARE(game->currentRobotId(), 3);
    QTRY_VERIFY(page->property("commandDashboardActive").toBool());
    QTRY_VERIFY(page->findChild<QQuickItem *>("commandDashboardMap"));

    const QString screenshotDir = qEnvironmentVariable("RM_DASHBOARD_SCREENSHOTS");
    auto capture = [&](const QString &name, bool includeWidgets = false) {
      if (screenshotDir.isEmpty())
        return;
      QTest::qWait(360);
      const QImage image = includeWidgets ? window.grab().toImage() : screen->grabFramebuffer();
      QVERIFY2(!image.isNull(), qPrintable(name));
      QVERIFY(image.save(QDir(screenshotDir).filePath(name)));
    };
    auto stage = [&](int phase, int remaining, int winner = -1) {
      robomaster::GameStatus data;
      data.set_current_stage(phase);
      data.set_stage_countdown_sec(remaining);
      data.set_current_round(1);
      data.set_total_rounds(3);
      data.set_red_score(0);
      data.set_blue_score(winner == 2 ? 1 : 0);
      if (winner >= 0) {
        data.set_game_result(winner);
        data.set_end_reason(2);
      }
      game->updateGameStatus(data);
    };
    auto units = [&](int redPoints, int bluePoints, int infantryHp) {
      robomaster::GlobalUnitStatus data;
      data.set_base_health(7260);
      data.set_enemy_base_health(8390);
      data.set_base_status(2);
      data.set_enemy_base_status(0);
      data.set_outpost_health(500);
      data.set_enemy_outpost_health(620);
      data.set_outpost_status(4);
      data.set_enemy_outpost_status(4);
      data.set_total_cp_ally(redPoints);
      data.set_total_cp_enemy(bluePoints);
      data.set_total_damage_ally(2840);
      data.set_total_damage_enemy(2840);
      for (const int hp : {290, infantryHp, 126, 310, 261, 215, 105, 360})
        data.add_robot_health(hp);
      for (const int ammo : {18, 134, 152, 260})
        data.add_robot_bullets(ammo);
      game->updateGlobalUnitStatus(data);
    };
    auto sendKey = [&](QEvent::Type type, int key, Qt::KeyboardModifiers mods = Qt::NoModifier) {
      QKeyEvent event(type, key, mods);
      QApplication::sendEvent(window.centralWidget(), &event);
      QVERIFY(event.isAccepted());
    };
    auto label = [&]() {
      const auto *text = page->findChild<QObject *>("commandAdvantageLabel");
      return text ? text->property("text").toString() : QString{};
    };
    auto *pilotVideo = window.findChild<RM::VideoBackgroundWidget *>();
    QVERIFY(pilotVideo);
    auto presentPilotFrame = [&](const QImage &frame) {
      const quint64 previousCount = pilotVideo->presentedFrameCount();
      QVERIFY(QMetaObject::invokeMethod(pilotVideo, "onImageReceived", Qt::DirectConnection,
                                        Q_ARG(QImage, frame)));
      QTRY_VERIFY(pilotVideo->presentedFrameCount() > previousCount);
    };
    auto capturePilotFrame = [&](const QString &name) {
      if (screenshotDir.isEmpty())
        return;
      const QImage testFrame(":/images/tactical_camera_preview_default.png");
      QVERIFY(!testFrame.isNull());
      presentPilotFrame(testFrame);
      capture(name, true);
    };

    stage(4, 98);
    for (const int id : {1,3,4,7,101,103,104,107}) {
      robomaster::RobotStaticStatus data;
      data.set_robot_id(id);
      data.set_connection_state(1);
      data.set_max_health(id % 100 == 1 ? 350 : id % 100 == 7 ? 400 : 250);
      data.set_max_heat(200);
      data.set_max_power(80);
      data.set_heat_cooldown_rate(24);
      data.set_level(1);
      game->updateRobotStaticStatus(data);
    }
    robomaster::GlobalLogisticsStatus economy;
    economy.set_remaining_economy(1340);
    economy.set_total_economy_obtained(4520);
    economy.set_tech_level(3);
    economy.set_encryption_level(1);
    game->updateGlobalLogisticsStatus(economy);
    economy.set_remaining_economy(1260);
    game->updateGlobalLogisticsStatus(economy);
    robomaster::RobotDynamicStatus dynamic;
    dynamic.set_current_health(174);
    dynamic.set_current_heat(178);
    dynamic.set_current_buffer_energy(43);
    game->updateRobotDynamicStatus(dynamic);
    robomaster::Buff buff;
    buff.set_robot_id(4);
    buff.set_buff_type(2);
    buff.set_buff_level(50);
    buff.set_buff_max_time(18);
    buff.set_buff_left_time(12);
    game->updateBuff(buff);
    robomaster::SentryStatusSync sentry;
    sentry.set_posture_id(3);
    game->updateDashboardSentry(sentry);
    sentry.set_posture_id(2);
    sentry.set_is_powered(true);
    game->updateDashboardSentry(sentry);
    robomaster::RobotInjuryStat injury;
    injury.set_small_projectile_damage(34);
    game->updateRobotInjury(injury);
    injury.set_small_projectile_damage(61);
    injury.set_large_projectile_damage(100);
    injury.set_module_offline_damage(11);
    game->updateRobotInjury(injury);
    game->simulateDartHit(1, 2);
    game->simulateDartHit(1, 5);
    game->addSystemMessage(QStringLiteral("重装完成三级装配"), "#dcac79");
    game->addSystemMessage(QStringLiteral("蓝方重建 R 前哨"), "#8dc4ff");
    game->addSystemMessage(QStringLiteral("红方步兵进入堡垒"), "#ff9b8d");
    game->addSystemMessage(QStringLiteral("蓝方步兵完成补给"), "#8dc4ff");
    robomaster::GlobalSpecialMechanism special;
    special.add_mechanism_id(1);
    special.add_mechanism_time_sec(4);
    game->updateGlobalSpecialMechanism(special);

    robomaster::RadarInfoToClient radar;
    for (const auto &pos : {QPointF(8.7, 9.1), QPointF(10.9, 6.2), QPointF(5.5, 11.1), QPointF(4.7, 3.9),
                            QPointF(18.8, 8.0), QPointF(20.1, 11.4), QPointF(21.1, 4.1), QPointF(15.3, 10.8)}) {
      auto *entry = radar.add_robot_info();
      entry->set_target_pos_x(qRound(pos.x() * 100));
      entry->set_target_pos_y(qRound(pos.y() * 100));
    }
    game->updateRadarInfo(radar);
    const auto currentMap = [&] { return page->findChild<QQuickItem *>("commandDashboardMap"); };
    const auto camera = [&] { return page->findChild<QQuickItem *>("commandDashboardCamera"); };
    const auto visibleRosterHeight = [&] {
      const auto *roster = page->findChild<QQuickItem *>("commandSummaryRedRoster");
      return roster && roster->isVisible() ? roster->height() : qreal{0};
    };
    const auto findPilotHud = [&]() -> QQuickItem * {
      for (const auto *widget : window.findChildren<QQuickWidget *>()) {
        if (const auto *root = widget->rootObject()) {
          if (auto *hud = root->findChild<QQuickItem *>("pilotMatchHud"))
            return hud;
        }
      }
      return nullptr;
    };
    const auto *map = currentMap();
    QVERIFY(map && map->width() > screen->width() * 0.57 && map->height() > 400);
    units(126, 126, 215);
    QTRY_COMPARE(game->dashboardData().value("leadingSide").toInt(), 0);
    QTRY_VERIFY(label().contains(QStringLiteral("均势")));
    capture("02-neutral.png");

    units(148, 126, 174);
    QTRY_VERIFY(label().contains(QStringLiteral("红方领先")));
    capture("03-red-leading.png");
    units(126, 148, 174);
    QTRY_VERIFY(label().contains(QStringLiteral("蓝方领先")));
    const auto data = game->dashboardData();
    QVERIFY(data.value("redRobots").toList().size() == 5);
    QVERIFY(data.value("blueRobots").toList().size() == 5);
    QVERIFY(data.value("redDarts").toList().size() >= 1);
    QVERIFY(!data.value("injury").toList().isEmpty());
    const auto *report = page->findChild<QQuickItem *>("commandBattlefieldReport");
    const auto *timeline = page->findChild<QQuickItem *>("commandBattlefieldTimeline");
    QVERIFY(report && report->parentItem() && timeline);
    QTRY_VERIFY(std::abs(report->parentItem()->height() - (report->y() + report->height()) - 5) < 1);
    QTRY_VERIFY(timeline->property("capacity").toInt() > 4);
    capture("04-blue-leading.png");

    sendKey(QEvent::KeyPress, Qt::Key_Tab);
    QTRY_VERIFY(page->property("commandSummaryMode").toBool());
    QTRY_VERIFY(visibleRosterHeight() > 350);
    capture("05-tab-summary.png");
    sendKey(QEvent::KeyRelease, Qt::Key_Tab);
    QTRY_VERIFY(!page->property("commandSummaryMode").toBool());
    sendKey(QEvent::KeyPress, Qt::Key_M);
    QTRY_VERIFY(window.tacticalLargeMapRendered());
    capture("06-large-map.png");
    sendKey(QEvent::KeyPress, Qt::Key_Tab);
    QTRY_VERIFY(page->property("commandSummaryMode").toBool());
    QVERIFY(!window.tacticalLargeMapRendered());
    sendKey(QEvent::KeyRelease, Qt::Key_Tab);
    QTRY_VERIFY(window.tacticalLargeMapRendered());
    sendKey(QEvent::KeyPress, Qt::Key_M);
    QTRY_VERIFY(page->property("commandDashboardActive").toBool());
    sendKey(QEvent::KeyPress, Qt::Key_P);
    auto settingsVisible = [&]() {
      const auto panels = window.findChildren<QQuickWidget *>();
      return std::any_of(panels.cbegin(), panels.cend(), [](QQuickWidget *panel) {
        return panel->isVisible() && panel->source().path().endsWith("/SettingsPanel.qml");
      });
    };
    QTRY_VERIFY(settingsVisible());
    sendKey(QEvent::KeyPress, Qt::Key_P);
    QTRY_VERIFY(!settingsVisible());
    auto *help = window.findChild<QWidget *>("helpOverlayPanel");
    QVERIFY(help && !help->isVisible());
    sendKey(QEvent::KeyPress, Qt::Key_F12);
    QTRY_VERIFY(help->isVisible());
    capture("18-shortcuts.png", true);
    sendKey(QEvent::KeyRelease, Qt::Key_F12);
    QTRY_VERIFY(!help->isVisible());
    sendKey(QEvent::KeyPress, Qt::Key_Y, Qt::ControlModifier);
    QTRY_VERIFY(page->property("videoPrimary").toBool());
    QTRY_VERIFY(camera() && currentMap() && camera()->isVisible());
    QVERIFY(camera()->property("centralMode").toBool());
    QVERIFY(camera()->width() > currentMap()->width());
    sendKey(QEvent::KeyPress, Qt::Key_Tab);
    QTRY_VERIFY(page->property("commandSummaryMode").toBool());
    sendKey(QEvent::KeyRelease, Qt::Key_Tab);
    QVERIFY(page->property("videoPrimary").toBool());
    QTRY_VERIFY(camera()->isVisible());
    sendKey(QEvent::KeyPress, Qt::Key_Y, Qt::ControlModifier);
    QTRY_VERIFY(page->property("commandDashboardActive").toBool());
    sendKey(QEvent::KeyPress, Qt::Key_T, Qt::ControlModifier);
    QTRY_VERIFY(!screen->isVisible());
    QTRY_VERIFY(findPilotHud() && findPilotHud()->isVisible());
    const auto *pilotHint = window.findChild<QWidget *>("pilotViewShortcuts");
    const auto *pilotMap = window.findChild<QQuickWidget *>("pilotMiniMap");
    QVERIFY(pilotHint && pilotHint->isVisible());
    QVERIFY(pilotMap && pilotMap->rootObject());
    auto *aimingHud = window.findChild<QQuickWidget *>("pilotAimingHud");
    auto *vehiclePanel = window.findChild<QQuickWidget *>("pilotVehiclePanel");
    QVERIFY(aimingHud && aimingHud->rootObject() && vehiclePanel && vehiclePanel->rootObject());
    auto *aimingCenter = findVisualItem(aimingHud->rootObject(), QStringLiteral("pilotAimingCenter"));
    auto *vehicleCard = findVisualItem(vehiclePanel->rootObject(), QStringLiteral("pilotVehicleCard"));
    QVERIFY(aimingCenter && vehicleCard);
    auto *portrait = findVisualItem(vehicleCard, QStringLiteral("pilotVehiclePortrait"));
    QVERIFY(portrait && portrait->isVisible());
    auto verifyPilotGeometry = [&]() {
      const QPoint marker = aimingHud->mapTo(window.centralWidget(),
          aimingCenter->mapToScene(aimingCenter->boundingRect().center()).toPoint());
      const QPoint expected(window.centralWidget()->width() / 2,
                            window.centralWidget()->height() / 2);
      QVERIFY((marker - expected).manhattanLength() <= 2);
      QVERIFY(vehicleCard->boundingRect().contains(
          portrait->mapRectToItem(vehicleCard, portrait->boundingRect())));
    };
    verifyPilotGeometry();
    const auto *pilotMapFrame = pilotMap->rootObject()->findChild<QQuickItem *>("pilotMapFrame");
    QVERIFY(pilotMapFrame && pilotMapFrame->isVisible());
    const auto *pilotAdvantage = findPilotHud()->findChild<QQuickItem *>("commandAdvantageLabel");
    const auto *pilotClock = findPilotHud()->findChild<QQuickItem *>("dashboardMatchClock");
    const auto *pilotRedBase = findPilotHud()->findChild<QQuickItem *>("redDashboardHealth");
    const auto *pilotBlueBase = findPilotHud()->findChild<QQuickItem *>("blueDashboardHealth");
    QVERIFY(pilotAdvantage && pilotClock && pilotRedBase && pilotBlueBase);
    QVERIFY(pilotRedBase->property("structures").toBool() && pilotBlueBase->property("structures").toBool());
    stage(4, 98);
    units(126, 148, 174);
    QTRY_VERIFY(pilotAdvantage->property("text").toString().contains(QStringLiteral("蓝方领先")));
    QTRY_COMPARE(pilotAdvantage->property("text").toString(), label());
    QCOMPARE(pilotClock->property("text").toString(), QStringLiteral("01:38"));
    QCOMPARE(pilotVideo->geometry(), window.centralWidget()->rect());
    pilotVideo->playUrl(QStringLiteral("udp://127.0.0.1:0"));
    const QColor topVideoColor("#559b79");
    QImage edgeTestFrame(64, 36, QImage::Format_RGB32);
    edgeTestFrame.fill(QColor("#304852"));
    for (int x = 0; x < edgeTestFrame.width(); ++x)
      edgeTestFrame.setPixelColor(x, 0, topVideoColor);
    presentPilotFrame(edgeTestFrame);
    const QImage composite = window.grab().toImage();
    QVERIFY(!composite.isNull());
    const qreal pixelRatio = composite.devicePixelRatio();
    const QPoint origin = pilotVideo->mapTo(&window, QPoint(0, 0));
    const int top = qRound((origin.y() + 1) * pixelRatio);
    for (int quarter : {1, 3}) {
      const int x = qRound((origin.x() + pilotVideo->width() * quarter / 4) * pixelRatio);
      QCOMPARE(composite.pixelColor(x, top), topVideoColor);
    }
    capturePilotFrame("21-rmuc-pilot.png");
    window.resize(1280, 720);
    QTRY_COMPARE(window.centralWidget()->size(), QSize(1280, 720));
    verifyPilotGeometry();
    capturePilotFrame("26-720p-pilot.png");
    window.resize(1536, 960);
    const auto *debugButton = window.findChild<QWidget *>("pilotDebugButton");
    QVERIFY(debugButton && debugButton->isVisible());
    QVERIFY(debugButton->mapTo(&window, QPoint()).x() >= window.width() - 40);
    sendKey(QEvent::KeyPress, Qt::Key_Tab);
    QTRY_VERIFY(screen->isVisible() && page->property("commandSummaryMode").toBool());
    sendKey(QEvent::KeyPress, Qt::Key_Escape);
    QTRY_VERIFY(!screen->isVisible() && !page->property("commandSummaryMode").toBool());
    sendKey(QEvent::KeyRelease, Qt::Key_Tab);
    sendKey(QEvent::KeyPress, Qt::Key_Tab);
    QTRY_VERIFY(screen->isVisible());
    sendKey(QEvent::KeyRelease, Qt::Key_Tab);
    QTRY_VERIFY(!screen->isVisible());
    sendKey(QEvent::KeyPress, Qt::Key_T, Qt::ControlModifier);
    QTRY_VERIFY(screen->isVisible());

    const auto frameVisible = [&] { return camera() && camera()->property("hasVisibleVideo").toBool(); };
    QTRY_VERIFY(camera() && !frameVisible());
    QImage frame(320, 180, QImage::Format_RGB32);
    frame.fill(QColor("#4a928e"));
    game->onVideoFrameReceived(frame);
    QTRY_VERIFY(frameVisible());
    game->clearHeroFrame();
    QTRY_VERIFY(camera() && !frameVisible());

    units(126, 148, 0);
    stage(4, 97);
    game->processRobotRespawnStatusMap({{"is_pending_respawn", true}, {"total_respawn_progress", 10},
                                      {"current_respawn_progress", 4}, {"can_free_respawn", false}});
    const auto localSeat = game->dashboardData().value("redRobots").toList().at(1).toMap();
    QCOMPARE(localSeat.value("respawnLeft").toInt(), 6);
    capture("07-respawn.png");

    for (const auto size : {QSize(1280, 720), QSize(1920, 1080)}) {
      window.resize(size);
      QTRY_COMPARE(screen->size(), size);
      QTRY_VERIFY(currentMap() && currentMap()->width() > size.width() * 0.56);
      QTRY_VERIFY(currentMap()->height() >= size.height() * 0.47);
      if (size.width() == 1280) {
        capture("12-720p.png");
        sendKey(QEvent::KeyPress, Qt::Key_Tab);
        QTRY_VERIFY(visibleRosterHeight() >= 300);
        capture("19-720p-tab.png");
        sendKey(QEvent::KeyRelease, Qt::Key_Tab);
      }
    }
    window.resize(1536, 960);
    QTRY_COMPARE(screen->size(), QSize(1536, 960));

    game->setCurrentRobotId(103);
    stage(4, 90);
    units(148, 126, 174);
    game->updateRadarInfo(radar);
    QTRY_VERIFY(currentMap()->property("viewMirrored").toBool());
    QCOMPARE(game->dashboardData().value("redOutpostHp").toInt(), 500);
    QCOMPARE(game->dashboardData().value("blueBaseHp").toInt(), 7260);
    capture("11-blue-view.png");
    game->setCurrentRobotId(3);

    for (const auto profile : {RM::MatchProfile::Rmul3v3, RM::MatchProfile::Rmul1v1}) {
      game->setMatchProfile(profile);
      stage(4, 64);
      robomaster::RobotStaticStatus data;
      data.set_robot_id(3);
      data.set_connection_state(1);
      data.set_max_health(250);
      game->updateRobotStaticStatus(data);
      data.set_robot_id(103);
      game->updateRobotStaticStatus(data);
      units(162, 148, 174);
      QTRY_VERIFY(page->property("commandDashboardActive").toBool());
      const int cards = profile == RM::MatchProfile::Rmul3v3 ? 3 : 1;
      QCOMPARE(game->dashboardData().value("redRobots").toList().size(), cards);
      const auto messages = game->getSystemMessages();
      QVERIFY(std::none_of(messages.cbegin(), messages.cend(), [](const QVariant &message) {
        return message.toMap().value("text").toString().contains(QStringLiteral("空中支援"));
      }));
      if (!screenshotDir.isEmpty()) {
        game->addSystemMessage(QStringLiteral("红方步兵进入赛场"), "#ff9b8d");
        game->addSystemMessage(QStringLiteral("蓝方步兵进入赛场"), "#8dc4ff");
      }
      capture(profile == RM::MatchProfile::Rmul3v3 ? "08-3v3.png" : "09-1v1.png");

      sendKey(QEvent::KeyPress, Qt::Key_T, Qt::ControlModifier);
      QTRY_VERIFY(!screen->isVisible() && findPilotHud() && findPilotHud()->isVisible());
      const auto *hud = findPilotHud();
      const auto *redRoster = hud->findChild<QObject *>("redPilotRoster");
      const auto *blueRoster = hud->findChild<QObject *>("bluePilotRoster");
      QVERIFY(redRoster && blueRoster);
      QTRY_COMPARE(redRoster->property("count").toInt(), cards);
      QTRY_COMPARE(blueRoster->property("count").toInt(), cards);
      const auto *redStructures = hud->findChild<QQuickItem *>("redDashboardHealth");
      const auto *blueStructures = hud->findChild<QQuickItem *>("blueDashboardHealth");
      QVERIFY(redStructures && !redStructures->property("structures").toBool());
      QVERIFY(blueStructures && !blueStructures->property("structures").toBool());

      if (profile == RM::MatchProfile::Rmul1v1) {
        QCOMPARE(pilotMap->width(), pilotMap->height());
        capturePilotFrame("14-1v1-pilot.png");
      }
      sendKey(QEvent::KeyPress, Qt::Key_Tab);
      QTRY_VERIFY(screen->isVisible() && page->property("commandSummaryMode").toBool());
      QTRY_VERIFY(visibleRosterHeight() >= 72 + cards * 67);
      const auto *summaryMap = page->findChild<QQuickItem *>("commandSummaryMap");
      QTRY_VERIFY(summaryMap && currentMap() && currentMap()->parentItem() == summaryMap &&
                  currentMap()->isVisible());
      capture(profile == RM::MatchProfile::Rmul3v3 ? "20-3v3-tab.png" : "13-1v1-tab.png", true);
      sendKey(QEvent::KeyRelease, Qt::Key_Tab);
      QTRY_VERIFY(!screen->isVisible());

      sendKey(QEvent::KeyPress, Qt::Key_T, Qt::ControlModifier);
      sendKey(QEvent::KeyPress, Qt::Key_Y, Qt::ControlModifier);
      QTRY_VERIFY(camera() && camera()->property("centralMode").toBool());
      if (!screenshotDir.isEmpty()) {
        game->onVideoFrameReceived(QImage(":/images/tactical_camera_preview_default.png"));
        QTRY_VERIFY(frameVisible());
        capture(profile == RM::MatchProfile::Rmul3v3 ? "15-3v3-camera.png" : "16-1v1-camera.png");
        if (profile == RM::MatchProfile::Rmul1v1) {
          window.resize(1280, 720);
          QTRY_COMPARE(screen->size(), QSize(1280, 720));
          capture("17-720p-camera.png");
          window.resize(1536, 960);
          QTRY_COMPARE(screen->size(), QSize(1536, 960));
        }
        game->clearHeroFrame();
      }
      sendKey(QEvent::KeyPress, Qt::Key_Y, Qt::ControlModifier);
      QTRY_VERIFY(page->property("commandDashboardActive").toBool() &&
                  !page->property("videoPrimary").toBool());
    }
    game->setMatchProfile(RM::MatchProfile::Rmuc2027);
    stage(4, 45);
    units(148, 180, 100);
    game->processRobotRespawnStatusMap({{"is_pending_respawn", true}, {"total_respawn_progress", 10},
                                      {"current_respawn_progress", 4}});
    stage(5, 0, 2);
    QVERIFY(game->robotRespawnStatus().isEmpty());
    QTRY_VERIFY(label().contains(QStringLiteral("蓝方获胜")));
    auto *result = window.findChild<GameResultWidget *>();
    QTRY_VERIFY(result && result->isVisible());
    if (!screenshotDir.isEmpty())
      QTest::qWait(2400);
    capture("10-settlement.png", true);
    QTest::keyClick(result, Qt::Key_Escape);
    QTRY_VERIFY(!result->isVisible());
  }
  void test2027PilotPanelShortcuts() {
    qputenv("RM_START_VIEW", "video");
    qputenv("RM_MATCH_PROFILE", "rmuc2027");
    qputenv("RM_CLIENT_ROBOT_ID", "3");
    const auto resetEnvironment = qScopeGuard([] {
      qunsetenv("RM_START_VIEW");
      qputenv("RM_MATCH_PROFILE", "legacy2026");
      qunsetenv("RM_CLIENT_ROBOT_ID");
    });
    RM::MainWindow window;
    window.resize(1536, 960);
    window.show();
    auto *game = window.findChild<GameData *>();
    auto *miniMap = window.findChild<QQuickWidget *>("pilotMiniMap");
    auto *screen = window.findChild<QQuickWidget *>("tacticalPageWidget");
    QVERIFY(game && miniMap && screen);
    QTRY_VERIFY(miniMap->isVisible() && !screen->isVisible());
    QVERIFY(!game->canRemoteAmmo());

    const auto findPanel = [&](const QString &file, int command = 0) -> QQuickWidget * {
      for (auto *panel : window.findChildren<QQuickWidget *>()) {
        if (panel->source().fileName() == file && panel->rootObject() &&
            (!command || panel->rootObject()->property("commandType").toInt() == command))
          return panel;
      }
      return nullptr;
    };

    // An editor that accepts key events exercises the window's shortcut filter.
    QLineEdit focusedChild(window.centralWidget());
    focusedChild.show();
    focusedChild.setFocus();
    QTest::keyClick(&focusedChild, Qt::Key_O);
    auto *supply17 = findPanel(QStringLiteral("AmmoSupplyPanel.qml"), 1);
    QVERIFY(supply17 && supply17->isVisible());
    QVERIFY(!supply17->rootObject()->property("canRemoteAmmo").toBool());
    QTest::keyClick(supply17, Qt::Key_I);
    auto *supply42 = findPanel(QStringLiteral("AmmoSupplyPanel.qml"), 2);
    QVERIFY(supply42 && supply42->isVisible());
    QVERIFY(!supply17->isVisible());
    QTest::keyClick(supply42, Qt::Key_I);
    QVERIFY(!supply42->isVisible());

    QTest::keyClick(miniMap, Qt::Key_H);
    QVERIFY(!findPanel(QStringLiteral("ExchangePanel.qml")));
    QVERIFY(QToolTip::text().contains(QStringLiteral("选择车位并登录")));
    QVERIFY(QMetaObject::invokeMethod(&window, "onSettingsLoginRequested",
                                      Q_ARG(QString, QStringLiteral("R3 - 步兵"))));
    QTest::keyClick(miniMap, Qt::Key_H);
    QVERIFY(!findPanel(QStringLiteral("ExchangePanel.qml")));
    QVERIFY(QToolTip::text().contains(QStringLiteral("未开放远程兑换")));
    robomaster::RobotDynamicStatus dynamicStatus;
    dynamicStatus.set_current_health(174);
    dynamicStatus.set_can_remote_ammo(true);
    game->updateRobotDynamicStatus(dynamicStatus);
    QTest::keyClick(miniMap, Qt::Key_H);
    auto *exchange = findPanel(QStringLiteral("ExchangePanel.qml"));
    QVERIFY(exchange && exchange->isVisible());
    QTest::keyClick(exchange, Qt::Key_H);
    QVERIFY(!exchange->isVisible());

    QTest::keyClick(miniMap, Qt::Key_M);
    auto *largeMap = window.findChild<QQuickWidget *>("pilotLargeMiniMap");
    QTRY_VERIFY(largeMap && largeMap->isVisible());
    QTest::keyClick(largeMap, Qt::Key_I);
    QVERIFY(largeMap->isVisible());
    QVERIFY(!supply42->isVisible());
    QTest::keyClick(largeMap, Qt::Key_M);

    QTest::keyClick(miniMap, Qt::Key_O);
    QVERIFY(supply17->isVisible());
    QTest::keyClick(supply17, Qt::Key_T, Qt::ControlModifier);
    QTRY_VERIFY(screen->isVisible() && !supply17->isVisible());
    QTest::keyClick(screen, Qt::Key_O);
    QTest::keyClick(screen, Qt::Key_H);
    QVERIFY(!supply17->isVisible() && !exchange->isVisible());
    QVERIFY(QToolTip::text().contains(QStringLiteral("切换到图传操作界面")));
    QToolTip::hideText();
  }

  void test2027HeavyCardAnimation_data() {
    QTest::addColumn<bool>("blueSide");
    QTest::newRow("red-infantry") << false;
    QTest::newRow("blue-infantry") << true;
  }

  void test2027HeavyCardAnimation() {
    QFETCH(bool, blueSide);
    const int ownHeavyId = blueSide ? 101 : 1;
    const int otherHeavyId = blueSide ? 1 : 101;
    const QString ownSide = blueSide ? QStringLiteral("blue") : QStringLiteral("red");
    const QString otherSide = blueSide ? QStringLiteral("red") : QStringLiteral("blue");
    const QString teamAccent = blueSide ? QStringLiteral("#8bcdff") : QStringLiteral("#fa9284");
    qputenv("RM_MATCH_PROFILE", "rmuc2027");
    qputenv("RM_CLIENT_ROBOT_ID", QByteArray::number(ownHeavyId + 2));
    const auto resetEnvironment = qScopeGuard([] {
      qputenv("RM_MATCH_PROFILE", "legacy2026");
      qunsetenv("RM_CLIENT_ROBOT_ID");
    });
    RM::MainWindow window;
    window.resize(1536, 960);
    window.show();
    auto *game = window.findChild<GameData *>();
    auto *screen = window.findChild<QQuickWidget *>("tacticalPageWidget");
    QVERIFY(game && screen && screen->rootObject());
    QCOMPARE(game->getCurrentRobotId(), ownHeavyId + 2);
    auto *page = screen->rootObject();
    QTRY_VERIFY(findVisualItem(page, ownSide + QStringLiteral("HeavyCard")));
    auto *ownCard = findVisualItem(page, ownSide + QStringLiteral("HeavyCard"));
    auto *otherCard = findVisualItem(page, otherSide + QStringLiteral("HeavyCard"));
    QVERIFY(otherCard);
    auto *effect = ownCard->findChild<QQuickItem *>("heavyCardEffects");
    auto *title = ownCard->findChild<QQuickItem *>("heavyCardTitle");
    auto *otherTitle = otherCard->findChild<QQuickItem *>("heavyCardTitle");
    QVERIFY(effect && title && otherTitle);
    QVERIFY(title->property("text").toString().contains(teamAccent + QStringLiteral("'>工")));
    QVERIFY(!otherTitle->isVisible());
    QVERIFY(!otherCard->findChild<QQuickItem *>("heavyCardEffects"));

    robomaster::GameStatus status;
    status.set_current_stage(static_cast<int>(GameStage::BATTLE));
    status.set_stage_countdown_sec(290);
    status.set_stage_elapsed_sec(130);
    status.set_current_round(1);
    game->updateGameStatus(status);
    for (const int id : {1, 101}) {
      robomaster::RobotStaticStatus robot;
      robot.set_robot_id(id);
      robot.set_max_health(500);
      robot.set_connection_state(1);
      game->updateRobotStaticStatus(robot);
    }
    robomaster::GlobalUnitStatus units;
    units.set_base_health(9125);
    units.set_enemy_base_health(9740);
    units.set_total_cp_ally(450);
    units.set_total_cp_enemy(512);
    for (const int hp : {474, 250, 240, 310, 481, 174, 188, 320})
      units.add_robot_health(hp);
    game->updateGlobalUnitStatus(units);
    robomaster::GlobalLogisticsStatus logistics;
    logistics.set_tech_level(3);
    game->updateGlobalLogisticsStatus(logistics);
    QTRY_VERIFY(effect->property("allowed").toBool());
    robomaster::RobotStaticStatus heavyStats;
    heavyStats.set_robot_id(ownHeavyId);
    heavyStats.set_connection_state(1);
    heavyStats.set_max_health(600);
    game->updateRobotStaticStatus(heavyStats);
    QCOMPARE(effect->property("ionColor").value<QColor>(), QColor(teamAccent));
    QCOMPARE(effect->z() + effect->parentItem()->z(), 10.0);

    const QString dir = qEnvironmentVariable("RM_DASHBOARD_SCREENSHOTS");
    auto capture = [&](const QString &name) {
      if (dir.isEmpty())
        return;
      const QImage frame = screen->grabFramebuffer();
      QVERIFY(!frame.isNull());
      QVERIFY(frame.save(QDir(dir).filePath(blueSide ? QStringLiteral("蓝方-") + name : name)));
    };
    if (!dir.isEmpty()) {
      QTRY_VERIFY(effect->property("charge").toDouble() > 0.95);
      capture("22-重装待安装.png");
    }
    robomaster::RoboMasterMessage wrapper;
    wrapper.mutable_event()->set_event_id(16);
    wrapper.mutable_event()->set_param(std::to_string(otherHeavyId));
    game->processProtocolData(wrapper);
    QVERIFY(!effect->property("transforming").toBool());
    wrapper.mutable_event()->set_param(std::to_string(ownHeavyId));
    game->processProtocolData(wrapper);
    QTRY_VERIFY(effect->property("transforming").toBool());
    QTRY_VERIFY(effect->property("phase").toDouble() >= 0.48);
    QVERIFY(effect->property("transforming").toBool());
    QVERIFY(effect->property("heroMix").toDouble() > 0);
    capture("23-重装变形浪潮.png");
    QTRY_VERIFY(effect->property("hero").toBool() &&
                !effect->property("transforming").toBool());
    QVERIFY(!otherCard->findChild<QQuickItem *>("heavyCardEffects"));
    QCOMPARE(effect->property("ionColor").value<QColor>(), QColor("#ffcb77"));
    QVERIFY(title->property("text").toString().contains("#ffcb77'>雄"));
    capture("24-重装英雄形态.png");
    QTest::keyPress(window.centralWidget(), Qt::Key_Tab);
    QTRY_VERIFY(page->property("commandSummaryMode").toBool());
    auto *summaryEffects = findVisualItem(page, ownSide + QStringLiteral("SummaryHeavyEffects"));
    QVERIFY(summaryEffects);
    QVERIFY(!findVisualItem(page, otherSide + QStringLiteral("SummaryHeavyEffects")));
    auto *summaryItem = summaryEffects->property("item").value<QObject *>();
    QVERIFY(summaryItem && summaryItem->property("hero").toBool());
    capture("27-步兵端重装全屏战况.png");
    QTest::keyRelease(window.centralWidget(), Qt::Key_Tab);
    QTRY_VERIFY(!page->property("commandSummaryMode").toBool());
    window.resize(1280, 720);
    QTRY_COMPARE(screen->size(), QSize(1280, 720));
    capture("25-重装720p.png");
    // The rich-text spans only color the glyphs. Measure the rendered label without markup.
    const QFontMetricsF titleMetrics(title->property("font").value<QFont>());
    const qreal titleWidth = titleMetrics.horizontalAdvance(QStringLiteral("重装（工/雄）"));
    QTRY_VERIFY(title->isVisible() && title->width() >= titleWidth);
    status.set_current_round(2);
    game->updateGameStatus(status);
    QTRY_VERIFY(!effect->property("hero").toBool() && !effect->property("allowed").toBool());

    const auto seat = blueSide ? QStringLiteral("R3 - 步兵") : QStringLiteral("B3 - 步兵");
    QVERIFY(QMetaObject::invokeMethod(&window, "onSettingsLoginRequested", Q_ARG(QString, seat)));
    QCOMPARE(game->getCurrentRobotId(), otherHeavyId + 2);
    QVERIFY(otherCard->property("showsHeavyForm").toBool());
    QVERIFY(!ownCard->property("showsHeavyForm").toBool());
    QTRY_VERIFY(otherCard->findChild<QQuickItem *>("heavyCardEffects"));
    QTRY_VERIFY(!ownCard->findChild<QQuickItem *>("heavyCardEffects"));
  }
};

QTEST_MAIN(TestMainWindowTacticalHotkeyIntegration)
#include "test_mainwindow_tactical_hotkey_integration.moc"
