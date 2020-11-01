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

#include "gui/connection_listwidget_items.h"

#include <string>

#include <common/qt/convert2string.h>

#include <fastonosql/core/connection_types.h>

#include "gui/gui_factory.h"

namespace fastonosql {
namespace gui {

DirectoryListWidgetItem::DirectoryListWidgetItem(const proxy::connection_path_t& path) : path_(path) {
  std::string dir_name = path.GetName();
  QString qdir_name;
  if (common::ConvertFromString(dir_name, &qdir_name)) {
    setText(0, qdir_name);
  }
  setIcon(0, GuiFactory::GetInstance().directoryIcon());
  if (common::ConvertFromString(path_.GetDirectory(), &qdir_name)) {
    setText(1, qdir_name);
  }
}

proxy::connection_path_t DirectoryListWidgetItem::path() const {
  return path_;
}

IConnectionListWidgetItem::IConnectionListWidgetItem(QTreeWidgetItem* parent)
    : QTreeWidgetItem(parent), connection_() {}

void IConnectionListWidgetItem::setConnection(proxy::IConnectionSettingsBaseSPtr cons) {
  connection_ = cons;
}

proxy::IConnectionSettingsBaseSPtr IConnectionListWidgetItem::connection() const {
  return connection_;
}

ConnectionListWidgetItem::ConnectionListWidgetItem(QTreeWidgetItem* parent) : IConnectionListWidgetItem(parent) {}

void ConnectionListWidgetItem::setConnection(proxy::IConnectionSettingsBaseSPtr cons) {
  if (!cons) {
    DNOTREACHED();
    return;
  }

  proxy::connection_path_t path = cons->GetPath();
  QString conName;
  if (common::ConvertFromString(path.GetName(), &conName)) {
    setText(0, conName);
  }
  core::ConnectionType conType = cons->GetType();
  setIcon(0, GuiFactory::GetInstance().icon(conType));
  if (common::ConvertFromString(cons->GetFullAddress(), &conName)) {
    setText(1, conName);
  }
  IConnectionListWidgetItem::setConnection(cons);
}

IConnectionListWidgetItem::itemConnectionType ConnectionListWidgetItem::type() const {
  return Common;
}

#if defined(PRO_VERSION) || defined(ENTERPRISE_VERSION)
ConnectionListWidgetItemDiscovered::ConnectionListWidgetItemDiscovered(const core::ServerCommonInfo& info,
                                                                       QTreeWidgetItem* parent)
    : ConnectionListWidgetItem(parent), info_(info) {
  const std::string stype = common::ConvertToString(info_.type);
  QString qstype;
  if (common::ConvertFromString(stype, &qstype)) {
    setText(2, qstype);
  }

  const std::string sstate = common::ConvertToString(info_.state);
  QString qsstate;
  if (common::ConvertFromString(sstate, &qsstate)) {
    setText(3, qsstate);
  }
}

IConnectionListWidgetItem::itemConnectionType ConnectionListWidgetItemDiscovered::type() const {
  return Discovered;
}

SentinelConnectionWidgetItem::SentinelConnectionWidgetItem(const core::ServerCommonInfo& info,
                                                           SentinelConnectionListWidgetItemContainer* parent)
    : ConnectionListWidgetItemDiscovered(info, parent) {  // core::SENTINEL
}

IConnectionListWidgetItem::itemConnectionType SentinelConnectionWidgetItem::type() const {
  return Sentinel;
}

SentinelConnectionListWidgetItemContainer::SentinelConnectionListWidgetItemContainer(
    proxy::ISentinelSettingsBaseSPtr connection,
    QTreeWidgetItem* parent)
    : QTreeWidgetItem(parent), connection_() {
  setConnection(connection);

  proxy::ISentinelSettingsBase::sentinel_connections_t sentinels = connection_->GetSentinels();
  for (size_t i = 0; i < sentinels.size(); ++i) {
    proxy::SentinelSettings sent = sentinels[i];
    SentinelConnectionWidgetItem* item = new SentinelConnectionWidgetItem(core::ServerCommonInfo(), this);
    item->setConnection(sent.sentinel);
    addChild(item);
    for (size_t j = 0; j < sent.sentinel_nodes.size(); ++j) {
      proxy::IConnectionSettingsBaseSPtr con = sent.sentinel_nodes[j];
      ConnectionListWidgetItem* child = new ConnectionListWidgetItem(item);
      child->setConnection(con);
      item->addChild(child);
    }
  }
}

void SentinelConnectionListWidgetItemContainer::setConnection(proxy::ISentinelSettingsBaseSPtr cons) {
  if (!cons) {
    return;
  }

  connection_ = cons;
  const std::string path = connection_->GetPath().ToString();
  QString qpath;
  if (common::ConvertFromString(path, &qpath)) {
    setText(0, qpath);
  }
  setIcon(0, GuiFactory::GetInstance().sentinelIcon());
}

proxy::ISentinelSettingsBaseSPtr SentinelConnectionListWidgetItemContainer::connection() const {
  return connection_;
}

ClusterConnectionListWidgetItemContainer::ClusterConnectionListWidgetItemContainer(
    proxy::IClusterSettingsBaseSPtr connection,
    QTreeWidgetItem* parent)
    : QTreeWidgetItem(parent), connection_() {
  setConnection(connection);

  auto nodes = connection_->GetNodes();
  for (size_t i = 0; i < nodes.size(); ++i) {
    proxy::IConnectionSettingsBaseSPtr con = nodes[i];
    ConnectionListWidgetItem* item = new ConnectionListWidgetItem(this);
    item->setConnection(con);
    addChild(item);
  }
}

void ClusterConnectionListWidgetItemContainer::setConnection(proxy::IClusterSettingsBaseSPtr cons) {
  if (!cons) {
    return;
  }

  connection_ = cons;
  const std::string path = connection_->GetPath().ToString();
  QString qpath;
  if (common::ConvertFromString(path, &qpath)) {
    setText(0, qpath);
  }
  setIcon(0, GuiFactory::GetInstance().clusterIcon());
}

proxy::IClusterSettingsBaseSPtr ClusterConnectionListWidgetItemContainer::connection() const {
  return connection_;
}
#endif

}  // namespace gui
}  // namespace fastonosql
