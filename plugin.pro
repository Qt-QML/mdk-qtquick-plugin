TARGET = $$qtLibraryTarget(mdkwrapperplugin)
QT += quick
CONFIG += c++17 strict_c++ warn_on rtti_off exceptions_off
VERSION = 1.0.0.0
win32: shared {
    QMAKE_TARGET_PRODUCT = "MdkDeclarativeWrapper"
    QMAKE_TARGET_DESCRIPTION = "MDK wrapper for Qt Quick"
    QMAKE_TARGET_COPYRIGHT = "GNU Lesser General Public License version 3"
    QMAKE_TARGET_COMPANY = "wangwenx190"
    CONFIG += skip_target_version_ext
}
isEmpty(MDK_SDK_DIR) {
    error(You have to setup \"MDK_SDK_DIR\" in \".qmake.conf\" first!)
} else {
    MDK_ARCH = x64
    contains(QT_ARCH, x.*64) {
        android: MDK_ARCH = x86_64
        else: MDK_ARCH = x64
    } else: contains(QT_ARCH, .*86) {
        MDK_ARCH = x86
    } else: contains(QT_ARCH, a.*64) {
        android: MDK_ARCH = arm64-v8a
        else: MDK_ARCH = arm64
    } else: contains(QT_ARCH, arm.*) {
        android: MDK_ARCH = armeabi-v7a
        else: MDK_ARCH = arm
    }
    INCLUDEPATH += $$MDK_SDK_DIR/include
    LIBS += -L$$MDK_SDK_DIR/lib/$$MDK_ARCH -lmdk
}
HEADERS += mdkobject.h
SOURCES += mdkobject.cpp plugin.cpp
uri = wangwenx190.QuickMdk
include(qmlplugin.pri)
