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

#pragma once

#include <QString>

#include <common/qt/gui/base/table_item.h>

#include <fastonosql/core/server_property_info.h>

namespace fastonosql {
namespace gui {

class PropertyTableItem : public common::qt::gui::TableItem {
 public:
  explicit PropertyTableItem(const core::property_t& prop);
  QString key() const;
  QString value() const;

  core::property_t property() const;
  void setProperty(const core::property_t& prop);

 private:
  core::property_t prop_;
};

}  // namespace gui
}  // namespace fastonosql
