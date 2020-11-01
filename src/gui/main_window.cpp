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

#include "gui/main_window.h"

#include <algorithm>
#include <string>

#include <QAction>
#include <QDateTime>
#include <QDesktopServices>
#include <QDockWidget>
#include <QFileDialog>
#include <QMenuBar>
#include <QMessageBox>
#include <QThread>
#include <QTimer>
#include <QUrl>

#if defined(OS_ANDROID)
#include <QApplication>
#include <QGestureEvent>
#endif

#include <common/convert2string.h>
#include <common/file_system/file.h>
#include <common/file_system/file_system.h>
#include <common/text_decoders/iedcoder_factory.h>

#include <common/qt/convert2string.h>
#include <common/qt/logger.h>
#include <common/qt/translations/translations.h>

#include <fastonosql/core/logger.h>

#include "proxy/cluster/icluster.h"
#include "proxy/command/command_logger.h"
#include "proxy/sentinel/isentinel.h"
#include "proxy/server/iserver.h"
#include "proxy/servers_manager.h"
#include "proxy/settings_manager.h"

#include "gui/dialogs/about_dialog.h"
#include "gui/dialogs/connections_dialog.h"
#include "gui/dialogs/encode_decode_dialog.h"
#include "gui/dialogs/how_to_use_dialog.h"
#include "gui/dialogs/preferences_dialog.h"
#include "gui/explorer/explorer_tree_widget.h"
#include "gui/gui_factory.h"
#include "gui/shortcuts.h"
#include "gui/utils.h"
#include "gui/widgets/log_tab_widget.h"
#include "gui/widgets/main_widget.h"
#include "gui/workers/statistic_sender.h"
#include "gui/workers/update_checker.h"

#include "translations/global.h"

namespace {

const QString trImportSettingsFailed = QObject::tr("Import settings failed!");
const QString trExportSettingsFailed = QObject::tr("Export settings failed!");
const QString trSettingsLoadedS = QObject::tr("Settings successfully loaded!");
const QString trSettingsImportedS = QObject::tr("Settings successfully imported!");
const QString trSettingsExportedS = QObject::tr("Settings successfully encrypted and exported!");

bool IsNeedUpdate(uint32_t cver) {
  return PROJECT_VERSION_NUMBER < cver;
}

const QKeySequence kLogsKeySequence = Qt::CTRL + Qt::Key_L;
const QKeySequence kExplorerKeySequence = Qt::CTRL + Qt::Key_T;

void LogWatcherRedirect(common::logging::LOG_LEVEL level, const std::string& message, bool notify) {
  LOG_MSG(message, level, notify);
}

}  // namespace

