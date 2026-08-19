/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2012 Synergy App Ltd
 * SPDX-FileCopyrightText: (C) 2008 Volker Lanz <vl@fidra.de>
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QGraphicsView>

class QWidget;
class QMouseEvent;
class QResizeEvent;
class QDragEnterEvent;
class QDropEvent;
class QKeyEvent;
class QGraphicsScene;
class QAbstractItemModel;
class QPainter;
class ScreenSetupModel;

class ScreenSetupView : public QGraphicsView
{
  Q_OBJECT

public:
  explicit ScreenSetupView(QWidget *parent = nullptr);
  void setModel(QAbstractItemModel *model);
  ScreenSetupModel *model() const;
  void reset();
  QPointF snapPosition(int screenIndex, const QPointF &position) const;
  void setScreenPosition(int screenIndex, const QPointF &position);
  void finishScreenMove();
  void editScreen(int screenIndex);
  void removeScreen(int screenIndex);
  void showDisplayIdentifier(int screenIndex, int displayIndex);

private:
  void rebuildScene();
  void fitLayout();

protected:
  void resizeEvent(QResizeEvent *) override;
  void dragEnterEvent(QDragEnterEvent *event) override;
  void dragMoveEvent(QDragMoveEvent *event) override;
  void dropEvent(QDropEvent *event) override;
  void keyPressEvent(QKeyEvent *event) override;
  void drawBackground(QPainter *painter, const QRectF &rect) override;

private:
  ScreenSetupModel *m_model = nullptr;
  QGraphicsScene *m_scene = nullptr;
  bool m_rebuilding = false;
};
