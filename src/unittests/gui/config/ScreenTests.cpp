/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025 Chris Rizzitello <sithlord48@gmail.com>
 * SPDX-FileCopyrightText: (C) 2024 Synergy App Ltd
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "ScreenTests.h"

#include "common/Settings.h"
#include "gui/ScreenSetupModel.h"
#include "gui/config/Screen.h"
#include "gui/widgets/ScreenSetupView.h"

#include <QPixmap>

void ScreenTests::initTestCase()
{
  QDir dir;
  QVERIFY(dir.mkpath(m_settingsPath));

  QFile oldSettings(m_settingsFile);
  if (oldSettings.exists())
    oldSettings.remove();

  Settings::setSettingsFile(m_settingsFile);
  Settings::setStateFile(m_stateFile);
}

void ScreenTests::basicFunctionality()
{
  Screen screen;
  QVERIFY(screen.isNull());

  screen.setName("stub");
  QVERIFY(!screen.isNull());

  screen.saveSettings(Settings::proxy());
  screen.loadSettings(Settings::proxy());
  QCOMPARE("stub", screen.name());
}

void ScreenTests::displayGeometryPersistence()
{
  Screen saved("multi-display");
  saved.setLayoutPosition(QPoint(-2560, 240));
  saved.setDisplayGeometries({QRect(0, 0, 2560, 1440), QRect(2560, -360, 1920, 1080)});
  saved.saveSettings(Settings::proxy());

  Screen loaded;
  loaded.loadSettings(Settings::proxy());

  QCOMPARE(loaded.name(), saved.name());
  QCOMPARE(loaded.layoutPosition(), saved.layoutPosition());
  QCOMPARE(loaded.displayGeometries(), saved.displayGeometries());
  QCOMPARE(loaded.displayBounds(), QRect(0, -360, 4480, 1800));
}

void ScreenTests::displayLayoutPreviewRenders()
{
  ScreenList screens;
  screens.resize(9);
  ScreenSetupModel model(screens, 3, 3);

  Screen primary("primary");
  primary.markAsServer();
  primary.setDisplayGeometries({QRect(0, 0, 2560, 1440), QRect(2560, -360, 1920, 1080)});
  QVERIFY(model.addScreenAt(primary, QPoint(-1920, 180)) >= 0);

  Screen laptop("laptop");
  laptop.setDisplayGeometries({QRect(0, 0, 1920, 1200)});
  QVERIFY(model.addScreenAt(laptop, QPoint(2560, 0)) >= 0);

  ScreenSetupView view;
  view.resize(960, 640);
  view.setModel(&model);
  view.show();
  QVERIFY(QTest::qWaitForWindowExposed(&view));

  QCOMPARE(view.items().size(), 2);
  const QPixmap preview = view.grab();
  QVERIFY(!preview.isNull());
  QCOMPARE(preview.deviceIndependentSize().toSize(), view.size());

  view.close();
}

QTEST_MAIN(ScreenTests)
