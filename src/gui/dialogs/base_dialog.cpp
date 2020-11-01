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

#include "gui/dialogs/base_dialog.h"

#include <QEvent>

namespace fastonosql {
namespace gui {

BaseDialog::BaseDialog(const QString& title, QWidget* parent) : base_class(parent) {
  setWindowTitle(title);
  setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);  // Remove help button (?)
}

BaseDialog::~BaseDialog() {}

void BaseDialog::changeEvent(QEvent* e) {
  if (e->type() == QEvent::LanguageChange) {
    retranslateUi();
  } else if (e->type() == QEvent::FontChange) {
    updateFont();
  }

  base_class::changeEvent(e);
}

void BaseDialog::retranslateUi() {}

void BaseDialog::updateFont() {}

}  // namespace gui
}  // namespace fastonosql
