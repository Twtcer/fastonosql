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

#include "proxy/sentinel/isentinel.h"

#include <string>

namespace fastonosql {
namespace proxy {

ISentinel::ISentinel(const std::string& name) : name_(name), sentinels_() {}

std::string ISentinel::GetName() const {
  return name_;
}

void ISentinel::AddSentinel(sentinel_t serv) {
  sentinels_.push_back(serv);
}

ISentinel::sentinels_t ISentinel::GetSentinels() const {
  return sentinels_;
}

}  // namespace proxy
}  // namespace fastonosql