namespace fastonosql {
namespace gui {

MainWindow::MainWindow() : QMainWindow() {
#if defined(OS_ANDROID)
  setAttribute(Qt::WA_AcceptTouchEvents);
  // setAttribute(Qt::WA_StaticContents);

  // grabGesture(Qt::TapGesture);  // click
  grabGesture(Qt::TapAndHoldGesture);  // long tap

  // grabGesture(Qt::SwipeGesture);  // swipe
  // grabGesture(Qt::PanGesture);  // drag and drop
  // grabGesture(Qt::PinchGesture);  // zoom
#endif

#if defined(PRO_VERSION) || defined(ENTERPRISE_VERSION)
  proxy::UserInfo user_info = proxy::SettingsManager::GetInstance()->GetUserInfo();
  proxy::UserInfo::SubscriptionState user_sub_state = user_info.GetSubscriptionState();
  if (user_sub_state != proxy::UserInfo::SUBSCRIBED) {
    const time_t expire_application_utc_time = user_info.GetExpireTime();
    const QDateTime end_date = QDateTime::fromTime_t(expire_application_utc_time, Qt::LocalTime);
    const QString date_fmt = end_date.toString(Qt::ISODate);
    setWindowTitle(QString(PROJECT_NAME_TITLE " " PROJECT_VERSION " (expiration date: %1)").arg(date_fmt));
  } else {
    setWindowTitle(PROJECT_NAME_TITLE " " PROJECT_VERSION);
  }
#else
  setWindowTitle(PROJECT_NAME_TITLE " " PROJECT_VERSION);
#endif

  connect_action_ = new QAction(this);
  connect_action_->setIcon(GuiFactory::GetInstance().connectDBIcon());
  connect_action_->setShortcut(g_new_key);
  VERIFY(connect(connect_action_, &QAction::triggered, this, &MainWindow::open));

  load_from_file_action_ = new QAction(this);
  load_from_file_action_->setIcon(GuiFactory::GetInstance().loadIcon());
  // import_action_->setShortcut(g_open_key);
  VERIFY(connect(load_from_file_action_, &QAction::triggered, this, &MainWindow::loadConnection));

  import_action_ = new QAction(this);
  import_action_->setIcon(GuiFactory::GetInstance().importIcon());
  // import_action_->setShortcut(g_open_key);
  VERIFY(connect(import_action_, &QAction::triggered, this, &MainWindow::importConnection));

  export_action_ = new QAction(this);
  export_action_->setIcon(GuiFactory::GetInstance().exportIcon());
  // export_action_->setShortcut(g_open_key);
  VERIFY(connect(export_action_, &QAction::triggered, this, &MainWindow::exportConnection));

  // Exit action
  exit_action_ = new QAction(this);
  exit_action_->setShortcut(g_quit_key);
  VERIFY(connect(exit_action_, &QAction::triggered, this, &MainWindow::close));

  // File menu
  QMenu* file_menu = new QMenu(this);
  file_action_ = menuBar()->addMenu(file_menu);
  file_menu->addAction(connect_action_);
  file_menu->addAction(load_from_file_action_);
  file_menu->addAction(import_action_);
  file_menu->addAction(export_action_);
  QMenu* recent_menu = new QMenu(this);
  recent_connections_ = file_menu->addMenu(recent_menu);
  for (auto i = 0; i < max_recent_connections; ++i) {
    recent_connections_acts_[i] = new QAction(this);
    VERIFY(connect(recent_connections_acts_[i], &QAction::triggered, this, &MainWindow::openRecentConnection));
    recent_menu->addAction(recent_connections_acts_[i]);
  }

  clear_menu_ = new QAction(this);
  recent_menu->addSeparator();
  VERIFY(connect(clear_menu_, &QAction::triggered, this, &MainWindow::clearRecentConnectionsMenu));
  recent_menu->addAction(clear_menu_);

  file_menu->addSeparator();
  file_menu->addAction(exit_action_);
  updateRecentConnectionActions();

  preferences_action_ = new QAction(this);
  preferences_action_->setIcon(GuiFactory::GetInstance().preferencesIcon());
  VERIFY(connect(preferences_action_, &QAction::triggered, this, &MainWindow::openPreferences));

  // edit menu
  QMenu* edit_menu = new QMenu(this);
  edit_action_ = menuBar()->addMenu(edit_menu);
  edit_menu->addAction(preferences_action_);

  // tools menu
  QMenu* tools = new QMenu(this);
  tools_action_ = menuBar()->addMenu(tools);

  encode_decode_dialog_action_ = new QAction(this);
  encode_decode_dialog_action_->setIcon(GuiFactory::GetInstance().encodeDecodeIcon());
  VERIFY(connect(encode_decode_dialog_action_, &QAction::triggered, this, &MainWindow::openEncodeDecodeDialog));
  tools->addAction(encode_decode_dialog_action_);

  // window menu
  QMenu* window = new QMenu(this);
  window_action_ = menuBar()->addMenu(window);
  full_screan_action_ = new QAction(this);
  full_screan_action_->setShortcut(g_full_screen_key);
  VERIFY(connect(full_screan_action_, &QAction::triggered, this, &MainWindow::enterLeaveFullScreen));
  window->addAction(full_screan_action_);

  QMenu* views = new QMenu(translations::trViews, this);
  window->addMenu(views);

  QMenu* help_menu = new QMenu(this);
  about_action_ = new QAction(this);
  VERIFY(connect(about_action_, &QAction::triggered, this, &MainWindow::about));

  howtouse_action_ = new QAction(this);
  VERIFY(connect(howtouse_action_, &QAction::triggered, this, &MainWindow::howToUse));

  help_action_ = menuBar()->addMenu(help_menu);

  check_update_action_ = new QAction(this);
  VERIFY(connect(check_update_action_, &QAction::triggered, this, &MainWindow::checkUpdate));

  report_bug_action_ = new QAction(this);
  VERIFY(connect(report_bug_action_, &QAction::triggered, this, &MainWindow::reportBug));

  help_menu->addAction(check_update_action_);
  help_menu->addSeparator();
  help_menu->addAction(report_bug_action_);
  help_menu->addAction(howtouse_action_);
  help_menu->addSeparator();
  help_menu->addAction(about_action_);

  MainWidget* main_widget = new MainWidget;
  setCentralWidget(main_widget);

  exp_ = createWidget<ExplorerTreeWidget>();
  VERIFY(connect(exp_, &ExplorerTreeWidget::consoleOpened, main_widget, &MainWidget::openConsole));
  VERIFY(connect(exp_, &ExplorerTreeWidget::consoleOpenedAndExecute, main_widget, &MainWidget::openConsoleAndExecute));
  VERIFY(connect(exp_, &ExplorerTreeWidget::serverClosed, this, &MainWindow::closeServer, Qt::DirectConnection));
#if defined(PRO_VERSION) || defined(ENTERPRISE_VERSION)
  VERIFY(connect(exp_, &ExplorerTreeWidget::clusterClosed, this, &MainWindow::closeCluster, Qt::DirectConnection));
  VERIFY(connect(exp_, &ExplorerTreeWidget::sentinelClosed, this, &MainWindow::closeSentinel, Qt::DirectConnection));
#endif
  exp_dock_ = new QDockWidget;
  explorer_action_ = exp_dock_->toggleViewAction();
  explorer_action_->setShortcut(kExplorerKeySequence);
  explorer_action_->setChecked(true);
  views->addAction(explorer_action_);

  exp_dock_->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea | Qt::BottomDockWidgetArea |
                             Qt::TopDockWidgetArea);
  exp_dock_->setWidget(exp_);
  exp_dock_->setFeatures(QDockWidget::DockWidgetClosable | QDockWidget::DockWidgetMovable);
  addDockWidget(Qt::LeftDockWidgetArea, exp_dock_);

