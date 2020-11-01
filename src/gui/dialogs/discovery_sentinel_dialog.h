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

#include <vector>

#include <fastonosql/core/sentinel/sentinel_discovery_info.h>

#include "gui/dialogs/base_dialog.h"

#include "proxy/connection_settings/iconnection_settings.h"

class QLabel;
class QTreeWidget;

namespace common {
namespace qt {
namespace gui {
class GlassWidget;
}
}  // namespace qt
}  // namespace common

namespace fastonosql {
namespace gui {
class ConnectionListWidgetItemDiscovered;

class DiscoverySentinelDiagnosticDialog : public BaseDialog {
  Q_OBJECT

 public:
  typedef BaseDialog base_class;
  template <typename T, typename... Args>
  friend T* createDialog(Args&&... args);

  enum { fix_width = 480, fix_height = 320 };

  std::vector<ConnectionListWidgetItemDiscovered*> selectedConnections() const;

 private Q_SLOTS:
  void connectionResultReady(common::Error err,
                             qint64 exec_mstime,
                             std::vector<core::ServerDiscoverySentinelInfoSPtr> infos);

 protected:
  DiscoverySentinelDiagnosticDialog(const QString& title,
                                    const QIcon& icon,
                                    proxy::IConnectionSettingsBaseSPtr connection,
                                    QWidget* parent = Q_NULLPTR);
  void showEvent(QShowEvent* e) override;

 private:
  void setIcon(const QIcon& icon);
  void testConnection(proxy::IConnectionSettingsBaseSPtr connection);

  common::qt::gui::GlassWidget* glass_widget_;
  QLabel* execute_time_label_;
  QLabel* status_label_;
  QLabel* note_label_;
  QTreeWidget* list_widget_;
  QLabel* icon_label_;
};

}  // namespace gui
}  // namespace fastonosql
