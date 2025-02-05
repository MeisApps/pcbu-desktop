#include "I18nWrapper.h"

#include "utils/QtI18n.h"

QString QI18n::Get(const QString &key, const QVariantList& args) {
    return QString::fromUtf8(QtI18n::Get(key.toStdString()));
}
