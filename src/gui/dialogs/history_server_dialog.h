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

#include "gui/dialogs/base_dialog.h"

#include "proxy/events/events_info.h"
#include "proxy/proxy_fwd.h"

class QComboBox;
class QPushButton;

namespace common {
namespace qt {
namespace gui {
class GlassWidget;
class GraphWidget;
}  // namespace gui
}  // namespace qt
}  // namespace common

namespace fastonosql {
namespace gui {

class ServerHistoryDialog : public BaseDialog {
  Q_OBJECT

 public:
  typedef BaseDialog base_class;
  template <typename T, typename... Args>
  friend T* createDialog(Args&&... args);

  enum { min_width = 640, min_height = 480 };

 private Q_SLOTS:
  void startLoadServerHistoryInfo(const proxy::events_info::ServerInfoHistoryRequest& req);
  void finishLoadServerHistoryInfo(const proxy::events_info::ServerInfoHistoryResponse& res);
  void startClearServerHistory(const proxy::events_info::ClearServerHistoryRequest& req);
  void finishClearServerHistory(const proxy::events_info::ClearServerHistoryResponse& res);
  void snapShotAdd(core::ServerInfoSnapShoot snapshot);
  void clearHistory();

  void refreshInfoFields(int index);
  void refreshGraph(int index);

 protected:
  explicit ServerHistoryDialog(const QString& title,
                               const QIcon& icon,
                               proxy::IServerSPtr server,
                               QWidget* parent = Q_NULLPTR);

  void showEvent(QShowEvent* e) override;

  void retranslateUi() override;

 private:
  void reset();
  void requestHistoryInfo();

  QWidget* settings_graph_;
  QPushButton* clear_history_;
  QComboBox* server_info_groups_names_;
  QComboBox* server_info_fields_;

  common::qt::gui::GraphWidget* graph_widget_;

  common::qt::gui::GlassWidget* glass_widget_;
  proxy::events_info::ServerInfoHistoryResponse::infos_container_type infos_;
  const proxy::IServerSPtr server_;
};

}  // namespace gui
}  // namespace fastonosql