  LogTabWidget* log = new LogTabWidget;
  VERIFY(connect(&common::qt::Logger::GetInstance(), &common::qt::Logger::printed, log, &LogTabWidget::addLogMessage));
  VERIFY(connect(&proxy::CommandLogger::GetInstance(), &proxy::CommandLogger::Printed, log, &LogTabWidget::addCommand));
  SET_LOG_WATCHER(&LogWatcherRedirect);
  log_dock_ = new QDockWidget;
  logs_action_ = log_dock_->toggleViewAction();
  logs_action_->setShortcut(kLogsKeySequence);
  logs_action_->setChecked(true);
  views->addAction(logs_action_);

  log_dock_->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea | Qt::BottomDockWidgetArea |
                             Qt::TopDockWidgetArea);
  log_dock_->setWidget(log);
  log_dock_->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
  log_dock_->setFeatures(QDockWidget::DockWidgetClosable | QDockWidget::DockWidgetMovable);
  addDockWidget(Qt::BottomDockWidgetArea, log_dock_);

  setMinimumSize(QSize(min_width, min_height));
  createStatusBar();
  retranslateUi();
}

MainWindow::~MainWindow() {
  proxy::ServersManager::GetInstance().Clear();
}

void MainWindow::changeEvent(QEvent* ev) {
  if (ev->type() == QEvent::LanguageChange) {
    retranslateUi();
  }

  return QMainWindow::changeEvent(ev);
}

