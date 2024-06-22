#ifndef PCBU_DESKTOP_DEVICESTABLEMODEL_H
#define PCBU_DESKTOP_DEVICESTABLEMODEL_H

#include <QAbstractTableModel>
#include <QtQmlIntegration>

class DevicesTableModel : public QAbstractTableModel {
    Q_OBJECT
    QML_ELEMENT
    enum TableRoles {
        TableDataRole = Qt::UserRole + 1,
        PairingIdRole
    };
public:
    explicit DevicesTableModel(QObject *parent = nullptr);

    Q_INVOKABLE QVector<QString> get(int rowIdx);

    [[nodiscard]] int rowCount(const QModelIndex&) const override;
    [[nodiscard]] int columnCount(const QModelIndex&) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

private:
    QVector<QVector<QString>> m_TableData{};
};

#endif //PCBU_DESKTOP_DEVICESTABLEMODEL_H
