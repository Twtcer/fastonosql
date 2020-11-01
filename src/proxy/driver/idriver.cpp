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

#include "proxy/driver/idriver.h"

#include <string>
#include <vector>

#include <QApplication>
#include <QThread>

#include <common/convert2string.h>
#include <common/file_system/file.h>
#include <common/file_system/file_system.h>
#include <common/file_system/string_path_utils.h>
#include <common/sprintf.h>
#include <common/threads/platform_thread.h>
#include <common/time.h>

#include "proxy/command/command_logger.h"
#include "proxy/driver/first_child_update_root_locker.h"

namespace {

const char kStampMagicNumber = 0x1E;
const char kEndLine = '\n';
std::string CreateStamp(common::time64_t time) {
  return kStampMagicNumber + common::ConvertToString(time) + kEndLine;
}

bool GetStamp(std::string stamp, common::time64_t* time_out) {
  if (stamp.empty()) {
    return false;
  }

  if (stamp[0] != kStampMagicNumber) {
    return false;
  }

  stamp.erase(stamp.begin());  // pop_front
  if (stamp.empty()) {
    return false;
  }

  if (stamp[stamp.size() - 1] == kEndLine) {
    stamp.pop_back();
  }

  common::time64_t ltime_out;
  if (!common::ConvertFromString(stamp, &ltime_out)) {
    return false;
  }
  *time_out = ltime_out;
  return ltime_out != 0;
}
}  // namespace

