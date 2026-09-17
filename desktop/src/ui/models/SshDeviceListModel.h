#ifndef PCBU_DESKTOP_SSHDEVICELISTMODEL_H
#define PCBU_DESKTOP_SSHDEVICELISTMODEL_H

#include <QAbstractListModel>
#include <QtQmlIntegration>

struct SshDeviceListItem {
  Q_GADGET
public:
  QString deviceId{};
  QString deviceName{};

  Q_PROPERTY(QString deviceId MEMBER deviceId)
  Q_PROPERTY(QString deviceName MEMBER deviceName)
};

class SshDeviceListModel : public QAbstractListModel {
  Q_OBJECT
  QML_ELEMENT
  enum DeviceListRoles { DeviceIdRole = Qt::UserRole + 1, DeviceNameRole };

public:
  explicit SshDeviceListModel(QObject *parent = nullptr);

  Q_INVOKABLE SshDeviceListItem get(int index);
  Q_INVOKABLE qsizetype size();

  [[nodiscard]] QHash<int, QByteArray> roleNames() const override;
  [[nodiscard]] int rowCount(const QModelIndex &parent) const override;
  [[nodiscard]] QVariant data(const QModelIndex &index, int role) const override;

private:
  QVector<SshDeviceListItem> m_ListData{};
};

#endif // PCBU_DESKTOP_SSHDEVICELISTMODEL_H
