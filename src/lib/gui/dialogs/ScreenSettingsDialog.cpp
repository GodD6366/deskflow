/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2012 Synergy App Ltd
 * SPDX-FileCopyrightText: (C) 2008 Volker Lanz <vl@fidra.de>
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "ScreenSettingsDialog.h"
#include "ui_ScreenSettingsDialog.h"

#include "gui/config/Screen.h"
#include "validators/AliasValidator.h"
#include "validators/ScreenNameValidator.h"
#include "validators/ValidationError.h"

#include <QGroupBox>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QMessageBox>
#include <QScreen>
#include <QTableWidget>
#include <QToolButton>
#include <QVBoxLayout>

using enum ScreenConfig::Modifier;
using enum ScreenConfig::SwitchCorner;
using enum ScreenConfig::Fix;

ScreenSettingsDialog::~ScreenSettingsDialog() = default;

ScreenSettingsDialog::ScreenSettingsDialog(QWidget *parent, Screen *screen, const ScreenList *screens)
    : QDialog(parent, Qt::WindowTitleHint | Qt::WindowSystemMenuHint),
      ui{std::make_unique<Ui::ScreenSettingsDialog>()},
      m_screen(screen)
{

  ui->setupUi(this);
  ui->buttonBox->button(QDialogButtonBox::Cancel)->setFocus();

  ui->lineNameEdit->setText(m_screen->name());

  auto *displayGroup = new QGroupBox(tr("Displays"), this);
  auto *displayLayout = new QVBoxLayout(displayGroup);
  m_displays = new QTableWidget(displayGroup);
  m_displays->setColumnCount(5);
  m_displays->setHorizontalHeaderLabels({tr("Display"), tr("X"), tr("Y"), tr("Width"), tr("Height")});
  m_displays->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
  m_displays->verticalHeader()->hide();
  m_displays->setSelectionBehavior(QAbstractItemView::SelectRows);
  m_displays->setSelectionMode(QAbstractItemView::SingleSelection);
  displayLayout->addWidget(m_displays);

  auto *displayButtons = new QHBoxLayout;
  auto *addDisplayButton = new QToolButton(displayGroup);
  addDisplayButton->setIcon(QIcon::fromTheme(QIcon::ThemeIcon::ListAdd));
  addDisplayButton->setToolTip(tr("Add display"));
  m_removeDisplay = new QToolButton(displayGroup);
  m_removeDisplay->setIcon(QIcon::fromTheme(QIcon::ThemeIcon::ListRemove));
  m_removeDisplay->setToolTip(tr("Remove display"));
  auto *detectDisplaysButton = new QToolButton(displayGroup);
  detectDisplaysButton->setIcon(QIcon::fromTheme(QIcon::ThemeIcon::ViewRefresh));
  detectDisplaysButton->setToolTip(tr("Detect local displays"));
  detectDisplaysButton->setVisible(m_screen->isServer());
  displayButtons->addWidget(addDisplayButton);
  displayButtons->addWidget(m_removeDisplay);
  displayButtons->addStretch();
  displayButtons->addWidget(detectDisplaysButton);
  displayLayout->addLayout(displayButtons);
  static_cast<QVBoxLayout *>(layout())->insertWidget(1, displayGroup);
  populateDisplays(m_screen->displayGeometries());

  const auto valNameError = new validators::ValidationError(this, ui->lblNameError);
  const auto valName = new validators::ScreenNameValidator(ui->lineNameEdit, valNameError, screens);
  ui->lineNameEdit->setValidator(valName);

  const auto valAliasError = new validators::ValidationError(this, ui->lblAliasError);
  const auto valAlias = new validators::AliasValidator(ui->lineAddAlias, valAliasError);
  ui->lineAddAlias->setValidator(valAlias);

  for (int i = 0; i < m_screen->aliases().count(); i++)
    new QListWidgetItem(m_screen->aliases()[i], ui->listAliases);

  ui->comboShift->setCurrentIndex(m_screen->modifier(static_cast<int>(Shift)));
  ui->comboCtrl->setCurrentIndex(m_screen->modifier(static_cast<int>(Ctrl)));
  ui->comboAlt->setCurrentIndex(m_screen->modifier(static_cast<int>(Alt)));
  ui->comboMeta->setCurrentIndex(m_screen->modifier(static_cast<int>(Meta)));
  ui->comboSuper->setCurrentIndex(m_screen->modifier(static_cast<int>(Super)));
  ui->comboAltGr->setCurrentIndex(m_screen->modifier(static_cast<int>(AltGr)));

  ui->chkDeadTopLeft->setChecked(m_screen->switchCorner(static_cast<int>(TopLeft)));
  ui->chkDeadTopRight->setChecked(m_screen->switchCorner(static_cast<int>(TopRight)));
  ui->chkDeadBottomLeft->setChecked(m_screen->switchCorner(static_cast<int>(BottomLeft)));
  ui->chkDeadBottomRight->setChecked(m_screen->switchCorner(static_cast<int>(BottomRight)));
  ui->sbSwitchCornerSize->setValue(m_screen->switchCornerSize());

  ui->chkFixCapsLock->setChecked(m_screen->fix(CapsLock));
  ui->chkFixNumLock->setChecked(m_screen->fix(NumLock));
  ui->chkFixScrollLock->setChecked(m_screen->fix(ScrollLock));
  ui->chkFixXTest->setChecked(m_screen->fix(XTest));

  connect(ui->buttonBox, &QDialogButtonBox::accepted, this, &ScreenSettingsDialog::accept);
  connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &ScreenSettingsDialog::reject);
  connect(ui->btnAddAlias, &QPushButton::clicked, this, &ScreenSettingsDialog::addAlias);
  connect(ui->btnRemoveAlias, &QPushButton::clicked, this, &ScreenSettingsDialog::removeAlias);
  connect(ui->lineAddAlias, &QLineEdit::textChanged, this, &ScreenSettingsDialog::checkNewAliasName);
  connect(ui->listAliases, &QListWidget::itemSelectionChanged, this, &ScreenSettingsDialog::aliasSelected);
  connect(addDisplayButton, &QToolButton::clicked, this, &ScreenSettingsDialog::addDisplay);
  connect(m_removeDisplay, &QToolButton::clicked, this, &ScreenSettingsDialog::removeDisplay);
  connect(detectDisplaysButton, &QToolButton::clicked, this, &ScreenSettingsDialog::detectLocalDisplays);
  connect(m_displays, &QTableWidget::itemSelectionChanged, this, [this] {
    m_removeDisplay->setEnabled(m_displays->rowCount() > 1 && !m_displays->selectedItems().isEmpty());
  });
}

