/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2012 Synergy App Ltd
 * SPDX-FileCopyrightText: (C) 2008 Volker Lanz <vl@fidra.de>
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QDialog>
#include <QList>
#include <QRect>

class QWidget;
class QString;
class QTableWidget;
class QToolButton;

class Screen;
class ScreenList;

namespace Ui {
class ScreenSettingsDialog;
}

class ScreenSettingsDialog : public QDialog
{
  Q_OBJECT

public:
  explicit ScreenSettingsDialog(QWidget *parent, Screen *screen = nullptr, const ScreenList *screens = nullptr);
  ~ScreenSettingsDialog() override;

public Q_SLOTS:
  void accept() override;

private Q_SLOTS:
  void addAlias();
  void removeAlias() const;
  void checkNewAliasName(const QString &text);
  void aliasSelected();
  void addDisplay();
  void removeDisplay();
  void detectLocalDisplays();

private:
  void populateDisplays(const QList<QRect> &displays);
  QList<QRect> displayGeometries(bool *valid = nullptr) const;

  std::unique_ptr<Ui::ScreenSettingsDialog> ui;
  Screen *m_screen;
  QTableWidget *m_displays = nullptr;
  QToolButton *m_removeDisplay = nullptr;
};
