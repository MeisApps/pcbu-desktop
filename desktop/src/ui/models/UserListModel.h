#ifndef PCBU_DESKTOP_USERLISTMODEL_H
#define PCBU_DESKTOP_USERLISTMODEL_H

#include <QAbstractListModel>
#include <QtQmlIntegration>

struct UserListItem {
    Q_GADGET
public:
    QString userName{};
    bool isCurrentUser{};
    bool isMicrosoftAccount{};

    Q_PROPERTY(QString userName MEMBER userName)
    Q_PROPERTY(bool isCurrentUser MEMBER isCurrentUser)
    Q_PROPERTY(bool isMicrosoftAccount MEMBER isMicrosoftAccount)
};

class UserListModel : public QAbstractListModel {
    Q_OBJECT
    QML_ELEMENT
    enum UserListRoles {
        UserNameRole = Qt::UserRole + 1,
        CurrentUserRole,
        MicrosoftRole
    };

public:
    explicit UserListModel(QObject *parent = nullptr);

    Q_INVOKABLE UserListItem get(int index);
    Q_INVOKABLE qsizetype size();

    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;
    [[nodiscard]] int rowCount(const QModelIndex& parent) const override;
    [[nodiscard]] QVariant data(const QModelIndex & index, int role) const override;

private:
    QVector<UserListItem> m_ListData{};
};

#endif //PCBU_DESKTOP_USERLISTMODEL_H
