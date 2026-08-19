/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "ScreenSetupView.h"

#include "ScreenSetupModel.h"
#include "dialogs/ScreenSettingsDialog.h"
#include "widgets/TrashScreenWidget.h"

#include <QApplication>
#include <QDataStream>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QGuiApplication>
#include <QGraphicsScene>
#include <QGraphicsSceneContextMenuEvent>
#include <QGraphicsItem>
#include <QGraphicsSceneMouseEvent>
#include <QKeyEvent>
#include <QMenu>
#include <QMimeData>
#include <QPainter>
#include <QPaintEvent>
#include <QResizeEvent>
#include <QScreen>
#include <QTimer>
#include <QWidget>

#include <algorithm>
#include <array>
#include <cmath>

namespace {

constexpr qreal kSceneMargin = 500.0;
constexpr qreal kSnapDistance = 100.0;

QColor computerColor(int screenIndex)
{
  static const std::array<QColor, 8> colors = {
      QColor("#0F766E"), QColor("#C2410C"), QColor("#2563EB"), QColor("#BE123C"),
      QColor("#4D7C0F"), QColor("#7C3AED"), QColor("#A16207"), QColor("#0369A1"),
  };
  return colors.at(static_cast<size_t>(screenIndex) % colors.size());
}

QString displayLabel(int displayIndex)
{
  QString label;
  do {
    label.prepend(QChar('A' + displayIndex % 26));
    displayIndex = displayIndex / 26 - 1;
  } while (displayIndex >= 0);
  return label;
}

class DisplayIdentifierOverlay : public QWidget
{
public:
  DisplayIdentifierOverlay(QScreen *screen, const QString &label)
      : QWidget(nullptr, Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint),
        m_label(label)
  {
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setGeometry(screen->geometry());
  }

protected:
  void paintEvent(QPaintEvent *) override
  {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.fillRect(rect(), QColor(0, 0, 0, 48));

    const QSize boxSize(std::min(width(), height()) / 3, std::min(width(), height()) / 3);
    const QRect box(QPoint((width() - boxSize.width()) / 2, (height() - boxSize.height()) / 2), boxSize);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(0, 0, 0, 190));
    painter.drawRoundedRect(box, 16, 16);

    QFont font = painter.font();
    font.setBold(true);
    font.setPixelSize(std::max(96, box.height() / 2));
    painter.setFont(font);
    painter.setPen(Qt::white);
    painter.drawText(box, Qt::AlignCenter, m_label);
  }

private:
  QString m_label;
};

class ComputerItem : public QGraphicsItem
{
public:
  ComputerItem(ScreenSetupView *view, int screenIndex, int firstDisplayIndex, Screen *screen)
      : m_view(view),
        m_screenIndex(screenIndex),
        m_firstDisplayIndex(firstDisplayIndex),
        m_screen(screen)
  {
    setFlags(ItemIsMovable | ItemIsSelectable | ItemSendsGeometryChanges);
    setPos(screen->layoutPosition());
    setToolTip(ScreenSetupView::tr("%1 - %2 display(s)").arg(screen->name()).arg(screen->displayGeometries().size()));
  }

  int screenIndex() const
  {
    return m_screenIndex;
  }

  QRectF boundingRect() const override
  {
    return QRectF(m_screen->displayBounds()).adjusted(-24, -24, 24, 120);
  }