void MainWindow::showEvent(QShowEvent* ev) {
  QMainWindow::showEvent(ev);
  static bool statistic_sent = false;
  if (!statistic_sent) {
    sendStatisticAndCheckVersion();
    statistic_sent = true;
    QTimer::singleShot(0, this, SLOT(open()));
  }
}

void MainWindow::sendStatisticAndCheckVersion() {
  bool check_updates = proxy::SettingsManager::GetInstance()->GetAutoCheckUpdates();
  if (check_updates) {
    checkUpdate();
  }

  bool send_statistic = proxy::SettingsManager::GetInstance()->GetSendStatistic();
  if (send_statistic) {
    sendStatistic();
  }
}

void MainWindow::open() {
  auto dlg =
      createDialog<ConnectionsDialog>(translations::trConnections, GuiFactory::GetInstance().connectIcon(), this);  // +
  int result = dlg->exec();
  if (result != QDialog::Accepted) {
    return;
  }

  if (proxy::IConnectionSettingsBaseSPtr con = dlg->selectedConnection()) {
    createServer(con);
  }
#if defined(PRO_VERSION) || defined(ENTERPRISE_VERSION)
  else if (proxy::IClusterSettingsBaseSPtr clus = dlg->selectedCluster()) {
    createCluster(clus);
  } else if (proxy::ISentinelSettingsBaseSPtr sent = dlg->selectedSentinel()) {
    createSentinel(sent);
  }
#endif
  else {
    NOTREACHED();
  }
}

void MainWindow::about() {
  auto dlg = createDialog<AboutDialog>(this);  // +
  dlg->exec();
}

void MainWindow::howToUse() {
  auto dlg = createDialog<HowToUseDialog>(this);  // +
  dlg->exec();
}

void MainWindow::openPreferences() {
  auto dlg = createDialog<PreferencesDialog>(this);  // +
  dlg->exec();
}

void MainWindow::checkUpdate() {
  QThread* th = new QThread;
  UpdateChecker* cheker = new UpdateChecker;
  cheker->moveToThread(th);
  VERIFY(connect(th, &QThread::started, cheker, &UpdateChecker::routine));
  VERIFY(connect(cheker, &UpdateChecker::versionAvailibled, this, &MainWindow::versionAvailible));
  VERIFY(connect(cheker, &UpdateChecker::versionAvailibled, th, &QThread::quit));
  VERIFY(connect(th, &QThread::finished, cheker, &UpdateChecker::deleteLater));
  VERIFY(connect(th, &QThread::finished, th, &QThread::deleteLater));
  th->start();
}

void MainWindow::sendStatistic() {
  QThread* th = new QThread;
#if defined(PRO_VERSION) || defined(ENTERPRISE_VERSION)
  const proxy::UserInfo uinf = proxy::SettingsManager::GetInstance()->GetUserInfo();

  const std::string login = uinf.GetLogin();
  const std::string build_strategy = common::ConvertToString(uinf.GetBuildStrategy());
  StatisticSender* sender = new StatisticSender(login, build_strategy);
#else
  AnonymousStatisticSender* sender = new AnonymousStatisticSender;
#endif
  sender->moveToThread(th);
  VERIFY(connect(th, &QThread::started, sender, &AnonymousStatisticSender::routine));
  VERIFY(connect(sender, &AnonymousStatisticSender::statisticSended, this, &MainWindow::statitsticSent));
  VERIFY(connect(sender, &AnonymousStatisticSender::statisticSended, th, &QThread::quit));
  VERIFY(connect(th, &QThread::finished, sender, &AnonymousStatisticSender::deleteLater));
  VERIFY(connect(th, &QThread::finished, th, &QThread::deleteLater));
  th->start();
}

