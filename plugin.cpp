#include <QQmlEngineExtensionPlugin>

extern void qml_register_types_wangwenx190_QuickMdk();

class MdkDeclarativeWrapper : public QQmlEngineExtensionPlugin {
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(MdkDeclarativeWrapper)
    Q_PLUGIN_METADATA(IID QQmlEngineExtensionInterface_iid)

public:
    explicit MdkDeclarativeWrapper(QObject *parent = nullptr)
        : QQmlEngineExtensionPlugin(parent) {
        volatile auto registration = &qml_register_types_wangwenx190_QuickMdk;
        Q_UNUSED(registration)
    }
    ~MdkDeclarativeWrapper() override = default;
};

#include "plugin.moc"
