#ifndef PCBU_DESKTOP_I18NWRAPPER_H
#define PCBU_DESKTOP_I18NWRAPPER_H

#include <QObject>
#include <QtQmlIntegration>

class QI18n : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
public:
    Q_INVOKABLE QString Get(const QString& key, const QVariantList& args = QVariantList());
};

#endif //PCBU_DESKTOP_I18NWRAPPER_H
