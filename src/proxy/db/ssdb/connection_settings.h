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

#include "proxy/connection_settings/iconnection_settings_remote.h"

#include <fastonosql/core/db/ssdb/config.h>

namespace fastonosql {
namespace proxy {
namespace ssdb {

class ConnectionSettings : public IConnectionSettingsRemote {
 public:
  ConnectionSettings(const connection_path_t& connection_path, const std::string& log_directory);

  core::ssdb::Config GetInfo() const;
  void SetInfo(const core::ssdb::Config& info);

  std::string GetDelimiter() const override;
  void SetDelimiter(const std::string& delimiter) override;

  common::net::HostAndPort GetHost() const override;
  void SetHost(const common::net::HostAndPort& host) override;

  std::string GetCommandLine() const override;
  void SetCommandLine(const std::string& line) override;

  ConnectionSettings* Clone() const override;

 private:
  core::ssdb::Config info_;
};

}  // namespace ssdb
}  // namespace proxy
}  // namespace fastonosql