void MainWindow::reportBug() {
  QDesktopServices::openUrl(QUrl(PROJECT_GITHUB_ISSUES));
}

void MainWindow::enterLeaveFullScreen() {
  if (isFullScreen()) {
    showNormal();
  } else {
    showFullScreen();
  }
}

void MainWindow::openEncodeDecodeDialog() {
  auto dlg = createDialog<EncodeDecodeDialog>(translations::trEncodeDecode,
                                              GuiFactory::GetInstance().encodeDecodeIcon(), this);  // +
  dlg->exec();
}

void MainWindow::openRecentConnection() {
  QAction* action = qobject_cast<QAction*>(sender());
  if (!action) {
    return;
  }

  const QString rcon = action->text();
  const std::string srcon = common::ConvertToString(rcon);
  const proxy::connection_path_t path(srcon);
  auto conns = proxy::SettingsManager::GetInstance()->GetConnections();
  for (auto it = conns.begin(); it != conns.end(); ++it) {
    proxy::IConnectionSettingsBaseSPtr con = *it;
    if (con && con->GetPath() == path) {
      createServer(con);
      return;
    }
  }
}

void MainWindow::loadConnection() {
  QString standardIni;
  common::ConvertFromString(proxy::SettingsManager::GetSettingsFilePath(), &standardIni);
  QString filepathR =
      QFileDialog::getOpenFileName(this, tr("Select settings file"), standardIni, tr("Settings files (*.ini)"));
  if (filepathR.isNull()) {
    return;
  }

  proxy::SettingsManager::GetInstance()->ReloadFromPath(common::ConvertToString(filepathR), false);
  QMessageBox::information(this, translations::trInfo, trSettingsLoadedS);
}

void MainWindow::importConnection() {
  typedef common::file_system::FileGuard<common::file_system::ANSIFile> FileImport;
  std::string dir_path = proxy::SettingsManager::GetSettingsDirPath();
  QString qdir_path;
  common::ConvertFromString(dir_path, &qdir_path);
  QString filepathR = QFileDialog::getOpenFileName(this, tr("Select encrypted settings file"), qdir_path,
                                                   tr("Encrypted settings files (*.cini)"));
  if (filepathR.isNull()) {
    return;
  }

  std::string tmp = proxy::SettingsManager::GetSettingsFilePath() + ".tmp";

  common::file_system::ascii_string_path wp(tmp);
  FileImport write_file;
  common::ErrnoError err = write_file.Open(wp, "wb");
  if (err) {
    QMessageBox::critical(this, translations::trError, trImportSettingsFailed);
    return;
  }

  common::file_system::ascii_string_path rp(common::ConvertToString(filepathR));
  FileImport read_file;
  err = read_file.Open(rp, "rb");
  if (err) {
    err = common::file_system::remove_file(wp.GetPath());
    if (err) {
      DNOTREACHED();
    }
    QMessageBox::critical(this, translations::trError, trImportSettingsFailed);
    return;
  }

  common::IEDcoder* hexEnc = common::CreateEDCoder(common::ED_HEX);
  if (!hexEnc) {
    err = common::file_system::remove_file(wp.GetPath());
    if (err) {
      DNOTREACHED();
    }
    QMessageBox::critical(this, translations::trError, trImportSettingsFailed);
    return;
  }

  while (!read_file.IsEOF()) {
    std::string data;
    bool res = read_file.Read(&data, 1024);
    if (!res) {
      break;
    }

    if (data.empty()) {
      continue;
    }

    common::char_buffer_t edata;
    common::Error err = hexEnc->Decode(data, &edata);
    if (err) {
      common::ErrnoError err = common::file_system::remove_file(wp.GetPath());
      if (err) {
        DNOTREACHED();
      }
      QMessageBox::critical(this, translations::trError, trImportSettingsFailed);
      return;
    } else {
      write_file.Write(edata);
    }
  }

  proxy::SettingsManager::GetInstance()->ReloadFromPath(tmp, false);
  err = common::file_system::remove_file(tmp);
  if (err) {
    DNOTREACHED();
  }
  QMessageBox::information(this, translations::trInfo, trSettingsImportedS);
}

