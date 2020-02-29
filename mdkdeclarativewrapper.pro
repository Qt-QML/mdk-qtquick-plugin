TARGET = $$qtLibraryTarget(mdkwrapperplugin)

TEMPLATE = lib

QT += qml quick

CONFIG += \
    plugin qmltypes relative_qt_rpath c++17 strict_c++ \
    utf8_source warn_on rtti_off exceptions_off

DEFINES += QT_NO_CAST_FROM_ASCII QT_NO_CAST_TO_ASCII

QML_IMPORT_NAME = wangwenx190.QuickMdk

import_path = $$replace(QML_IMPORT_NAME, \., /)

QML_IMPORT_MAJOR_VERSION = 1

DESTDIR = imports/$$import_path

QMLTYPES_FILENAME = $$DESTDIR/plugins.qmltypes

isEmpty(MDK_SDK_DIR) {
    error("You have to setup \"MDK_SDK_DIR\" in \"user.conf\" first!")
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

HEADERS += mdkdeclarativeobject.h
SOURCES += mdkdeclarativeobject.cpp plugin.cpp

PLUGINFILES = \
    imports/quickmdk/qmldir \
    imports/quickmdk/MdkPlayer.qml

QMAKE_MOC_OPTIONS += -Muri=$$QML_IMPORT_NAME

static {
    static_plugin_resources.files = $$PLUGINFILES
    static_plugin_resources.prefix = /qt-project.org/imports/$$import_path
    RESOURCES += static_plugin_resources
}

VERSION = 1.0.0.0
win32: shared {
    QMAKE_TARGET_PRODUCT = "MdkDeclarativeWrapper"
    QMAKE_TARGET_DESCRIPTION = "MDK wrapper for Qt Quick"
    QMAKE_TARGET_COPYRIGHT = "GNU Lesser General Public License version 3"
    QMAKE_TARGET_COMPANY = "wangwenx190"
    CONFIG += skip_target_version_ext
}

install_path = $$[QT_INSTALL_QML]/$$import_path

target.path = $$install_path

pluginfiles_copy.files = $$PLUGINFILES
pluginfiles_copy.path = $$DESTDIR

pluginfiles_install.files = $$PLUGINFILES $$OUT_PWD/$$DESTDIR/plugins.qmltypes
pluginfiles_install.path = $$install_path
pluginfiles_install.CONFIG += no_check_exist

INSTALLS += target pluginfiles_install

COPIES += pluginfiles_copy

OTHER_FILES += $$PLUGINFILES

# CONFIG += install_ok  # Do not cargo-cult this!
