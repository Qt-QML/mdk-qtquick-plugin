#pragma once

#ifndef _MDKDECLARATIVEOBJECT_H
#define _MDKDECLARATIVEOBJECT_H

#include <QQuickFramebufferObject>
#include <QUrl>
#include <mdk/Player.h>
#include <memory>

class MdkDeclarativeObject : public QQuickFramebufferObject {
    Q_OBJECT

    Q_PROPERTY(QUrl source READ source WRITE setSource NOTIFY sourceChanged)

public:
    explicit MdkDeclarativeObject(QQuickItem *parent = nullptr);

    Renderer *createRenderer() const override;

    Q_INVOKABLE void renderVideo();
    Q_INVOKABLE void setVideoSurfaceSize(int width, int height);

    QUrl source() const;
    void setSource(const QUrl &url);

    Q_INVOKABLE void play();

Q_SIGNALS:
    void sourceChanged();

private:
    QUrl m_source = QUrl();
    std::unique_ptr<mdk::Player> player;
};

#endif