void MainWindow::exportConnection() {
  typedef common::file_system::FileGuard<common::file_system::ANSIFile> FileImport;
  std::string dir_path = proxy::SettingsManager::GetSettingsDirPath();
  QString qdir;
  common::ConvertFromString(dir_path, &qdir);
  QString filepathW = showSaveFileDialog(this, tr("Select file to save settings"), qdir, tr("Settings files (*.cini)"));
  if (filepathW.isEmpty()) {
    return;
  }

  common::file_system::ascii_string_path wp(common::ConvertToString(filepathW));
  FileImport write_file;
  common::ErrnoError err = write_file.Open(wp, "wb");
  if (err) {
    QMessageBox::critical(this, translations::trError, trExportSettingsFailed);
    return;
  }

  common::file_system::ascii_string_path rp(proxy::SettingsManager::GetSettingsFilePath());
  FileImport read_file;
  err = read_file.Open(rp, "rb");
  if (err) {
    common::ErrnoError err = common::file_system::remove_file(wp.GetPath());
    if (err) {
      DNOTREACHED();
    }
    QMessageBox::critical(this, translations::trError, trExportSettingsFailed);
    return;
  }

  common::IEDcoder* hexEnc = common::CreateEDCoder(common::ED_HEX);
  if (!hexEnc) {
    common::ErrnoError err = common::file_system::remove_file(wp.GetPath());
    if (err) {
      DNOTREACHED();
    }
    QMessageBox::critical(this, translations::trError, trExportSettingsFailed);
    return;
  }

  while (!read_file.IsEOF()) {
    std::string data;
    bool res = read_file.Read(&data, 512);
    if (!res) {
      break;
    }

    if (data.empty()) {
      continue;
    }

    common::char_buffer_t edata;
    common::Error err = hexEnc->Encode(data, &edata);
    if (err) {
      common::ErrnoError err = common::file_system::remove_file(wp.GetPath());
      if (err) {
        DNOTREACHED();
      }
      QMessageBox::critical(this, translations::trError, trExportSettingsFailed);
      return;
    } else {
      write_file.Write(edata);
    }
  }

  QMessageBox::information(this, translations::trInfo, trSettingsExportedS);
}

void MainWindow::versionAvailible(common::Error err, unsigned version) {
  if (err) {
    QString qerror;
    common::ConvertFromString(err->GetDescription(), &qerror);
    QMessageBox::information(this, translations::trCheckVersion, qerror);
    check_update_action_->setEnabled(true);
    return;
  }

  bool is_need_update = IsNeedUpdate(version);
  if (is_need_update) {
    std::string version_str = common::ConvertVersionNumberTo2DotString(version);
    QString qversion_str;
    common::ConvertFromString(version_str, &qversion_str);
#if defined(PRO_VERSION) || defined(ENTERPRISE_VERSION)
    QMessageBox::information(
        this, translations::trCheckVersion,
        QObject::tr("<h4>A new version(%1) of " PROJECT_NAME_TITLE " is availible!</h4>"
                    "You can download it in your <a href=\"" PROJECT_DOWNLOAD_LINK "\">profile page</a>")
            .arg(qversion_str));
#else
    QMessageBox::information(this, translations::trCheckVersion,
                             QObject::tr("<h4>A new version(%1) of " PROJECT_NAME_TITLE " is availible!</h4>"
                                         "You can download it in our <a href=\"" PROJECT_DOWNLOAD_LINK "\">website</a>")
                                 .arg(qversion_str));
#endif
  }

  check_update_action_->setEnabled(is_need_update);
}

