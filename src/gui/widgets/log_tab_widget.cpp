/*  Copyright (C) 2014-2020 FastoGT. All right reserved.

    This file is part of FastoNoSQL.

    FastoNoSQL is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    FastoNoSQL is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with FastoNoSQL. If not, see <http://www.gnu.org/licenses/>.
*/

#include "gui/widgets/log_tab_widget.h"

#include <QEvent>
#include <QTabBar>

#include "gui/gui_factory.h"

#include "gui/widgets/commands_widget.h"
#include "gui/widgets/log_widget.h"

#include "translations/global.h"

namespace fastonosql {
namespace gui {

LogTabWidget::LogTabWidget(QWidget* parent) : QTabWidget(parent) {
  QTabBar* tab = new QTabBar;
  setTabBar(tab);
  setTabsClosable(false);
  setElideMode(Qt::ElideRight);
  setMovable(true);

  // logs
  log_ = createWidget<LogWidget>();
  addTab(log_, GuiFactory::GetInstance().loggingIcon(), QString());

  // commands
  commands_ = createWidget<CommandsWidget>();
  addTab(commands_, GuiFactory::GetInstance().commandIcon(), QString());

  retranslateUi();
}

QSize LogTabWidget::sizeHint() const {
  return QSize(size_hint_width, size_hint_height);
}

void LogTabWidget::addLogMessage(const QString& message, common::logging::LOG_LEVEL level) {
  log_->addLogMessage(message, level);
}

void LogTabWidget::addCommand(core::FastoObjectCommandIPtr command) {
  commands_->addCommand(command);
}

void LogTabWidget::changeEvent(QEvent* e) {
  if (e->type() == QEvent::LanguageChange) {
    retranslateUi();
  }

  QTabWidget::changeEvent(e);
}

void LogTabWidget::retranslateUi() {
  setTabText(0, translations::trLogs);
  setTabText(1, translations::trCommands);
}

}  // namespace gui
}  // namespace fastonosql
