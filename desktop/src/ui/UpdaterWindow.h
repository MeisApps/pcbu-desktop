#ifndef PCBU_DESKTOP_UPDATERWINDOW_H
#define PCBU_DESKTOP_UPDATERWINDOW_H

#include <QObject>
#include <QtQmlIntegration>
#include <mutex>
#include <thread>

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
  QString m_LatestVersion{};
  std::mutex m_VersionMutex{};
  std::thread m_CheckThread{};
  std::thread m_DownloadThread{};
};

#endif // PCBU_DESKTOP_UPDATERWINDOW_H