  void paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *) override
  {
    const auto palette = m_view->palette();
    const QColor accent = computerColor(m_screenIndex);
    QColor fill = accent;
    fill.setAlpha(42);
    const QColor border = isSelected() ? palette.highlight().color() : accent;
    const QColor text = isSelected() ? palette.highlightedText().color() : accent.darker(160);

    painter->setRenderHint(QPainter::Antialiasing);
    int displayNumber = 0;
    for (const auto &display : m_screen->displayGeometries()) {
      const QRectF rect(display);
      painter->setPen(QPen(border, isSelected() ? 18 : 10));
      painter->setBrush(fill);
      painter->drawRoundedRect(rect.adjusted(5, 5, -5, -5), 28, 28);

      const qreal labelSize = std::clamp(rect.height() * 0.22, 120.0, 260.0);
      QFont labelFont = painter->font();
      labelFont.setPixelSize(static_cast<int>(labelSize));
      labelFont.setBold(true);
      painter->setFont(labelFont);
      painter->setPen(text);
      painter->drawText(rect, Qt::AlignCenter, displayLabel(m_firstDisplayIndex + displayNumber));

      QFont detailsFont = painter->font();
      detailsFont.setPixelSize(static_cast<int>(std::clamp(rect.height() * 0.075, 56.0, 110.0)));
      detailsFont.setBold(false);
      painter->setFont(detailsFont);
      painter->drawText(
          rect.adjusted(35, 0, -35, -35), Qt::AlignHCenter | Qt::AlignBottom,
          QStringLiteral("%1 x %2").arg(display.width()).arg(display.height())
      );
      ++displayNumber;
    }

    const QRect bounds = m_screen->displayBounds();
    QFont nameFont = painter->font();
    nameFont.setPixelSize(90);
    nameFont.setBold(m_screen->isServer());
    painter->setFont(nameFont);
    painter->setPen(palette.text().color());
    painter->drawText(
        QRectF(bounds.left(), bounds.bottom() + 30, bounds.width(), 90), Qt::AlignCenter, m_screen->name()
    );
  }

protected:
  QVariant itemChange(GraphicsItemChange change, const QVariant &value) override
  {
    if (change == ItemPositionChange && scene())
      return m_view->snapPosition(m_screenIndex, value.toPointF());
    if (change == ItemPositionHasChanged && scene())
      m_view->setScreenPosition(m_screenIndex, pos());
    return QGraphicsItem::itemChange(change, value);
  }

  void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override
  {
    QGraphicsItem::mouseReleaseEvent(event);
    QWidget *dropTarget = QApplication::widgetAt(event->screenPos());
    while (dropTarget && !qobject_cast<TrashScreenWidget *>(dropTarget))
      dropTarget = dropTarget->parentWidget();
    if (dropTarget && !m_screen->isServer()) {
      m_view->removeScreen(m_screenIndex);
      return;
    }
    m_view->finishScreenMove();
  }

  void mousePressEvent(QGraphicsSceneMouseEvent *event) override
  {
    int displayIndex = 0;
    for (const auto &display : m_screen->displayGeometries()) {
      if (QRectF(display).contains(event->pos())) {
        m_view->showDisplayIdentifier(m_screenIndex, displayIndex);
        break;
      }
      ++displayIndex;
    }
    QGraphicsItem::mousePressEvent(event);
  }

  void mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event) override
  {
    m_view->editScreen(m_screenIndex);
    event->accept();
  }

  void contextMenuEvent(QGraphicsSceneContextMenuEvent *event) override
  {
    QMenu menu;
    QAction *editAction = menu.addAction(ScreenSetupView::tr("Edit computer"));
    QAction *removeAction = nullptr;
    if (!m_screen->isServer())
      removeAction = menu.addAction(ScreenSetupView::tr("Remove computer"));

    QAction *selected = menu.exec(event->screenPos());
    if (selected == editAction)
      m_view->editScreen(m_screenIndex);
    else if (selected == removeAction)
      m_view->removeScreen(m_screenIndex);
  }

private:
  ScreenSetupView *m_view;
  int m_screenIndex;
  int m_firstDisplayIndex;
  Screen *m_screen;
};

qreal snapCoordinate(qreal value, const QList<qreal> &candidates)
{
  qreal result = value;
  qreal bestDistance = kSnapDistance;
  for (const qreal candidate : candidates) {
    const qreal distance = std::abs(value - candidate);
    if (distance < bestDistance) {
      bestDistance = distance;
      result = candidate;
    }
  }
  return result;
}

} // namespace

ScreenSetupView::ScreenSetupView(QWidget *parent)
    : QGraphicsView(parent),
      m_scene(new QGraphicsScene(this))
{
  setScene(m_scene);
  setAcceptDrops(true);
  setDragMode(QGraphicsView::RubberBandDrag);
  setRenderHint(QPainter::Antialiasing);
  setFrameShape(QFrame::StyledPanel);
  setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  setTransformationAnchor(QGraphicsView::AnchorViewCenter);
  setResizeAnchor(QGraphicsView::AnchorViewCenter);
}

