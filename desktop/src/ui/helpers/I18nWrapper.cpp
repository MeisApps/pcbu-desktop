#include "I18nWrapper.h"

#include "utils/I18n.h"

QString QI18n::Get(const QString &key, const QVariantList &args) {
  return QString::fromUtf8(I18n::Get(key.toStdString()));
}
