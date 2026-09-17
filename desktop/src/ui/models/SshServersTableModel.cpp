#include "SshServersTableModel.h"

#include "storage/PairedDevicesStorage.h"

SshServersTableModel::SshServersTableModel(QObject *parent) : QAbstractTableModel(parent) {}

void SshServersTableModel::LoadData() {
  m_TableData.clear();
  auto device = PairedDevicesStorage::GetDeviceByID(m_DeviceId.toStdString());
  if(!device.has_value())
    return;
  auto display = [](const std::string &value) { return value.empty() ? QString("-") : QString::fromUtf8(value); };
  for(size_t i = 0; i < device->sshServers.size(); i++) {
    const auto &server = device->sshServers[i];
    m_TableData.append({QString::fromUtf8(device->id), QString::number(i), display(server.host), display(server.user), display(server.keyPath)});
  }
}

QVector<QString> SshServersTableModel::get(int rowIdx) {
  if(rowIdx < 0 || rowIdx >= m_TableData.size())
    return {};
  return m_TableData.at(rowIdx);
}

void SshServersTableModel::reload(const QString &deviceId) {
  if(!deviceId.isEmpty())
    m_DeviceId = deviceId;
  beginResetModel();
  LoadData();
  endResetModel();
}

int SshServersTableModel::rowCount(const QModelIndex &) const {
  return (int)m_TableData.size();
}

int SshServersTableModel::columnCount(const QModelIndex &) const {
  return 3;
}

QVariant SshServersTableModel::data(const QModelIndex &index, int role) const {
  if(role == TableDataRole)
    return m_TableData.at(index.row()).at(index.column() + 2);
  return {};
}

QHash<int, QByteArray> SshServersTableModel::roleNames() const {
  QHash<int, QByteArray> roles{};
  roles[TableDataRole] = "tableData";
  return roles;
}
