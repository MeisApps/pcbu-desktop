#include "DevicesTableModel.h"

#include "storage/PairedDevicesStorage.h"

DevicesTableModel::DevicesTableModel(QObject *parent) : QAbstractTableModel(parent) {
  for(const auto &device : PairedDevicesStorage::GetDevices()) {
    auto pairingId = QString::fromUtf8(device.id);
    auto deviceName = QString::fromUtf8(device.deviceName);
    auto userName = QString::fromUtf8(device.userName);
    auto method = QString::fromUtf8(PairingMethodUtils::ToString(device.pairingMethod));
    m_TableData.append({pairingId, deviceName, userName, method});
  }
}

Q_INVOKABLE QVector<QString> DevicesTableModel::get(int rowIdx) {
  if(rowIdx >= m_TableData.at(0).size())
    return {};
  return m_TableData.at(rowIdx);
}

int DevicesTableModel::rowCount(const QModelIndex &) const {
  return (int)m_TableData.size();
}

int DevicesTableModel::columnCount(const QModelIndex &) const {
  return !m_TableData.empty() ? (int)m_TableData.at(0).size() - 1 : 0;
}

QVariant DevicesTableModel::data(const QModelIndex &index, int role) const {
  if(role == TableDataRole)
    return m_TableData.at(index.row()).at(index.column() + 1);
  if(role == PairingIdRole)
    return m_TableData.at(index.row()).at(0);
  return {};
}

QHash<int, QByteArray> DevicesTableModel::roleNames() const {
  QHash<int, QByteArray> roles{};
  roles[TableDataRole] = "tableData";
  roles[PairingIdRole] = "pairingId";
  return roles;
}
