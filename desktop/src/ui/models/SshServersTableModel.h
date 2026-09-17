#ifndef PCBU_DESKTOP_SSHSERVERSTABLEMODEL_H
#define PCBU_DESKTOP_SSHSERVERSTABLEMODEL_H

#include <QAbstractTableModel>
#include <QtQmlIntegration>

class SshServersTableModel : public QAbstractTableModel {
  Q_OBJECT
  QML_ELEMENT
  enum TableRoles { TableDataRole = Qt::UserRole + 1 };

public:
  explicit SshServersTableModel(QObject *parent = nullptr);

  Q_INVOKABLE QVector<QString> get(int rowIdx);
  Q_INVOKABLE void reload(const QString &deviceId = {});

  [[nodiscard]] int rowCount(const QModelIndex &) const override;
  [[nodiscard]] int columnCount(const QModelIndex &) const override;
  [[nodiscard]] QVariant data(const QModelIndex &index, int role) const override;
  [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

private:
  void LoadData();

  QString m_DeviceId{};
  QVector<QVector<QString>> m_TableData{};
};

#endif // PCBU_DESKTOP_SSHSERVERSTABLEMODEL_H