void ScreenSettingsDialog::accept()
{
  if (ui->lineNameEdit->text().isEmpty()) {
    QMessageBox::warning(
        this, tr("Screen name is empty"),
        tr("The screen name cannot be empty. "
           "Please either fill in a name or cancel the dialog.")
    );
    return;
  }
  if (!ui->lblNameError->text().isEmpty()) {
    return;
  }

  bool validDisplays = false;
  const auto displays = displayGeometries(&validDisplays);
  if (!validDisplays) {
    QMessageBox::warning(
        this, tr("Invalid display geometry"), tr("Display width and height must be positive whole numbers.")
    );
    return;
  }

  m_screen->setName(ui->lineNameEdit->text());
  m_screen->setDisplayGeometries(displays);

  m_screen->aliases().clear();

  for (int i = 0; i < ui->listAliases->count(); i++) {
    QString alias(ui->listAliases->item(i)->text());
    if (alias == ui->lineNameEdit->text()) {
      QMessageBox::warning(
          this, tr("Screen name matches alias"),
          tr("The screen name cannot be the same as an alias. "
             "Please either remove the alias or change the screen name.")
      );
      return;
    }
    if (!m_screen->aliases().contains(alias))
      m_screen->addAlias(alias);
  }

  m_screen->setModifier(Shift, ui->comboShift->currentIndex());
  m_screen->setModifier(Ctrl, ui->comboCtrl->currentIndex());
  m_screen->setModifier(Alt, ui->comboAlt->currentIndex());
  m_screen->setModifier(Meta, ui->comboMeta->currentIndex());
  m_screen->setModifier(Super, ui->comboSuper->currentIndex());
  m_screen->setModifier(AltGr, ui->comboAltGr->currentIndex());

  m_screen->setSwitchCorner(TopLeft, ui->chkDeadTopLeft->isChecked());
  m_screen->setSwitchCorner(TopRight, ui->chkDeadTopRight->isChecked());
  m_screen->setSwitchCorner(BottomLeft, ui->chkDeadBottomLeft->isChecked());
  m_screen->setSwitchCorner(BottomRight, ui->chkDeadBottomRight->isChecked());
  m_screen->setSwitchCornerSize(ui->sbSwitchCornerSize->value());

  m_screen->setFix(CapsLock, ui->chkFixCapsLock->isChecked());
  m_screen->setFix(NumLock, ui->chkFixNumLock->isChecked());
  m_screen->setFix(ScrollLock, ui->chkFixScrollLock->isChecked());
  m_screen->setFix(XTest, ui->chkFixXTest->isChecked());

  QDialog::accept();
}

