#include "SshDeviceListModel.h"

#include "platform/PlatformHelper.h"
#include "storage/PairedDevicesStorage.h"

SshDeviceListModel::SshDeviceListModel(QObject *parent) : QAbstractListModel(parent) {
  for(const auto &device : PairedDevicesStorage::GetDevicesForUser(PlatformHelper::GetCurrentUser()))
    m_ListData.append({QString::fromUtf8(device.id), QString::fromUtf8(device.deviceName)});
}

SshDeviceListItem SshDeviceListModel::get(int index) {
  if(index < 0 || index >= m_ListData.size())
    return {};
  return m_ListData.at(index);
}

qsizetype SshDeviceListModel::size() {
  return m_ListData.size();
}

QHash<int, QByteArray> SshDeviceListModel::roleNames() const {
  return {{DeviceIdRole, "deviceId"}, {DeviceNameRole, "deviceName"}};
}

int SshDeviceListModel::rowCount(const QModelIndex &parent) const {
  if(parent.isValid())
    return 0;
  return (int)m_ListData.size();
}

QVariant SshDeviceListModel::data(const QModelIndex &index, int role) const {
  if(!hasIndex(index.row(), index.column(), index.parent()))
    return {};
  auto item = m_ListData.at(index.row());
  if(role == DeviceIdRole)
    return item.deviceId;
  if(role == DeviceNameRole)
    return item.deviceName;
  return {};
}
