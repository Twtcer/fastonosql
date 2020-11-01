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

#include "gui/widgets/list_type_view.h"

#include <QHeaderView>

#include <common/qt/utils_qt.h>

#include "gui/models/items/value_table_item.h"
#include "gui/models/list_table_model.h"
#include "gui/widgets/delegate/action_cell_delegate.h"

#include "translations/global.h"

namespace fastonosql {
namespace gui {

ListTypeView::ListTypeView(QWidget* parent) : QTableView(parent), model_(nullptr), mode_(kArray) {
  model_ = new ListTableModel(this);
  model_->setFirstColumnName(translations::trValue);
  setModel(model_);

  ActionDelegate* del = new ActionDelegate(this);
  VERIFY(connect(del, &ActionDelegate::addClicked, this, &ListTypeView::addRow));
  VERIFY(connect(del, &ActionDelegate::removeClicked, this, &ListTypeView::removeRow));
  QAbstractItemDelegate* default_del = itemDelegate();
  VERIFY(connect(default_del, &QAbstractItemDelegate::closeEditor, this, &ListTypeView::dataChangedSignal));

  setItemDelegateForColumn(ListTableModel::kAction, del);
  setContextMenuPolicy(Qt::ActionsContextMenu);
  setSelectionBehavior(QAbstractItemView::SelectRows);
  setSelectionMode(QAbstractItemView::SingleSelection);

  QHeaderView* header = horizontalHeader();
  header->setSectionResizeMode(QHeaderView::Stretch);

  // sync
  setCurrentMode(mode_);
}

common::ArrayValue* ListTypeView::arrayValue() const {
  return model_->arrayValue();
}

common::SetValue* ListTypeView::setValue() const {
  return model_->setValue();
}

void ListTypeView::insertRow(const row_t& value) {
  model_->insertRow(value);
  emit dataChangedSignal();
}

void ListTypeView::clear() {
  model_->clear();
  emit dataChangedSignal();
}

ListTypeView::Mode ListTypeView::currentMode() const {
  return mode_;
}

void ListTypeView::setCurrentMode(Mode mode) {
  mode_ = mode;
  if (mode == kArray) {
    model_->setFirstColumnName(translations::trValue);
  } else if (mode == kSet) {
    model_->setFirstColumnName(translations::trMember);
  } else {
    NOTREACHED() << "Unhandled mode: " << mode;
  }
}

void ListTypeView::addRow(const QModelIndex& index) {
  ValueTableItem* node = common::qt::item<common::qt::gui::TableItem*, ValueTableItem*>(index);
  insertRow(node->value());
}

void ListTypeView::removeRow(const QModelIndex& index) {
  model_->removeRow(index.row());
  emit dataChangedSignal();
}

void ListTypeView::currentChanged(const QModelIndex& current, const QModelIndex& previous) {
  base_class::currentChanged(current, previous);

  if (current.isValid()) {
    ValueTableItem* node = common::qt::item<common::qt::gui::TableItem*, ValueTableItem*>(current);
    emit rowChanged(node->value());
  }
}

}  // namespace gui
}  // namespace fastonosql
