/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025 Chris Rizzitello <sithlord48@gmail.com>
 * SPDX-FileCopyrightText: (C) 2012 Synergy App Ltd
 * SPDX-FileCopyrightText: (C) 2008 Volker Lanz <vl@fidra.de>
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "Screen.h"
#include "config/ScreenConfig.h"
#include <common/Settings.h>

using enum ScreenConfig::Modifier;
using enum ScreenConfig::SwitchCorner;
using enum ScreenConfig::Fix;

Screen::Screen(const QString &name)
{
  setName(name);
  if (!name.isEmpty())
    ensureDefaultDisplay();
}

void Screen::loadSettings(QSettingsProxy &settings)
{
  const auto name = settings.value("name").toString();
  setName(name);

  if (name.isEmpty())
    return;

  m_LayoutPosition = settings.value("layoutPosition").toPoint();
  const int displayCount = settings.beginReadArray("displays");
  QList<QRect> displays;
  displays.reserve(displayCount);
  for (int i = 0; i < displayCount; ++i) {
    settings.setArrayIndex(i);
    const QRect geometry = settings.value("geometry").toRect();
    if (geometry.width() > 0 && geometry.height() > 0)
      displays.append(geometry);
  }
  settings.endArray();
  if (!displays.isEmpty()) {
    m_DisplayGeometries = displays;
  } else {
    ensureDefaultDisplay();
  }

  setSwitchCornerSize(settings.value("switchCornerSize").toInt());

  readSettings(settings, modifiers(), "modifier", static_cast<int>(DefaultMod), static_cast<int>(NumModifiers));
  readSettings(settings, switchCorners(), "switchCorner", false, static_cast<int>(NumSwitchCorners));
  readSettings(settings, fixes(), "fix", 0, static_cast<int>(NumFixes));

  m_Aliases = Settings::value(Settings::Screen::Aliases.arg(name)).toStringList();
}

void Screen::saveSettings(QSettingsProxy &settings) const
{

  const auto screenName = name();
  settings.setValue("name", screenName);

  if (screenName.isEmpty())
    return;

  Settings::setValue(Settings::Screen::Aliases.arg(screenName), m_Aliases);

  settings.setValue("layoutPosition", m_LayoutPosition);
  settings.beginWriteArray("displays");
  for (int i = 0; i < m_DisplayGeometries.size(); ++i) {
    settings.setArrayIndex(i);
    settings.setValue("geometry", m_DisplayGeometries.at(i));
  }
  settings.endArray();

  settings.setValue("switchCornerSize", switchCornerSize());

  writeSettings(settings, modifiers(), "modifier");
  writeSettings(settings, switchCorners(), "switchCorner");
  writeSettings(settings, fixes(), "fix");
}

QString Screen::screensSection() const
{
  const auto lineTemplate = QStringLiteral("\t\t%1 = %2\n");

  QString out = QStringLiteral("\t%1:\n").arg(name());
  for (int i = 0; i < modifiers().size(); i++) {
    if (modifier(i) != i)
      out.append(lineTemplate.arg(modifierName(i), modifierName(modifier(i))));
  }

  for (int i = 0; i < fixes().size(); i++)
    out.append(lineTemplate.arg(fixName(i), fixes().at(i) ? QStringLiteral("true") : QStringLiteral("false")));

  auto corners = QStringLiteral("none");
  for (int i = 0; i < switchCorners().size(); i++) {
    if (switchCorners()[i])
      corners.append(QStringLiteral(" +%1 ").arg(switchCornerName(i)));
  }
  out.append(lineTemplate.arg(QStringLiteral("switchCorners"), corners));

  out.append(lineTemplate.arg(QStringLiteral("switchCornerSize"), QString::number(switchCornerSize())));

  return out;
}

bool Screen::operator==(const Screen &screen) const
{
  return m_Name == screen.m_Name && m_Aliases == screen.m_Aliases && m_Modifiers == screen.m_Modifiers &&
         m_SwitchCorners == screen.m_SwitchCorners && m_SwitchCornerSize == screen.m_SwitchCornerSize &&
         m_Fixes == screen.m_Fixes && m_Swapped == screen.m_Swapped && m_isServer == screen.m_isServer &&
         m_LayoutPosition == screen.m_LayoutPosition && m_DisplayGeometries == screen.m_DisplayGeometries;
}

QRect Screen::displayBounds() const
{
  QRect bounds;
  for (const auto &geometry : m_DisplayGeometries)
    bounds = bounds.isNull() ? geometry : bounds.united(geometry);
  return bounds;
}

QList<QRect> Screen::workspaceDisplayGeometries() const
{
  QList<QRect> result;
  result.reserve(m_DisplayGeometries.size());
  for (const auto &geometry : m_DisplayGeometries)
    result.append(geometry.translated(m_LayoutPosition));
  return result;
}

void Screen::setDisplayGeometries(const QList<QRect> &geometries)
{
  m_DisplayGeometries.clear();
  for (const auto &geometry : geometries) {
    if (geometry.width() > 0 && geometry.height() > 0)
      m_DisplayGeometries.append(geometry);
  }
  ensureDefaultDisplay();
}

void Screen::ensureDefaultDisplay()
{
  if (m_DisplayGeometries.isEmpty())
    m_DisplayGeometries.append(QRect(0, 0, 1920, 1080));
}
