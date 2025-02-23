#ifndef PCBU_DESKTOP_NETWORKLISTMODEL_H
#define PCBU_DESKTOP_NETWORKLISTMODEL_H

#include <QAbstractListModel>
#include <QtQmlIntegration>

struct NetworkListItem {
  Q_GADGET
public:
  QString ifName{};
  QString ipAddress{};
  QString macAddress{};

  Q_PROPERTY(QString ifName MEMBER ifName)
  Q_PROPERTY(QString ipAddress MEMBER ipAddress)
  Q_PROPERTY(QString macAddress MEMBER macAddress)
};

class NetworkListModel : public QAbstractListModel {
  Q_OBJECT
  QML_ELEMENT
  enum NetworkListRoles { InterfaceNameRole = Qt::UserRole + 1, IPAddressRole, MACAddressRole, IfWithIPRole };

public:
  explicit NetworkListModel(QObject *parent = nullptr);

  Q_INVOKABLE NetworkListItem get(int index);
  Q_INVOKABLE qsizetype size();

  [[nodiscard]] QHash<int, QByteArray> roleNames() const override;
  [[nodiscard]] int rowCount(const QModelIndex &parent) const override;
  [[nodiscard]] QVariant data(const QModelIndex &index, int role) const override;

private:
  QVector<NetworkListItem> m_ListData{};
};

#endif // PCBU_DESKTOP_NETWORKLISTMODEL_H
