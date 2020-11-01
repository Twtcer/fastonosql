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

#include <common/net/types.h>

#include "proxy/events/events_info.h"
#include "proxy/proxy_fwd.h"
#include "proxy/server/iserver_base.h"

namespace fastonosql {
namespace proxy {

class ICluster : public IServerBase {
 public:
  typedef IServerSPtr node_t;
  typedef std::vector<node_t> nodes_t;

  std::string GetName() const override;
  nodes_t GetNodes() const;
  void AddServer(node_t serv);

  node_t GetRoot() const;

 private Q_SLOTS:
  void RedirectRequest(const common::net::HostAndPortAndSlot& host, const events_info::ExecuteInfoRequest& req);

 protected:
  explicit ICluster(const std::string& name);

 private:
  const std::string name_;
  nodes_t nodes_;
};

}  // namespace proxy
}  // namespace fastonosql