void ScreenSettingsDialog::addAlias()
{
  if (!ui->lineAddAlias->text().isEmpty() &&
      ui->listAliases->findItems(ui->lineAddAlias->text(), Qt::MatchFixedString).isEmpty()) {
    new QListWidgetItem(ui->lineAddAlias->text(), ui->listAliases);
    ui->lineAddAlias->clear();
  }
}

void ScreenSettingsDialog::removeAlias() const
{
  QList<QListWidgetItem *> items = ui->listAliases->selectedItems();
  qDeleteAll(items);
}

void ScreenSettingsDialog::checkNewAliasName(const QString &text)
{
  ui->btnAddAlias->setEnabled(!text.isEmpty() && ui->lblAliasError->text().isEmpty());
}

void ScreenSettingsDialog::aliasSelected()
{
  ui->btnRemoveAlias->setEnabled(!ui->listAliases->selectedItems().isEmpty());
}

void ScreenSettingsDialog::populateDisplays(const QList<QRect> &displays)
{
  m_displays->setRowCount(0);
  int row = 0;
  for (const auto &display : displays) {
    m_displays->insertRow(row);
    auto *name = new QTableWidgetItem(QString(QChar('A' + row)));
    name->setFlags(name->flags() & ~Qt::ItemIsEditable);
    m_displays->setItem(row, 0, name);
    m_displays->setItem(row, 1, new QTableWidgetItem(QString::number(display.x())));
    m_displays->setItem(row, 2, new QTableWidgetItem(QString::number(display.y())));
    m_displays->setItem(row, 3, new QTableWidgetItem(QString::number(display.width())));
    m_displays->setItem(row, 4, new QTableWidgetItem(QString::number(display.height())));
    ++row;
  }
  m_removeDisplay->setEnabled(m_displays->rowCount() > 1);
}

QList<QRect> ScreenSettingsDialog::displayGeometries(bool *valid) const
{
  QList<QRect> displays;
  bool allValid = m_displays->rowCount() > 0;
  for (int row = 0; row < m_displays->rowCount(); ++row) {
    bool xValid = false;
    bool yValid = false;
    bool widthValid = false;
    bool heightValid = false;
    const int x = m_displays->item(row, 1)->text().toInt(&xValid);
    const int y = m_displays->item(row, 2)->text().toInt(&yValid);
    const int width = m_displays->item(row, 3)->text().toInt(&widthValid);
    const int height = m_displays->item(row, 4)->text().toInt(&heightValid);
    allValid = allValid && xValid && yValid && widthValid && heightValid && width > 0 && height > 0;
    if (allValid)
      displays.append(QRect(x, y, width, height));
  }
  if (valid)
    *valid = allValid && displays.size() == m_displays->rowCount();
  return displays;
}

void ScreenSettingsDialog::addDisplay()
{
  bool valid = false;
  auto displays = displayGeometries(&valid);
  if (!valid)
    return;
  QRect bounds;
  for (const auto &display : std::as_const(displays))
    bounds = bounds.isNull() ? display : bounds.united(display);
  displays.append(QRect(bounds.right() + 1, bounds.top(), 1920, 1080));
  populateDisplays(displays);
  m_displays->selectRow(m_displays->rowCount() - 1);
}

void ScreenSettingsDialog::removeDisplay()
{
  if (m_displays->rowCount() <= 1)
    return;
  const int row = m_displays->currentRow();
  if (row >= 0)
    m_displays->removeRow(row);
  for (int i = 0; i < m_displays->rowCount(); ++i)
    m_displays->item(i, 0)->setText(QString(QChar('A' + i)));
  m_removeDisplay->setEnabled(m_displays->rowCount() > 1);
}

void ScreenSettingsDialog::detectLocalDisplays()
{
  QList<QRect> displays;
  for (const auto *screen : QGuiApplication::screens())
    displays.append(screen->geometry());
  if (!displays.isEmpty())
    populateDisplays(displays);
}
