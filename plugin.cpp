#include <QQmlEngineExtensionPlugin>

class MdkDeclarativeWrapper : public QQmlEngineExtensionPlugin {
    Q_OBJECT
    Q_PLUGIN_METADATA(IID QQmlEngineExtensionInterface_iid)
};

#include "plugin.moc"
