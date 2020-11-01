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

#include "gui/workers/load_welcome_page.h"

#include <common/net/http_client.h>
#include <common/qt/convert2string.h>
#include <common/uri/gurl.h>

#include "gui/socket_tls.h"

#define CONTENT_PORT 443
#if defined(PRO_VERSION)
#define CONTENT_PATH "/welcome_app_pro/" PROJECT_VERSION ".html"
#elif defined(ENTERPRISE_VERSION)
#define CONTENT_PATH "/welcome_app_enterprise/" PROJECT_VERSION ".html"
#else
#define CONTENT_PATH "/welcome_app/" PROJECT_VERSION ".html"
#endif

namespace {
const common::uri::GURL kContentUrl = common::uri::GURL(PROJECT_DOMAIN CONTENT_PATH);

class HttpsClient : public common::net::IHttpClient {
 public:
  typedef common::net::IHttpClient base_class;
  explicit HttpsClient(const common::net::HostAndPort& host) : base_class(new common::net::SocketTls(host)) {}

  common::ErrnoError Connect(struct timeval* tv = nullptr) override {
    common::net::SocketTls* sock = static_cast<common::net::SocketTls*>(GetSocket());
    return sock->Connect(tv);
  }

  bool IsConnected() const override {
    common::net::SocketTls* sock = static_cast<common::net::SocketTls*>(GetSocket());
    return sock->IsConnected();
  }

  common::ErrnoError Disconnect() override {
    common::net::SocketTls* sock = static_cast<common::net::SocketTls*>(GetSocket());
    return sock->Disconnect();
  }

  common::net::HostAndPort GetHost() const override {
    common::net::SocketTls* sock = static_cast<common::net::SocketTls*>(GetSocket());
    return sock->GetHost();
  }

  common::ErrnoError SendFile(descriptor_t file_fd, size_t file_size) override {
    UNUSED(file_fd);
    UNUSED(file_size);
    return common::ErrnoError();
  }
};

common::Error loadPageRoutine(common::http::HttpResponse* resp) {
  if (!resp) {
    return common::make_error_inval();
  }

  const auto hs = common::net::HostAndPort(kContentUrl.host(), CONTENT_PORT);
  HttpsClient cl(hs);
  common::ErrnoError errn = cl.Connect();
  if (errn) {
    return common::make_error_from_errno(errn);
  }

  const auto path = kContentUrl.PathForRequest();
  common::Error err = cl.Get(path);
  if (err) {
    cl.Disconnect();
    return err;
  }

  common::http::HttpResponse lresp;
  err = cl.ReadResponse(&lresp);
  if (err) {
    cl.Disconnect();
    return err;
  }

  if (lresp.IsEmptyBody()) {
    cl.Disconnect();
    return common::make_error("Empty body");
  }

  *resp = lresp;
  cl.Disconnect();
  return common::Error();
}
}  // namespace

namespace fastonosql {
namespace gui {

LoadWelcomePage::LoadWelcomePage(QObject* parent) : QObject(parent) {
  qRegisterMetaType<common::Error>("common::Error");
}

void LoadWelcomePage::routine() {
  common::http::HttpResponse resp;
  common::Error err = loadPageRoutine(&resp);
  if (err) {
    emit pageLoaded(err, QString());
    return;
  }

  QString body;
  common::ConvertFromBytes(resp.GetBody(), &body);
  emit pageLoaded(common::Error(), body);
}

}  // namespace gui
}  // namespace fastonosql
