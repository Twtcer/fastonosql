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

#include "proxy/driver/idriver_remote.h"

#include "proxy/connection_settings/iconnection_settings_remote.h"

namespace fastonosql {
namespace proxy {

IDriverRemote::IDriverRemote(IConnectionSettingsBaseSPtr settings) : IDriver(settings) {
  CHECK(IsRemoteType(GetType()));
}

common::net::HostAndPort IDriverRemote::GetHost() const {
  auto remote_settings = GetSpecificSettings<IConnectionSettingsRemote>();
  return remote_settings->GetHost();
}

}  // namespace proxy
}  // namespace fastonosql