void MainWindow::statitsticSent(common::Error err) {
  UNUSED(err);
}

void MainWindow::closeServer(proxy::IServerSPtr server) {
  proxy::ServersManager::GetInstance().CloseServer(server);
}

#if defined(PRO_VERSION) || defined(ENTERPRISE_VERSION)
void MainWindow::closeSentinel(proxy::ISentinelSPtr sentinel) {
  proxy::ServersManager::GetInstance().CloseSentinel(sentinel);
}

void MainWindow::closeCluster(proxy::IClusterSPtr cluster) {
  proxy::ServersManager::GetInstance().CloseCluster(cluster);
}
#endif

#if defined(OS_ANDROID)
bool MainWindow::event(QEvent* event) {
  if (event->type() == QEvent::Gesture) {
    QGestureEvent* gest = static_cast<QGestureEvent*>(event);
    if (gest) {
      return gestureEvent(gest);
    }
  }
  return QMainWindow::event(event);
}

bool MainWindow::gestureEvent(QGestureEvent* event) {
  /*if (QGesture* qpan = event->gesture(Qt::PanGesture)){
    QPanGesture* pan = static_cast<QPanGesture*>(qpan);
  }
  if (QGesture* qpinch = event->gesture(Qt::PinchGesture)){
    QPinchGesture* pinch =
  static_cast<QPinchGesture*>(qpinch);
  }
  if (QGesture* qtap = event->gesture(Qt::TapGesture)){
    QTapGesture* tap = static_cast<QTapGesture*>(qtap);
  }*/

  if (QGesture* qswipe = event->gesture(Qt::SwipeGesture)) {
    QSwipeGesture* swipe = static_cast<QSwipeGesture*>(qswipe);
    swipeTriggered(swipe);
  } else if (QGesture* qtapandhold = event->gesture(Qt::TapAndHoldGesture)) {
    QTapAndHoldGesture* tapandhold = static_cast<QTapAndHoldGesture*>(qtapandhold);
    tapAndHoldTriggered(tapandhold);
    event->accept();
  }

  return true;
}

void MainWindow::swipeTriggered(QSwipeGesture* swipeEvent) {}

void MainWindow::tapAndHoldTriggered(QTapAndHoldGesture* tapEvent) {
  QPoint pos = tapEvent->position().toPoint();
  QContextMenuEvent* cont = new QContextMenuEvent(QContextMenuEvent::Mouse, pos, mapToGlobal(pos));
  QWidget* rec = childAt(pos);
  QApplication::postEvent(rec, cont);
}
#endif

void MainWindow::createStatusBar() {}

void MainWindow::retranslateUi() {
  connect_action_->setText(translations::trConnect + "...");
  load_from_file_action_->setText(translations::trLoadSettingsFromFile + "...");
  import_action_->setText(translations::trImportSettings);
  export_action_->setText(translations::trExportSettings);
  exit_action_->setText(translations::trExit);
  file_action_->setText(translations::trFile);
  tools_action_->setText(translations::trTools);
  encode_decode_dialog_action_->setText(translations::trEncodeDecode);
  preferences_action_->setText(translations::trPreferences);
  check_update_action_->setText(translations::trCheckUpdate + "...");
  edit_action_->setText(translations::trEdit);
  window_action_->setText(translations::trWindow);
  full_screan_action_->setText(translations::trFullScreen);
  report_bug_action_->setText(translations::trReportBug + "...");
  about_action_->setText(translations::trAbout + QString(" " PROJECT_NAME_TITLE "..."));
  howtouse_action_->setText(translations::trHowToUse + "...");
  help_action_->setText(translations::trHelp);
  explorer_action_->setText(translations::trExpTree);
  logs_action_->setText(translations::trLogs);
  recent_connections_->setText(translations::trRecentConnections);
  clear_menu_->setText(translations::trClearMenu);

  exp_dock_->setWindowTitle(translations::trExpTree);
  log_dock_->setWindowTitle(translations::trLogs);
}

