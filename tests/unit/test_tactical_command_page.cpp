#include "simulator/MockMatchSource.h"
#include "ui/MatchUiProjection.h"

#include <QJSValue>
#include <QPointer>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickView>
#include <QtTest>

class TacticalMainWindowStub : public QObject {
  Q_OBJECT
  Q_PROPERTY(bool tacticalLargeMapMode READ tacticalLargeMapMode WRITE
                 setTacticalLargeMapMode NOTIFY tacticalLargeMapModeChanged)

public:
  bool tacticalLargeMapMode() const { return m_tacticalLargeMapMode; }

  void setTacticalLargeMapMode(bool enabled) {
    if (m_tacticalLargeMapMode == enabled) {
      return;
    }
    m_tacticalLargeMapMode = enabled;
    emit tacticalLargeMapModeChanged();
  }

signals:
  void tacticalLargeMapModeChanged();

private:
  bool m_tacticalLargeMapMode = false;
};

class TacticalGameDataStub : public QObject {
  Q_OBJECT
  Q_PROPERTY(int currentRobotId READ currentRobotId WRITE setCurrentRobotId NOTIFY
                 currentRobotIdChanged)
  Q_PROPERTY(bool hasHeroFrame READ hasHeroFrame NOTIFY heroFrameUpdated)
  Q_PROPERTY(quint64 heroFrameRevision READ heroFrameRevision NOTIFY
                 heroFrameUpdated)
  Q_PROPERTY(QString heroFrameSource READ heroFrameSource NOTIFY
                 heroFrameUpdated)

public:
  int currentRobotId() const { return m_currentRobotId; }
  bool hasHeroFrame() const { return m_hasHeroFrame; }
  quint64 heroFrameRevision() const { return m_heroFrameRevision; }
  QString heroFrameSource() const {
    return m_hasHeroFrame
               ? QStringLiteral("image://herovideo/frame?rev=%1")
                     .arg(m_heroFrameRevision)
               : QString();
  }

  void setCurrentRobotId(int robotId) {
    if (m_currentRobotId == robotId) {
      return;
    }
    m_currentRobotId = robotId;
    emit currentRobotIdChanged();
  }

  void publishHeroFrame() {
    m_hasHeroFrame = true;
    ++m_heroFrameRevision;
    emit heroFrameUpdated();
  }

signals:
  void currentRobotIdChanged();
  void heroFrameUpdated();

private:
  int m_currentRobotId = 1;
  bool m_hasHeroFrame = false;
  quint64 m_heroFrameRevision = 0;
};

class TestTacticalCommandPage : public QObject {
  Q_OBJECT

private slots:
  void initTestCase() {
    Q_INIT_RESOURCE(qml);
    Q_INIT_RESOURCE(resources);
    Q_INIT_RESOURCE(mock);
  }

  void cleanupTestCase() {
    Q_CLEANUP_RESOURCE(mock);
    Q_CLEANUP_RESOURCE(resources);
    Q_CLEANUP_RESOURCE(qml);
  }

