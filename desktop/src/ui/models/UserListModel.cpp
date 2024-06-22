#include "UserListModel.h"

#include "platform/PlatformHelper.h"

UserListModel::UserListModel(QObject *parent)
    : QAbstractListModel(parent) {
    auto users = PlatformHelper::GetAllUsers();
    auto currUser = PlatformHelper::GetCurrentUser();
    for(const auto& user : users)
        m_ListData.append({QString::fromUtf8(user), user == currUser, user.starts_with("MicrosoftAccount")});
}

Q_INVOKABLE UserListItem UserListModel::get(int index) {
    if (index >= m_ListData.size())
        return {};
    return m_ListData.at(index);
}

Q_INVOKABLE qsizetype UserListModel::size() {
    return m_ListData.size();
}

QHash<int, QByteArray> UserListModel::roleNames() const {
    return {{UserNameRole, "userName"},
            {CurrentUserRole, "isCurrentUser"},
            {MicrosoftRole, "isMicrosoftAccount"}};
}

int UserListModel::rowCount(const QModelIndex& parent) const {
    if (parent.isValid())
        return 0;
    return (int)m_ListData.size();
}

QVariant UserListModel::data(const QModelIndex & index, int role) const {
    if (!hasIndex(index.row(), index.column(), index.parent()))
        return {};
    auto item = m_ListData.at(index.row());
    if (role == UserNameRole) return item.userName;
    if (role == CurrentUserRole) return item.isCurrentUser;
    if (role == MicrosoftRole) return item.isMicrosoftAccount;
    return {};
}