namespace fastonosql {
namespace proxy {
namespace {
const struct RegisterTypes {
  RegisterTypes() {
    qRegisterMetaType<common::ValueSPtr>("common::ValueSPtr");
    qRegisterMetaType<core::FastoObjectIPtr>("core::FastoObjectIPtr");
    qRegisterMetaType<core::NKey>("core::NKey");
    qRegisterMetaType<core::NDbKValue>("core::NDbKValue");
    qRegisterMetaType<core::IDataBaseInfoSPtr>("core::IDataBaseInfoSPtr");
    qRegisterMetaType<core::ttl_t>("core::ttl_t");
    qRegisterMetaType<core::nkey_t>("core::nkey_t");
    qRegisterMetaType<core::ServerInfoSnapShoot>("core::ServerInfoSnapShoot");
  }
} reg_type;

void NotifyProgressImpl(IDriver* sender, QObject* reciver, int value) {
  IDriver::Reply(reciver, new events::ProgressResponseEvent(sender, events::ProgressResponseEvent::value_type(value)));
}

template <typename event_request_type, typename event_response_type>
void ReplyNotImplementedYet(IDriver* sender, event_request_type* ev, const char* eventCommandText) {
  QObject* esender = ev->sender();
  NotifyProgressImpl(sender, esender, 0);
  typename event_response_type::value_type res(ev->value());

  std::string patternResult =
      common::MemSPrintf("Sorry, but now " PROJECT_NAME_TITLE " not supported %s command.", eventCommandText);
  common::Error er = common::make_error(patternResult);
  res.setErrorInfo(er);
  event_response_type* resp = new event_response_type(sender, res);
  IDriver::Reply(esender, resp);
  NotifyProgressImpl(sender, esender, 100);
}

}  // namespace

IDriver::IDriver(IConnectionSettingsBaseSPtr settings)
    : settings_(settings), thread_(nullptr), timer_info_id_(0), log_file_(nullptr), server_info_() {
  thread_ = new QThread(this);
  moveToThread(thread_);

  VERIFY(connect(thread_, &QThread::started, this, &IDriver::Init));
  VERIFY(connect(thread_, &QThread::finished, this, &IDriver::Clear));
}

IDriver::~IDriver() {
  if (log_file_) {
    log_file_->Close();
    destroy(&log_file_);
  }
}

common::Error IDriver::Execute(core::FastoObjectCommandIPtr cmd) {
  if (!cmd) {
    DNOTREACHED();
    return common::make_error_inval();
  }

  LOG_COMMAND(cmd);
  common::Error err = ExecuteImpl(cmd->GetInputCommand(), cmd.get());
  return err;
}

void IDriver::Reply(QObject* reciver, QEvent* ev) {
  qApp->postEvent(reciver, ev);
}

void IDriver::PrepareSettings() {
  settings_->PrepareInGuiIfNeeded();
}

core::ConnectionType IDriver::GetType() const {
  return settings_->GetType();
}

connection_path_t IDriver::GetConnectionPath() const {
  return settings_->GetPath();
}

std::string IDriver::GetDelimiter() const {
  return settings_->GetDelimiter();
}

NsDisplayStrategy IDriver::GetNsDisplayStrategy() const {
  return settings_->GetNsDisplayStrategy();
}

std::string IDriver::GetNsSeparator() const {
  return settings_->GetNsSeparator();
}

void IDriver::Start() {
  thread_->start();
}

void IDriver::Stop() {
  thread_->quit();
  thread_->wait();
}

void IDriver::Interrupt() {
  SetInterrupted(true);
}

void IDriver::Init() {
  if (settings_->IsHistoryEnabled()) {
    int interval = settings_->GetLoggingMsTimeInterval();
    timer_info_id_ = startTimer(interval);
    DCHECK_NE(timer_info_id_, 0);
  }
  InitImpl();
}

void IDriver::Clear() {
  if (timer_info_id_ != 0) {
    killTimer(timer_info_id_);
    timer_info_id_ = 0;
  }
  common::Error err = SyncDisconnect();
  if (err) {
    DNOTREACHED();
  }
  ClearImpl();
}

core::IServerInfoSPtr IDriver::GetCurrentServerInfoIfConnected() const {
  if (IsConnected()) {
    return server_info_;
  }

  return core::IServerInfoSPtr();
}

void IDriver::customEvent(QEvent* event) {
  SetInterrupted(false);

  QEvent::Type type = event->type();
  if (type == static_cast<QEvent::Type>(events::ConnectRequestEvent::EventType)) {
    events::ConnectRequestEvent* ev = static_cast<events::ConnectRequestEvent*>(event);
    HandleConnectEvent(ev);
  } else if (type == static_cast<QEvent::Type>(events::DisconnectRequestEvent::EventType)) {
    events::DisconnectRequestEvent* ev = static_cast<events::DisconnectRequestEvent*>(event);
    HandleDisconnectEvent(ev);
  } else if (type == static_cast<QEvent::Type>(events::ExecuteRequestEvent::EventType)) {
    events::ExecuteRequestEvent* ev = static_cast<events::ExecuteRequestEvent*>(event);
    HandleExecuteEvent(ev);
  } else if (type == static_cast<QEvent::Type>(events::LoadDatabasesInfoRequestEvent::EventType)) {
    events::LoadDatabasesInfoRequestEvent* ev = static_cast<events::LoadDatabasesInfoRequestEvent*>(event);
    HandleLoadDatabaseInfosEvent(ev);  //
  } else if (type == static_cast<QEvent::Type>(events::ServerInfoRequestEvent::EventType)) {
    events::ServerInfoRequestEvent* ev = static_cast<events::ServerInfoRequestEvent*>(event);
    HandleLoadServerInfoEvent(ev);  //
  } else if (type == static_cast<QEvent::Type>(events::ServerInfoHistoryRequestEvent::EventType)) {
    events::ServerInfoHistoryRequestEvent* ev = static_cast<events::ServerInfoHistoryRequestEvent*>(event);
    HandleLoadServerInfoHistoryEvent(ev);  //
  } else if (type == static_cast<QEvent::Type>(events::ClearServerHistoryRequestEvent::EventType)) {
    events::ClearServerHistoryRequestEvent* ev = static_cast<events::ClearServerHistoryRequestEvent*>(event);
    HandleClearServerHistoryEvent(ev);  //
  } else if (type == static_cast<QEvent::Type>(events::ServerPropertyInfoRequestEvent::EventType)) {
    events::ServerPropertyInfoRequestEvent* ev = static_cast<events::ServerPropertyInfoRequestEvent*>(event);
    HandleLoadServerPropertyEvent(ev);  // ni
  } else if (type == static_cast<QEvent::Type>(events::ChangeServerPropertyInfoRequestEvent::EventType)) {
    events::ChangeServerPropertyInfoRequestEvent* ev =
        static_cast<events::ChangeServerPropertyInfoRequestEvent*>(event);
    HandleServerPropertyChangeEvent(ev);  // ni
  } else if (type == static_cast<QEvent::Type>(events::LoadServerChannelsRequestEvent::EventType)) {
    events::LoadServerChannelsRequestEvent* ev = static_cast<events::LoadServerChannelsRequestEvent*>(event);
    HandleLoadServerChannelsRequestEvent(ev);  // ni
  } else if (type == static_cast<QEvent::Type>(events::LoadServerClientsRequestEvent::EventType)) {
    events::LoadServerClientsRequestEvent* ev = static_cast<events::LoadServerClientsRequestEvent*>(event);
    HandleLoadServerClientsRequestEvent(ev);  // ni
  } else if (type == static_cast<QEvent::Type>(events::BackupRequestEvent::EventType)) {
    events::BackupRequestEvent* ev = static_cast<events::BackupRequestEvent*>(event);
    HandleBackupEvent(ev);  // ni
  } else if (type == static_cast<QEvent::Type>(events::RestoreRequestEvent::EventType)) {
    events::RestoreRequestEvent* ev = static_cast<events::RestoreRequestEvent*>(event);
    HandleRestoreEvent(ev);  // ni
  } else if (type == static_cast<QEvent::Type>(events::LoadDatabaseContentRequestEvent::EventType)) {
    events::LoadDatabaseContentRequestEvent* ev = static_cast<events::LoadDatabaseContentRequestEvent*>(event);
    HandleLoadDatabaseContentEvent(ev);
  } else if (type == static_cast<QEvent::Type>(events::DiscoveryInfoRequestEvent::EventType)) {
    events::DiscoveryInfoRequestEvent* ev = static_cast<events::DiscoveryInfoRequestEvent*>(event);
    HandleDiscoveryInfoEvent(ev);  //
  }

  return QObject::customEvent(event);
}

void IDriver::timerEvent(QTimerEvent* event) {
  if (timer_info_id_ == event->timerId() && settings_->IsHistoryEnabled() && IsConnected()) {
    std::string path = settings_->GetLoggingPath();
    if (!log_file_) {
      std::string dir = common::file_system::get_dir_path(path);
      common::ErrnoError err = common::file_system::create_directory(dir, true);
      UNUSED(err);
      if (common::file_system::is_directory(dir) == common::SUCCESS) {
        log_file_ = new common::file_system::ANSIFile;
      }
    }

    if (log_file_ && !log_file_->IsOpen()) {
      common::ErrnoError err = log_file_->Open(path, "ab+");
      if (err) {
        DNOTREACHED();
      }
    }

    if (log_file_ && log_file_->IsOpen()) {
      common::time64_t time = common::time::current_utc_mstime();
      std::string stamp = CreateStamp(time);
      core::IServerInfo* info = nullptr;
      common::Error err = GetCurrentServerInfo(&info);
      if (err) {
        QObject::timerEvent(event);
        return;
      }

      core::ServerInfoSnapShoot shot(time, core::IServerInfoSPtr(info));
      emit ServerInfoSnapShooted(shot);

      log_file_->Write(stamp);
      log_file_->Write(info->ToString());
    }
  }
  QObject::timerEvent(event);
}

void IDriver::NotifyProgress(QObject* reciver, int value) {
  NotifyProgressImpl(this, reciver, value);
}

void IDriver::HandleConnectEvent(events::ConnectRequestEvent* ev) {
  QObject* sender = ev->sender();
  NotifyProgress(sender, 0);
  events::ConnectResponseEvent::value_type res(ev->value());
  NotifyProgress(sender, 25);
  common::Error err = SyncConnect();
  if (err) {
    res.setErrorInfo(err);
  }
  NotifyProgress(sender, 75);
  Reply(sender, new events::ConnectResponseEvent(this, res));
  NotifyProgress(sender, 100);
}

void IDriver::HandleDisconnectEvent(events::DisconnectRequestEvent* ev) {
  QObject* sender = ev->sender();
  NotifyProgress(sender, 0);
  events::DisconnectResponseEvent::value_type res(ev->value());
  NotifyProgress(sender, 50);

  common::Error err = SyncDisconnect();
  if (err) {
    res.setErrorInfo(err);
  }

  Reply(sender, new events::DisconnectResponseEvent(this, res));
  NotifyProgress(sender, 100);
}

void IDriver::HandleExecuteEvent(events::ExecuteRequestEvent* ev) {
  QObject* sender = ev->sender();
  NotifyProgress(sender, 0);
  events::ExecuteResponseEvent::value_type res(ev->value());

  const core::command_buffer_t input_line = res.text;
  std::vector<core::command_buffer_t> commands;
  common::Error err = ParseCommands(input_line, &commands);
  if (err) {
    res.setErrorInfo(err);
    Reply(sender, new events::ExecuteResponseEvent(this, res));
    NotifyProgress(sender, 100);
    return;
  }

  const bool silence = res.silence;
  const size_t repeat = res.repeat;
  const bool history = res.history;
  const common::time64_t msec_repeat_interval = res.msec_repeat_interval;
  const core::CmdLoggingType log_type = res.logtype;
  RootLocker* lock = history ? new RootLocker(this, sender, input_line, silence)
                             : new FirstChildUpdateRootLocker(this, sender, input_line, silence, commands);
  core::FastoObjectIPtr obj = lock->Root();
  const double step = 99.0 / static_cast<double>(commands.size() * (repeat + 1));
  double cur_progress = 0.0;
  for (size_t r = 0; r < repeat + 1; ++r) {
    common::time64_t start_ts = common::time::current_utc_mstime();
    for (size_t i = 0; i < commands.size(); ++i) {
      if (IsInterrupted()) {
        res.setErrorInfo(common::make_error(common::COMMON_EINTR));
        goto done;
      }

      cur_progress += step;
      NotifyProgress(sender, static_cast<int>(cur_progress));

      core::command_buffer_t command = commands[i];
      core::FastoObjectCommandIPtr cmd =
          silence ? CreateCommandFast(command, log_type) : CreateCommand(obj.get(), command, log_type);  //
      common::Error err = Execute(cmd);
      if (err) {
        res.setErrorInfo(err);
        goto done;
      }
      res.executed_commands.push_back(cmd);
    }

    common::time64_t finished_ts = common::time::current_utc_mstime();
    common::time64_t diff = finished_ts - start_ts;
    const common::time64_t sleep_time = msec_repeat_interval - diff;
    if (sleep_time > 0) {
      common::threads::PlatformThread::Sleep(sleep_time);
    }
  }

done:
  Reply(sender, new events::ExecuteResponseEvent(this, res));
  NotifyProgress(sender, 100);
  delete lock;
}

void IDriver::HandleLoadDatabaseContentEvent(events::LoadDatabaseContentRequestEvent* ev) {
  QObject* sender = ev->sender();
  NotifyProgress(sender, 0);
  events::LoadDatabaseContentResponseEvent::value_type res(ev->value());
  const core::command_buffer_t pattern_result = core::GetKeysPattern(res.cursor_in, res.pattern, res.keys_count);
  core::FastoObjectCommandIPtr cmd = CreateCommandFast(pattern_result, core::C_INNER);
  NotifyProgress(sender, 50);
  common::Error err = Execute(cmd);
  if (err) {
    res.setErrorInfo(err);
  } else {
    core::FastoObject::childs_t rchildrens = cmd->GetChildrens();
    if (rchildrens.size()) {
      CHECK_EQ(rchildrens.size(), 1);
      core::FastoObject* array = rchildrens[0].get();
      if (array) {
        auto array_value = array->GetValue();
        common::ArrayValue* arm = nullptr;
        if (!array_value->GetAsList(&arm)) {
          goto done;
        }

        CHECK_EQ(arm->GetSize(), 2);
        core::cursor_t cursor;
        bool isok = arm->GetUInteger(0, &cursor);
        if (!isok) {
          goto done;
        }
        res.cursor_out = cursor;

        common::ArrayValue* ar = nullptr;
        isok = arm->GetList(1, &ar);
        if (!isok) {
          goto done;
        }

        for (size_t i = 0; i < ar->GetSize(); ++i) {
          core::command_buffer_t key_str;
          if (ar->GetString(i, &key_str)) {
            const core::nkey_t key(key_str);
            const core::NKey k(key);
            const core::NValue empty_val(common::Value::CreateEmptyStringValue());
            const core::NDbKValue ress(k, empty_val);
            res.keys.push_back(ress);
          }
        }

        common::Error err = DBkcountImpl(&res.db_keys_count);
        DCHECK(!err) << "can't get db keys count!";
      }
    }
  }
done:
  NotifyProgress(sender, 75);
  Reply(sender, new events::LoadDatabaseContentResponseEvent(this, res));
  NotifyProgress(sender, 100);
}

void IDriver::HandleLoadServerPropertyEvent(events::ServerPropertyInfoRequestEvent* ev) {
  ReplyNotImplementedYet<events::ServerPropertyInfoRequestEvent, events::ServerPropertyInfoResponseEvent>(
      this, ev, "server property");
}

void IDriver::HandleServerPropertyChangeEvent(events::ChangeServerPropertyInfoRequestEvent* ev) {
  ReplyNotImplementedYet<events::ChangeServerPropertyInfoRequestEvent, events::ChangeServerPropertyInfoResponseEvent>(
      this, ev, "change server property");
}

void IDriver::HandleLoadServerChannelsRequestEvent(events::LoadServerChannelsRequestEvent* ev) {
  ReplyNotImplementedYet<events::LoadServerChannelsRequestEvent, events::LoadServerChannelsResponseEvent>(
      this, ev, "load server channels");
}

void IDriver::HandleLoadServerClientsRequestEvent(events::LoadServerClientsRequestEvent* ev) {
  ReplyNotImplementedYet<events::LoadServerClientsRequestEvent, events::LoadServerClientsResponseEvent>(
      this, ev, "load server clients");
}

void IDriver::HandleBackupEvent(events::BackupRequestEvent* ev) {
  ReplyNotImplementedYet<events::BackupRequestEvent, events::BackupResponseEvent>(this, ev, "backup server");
}

void IDriver::HandleRestoreEvent(events::RestoreRequestEvent* ev) {
  ReplyNotImplementedYet<events::RestoreRequestEvent, events::RestoreResponseEvent>(this, ev, "export server");
}

void IDriver::HandleLoadDatabaseInfosEvent(events::LoadDatabasesInfoRequestEvent* ev) {
  /*QObject* sender = ev->sender();
  NotifyProgress(sender, 0);
  events::LoadDatabasesInfoResponseEvent::value_type res(ev->value());
  NotifyProgress(sender, 50);
  core::IDataBaseInfo* info = nullptr;
  common::Error err = GetCurrentDataBaseInfo(&info);
  if (err) {
    res.setErrorInfo(err);
  } else {
    res.databases.push_back(core::IDataBaseInfoSPtr(info));
  }
  Reply(sender, new events::LoadDatabasesInfoResponseEvent(this, res));
  NotifyProgress(sender, 100);*/

  QObject* sender = ev->sender();
  NotifyProgress(sender, 0);
  events::LoadDatabasesInfoResponseEvent::value_type res(ev->value());
  NotifyProgress(sender, 50);

  core::IDataBaseInfo* info = nullptr;
  common::Error err = GetCurrentDataBaseInfo(&info);
  if (err) {
    res.setErrorInfo(err);
    NotifyProgress(sender, 75);
    Reply(sender, new events::LoadDatabasesInfoResponseEvent(this, res));
    NotifyProgress(sender, 100);
    return;
  }

  auto tran = GetTranslator();
  core::command_buffer_t get_dbs;
  err = tran->GetDatabasesCommand(&get_dbs);
  core::FastoObjectCommandIPtr cmd = CreateCommandFast(get_dbs, core::C_INNER);
  if (err) {
    res.setErrorInfo(err);
    NotifyProgress(sender, 75);
    Reply(sender, new events::LoadDatabasesInfoResponseEvent(this, res));
    NotifyProgress(sender, 100);
    return;
  }

  err = Execute(cmd.get());
  if (err) {
    res.setErrorInfo(err);
    NotifyProgress(sender, 75);
    Reply(sender, new events::LoadDatabasesInfoResponseEvent(this, res));
    NotifyProgress(sender, 100);
    return;
  }

  core::FastoObject::childs_t rchildrens = cmd->GetChildrens();
  CHECK_EQ(rchildrens.size(), 1);
  auto ar = std::static_pointer_cast<common::ArrayValue>(rchildrens[0]->GetValue());
  CHECK(ar);

  core::IDataBaseInfoSPtr curdb(info);
  if (!ar->IsEmpty()) {
    for (size_t i = 0; i < ar->GetSize(); ++i) {
      common::Value::string_t name;
      if (ar->GetString(i, &name)) {
        core::IDataBaseInfoSPtr dbInf(CreateDatabaseInfo(name, false, 0));
        if (dbInf->GetName() == curdb->GetName()) {
          res.databases.push_back(curdb);
        } else {
          res.databases.push_back(dbInf);
        }
      }
    }
  } else {
    res.databases.push_back(curdb);
  }
  NotifyProgress(sender, 75);
  Reply(sender, new events::LoadDatabasesInfoResponseEvent(this, res));
  NotifyProgress(sender, 100);
}

void IDriver::HandleLoadServerInfoEvent(events::ServerInfoRequestEvent* ev) {
  QObject* sender = ev->sender();
  NotifyProgress(sender, 0);
  events::ServerInfoResponseEvent::value_type res(ev->value());
  NotifyProgress(sender, 50);
  core::IServerInfo* info = nullptr;
  common::Error err = GetCurrentServerInfo(&info);
  if (err) {
    res.setErrorInfo(err);
    server_info_.reset();
  } else {
    core::IServerInfoSPtr mem(info);
    res.SetInfo(mem);
    server_info_ = mem;
  }
  NotifyProgress(sender, 75);
  Reply(sender, new events::ServerInfoResponseEvent(this, res));
  NotifyProgress(sender, 100);
}

void IDriver::HandleLoadServerInfoHistoryEvent(events::ServerInfoHistoryRequestEvent* ev) {
  QObject* sender = ev->sender();
  events::ServerInfoHistoryResponseEvent::value_type res(ev->value());

  std::string path = settings_->GetLoggingPath();
  common::file_system::FileGuard<common::file_system::ANSIFile> read_file;
  common::ErrnoError err = read_file.Open(path, "rb");
  if (err) {
    res.setErrorInfo(common::make_error_from_errno(err));
  } else {
    events::ServerInfoHistoryResponseEvent::value_type::infos_container_type tmp_infos;

    common::time64_t cur_stamp = 0;
    std::string data_info;

    while (!read_file.IsEOF()) {
      std::string data;
      bool res = read_file.ReadLine(&data);
      if (!res || read_file.IsEOF()) {
        if (cur_stamp) {
          core::ServerInfoSnapShoot shoot(cur_stamp, MakeServerInfoFromString(data_info));
          tmp_infos.push_back(shoot);
        }
        break;
      }

      common::time64_t tmp_stamp = 0;
      if (GetStamp(data, &tmp_stamp)) {
        if (cur_stamp) {
          core::ServerInfoSnapShoot shoot(cur_stamp, MakeServerInfoFromString(data_info));
          tmp_infos.push_back(shoot);
        }
        cur_stamp = tmp_stamp;
        data_info.clear();
      } else {
        data_info.insert(data_info.end(), data.begin(), data.end());
      }
    }
    res.SetInfos(tmp_infos);
  }

  Reply(sender, new events::ServerInfoHistoryResponseEvent(this, res));
}

void IDriver::HandleClearServerHistoryEvent(events::ClearServerHistoryRequestEvent* ev) {
  QObject* sender = ev->sender();
  events::ClearServerHistoryResponseEvent::value_type res(ev->value());

  bool ret = false;

  if (log_file_ && log_file_->IsOpen()) {
    common::ErrnoError err = log_file_->Truncate(0);
    ret = err ? false : true;
  } else {
    std::string path = settings_->GetLoggingPath();
    if (common::file_system::is_file_exist(path)) {
      common::ErrnoError err = common::file_system::remove_file(path);
      if (err) {
        ret = false;
      } else {
        ret = true;
      }
    } else {
      ret = true;
    }
  }

  if (!ret) {
    res.setErrorInfo(common::make_error("Clear file error!"));
  }

  Reply(sender, new events::ClearServerHistoryResponseEvent(this, res));
}

void IDriver::HandleDiscoveryInfoEvent(events::DiscoveryInfoRequestEvent* ev) {
  QObject* sender = ev->sender();
  NotifyProgress(sender, 0);
  events::DiscoveryInfoResponseEvent::value_type res(ev->value());
  NotifyProgress(sender, 50);

  if (IsConnected()) {
    core::IDataBaseInfo* db = nullptr;
    std::vector<const core::CommandInfo*> cmds;
    common::Error err = GetServerDiscoveryInfo(&db, &cmds);
    if (err) {
      res.setErrorInfo(err);
    } else {
      core::IDataBaseInfoSPtr current_database_info(db);
      res.dbinfo = current_database_info;
      res.commands = cmds;
    }
  } else {
    res.setErrorInfo(common::make_error("Not connected to server, impossible to get discovery info!"));
  }

  NotifyProgress(sender, 75);
  Reply(sender, new events::DiscoveryInfoResponseEvent(this, res));
  NotifyProgress(sender, 100);
}

common::Error IDriver::GetServerDiscoveryInfo(core::IDataBaseInfo** dbinfo,
                                              std::vector<const core::CommandInfo*>* commands) {
  std::vector<const core::CommandInfo*> lcommands;
  GetServerCommands(&lcommands);  // can be failed

  core::IDataBaseInfo* ldbinfo = nullptr;
  common::Error err = GetCurrentDataBaseInfo(&ldbinfo);
  if (err) {
    return err;
  }

  *commands = lcommands;
  *dbinfo = ldbinfo;
  return err;
}

void IDriver::OnFlushedCurrentDB() {
  emit DBFlushed();
}

void IDriver::OnCreatedDB(core::IDataBaseInfo* info) {
  core::IDataBaseInfoSPtr curdb(info->Clone());
  emit DBCreated(curdb);
}

void IDriver::OnRemovedDB(core::IDataBaseInfo* info) {
  core::IDataBaseInfoSPtr curdb(info->Clone());
  emit DBRemoved(curdb);
}

void IDriver::OnChangedCurrentDB(core::IDataBaseInfo* info) {
  core::IDataBaseInfoSPtr curdb(info->Clone());
  emit DBChanged(curdb);
}

void IDriver::OnRemovedKeys(const core::NKeys& keys) {
  for (size_t i = 0; i < keys.size(); ++i) {
    emit KeyRemoved(keys[i]);
  }
}

void IDriver::OnAddedKey(const core::NDbKValue& key) {
  emit KeyAdded(key);
}

void IDriver::OnLoadedKey(const core::NDbKValue& key) {
  core::NValue val = key.GetValue();
  if (val->GetType() == common::Value::TYPE_NULL) {
    return;
  }

  emit KeyLoaded(key);
}

void IDriver::OnRenamedKey(const core::NKey& key, const core::nkey_t& new_key) {
  emit KeyRenamed(key, new_key);
}

void IDriver::OnChangedKeyTTL(const core::NKey& key, core::ttl_t ttl) {
  emit KeyTTLChanged(key, ttl);
}

void IDriver::OnLoadedKeyTTL(const core::NKey& key, core::ttl_t ttl) {
  emit KeyTTLLoaded(key, ttl);
}

void IDriver::OnQuited() {
  emit Disconnected();
}

}  // namespace proxy
}  // namespace fastonosql