void ScreenSetupView::setModel(QAbstractItemModel *model)
{
  if (m_model)
    disconnect(m_model, nullptr, this, nullptr);

  m_model = qobject_cast<ScreenSetupModel *>(model);
  if (m_model) {
    connect(m_model, &ScreenSetupModel::screensChanged, this, &ScreenSetupView::rebuildScene);
    rebuildScene();
  }
}

ScreenSetupModel *ScreenSetupView::model() const
{
  return m_model;
}

void ScreenSetupView::reset()
{
  rebuildScene();
}

void ScreenSetupView::rebuildScene()
{
  if (!m_model)
    return;

  m_rebuilding = true;
  m_scene->clear();
  int firstDisplayIndex = 0;
  for (int i = 0; i < m_model->m_Screens.size(); ++i) {
    auto &screen = m_model->m_Screens[i];
    if (!screen.isNull()) {
      m_scene->addItem(new ComputerItem(this, i, firstDisplayIndex, &screen));
      firstDisplayIndex += screen.displayGeometries().size();
    }
  }
  m_rebuilding = false;
  fitLayout();
}

void ScreenSetupView::fitLayout()
{
  if (m_scene->items().isEmpty()) {
    m_scene->setSceneRect(QRectF(-2000, -1200, 4000, 2400));
    resetTransform();
    return;
  }

  const QRectF bounds = m_scene->itemsBoundingRect().adjusted(-kSceneMargin, -kSceneMargin, kSceneMargin, kSceneMargin);
  m_scene->setSceneRect(bounds);
  fitInView(bounds, Qt::KeepAspectRatio);
}

QPointF ScreenSetupView::snapPosition(int screenIndex, const QPointF &position) const
{
  if (!m_model || screenIndex < 0 || screenIndex >= m_model->m_Screens.size())
    return position;

  const Screen &moving = m_model->m_Screens[screenIndex];
  QList<qreal> xCandidates;
  QList<qreal> yCandidates;

  for (int i = 0; i < m_model->m_Screens.size(); ++i) {
    if (i == screenIndex || m_model->m_Screens[i].isNull())
      continue;
    for (const auto &other : m_model->m_Screens[i].workspaceDisplayGeometries()) {
      for (const auto &display : moving.displayGeometries()) {
        xCandidates << other.left() - display.right() - 1 << other.right() + 1 - display.left();
        yCandidates << other.top() - display.bottom() - 1 << other.bottom() + 1 - display.top();
      }
    }
  }

  return QPointF(snapCoordinate(position.x(), xCandidates), snapCoordinate(position.y(), yCandidates));
}

void ScreenSetupView::setScreenPosition(int screenIndex, const QPointF &position)
{
  if (m_rebuilding || !m_model || screenIndex < 0 || screenIndex >= m_model->m_Screens.size())
    return;
  m_model->m_Screens[screenIndex].setLayoutPosition(
      QPoint(std::lround(position.x()), std::lround(position.y()))
  );
}

void ScreenSetupView::finishScreenMove()
{
  if (m_model) {
    QTimer::singleShot(0, m_model, [model = m_model] { Q_EMIT model->screensChanged(); });
  }
}

void ScreenSetupView::editScreen(int screenIndex)
{
  if (!m_model || screenIndex < 0 || screenIndex >= m_model->m_Screens.size())
    return;
  ScreenSettingsDialog dialog(this, &m_model->m_Screens[screenIndex], &m_model->m_Screens);
  if (dialog.exec() == QDialog::Accepted) {
    QTimer::singleShot(0, m_model, [model = m_model] { Q_EMIT model->screensChanged(); });
  }
}

void ScreenSetupView::removeScreen(int screenIndex)
{
  if (m_model) {
    QTimer::singleShot(0, m_model, [model = m_model, screenIndex] { model->removeScreen(screenIndex); });
  }
}

