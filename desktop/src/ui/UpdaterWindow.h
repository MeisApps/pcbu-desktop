#ifndef PCBU_DESKTOP_UPDATERWINDOW_H
#define PCBU_DESKTOP_UPDATERWINDOW_H

#include <QObject>
#include <QtQmlIntegration>
#include <boost/filesystem.hpp>

class UpdaterWindow : public QObject {
  Q_OBJECT
  QML_ELEMENT
  QML_SINGLETON
public:
  ~UpdaterWindow() override;
  Q_INVOKABLE QString GetAppVersion();
  Q_INVOKABLE QString GetLatestVersion();
  Q_INVOKABLE void CheckForUpdates(QObject *window);

public slots:
  void OnDownloadClicked(QObject *window);

private:
  static std::string GetDownloadURL();
#ifndef WINDOWS
  static std::string InstallUpdate(const boost::filesystem::path &downloadPath);
#endif
#ifdef APPLE
  static boost::filesystem::path GetAppBundlePath();
#endif

  QString m_LatestVersion{};
  std::mutex m_VersionMutex{};
  std::thread m_CheckThread{};
  std::thread m_DownloadThread{};
};

#endif // PCBU_DESKTOP_UPDATERWINDOW_H