  void testObjectivePlateUsesAllOfItsBackground() {
    QQmlEngine engine;
    QQmlComponent component(&engine,
        QUrl(QStringLiteral("qrc:/qml/Tactical/FieldObjective.qml")));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));

    const QVariantMap props{
        {"objectName", "testObjective"}, {"isBase", true},
        {"teamColor", QColor("#f18a80")}, {"hp", 5000}, {"maxHp", 10000},
        {"viewportWidth", 250.0}, {"status", QStringLiteral("装甲展开")},
        {"x", 0.0}, {"y", 40.0}};
    QScopedPointer<QObject> objective(component.createWithInitialProperties(props));
    QVERIFY2(objective, qPrintable(component.errorString()));
    auto *plate = objective->findChild<QQuickItem *>("testObjectivePlate");
    auto *fill = objective->findChild<QQuickItem *>("testObjectiveFill");
    auto *hpLabel = objective->findChild<QObject *>("testObjectiveHp");
    QVERIFY(plate && fill && hpLabel);
    QCOMPARE(fill->height(), plate->height());
    QCOMPARE(fill->width(), plate->width() / 2.0);
    QCOMPARE(hpLabel->property("text").toString(), QString("5000 / 10000"));
    const qreal trackOpacity = plate->property("color").value<QColor>().alphaF();
    const qreal fillOpacity = fill->property("color").value<QColor>().alphaF();
    QVERIFY(trackOpacity < 0.10 && fillOpacity > trackOpacity && fillOpacity < 0.30);
    QVERIFY(plate->x() >= 0.0);
    QCOMPARE(objective->property("x").toReal(), 0.0);

    QVERIFY(objective->setProperty("hp", 0));
    QCOMPARE(fill->width(), 0.0);
    QCOMPARE(hpLabel->property("text").toString(), QString("0 / 10000"));
    QCOMPARE(objective->property("stateText").toString(), QStringLiteral("已摧毁"));

    QQuickItem mapLayer;
    QVERIFY(objective->setProperty("statusLayer", QVariant::fromValue(&mapLayer)));
    QVERIFY(objective->setProperty("x", 120.0));
    QCOMPARE(plate->parentItem(), &mapLayer);
    QCOMPARE(plate->z(), 2.0);
    QCOMPARE(plate->x() + plate->width() / 2.0, 120.0);
    objective.reset();
    QVERIFY(mapLayer.childItems().isEmpty());
  }

  void testFortressCaptureClearsWhenTheRefereeTimerEnds() {
    QQmlEngine engine;
    QQmlComponent component(&engine,
        QUrl(QStringLiteral("qrc:/qml/Tactical/FortressCapture.qml")));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    const QVariantMap props{
        {"captureColor", QColor("#82b9f3")}, {"remainingSec", 5}};
    QQuickItem mapLayer;
    QScopedPointer<QObject> capture(component.createWithInitialProperties(props));
    QVERIFY2(capture, qPrintable(component.errorString()));
    auto *overlay = qobject_cast<QQuickItem *>(capture.data());
    QVERIFY(overlay);
    overlay->setParentItem(&mapLayer);
    QCOMPARE(capture->property("progress").toReal(), 0.5);
    QVERIFY(capture->property("visible").toBool());
    QVERIFY(capture->setProperty("remainingSec", 0));
    QVERIFY(!capture->property("visible").toBool());
    QVERIFY(capture->setProperty("temporarilyControlled", true));
    QVERIFY(capture->property("visible").toBool());
    QVERIFY(!capture->property("timing").toBool());
  }

  void testLargeMapOverlayLifecycleAndGeometry_data() {
    QTest::addColumn<int>("pageWidth");
    QTest::addColumn<int>("pageHeight");

    // 下列数据仅验证跨平台一致的 Qt 逻辑坐标；Ubuntu 上的
    // X11/Wayland 场景图仍需在目标机器单独验证。
    QTest::newRow("low-resolution-4-by-3") << 800 << 600;
    QTest::newRow("hd-16-by-9") << 1280 << 720;
    QTest::newRow("laptop-near-16-by-9") << 1366 << 768;
    QTest::newRow("full-hd") << 1920 << 1080;
    QTest::newRow("16-by-10") << 1920 << 1200;
    QTest::newRow("qhd") << 2560 << 1440;
    QTest::newRow("ultrawide") << 3440 << 1440;
    QTest::newRow("4k") << 3840 << 2160;
  }

  void testLargeMapOverlayLifecycleAndGeometry() {
    QFETCH(int, pageWidth);
    QFETCH(int, pageHeight);

    QQmlEngine engine;
    TacticalMainWindowStub mainWindow;
    engine.rootContext()->setContextProperty(QStringLiteral("mainWindow"),
                                             &mainWindow);

    QQmlComponent component(
        &engine,
        QUrl(QStringLiteral("qrc:/qml/Tactical/TacticalCommandPage.qml")));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));

    QScopedPointer<QObject> pageObject(component.create());
    QVERIFY2(pageObject, qPrintable(component.errorString()));
    auto *page = qobject_cast<QQuickItem *>(pageObject.data());
    QVERIFY(page);
    page->setWidth(pageWidth);
    page->setHeight(pageHeight);

    QCOMPARE(page->property("largeMapMode").toBool(), false);
    QCOMPARE(page->property("largeMapOverlayReady").toBool(), false);
    const QString originalLayoutMode = page->property("layoutMode").toString();
    page->setProperty("selectedRobotId", QStringLiteral("A3"));

    QObject *loader =
        page->findChild<QObject *>(QStringLiteral("tacticalLargeMapLoader"));
    QVERIFY(loader);
    auto *centerMap = qobject_cast<QQuickItem *>(
        page->findChild<QObject *>(QStringLiteral("tacticalCenterMap")));
    auto *auxMap = qobject_cast<QQuickItem *>(
        page->findChild<QObject *>(QStringLiteral("tacticalAuxMap")));
    QVERIFY(centerMap);
    QVERIFY(auxMap);
    QCOMPARE(centerMap->property("visualScale").toReal(), 1.0);
    QCOMPARE(auxMap->property("visualScale").toReal(), 1.0);
    QVERIFY(!centerMap->property("threadedCanvasRendering").toBool());
    QVERIFY(!auxMap->property("threadedCanvasRendering").toBool());
    QVERIFY(centerMap->isVisible());
    QVERIFY(!auxMap->isVisible());
    QCOMPARE(loader->property("active").toBool(), false);
    QVERIFY(
        !page->findChild<QObject *>(QStringLiteral("tacticalLargeMapOverlay")));

    mainWindow.setTacticalLargeMapMode(true);
    QTRY_VERIFY(page->property("largeMapMode").toBool());
    QTRY_VERIFY(page->property("largeMapOverlayReady").toBool());

    QObject *overlayObject = nullptr;
    QTRY_VERIFY((overlayObject = page->findChild<QObject *>(
                     QStringLiteral("tacticalLargeMapOverlay"))) != nullptr);
    auto *overlay = qobject_cast<QQuickItem *>(overlayObject);
    QVERIFY(overlay);
    QTRY_COMPARE(overlay->width(), static_cast<qreal>(pageWidth));
    QTRY_COMPARE(overlay->height(), static_cast<qreal>(pageHeight));
    QVERIFY(!centerMap->isVisible());
    QVERIFY(!auxMap->isVisible());

    auto *inputBlocker = qobject_cast<QQuickItem *>(page->findChild<QObject *>(
        QStringLiteral("tacticalLargeMapInputBlocker")));
    QVERIFY(inputBlocker);
    QCOMPARE(inputBlocker->width(), overlay->width());
    QCOMPARE(inputBlocker->height(), overlay->height());

    QObject *largeMapObject =
        page->findChild<QObject *>(QStringLiteral("tacticalLargeMap"));
    auto *largeMap = qobject_cast<QQuickItem *>(largeMapObject);
    QVERIFY(largeMap);
    QCOMPARE(largeMap->property("pageRoot").value<QObject *>(),
             pageObject.data());
    QCOMPARE(largeMap->scale(), 1.0);
    QCOMPARE(largeMap->z(), 1.0);
    QVERIFY(largeMap->property("threadedCanvasRendering").toBool());
    QVERIFY(largeMap->z() > inputBlocker->z());

    const QRectF mappedBounds = largeMap->mapRectToItem(
        page, QRectF(0.0, 0.0, largeMap->width(), largeMap->height()));
    QVERIFY(mappedBounds.left() >= -1.0);
    QVERIFY(mappedBounds.top() >= -1.0);
    QVERIFY(mappedBounds.right() <= page->width() + 1.0);
    QVERIFY(mappedBounds.bottom() <= page->height() + 1.0);
    const bool fillsWidth = qAbs(mappedBounds.width() - page->width()) <= 10.0;
    const bool fillsHeight =
        qAbs(mappedBounds.height() - page->height()) <= 1.0;
    QVERIFY2(fillsWidth || fillsHeight,
             "The aspect-preserving map must fill at least one screen axis");
    QCOMPARE(largeMap->width(), mappedBounds.width());
    QCOMPARE(largeMap->height(), mappedBounds.height());
    const qreal expectedWidth =
        qMin(static_cast<qreal>(pageWidth),
             static_cast<qreal>(pageHeight) * 840.0 / 474.0);
    const qreal expectedHeight = expectedWidth * 474.0 / 840.0;
    QVERIFY(qAbs(largeMap->width() - expectedWidth) <= 0.001);
    QVERIFY(qAbs(largeMap->height() - expectedHeight) <= 0.001);
    QVERIFY(qAbs(largeMap->property("visualScale").toReal() -
                 largeMap->width() / 840.0) <= 0.001);

    QVariantMap robot;
    robot.insert(QStringLiteral("x"), 0.25);
    robot.insert(QStringLiteral("y"), 0.20);
    QVariant hitAtRenderedPosition;
    QVERIFY(QMetaObject::invokeMethod(
        largeMap, "isRobotHit", Q_RETURN_ARG(QVariant, hitAtRenderedPosition),
        Q_ARG(QVariant, QVariant(robot)), Q_ARG(QVariant, QVariant(0.25)),
        Q_ARG(QVariant, QVariant(0.80))));
    QVERIFY(hitAtRenderedPosition.toBool());
    QVariant missAtUnflippedPosition;
    QVERIFY(QMetaObject::invokeMethod(
        largeMap, "isRobotHit", Q_RETURN_ARG(QVariant, missAtUnflippedPosition),
        Q_ARG(QVariant, QVariant(robot)), Q_ARG(QVariant, QVariant(0.25)),
        Q_ARG(QVariant, QVariant(0.20))));
    QVERIFY(!missAtUnflippedPosition.toBool());

    const QJSValue model = largeMap->property("model").value<QJSValue>();
    QVERIFY(model.isObject());
    QCOMPARE(model.property(QStringLiteral("radarAgeMs")).toInt(), 118);

    QPointer<QObject> destroyedWithLoader = overlayObject;
    mainWindow.setTacticalLargeMapMode(false);
    QTRY_VERIFY(!page->property("largeMapMode").toBool());
    QTRY_VERIFY(!page->property("largeMapOverlayReady").toBool());
    QTRY_VERIFY(destroyedWithLoader.isNull());
    QVERIFY(centerMap->isVisible());
    QVERIFY(!auxMap->isVisible());
    QCOMPARE(page->property("layoutMode").toString(), originalLayoutMode);
    QCOMPARE(page->property("selectedRobotId").toString(),
             QStringLiteral("A3"));
  }

  void testLargeMapUsesSameCampViewMappingAsTacticalMaps() {
    QQmlEngine engine;
    TacticalMainWindowStub mainWindow;
    TacticalGameDataStub gameData;
    gameData.setCurrentRobotId(101);
    engine.rootContext()->setContextProperty(QStringLiteral("mainWindow"),
                                             &mainWindow);
    engine.rootContext()->setContextProperty(QStringLiteral("gameData"),
                                             &gameData);

    QQmlComponent component(
        &engine,
        QUrl(QStringLiteral("qrc:/qml/Tactical/TacticalCommandPage.qml")));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));

    QScopedPointer<QObject> pageObject(component.create());
    QVERIFY2(pageObject, qPrintable(component.errorString()));
    auto *page = qobject_cast<QQuickItem *>(pageObject.data());
    QVERIFY(page);
    page->setWidth(1920);
    page->setHeight(1080);

    QCOMPARE(page->property("allyIsBlue").toBool(), true);
    QCOMPARE(page->property("tacticalMapViewMirrored").toBool(), true);
    QCOMPARE(page->property("tacticalMapBackgroundSource").toString(),
             QStringLiteral("qrc:/images/minimap_bg_blue_left.png"));

    auto *centerMap = qobject_cast<QQuickItem *>(
        page->findChild<QObject *>(QStringLiteral("tacticalCenterMap")));
    QVERIFY(centerMap);
    QCOMPARE(centerMap->property("backgroundSource").toString(),
             QStringLiteral("qrc:/images/minimap_bg_blue_left.png"));
    QCOMPARE(centerMap->property("viewMirrored").toBool(), true);

    mainWindow.setTacticalLargeMapMode(true);
    QTRY_VERIFY(page->property("largeMapOverlayReady").toBool());

    auto *largeMap = qobject_cast<QQuickItem *>(
        page->findChild<QObject *>(QStringLiteral("tacticalLargeMap")));
    QVERIFY(largeMap);
    QCOMPARE(largeMap->property("backgroundSource").toString(),
             QStringLiteral("qrc:/images/minimap_bg_blue_left.png"));
    QCOMPARE(largeMap->property("viewMirrored").toBool(), true);

    QVariantMap robot;
    robot.insert(QStringLiteral("x"), 0.25);
    robot.insert(QStringLiteral("y"), 0.20);
    QVariant transformedRobot;
    QVERIFY(QMetaObject::invokeMethod(
        largeMap, "transformRobot", Q_RETURN_ARG(QVariant, transformedRobot),
        Q_ARG(QVariant, QVariant(robot))));
    QVariant hitAtRotatedPosition;
    QVERIFY(QMetaObject::invokeMethod(
        largeMap, "isRobotHit", Q_RETURN_ARG(QVariant, hitAtRotatedPosition),
        Q_ARG(QVariant, transformedRobot), Q_ARG(QVariant, QVariant(0.75)),
        Q_ARG(QVariant, QVariant(0.20))));
    QVERIFY(hitAtRotatedPosition.toBool());

    QVariant missAtRedPerspectivePosition;
    QVERIFY(QMetaObject::invokeMethod(
        largeMap, "isRobotHit",
        Q_RETURN_ARG(QVariant, missAtRedPerspectivePosition),
        Q_ARG(QVariant, transformedRobot), Q_ARG(QVariant, QVariant(0.25)),
        Q_ARG(QVariant, QVariant(0.80))));
    QVERIFY(!missAtRedPerspectivePosition.toBool());
  }

  void testCameraRefreshUsesHeroFrameRevisionDirectly() {
    QQmlEngine engine;
    TacticalMainWindowStub mainWindow;
    TacticalGameDataStub gameData;
    engine.rootContext()->setContextProperty(QStringLiteral("mainWindow"),
                                             &mainWindow);
    engine.rootContext()->setContextProperty(QStringLiteral("gameData"),
                                             &gameData);

    QQmlComponent component(
        &engine,
        QUrl(QStringLiteral("qrc:/qml/Tactical/TacticalCommandPage.qml")));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));

    QScopedPointer<QObject> pageObject(component.create());
    QVERIFY2(pageObject, qPrintable(component.errorString()));
    QObject *page = pageObject.data();

    gameData.publishHeroFrame();
    QCOMPARE(page->property("heroCameraFrameSource").toString(), QString());

    QVERIFY(page->setProperty("cameraRefreshEnabled", true));
    QTRY_COMPARE(page->property("heroCameraFrameSource").toString(),
                 gameData.heroFrameSource());

    QVERIFY(QMetaObject::invokeMethod(page, "sampleHeroCameraFps"));
    for (int i = 0; i < 7; ++i) {
      gameData.publishHeroFrame();
    }
    QVERIFY(QMetaObject::invokeMethod(page, "sampleHeroCameraFps"));
    QCOMPARE(page->property("heroCameraFps").toReal(), 7.0);

    QObject *camera =
        page->findChild<QObject *>(QStringLiteral("tacticalAuxCamera"));
    QVERIFY(camera);
    QTRY_COMPARE(camera->property("liveFrameSource").toString(),
                 gameData.heroFrameSource());
    QCOMPARE(camera->property("liveFps").toReal(), 7.0);
    QCOMPARE(camera->property("showGrid").toBool(), false);

    mainWindow.setTacticalLargeMapMode(true);
    QTRY_COMPARE(page->property("heroCameraFrameSource").toString(), QString());
    QTRY_COMPARE(camera->property("liveFrameSource").toString(), QString());
    QTRY_VERIFY(!camera->property("visible").toBool());
    mainWindow.setTacticalLargeMapMode(false);
    QTRY_COMPARE(camera->property("liveFrameSource").toString(),
                 gameData.heroFrameSource());
    QTRY_VERIFY(camera->property("visible").toBool());
    QVERIFY(QMetaObject::invokeMethod(page, "sampleHeroCameraFps"));
    QCOMPARE(page->property("heroCameraFps").toReal(), 0.0);
    for (int i = 0; i < 3; ++i) {
      gameData.publishHeroFrame();
    }
    QVERIFY(QMetaObject::invokeMethod(page, "sampleHeroCameraFps"));
    QCOMPARE(page->property("heroCameraFps").toReal(), 3.0);

    QVERIFY(page->setProperty("heroCameraGridVisible", true));
    QTRY_VERIFY(camera->property("showGrid").toBool());
  }

  void testMockMatchSourceDrivesCommandDashboard() {
    RM::MockMatchSource source;
    QVERIFY2(
        source.loadFromFile(
            QStringLiteral(":/mock/3v3/match_3v3_basic.json")),
        qPrintable(source.lastError()));
    RM::MatchUiProjection projection(source.matchSession());

    QQmlEngine engine;
    TacticalMainWindowStub mainWindow;
    engine.rootContext()->setContextProperty(QStringLiteral("mainWindow"),
                                             &mainWindow);
    engine.rootContext()->setContextProperty(QStringLiteral("mockMatchSource"),
                                             &source);
    engine.rootContext()->setContextProperty(QStringLiteral("mockUiSource"),
                                             &projection);

    QQmlComponent component(
        &engine,
        QUrl(QStringLiteral("qrc:/qml/Tactical/TacticalCommandPage.qml")));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    QScopedPointer<QObject> pageObject(component.create());
    QVERIFY2(pageObject, qPrintable(component.errorString()));
    auto *page = qobject_cast<QQuickItem *>(pageObject.data());
    QVERIFY(page);
    page->setWidth(1920);
    page->setHeight(1080);

    QTRY_VERIFY(page->property("uiFirstMode").toBool());
    QTRY_VERIFY(page->property("commandDashboardActive").toBool());
    QObject *scenarioPanel =
        page->findChild<QObject *>(QStringLiteral("mockScenarioPanel"));
    QVERIFY(scenarioPanel);
    QTRY_VERIFY(scenarioPanel->property("visible").toBool());
    QObject *clock =
        page->findChild<QObject *>(QStringLiteral("dashboardMatchClock"));
    QObject *redScore =
        page->findChild<QObject *>(QStringLiteral("dashboardRedScore"));
    auto *clockItem = qobject_cast<QQuickItem *>(clock);
    auto *advantageLabel = qobject_cast<QQuickItem *>(
        page->findChild<QObject *>(
            QStringLiteral("commandAdvantageLabel")));
    auto *clockWell = qobject_cast<QQuickItem *>(
        page->findChild<QObject *>(
            QStringLiteral("dashboardClockWell")));
    QVERIFY(clock);
    QVERIFY(redScore);
    QVERIFY(clockItem);
    QVERIFY(advantageLabel);
    QVERIFY(clockWell);
    const QRectF clockTextInWell = clockItem->mapRectToItem(
        clockWell, QRectF(0, 0, clockItem->width(), clockItem->height()));
    QVERIFY(clockTextInWell.top() >= 0.0);
    QVERIFY(clockTextInWell.bottom() <= clockWell->height() + 0.5);
    QVERIFY(!advantageLabel->isVisible());

    QVERIFY(source.start());
    QVERIFY(source.advanceByMs(12000));
    QVERIFY(source.pause());
    QTRY_COMPARE(clock->property("text").toString(), QStringLiteral("05:00"));
    QCOMPARE(redScore->property("text").toString(), QStringLiteral("0"));

    QVERIFY(source.resume());
    QVERIFY(source.advanceByMs(8000));
    QVERIFY(source.pause());
    QTRY_COMPARE(redScore->property("text").toString(), QStringLiteral("1"));

    QVERIFY(source.resume());
    QVERIFY(source.advanceByMs(40000));
    QTRY_COMPARE(clock->property("text").toString(), QStringLiteral("00:00"));
    QTRY_COMPARE(redScore->property("text").toString(), QStringLiteral("2"));
    QObject *settlementBanner =
        page->findChild<QObject *>(QStringLiteral("mockSettlementBanner"));
    QVERIFY(settlementBanner);
    QTRY_VERIFY(settlementBanner->property("visible").toBool());

    QVERIFY(QMetaObject::invokeMethod(
        page, "selectRobot", Q_ARG(QVariant, QVariant("red-3")),
        Q_ARG(QVariant, false)));
    QObject *selectedPanel =
        page->findChild<QObject *>(QStringLiteral("commandSelectedRobotPanel"));
    QVERIFY(selectedPanel);
    QTRY_VERIFY(selectedPanel->property("visible").toBool());
    QVERIFY(QMetaObject::invokeMethod(page, "clearSelection"));
    QTRY_VERIFY(!selectedPanel->property("visible").toBool());
  }

  void testBattlefieldReportTouchesRenderedMapEdge() {
    RM::MockMatchSource source;
    QVERIFY(source.loadFromFile(
        QStringLiteral(":/mock/3v3/match_3v3_basic.json")));
    RM::MatchUiProjection projection(source.matchSession());

    QQuickView view;
    view.rootContext()->setContextProperty(QStringLiteral("mockMatchSource"),
                                           &source);
    view.rootContext()->setContextProperty(QStringLiteral("mockUiSource"),
                                           &projection);
    view.setResizeMode(QQuickView::SizeRootObjectToView);
    view.resize(1920, 1080);
    view.setSource(
        QUrl(QStringLiteral("qrc:/qml/Tactical/TacticalCommandPage.qml")));
    QVERIFY2(view.status() == QQuickView::Ready,
             view.errors().isEmpty()
                 ? ""
                 : qPrintable(view.errors().first().toString()));
    view.show();
    QQuickItem *page = view.rootObject();
    QVERIFY(page);
    QTRY_VERIFY(page->property("commandDashboardActive").toBool());

    auto *mapItem = qobject_cast<QQuickItem *>(
        page->findChild<QObject *>(QStringLiteral("commandDashboardMap")));
    auto *mapArea = qobject_cast<QQuickItem *>(
        mapItem
            ? mapItem->findChild<QObject *>(
                  QStringLiteral("tacticalRadarMapArea"))
            : nullptr);
    auto *battlefieldReport = qobject_cast<QQuickItem *>(
        page->findChild<QObject *>(QStringLiteral("commandBattlefieldReport")));
    QVERIFY(mapArea);
    QVERIFY(mapItem);
    QVERIFY(battlefieldReport);
    QTRY_VERIFY(mapArea->width() > 0 && mapArea->height() > 0);
    QTRY_VERIFY(battlefieldReport->width() > 0
                && battlefieldReport->height() > 0);

    const QRectF mapAreaRect = mapArea->mapRectToItem(
        page, QRectF(0, 0, mapArea->width(), mapArea->height()));
    const QRectF reportRect = battlefieldReport->mapRectToItem(
        page,
        QRectF(0, 0, battlefieldReport->width(), battlefieldReport->height()));
    QVERIFY(reportRect.top() - mapAreaRect.bottom() <= 8.0);

    auto *scenarioPanel = qobject_cast<QQuickItem *>(
        page->findChild<QObject *>(QStringLiteral("mockScenarioPanel")));
    auto *blueRoster = qobject_cast<QQuickItem *>(
        page->findChild<QObject *>(QStringLiteral("commandBlueRoster")));
    QVERIFY(scenarioPanel);
    QVERIFY(blueRoster);
    QTRY_VERIFY(scenarioPanel->width() > 0 && blueRoster->width() > 0);
    const QRectF scenarioPanelRect = scenarioPanel->mapRectToItem(
        page,
        QRectF(0, 0, scenarioPanel->width(), scenarioPanel->height()));
    const QRectF blueRosterRect = blueRoster->mapRectToItem(
        page, QRectF(0, 0, blueRoster->width(), blueRoster->height()));
    QVERIFY(scenarioPanelRect.right() <= blueRosterRect.left());

    QVERIFY(QMetaObject::invokeMethod(
        page, "selectRobot", Q_ARG(QVariant, QVariant("red-3")),
        Q_ARG(QVariant, false)));
    auto *selectedPanel = qobject_cast<QQuickItem *>(
        page->findChild<QObject *>(
            QStringLiteral("commandSelectedRobotPanel")));
    QVERIFY(selectedPanel);
    QTRY_VERIFY(selectedPanel->isVisible());
    const QRectF selectedPanelRect = selectedPanel->mapRectToItem(
        page,
        QRectF(0, 0, selectedPanel->width(), selectedPanel->height()));
    QVERIFY(scenarioPanelRect.bottom() <= selectedPanelRect.top());
  }

  void testTopInfoBarUsesMockMatchSource() {
    RM::MockMatchSource source;
    QVERIFY(source.loadFromFile(
        QStringLiteral(":/mock/3v3/match_3v3_basic.json")));
    RM::MatchUiProjection projection(source.matchSession());

    QQmlEngine engine;
    QQmlComponent component(&engine, QUrl(QStringLiteral("qrc:/qml/TopInfoBar.qml")));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    const QVariantMap initialProperties{
        {QStringLiteral("tacticalSource"), QVariant::fromValue(&projection)},
        {QStringLiteral("uiFirstSource"), QVariant::fromValue(&projection)},
    };
    QScopedPointer<QObject> rootObject(
        component.createWithInitialProperties(initialProperties));
    QVERIFY2(rootObject, qPrintable(component.errorString()));
    auto *rootItem = qobject_cast<QQuickItem *>(rootObject.data());
    QVERIFY(rootItem);
    rootItem->setWidth(1920);
    rootItem->setHeight(170);

    QTRY_VERIFY(rootObject->property("currentSeason").toBool());
    QObject *hud = nullptr;
    QTRY_VERIFY((hud = rootObject->findChild<QObject *>(
                     QStringLiteral("pilotMatchHud"))) != nullptr);
    QObject *clock = nullptr;
    QObject *redScore = nullptr;
    QObject *blueScore = nullptr;
    QTRY_VERIFY((clock = hud->findChild<QObject *>(
                     QStringLiteral("dashboardMatchClock"))) != nullptr);
    QTRY_VERIFY((redScore = hud->findChild<QObject *>(
                     QStringLiteral("dashboardRedScore"))) != nullptr);
    QTRY_VERIFY((blueScore = hud->findChild<QObject *>(
                     QStringLiteral("dashboardBlueScore"))) != nullptr);
    auto *hudItem = qobject_cast<QQuickItem *>(hud);
    auto *redScoreItem = qobject_cast<QQuickItem *>(redScore);
    auto *blueScoreItem = qobject_cast<QQuickItem *>(blueScore);
    QVERIFY(hudItem);
    QVERIFY(redScoreItem);
    QVERIFY(blueScoreItem);
    const QRectF redRect = redScoreItem->mapRectToItem(
        hudItem, QRectF(0, 0, redScoreItem->width(), redScoreItem->height()));
    const QRectF blueRect = blueScoreItem->mapRectToItem(
        hudItem, QRectF(0, 0, blueScoreItem->width(), blueScoreItem->height()));
    QVERIFY(qAbs(redRect.center().y() - blueRect.center().y()) <= 1.0);

    QVERIFY(source.start());
    QVERIFY(source.advanceByMs(12000));
    QVERIFY(source.pause());
    QTRY_COMPARE(clock->property("text").toString(), QStringLiteral("05:00"));
    QCOMPARE(redScore->property("text").toString(), QStringLiteral("0"));

    QVERIFY(source.resume());
    QVERIFY(source.advanceByMs(8000));
    QVERIFY(source.pause());
    QTRY_COMPARE(redScore->property("text").toString(), QStringLiteral("1"));

  }

  void testOperatorVehiclePanelUsesMockSource() {
    RM::MockMatchSource source;
    QVERIFY(source.loadFromFile(
        QStringLiteral(":/mock/3v3/match_3v3_basic.json")));
    RM::MatchUiProjection projection(source.matchSession());

    QQmlEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("mockUiSource"),
                                             &projection);
    QQmlComponent component(
        &engine, QUrl(QStringLiteral("qrc:/qml/LeftBottomPanel.qml")));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    QScopedPointer<QObject> panelObject(component.create());
    QVERIFY2(panelObject, qPrintable(component.errorString()));
    auto *panel = qobject_cast<QQuickItem *>(panelObject.data());
    QVERIFY(panel);
    panel->setWidth(520);
    panel->setHeight(200);

    QTRY_VERIFY(panel->property("uiFirstMode").toBool());
    QVERIFY(panel->property("commandStyle").toBool());
    QCOMPARE(panel->property("currentHP").toInt(), 400);
    QCOMPARE(panel->property("bufferEnergy").toInt(), 40);
    QCOMPARE(panel->property("superCapEnergy").toReal(), 74.0);
    QCOMPARE(panel->property("chassisStatus").toString(),
             QStringLiteral("普通 · 55 W"));
    QTRY_VERIFY(panel->findChild<QObject *>(
                    QStringLiteral("pilotVehicleCard")) != nullptr);

    QVERIFY(source.start());
    QVERIFY(source.advanceByMs(40000));
    QTRY_COMPARE(panel->property("superCapEnergy").toReal(), 25.0);
    QCOMPARE(panel->property("bufferEnergy").toInt(), 18);
    QCOMPARE(panel->property("chassisStatus").toString(),
             QStringLiteral("小陀螺 · 97 W"));
  }

  void testCentralAimingHudUsesMockHeat() {
    RM::MockMatchSource source;
    QVERIFY(source.loadFromFile(
        QStringLiteral(":/mock/3v3/match_3v3_basic.json")));
    RM::MatchUiProjection projection(source.matchSession());

    QQmlEngine engine;
    QQmlComponent component(
        &engine, QUrl(QStringLiteral("qrc:/qml/CentralAimingHUD.qml")));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    const QVariantMap initialProperties{
        {QStringLiteral("matchUiSource"), QVariant::fromValue(&projection)},
    };
    QScopedPointer<QObject> hud(
        component.createWithInitialProperties(initialProperties));
    QVERIFY2(hud, qPrintable(component.errorString()));
    QTRY_VERIFY(hud->property("matchUiActive").toBool());
    QTRY_COMPARE(hud->property("currentHeat").toReal(), 10.0);
    QCOMPARE(hud->property("heatLimit").toReal(), 200.0);

    QVERIFY(source.start());
    QVERIFY(source.setTriggerHeld(true));
    QVERIFY(source.advanceByMs(2000));
    QTRY_VERIFY(hud->property("isOverheated").toBool());
    QVERIFY(source.setTriggerHeld(false));
  }
};

QTEST_MAIN(TestTacticalCommandPage)
#include "test_tactical_command_page.moc"
