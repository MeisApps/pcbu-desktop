#ifndef PCBU_DESKTOP_MAINWINDOW_H
#define PCBU_DESKTOP_MAINWINDOW_H

#include <QObject>
#include <QtQmlIntegration>
#include <QQmlApplicationEngine>

class MainWindow : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
public:
    ~MainWindow() override;

    Q_INVOKABLE bool IsInstalled();
    Q_INVOKABLE bool IsPaired();
    Q_INVOKABLE QString GetInstalledVersion();
    Q_INVOKABLE QString GetLicenseText();

    Q_INVOKABLE bool PerformStartupChecks(QObject *viewLoader, QObject *window);

public slots:
    void Show(QObject *viewLoader);
    void OnInstallClicked(QObject *window);
    void OnReinstallClicked(QObject *window);

    void OnTestUnlockClicked(QObject *viewLoader, const QString& pairingId);
    void OnRemoveDeviceClicked(QObject *viewLoader, const QString& pairingId);

private:
    std::thread m_LoadingThread{};
};

#endif //PCBU_DESKTOP_MAINWINDOW_H
