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

#include "proxy/connection_settings/iconnection_settings.h"

namespace fastonosql {
namespace proxy {

struct SentinelSettings {
  typedef std::vector<IConnectionSettingsBaseSPtr> sentinel_nodes_t;
  SentinelSettings();

  IConnectionSettingsBaseSPtr sentinel;
  sentinel_nodes_t sentinel_nodes;
};

class ISentinelSettingsBase : public IConnectionSettings {
 public:
  typedef SentinelSettings sentinel_connection_t;
  typedef std::vector<sentinel_connection_t> sentinel_connections_t;

  sentinel_connections_t GetSentinels() const;
  void AddSentinel(sentinel_connection_t sent);

  ISentinelSettingsBase* Clone() const override = 0;

 protected:
  ISentinelSettingsBase(const connection_path_t& connection_path, core::ConnectionType type);

 private:
  sentinel_connections_t sentinel_nodes_;
};

typedef std::shared_ptr<ISentinelSettingsBase> ISentinelSettingsBaseSPtr;

}  // namespace proxy
}  // namespace fastonosql