void MainWindow::updateRecentConnectionActions() {
  QStringList connections = proxy::SettingsManager::GetInstance()->GetRecentConnections();

  int num_recent_files = std::min(connections.size(), static_cast<int>(max_recent_connections));
  for (int i = 0; i < num_recent_files; ++i) {
    QString text = connections[i];
    recent_connections_acts_[i]->setText(text);
    recent_connections_acts_[i]->setVisible(true);
  }

  for (int j = num_recent_files; j < max_recent_connections; ++j) {
    recent_connections_acts_[j]->setVisible(false);
  }

  bool have_items = num_recent_files > 0;
  clear_menu_->setVisible(have_items);
  recent_connections_->setEnabled(have_items);
}

void MainWindow::clearRecentConnectionsMenu() {
  proxy::SettingsManager::GetInstance()->ClearRConnections();
  updateRecentConnectionActions();
}

void MainWindow::createServer(proxy::IConnectionSettingsBaseSPtr settings) {
  if (!settings) {
    return;
  }

  const std::string path = settings->GetPath().ToString();
  QString qpath;
  common::ConvertFromString(path, &qpath);
  proxy::SettingsManager::GetInstance()->RemoveRecentConnection(qpath);
  auto server = proxy::ServersManager::GetInstance().CreateServer(settings);
  if (!server) {
    return;
  }

  exp_->addServer(server);
  proxy::SettingsManager::GetInstance()->AddRecentConnection(qpath);
  updateRecentConnectionActions();
  if (proxy::SettingsManager::GetInstance()->GetAutoConnectDB()) {
    proxy::events_info::ConnectInfoRequest req(this);
    server->Connect(req);
  }

  if (!proxy::SettingsManager::GetInstance()->AutoOpenConsole()) {
    return;
  }

  MainWidget* mwidg = qobject_cast<MainWidget*>(centralWidget());
  if (mwidg) {
    mwidg->openConsole(server, QString());
  }
}

#if defined(PRO_VERSION) || defined(ENTERPRISE_VERSION)
void MainWindow::createSentinel(proxy::ISentinelSettingsBaseSPtr settings) {
  if (!settings) {
    return;
  }

  proxy::ISentinelSPtr sent = proxy::ServersManager::GetInstance().CreateSentinel(settings);
  if (!sent) {
    return;
  }

  auto sentinels = sent->GetSentinels();
  if (sentinels.empty()) {
    return;
  }

  exp_->addSentinel(sent);
  if (!proxy::SettingsManager::GetInstance()->AutoOpenConsole()) {
    return;
  }

  MainWidget* mwidg = qobject_cast<MainWidget*>(centralWidget());
  if (mwidg) {
    for (size_t i = 0; i < sentinels.size(); ++i) {
      proxy::IServerSPtr serv = sentinels[i].sentinel;
      mwidg->openConsole(serv, QString());
    }
  }
}

void MainWindow::createCluster(proxy::IClusterSettingsBaseSPtr settings) {
  if (!settings) {
    return;
  }

  proxy::IClusterSPtr cl = proxy::ServersManager::GetInstance().CreateCluster(settings);
  if (!cl) {
    return;
  }

  exp_->addCluster(cl);
  if (!proxy::SettingsManager::GetInstance()->AutoOpenConsole()) {
    return;
  }

  proxy::IServerSPtr root = cl->GetRoot();
  if (!root) {
    return;
  }

  MainWidget* mwidg = qobject_cast<MainWidget*>(centralWidget());
  if (mwidg) {
    mwidg->openConsole(root, QString());
  }
}
#endif

}  // namespace gui
}  // namespace fastonosql