void ScreenSetupView::showDisplayIdentifier(int screenIndex, int displayIndex)
{
  if (!m_model || screenIndex < 0 || screenIndex >= m_model->m_Screens.size())
    return;
  const Screen &computer = m_model->m_Screens[screenIndex];
  if (!computer.isServer() || displayIndex < 0 || displayIndex >= computer.displayGeometries().size())
    return;

  const QRect displayGeometry = computer.displayGeometries().at(displayIndex);
  const auto localScreens = QGuiApplication::screens();
  auto localScreen = std::ranges::find_if(localScreens, [&displayGeometry](const QScreen *screen) {
    return screen->geometry() == displayGeometry;
  });
  if (localScreen == localScreens.end() && displayIndex < localScreens.size())
    localScreen = localScreens.begin() + displayIndex;
  if (localScreen == localScreens.end())
    return;

  int globalDisplayIndex = displayIndex;
  for (int i = 0; i < screenIndex; ++i) {
    if (!m_model->m_Screens[i].isNull())
      globalDisplayIndex += m_model->m_Screens[i].displayGeometries().size();
  }
  auto *overlay = new DisplayIdentifierOverlay(*localScreen, displayLabel(globalDisplayIndex));
  overlay->show();
  QTimer::singleShot(1200, overlay, &QWidget::deleteLater);
}

void ScreenSetupView::resizeEvent(QResizeEvent *event)
{
  QGraphicsView::resizeEvent(event);
  fitLayout();
}

void ScreenSetupView::dragEnterEvent(QDragEnterEvent *event)
{
  if (event->mimeData()->hasFormat(ScreenSetupModel::mimeType()))
    event->acceptProposedAction();
  else
    event->ignore();
}

void ScreenSetupView::dragMoveEvent(QDragMoveEvent *event)
{
  if (event->mimeData()->hasFormat(ScreenSetupModel::mimeType()))
    event->acceptProposedAction();
  else
    event->ignore();
}

void ScreenSetupView::dropEvent(QDropEvent *event)
{
  if (!m_model || !event->mimeData()->hasFormat(ScreenSetupModel::mimeType())) {
    event->ignore();
    return;
  }

  QDataStream stream(event->mimeData()->data(ScreenSetupModel::mimeType()));
  int sourceColumn = -1;
  int sourceRow = -1;
  Screen screen;
  stream >> sourceColumn >> sourceRow >> screen;
  Q_UNUSED(sourceColumn)
  Q_UNUSED(sourceRow)
  screen.ensureDefaultDisplay();

  const QPointF scenePosition = mapToScene(event->position().toPoint());
  const QRect bounds = screen.displayBounds();
  const QPoint position(
      std::lround(scenePosition.x() - bounds.center().x()), std::lround(scenePosition.y() - bounds.center().y())
  );
  if (m_model->addScreenAt(screen, position) >= 0)
    event->acceptProposedAction();
  else
    event->ignore();
}

void ScreenSetupView::keyPressEvent(QKeyEvent *event)
{
  if (event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace) {
    const auto selected = m_scene->selectedItems();
    if (selected.size() == 1) {
      if (const auto *computer = dynamic_cast<ComputerItem *>(selected.constFirst())) {
        removeScreen(computer->screenIndex());
        event->accept();
        return;
      }
    }
  }
  QGraphicsView::keyPressEvent(event);
}

void ScreenSetupView::drawBackground(QPainter *painter, const QRectF &rect)
{
  painter->fillRect(rect, palette().base());
  QPen gridPen(palette().midlight().color());
  gridPen.setCosmetic(true);
  painter->setPen(gridPen);
  constexpr int grid = 240;
  const int left = static_cast<int>(std::floor(rect.left() / grid)) * grid;
  const int top = static_cast<int>(std::floor(rect.top() / grid)) * grid;
  for (int x = left; x < rect.right(); x += grid)
    painter->drawLine(x, rect.top(), x, rect.bottom());
  for (int y = top; y < rect.bottom(); y += grid)
    painter->drawLine(rect.left(), y, rect.right(), y);
}
