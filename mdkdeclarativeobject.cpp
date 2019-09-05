#include "mdkdeclarativeobject.h"
#include <QOpenGLFramebufferObject>

class MdkRenderer : public QQuickFramebufferObject::Renderer {
public:
    MdkRenderer(MdkDeclarativeObject *mdkdeclarativeobject) : mdkdeclarativeobject(mdkdeclarativeobject) {
        Q_ASSERT(this->mdkdeclarativeobject != nullptr);
    }

    void render() override { mdkdeclarativeobject->renderVideo(); }

    QOpenGLFramebufferObject *
    createFramebufferObject(const QSize &size) override {
        mdkdeclarativeobject->setVideoSurfaceSize(size.width(), size.height());
        return new QOpenGLFramebufferObject(size);
    }

private:
    MdkDeclarativeObject *mdkdeclarativeobject = nullptr;
};

MdkDeclarativeObject::MdkDeclarativeObject(QQuickItem *parent)
    : QQuickFramebufferObject(parent), player(std::make_unique<mdk::Player>()) {
    Q_ASSERT(player != nullptr);
    player->setVideoDecoders({"MFT:d3d=11", "MFT:d3d=9", "MFT", "D3D11", "DXVA",
                              "CUDA", "NVDEC", "FFmpeg"});
    player->setRenderCallback(
        [=](void *) { QMetaObject::invokeMethod(this, "update"); });
    // MUST set before setMedia() because setNextMedia() is called when media is
    // changed
    player->setPreloadImmediately(false);
}

QQuickFramebufferObject::Renderer *MdkDeclarativeObject::createRenderer() const {
    return new MdkRenderer(const_cast<MdkDeclarativeObject *>(this));
}

QUrl MdkDeclarativeObject::source() const { return m_source; }

void MdkDeclarativeObject::setSource(const QUrl &url) {
    if (!url.isValid() || (url == m_source)) {
        return;
    }
    player->setNextMedia(nullptr);
    player->setState(mdk::PlaybackState::Stopped);
    player->waitFor(mdk::PlaybackState::Stopped);
    // 1st url may be the same as current url
    player->setMedia(nullptr);
    player->setMedia(url.isLocalFile() ? qPrintable(url.toLocalFile())
                                       : qPrintable(url.url()));
    player->scale(1.0f, -1.0f);
    player->setState(mdk::PlaybackState::Playing);
    m_source = url;
    Q_EMIT sourceChanged();
}

void MdkDeclarativeObject::play() {
    player->setState(mdk::PlaybackState::Playing);
    // player->scale(1.0f, -1.0f);
}

void MdkDeclarativeObject::setVideoSurfaceSize(int width, int height) {
    player->setVideoSurfaceSize(width, height);
}

void MdkDeclarativeObject::renderVideo() { player->renderVideo(); }
