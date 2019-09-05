#pragma once

#ifndef _MDKDECLARATIVEWRAPPER_H
#define _MDKDECLARATIVEWRAPPER_H

#include <QQmlExtensionPlugin>

class MdkDeclarativeWrapper : public QQmlExtensionPlugin {
    Q_OBJECT
    Q_PLUGIN_METADATA(IID QQmlExtensionInterface_iid)

public:
    void registerTypes(const char *uri) override;
};

#endif
