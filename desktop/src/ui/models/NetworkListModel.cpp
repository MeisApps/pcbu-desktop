#include "NetworkListModel.h"

#include "platform/NetworkHelper.h"

NetworkListModel::NetworkListModel(QObject *parent) : QAbstractListModel(parent) {
  for(const auto &netIf : NetworkHelper::GetLocalNetInterfaces())
    m_ListData.append({QString::fromUtf8(netIf.ifName), QString::fromUtf8(netIf.ipAddress), QString::fromUtf8(netIf.macAddress)});
}

Q_INVOKABLE NetworkListItem NetworkListModel::get(int index) {
  if(index >= m_ListData.size())
    return {};
  return m_ListData.at(index);
}

qsizetype NetworkListModel::size() {
  return (qsizetype)m_ListData.size();
}

QHash<int, QByteArray> NetworkListModel::roleNames() const {
  return {{InterfaceNameRole, "ifName"}, {IPAddressRole, "ipAddress"}, {MACAddressRole, "macAddress"}, {IfWithIPRole, "ifWithIp"}};
}

int NetworkListModel::rowCount(const QModelIndex &parent) const {
  if(parent.isValid())
    return 0;
  return (int)m_ListData.size();
}

QVariant NetworkListModel::data(const QModelIndex &index, int role) const {
  if(!hasIndex(index.row(), index.column(), index.parent()))
    return {};
  auto item = m_ListData.at(index.row());
  if(role == InterfaceNameRole)
    return item.ifName;
  if(role == IPAddressRole)
    return item.ipAddress;
  if(role == MACAddressRole)
    return item.macAddress;
  if(role == IfWithIPRole)
    return QString("%1 (%2)").arg(item.ipAddress, item.ifName);
  return {};
}
