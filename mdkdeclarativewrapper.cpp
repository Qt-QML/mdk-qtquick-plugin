#include "mdkdeclarativewrapper.h"
#include "mdkdeclarativeobject.h"

void MdkDeclarativeWrapper::registerTypes(const char *uri) {
    Q_ASSERT(uri == "wangwenx190.QuickMdk");
    qmlRegisterType<MdkDeclarativeObject>(uri, 1, 0, "MdkObject");
}
