#include "mdkdeclarativeobject.h"
#include <QDebug>
#include <QOpenGLFramebufferObject>

class MdkRenderer : public QQuickFramebufferObject::Renderer {
public:
    MdkRenderer(MdkDeclarativeObject *mdkdeclarativeobject)
        : mdkdeclarativeobject(mdkdeclarativeobject) {
        Q_ASSERT(this->mdkdeclarativeobject != nullptr);
    }

    void render() override { mdkdeclarativeobject->renderVideo(); }

    QOpenGLFramebufferObject *
    createFramebufferObject(const QSize &size) override {
        mdkdeclarativeobject->setVideoSurfaceSize(size.width(), size.height());
        QMetaObject::invokeMethod(mdkdeclarativeobject, "initFinished");
        return new QOpenGLFramebufferObject(size);
    }

private:
    MdkDeclarativeObject *mdkdeclarativeobject = nullptr;
};

MdkDeclarativeObject::MdkDeclarativeObject(QQuickItem *parent)
    : QQuickFramebufferObject(parent), player(std::make_unique<mdk::Player>()) {
    Q_ASSERT(player != nullptr);
#ifdef Q_OS_WIN
    player->setVideoDecoders({"MFT:d3d=11", "MFT:d3d=9", "MFT", "D3D11", "DXVA",
                              "CUDA", "NVDEC", "FFmpeg"});
#elif defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
    player->setVideoDecoders({"VAAPI", "VDPAU", "CUDA", "NVDEC", "FFmpeg"});
#elif defined(Q_OS_MACOS)
    player->setVideoDecoders({"VT", "VideoToolbox", "FFmpeg"});
#endif
    player->setRenderCallback(
        [this](void *) { QMetaObject::invokeMethod(this, "update"); });
    // MUST set before setMedia() because setNextMedia() is called when media is
    // changed
    player->setPreloadImmediately(false);
    processMdkEvents();
}

QQuickFramebufferObject::Renderer *
MdkDeclarativeObject::createRenderer() const {
    return new MdkRenderer(const_cast<MdkDeclarativeObject *>(this));
}

QUrl MdkDeclarativeObject::source() const {
    return isStopped() ? QUrl() : m_source;
}

void MdkDeclarativeObject::setSource(const QUrl &value) {
    if (!value.isValid() || (value == m_source)) {
        return;
    }
    player->setNextMedia(nullptr);
    player->setState(mdk::PlaybackState::Stopped);
    player->waitFor(mdk::PlaybackState::Stopped);
    // 1st url may be the same as current url
    player->setMedia(nullptr);
    player->setMedia(value.isLocalFile() ? qPrintable(value.toLocalFile())
                                         : qPrintable(value.url()));
    player->scale(1.0f, -1.0f);
    player->setState(mdk::PlaybackState::Playing);
    m_source = value;
    Q_EMIT sourceChanged();
}

qint64 MdkDeclarativeObject::position() const {
    return isStopped() ? 0
                       : qMin(qMax(player->position(), qint64(0)), duration());
}

void MdkDeclarativeObject::setPosition(qint64 value) {
    if (isStopped() || (value == position())) {
        return;
    }
    seek(qMin(qMax(value, qint64(0)), duration()));
}

qint64 MdkDeclarativeObject::duration() const {
    return isStopped() ? 0 : qMax(player->mediaInfo().duration, qint64(0));
}

QSize MdkDeclarativeObject::videoSize() const {
    return isStopped()
        ? QSize(0, 0)
        : QSize(qMax(player->mediaInfo().video.at(0).codec.width, 0),
                qMax(player->mediaInfo().video.at(0).codec.height, 0));
}

float MdkDeclarativeObject::volume() const {
    return qMin(qMax(m_volume, 0.0f), 1.0f);
}

void MdkDeclarativeObject::setVolume(float value) {
    if (value == volume()) {
        return;
    }
    player->setVolume(qMin(qMax(value, 0.0f), 1.0f));
    m_volume = value;
    Q_EMIT volumeChanged();
}

bool MdkDeclarativeObject::mute() const { return m_mute; }

void MdkDeclarativeObject::setMute(bool value) {
    if (value == mute()) {
        return;
    }
    player->setMute(value);
    m_mute = value;
    Q_EMIT muteChanged();
}

bool MdkDeclarativeObject::seekable() const { return true; }

MdkDeclarativeObject::PlaybackState
MdkDeclarativeObject::playbackState() const {
    switch (player->state()) {
    case mdk::PlaybackState::Playing:
        return PlaybackState::PlayingState;
        break;
    case mdk::PlaybackState::Paused:
        return PlaybackState::PausedState;
        break;
    case mdk::PlaybackState::Stopped:
        return PlaybackState::StoppedState;
        break;
    }
    return PlaybackState::StoppedState;
}

void MdkDeclarativeObject::setPlaybackState(
    MdkDeclarativeObject::PlaybackState value) {
    if (isStopped() || (value == playbackState())) {
        return;
    }
    switch (value) {
    case PlaybackState::PlayingState:
        player->setState(mdk::PlaybackState::Playing);
        break;
    case PlaybackState::PausedState:
        player->setState(mdk::PlaybackState::Paused);
        break;
    case PlaybackState::StoppedState:
        player->setState(mdk::PlaybackState::Stopped);
        break;
    }
}

MdkDeclarativeObject::MediaStatus MdkDeclarativeObject::mediaStatus() const {
    switch (player->mediaStatus()) {
    case mdk::MediaStatus::NoMedia:
        return MediaStatus::NoMedia;
        break;
    case mdk::MediaStatus::Loading:
        return MediaStatus::LoadingMedia;
        break;
    case mdk::MediaStatus::Loaded:
        return MediaStatus::LoadedMedia;
        break;
    case mdk::MediaStatus::Stalled:
        return MediaStatus::StalledMedia;
        break;
    case mdk::MediaStatus::Buffering:
        return MediaStatus::BufferingMedia;
        break;
    case mdk::MediaStatus::Buffered:
        return MediaStatus::BufferedMedia;
        break;
    case mdk::MediaStatus::End:
        return MediaStatus::EndOfMedia;
        break;
    case mdk::MediaStatus::Invalid:
        return MediaStatus::InvalidMedia;
        break;
    default:
        return MediaStatus::UnknownMediaStatus;
        break;
    }
}

MdkDeclarativeObject::LogLevel MdkDeclarativeObject::logLevel() const {
    switch (mdk::logLevel()) {
    case mdk::LogLevel::Off:
        return LogLevel::NoLog;
        break;
    case mdk::LogLevel::Debug:
        return LogLevel::DebugLevel;
        break;
    case mdk::LogLevel::Warning:
        return LogLevel::WarningLevel;
        break;
    case mdk::LogLevel::Error:
        return LogLevel::CriticalLevel;
        break;
    case mdk::LogLevel::Info:
        return LogLevel::InfoLevel;
        break;
    default:
        return LogLevel::DebugLevel;
        break;
    }
}

void MdkDeclarativeObject::setLogLevel(MdkDeclarativeObject::LogLevel value) {
    if (value == logLevel()) {
        return;
    }
    switch (value) {
    case LogLevel::NoLog:
        mdk::setLogLevel(mdk::LogLevel::Off);
        break;
    case LogLevel::DebugLevel:
        mdk::setLogLevel(mdk::LogLevel::Debug);
        break;
    case LogLevel::WarningLevel:
        mdk::setLogLevel(mdk::LogLevel::Warning);
        break;
    case LogLevel::CriticalLevel:
    case LogLevel::FatalLevel:
        mdk::setLogLevel(mdk::LogLevel::Error);
        break;
    case LogLevel::InfoLevel:
        mdk::setLogLevel(mdk::LogLevel::Info);
        break;
    }
}

void MdkDeclarativeObject::open(const QUrl &value) {
    if (!value.isValid()) {
        return;
    }
    if (value != m_source) {
        setSource(value);
    }
    if (!isPlaying()) {
        play();
    }
}

void MdkDeclarativeObject::play() {
    if (!isPaused() || !m_source.isValid()) {
        return;
    }
    player->setState(mdk::PlaybackState::Playing);
}

void MdkDeclarativeObject::play(const QUrl &value) {
    if (!value.isValid()) {
        return;
    }
    if ((value == m_source) && !isPlaying()) {
        play();
    } else {
        open(value);
    }
}

void MdkDeclarativeObject::pause() {
    if (!isPlaying()) {
        return;
    }
    player->setState(mdk::PlaybackState::Paused);
}

void MdkDeclarativeObject::stop() {
    if (isStopped()) {
        return;
    }
    player->setState(mdk::PlaybackState::Stopped);
    m_source.clear();
}

void MdkDeclarativeObject::seek(qint64 value) {
    if (isStopped() || (value == position())) {
        return;
    }
    player->seek(qMin(qMax(value, qint64(0)), duration()));
}

void MdkDeclarativeObject::processMdkEvents() {
    player->currentMediaChanged([this] {
        qDebug().noquote() << "Current media changed:" << player->url();
        Q_EMIT this->sourceChanged();
    });
    player->onMediaStatusChanged([this](mdk::MediaStatus s) {
        Q_UNUSED(s)
        Q_EMIT this->mediaStatusChanged();
        return true;
    });
    player->onEvent([](const mdk::MediaEvent &e) {
        qDebug().noquote() << "Media event:" << e.category.data()
                           << e.detail.data();
        return false;
    });
    player->onLoop([](int count) {
        qDebug().noquote() << "onLoop:" << count;
        return false;
    });
    player->onStateChanged([this](mdk::PlaybackState s) {
        Q_UNUSED(s)
        Q_EMIT this->playbackStateChanged();
        if (this->isPlaying()) {
            Q_EMIT this->playing();
        }
        if (this->isPaused()) {
            Q_EMIT this->paused();
        }
        if (this->isStopped()) {
            Q_EMIT this->stopped();
        }
    });
}

bool MdkDeclarativeObject::isLoaded() const { return true; }

bool MdkDeclarativeObject::isPlaying() const {
    return playbackState() == PlaybackState::PlayingState;
}

bool MdkDeclarativeObject::isPaused() const {
    return playbackState() == PlaybackState::PausedState;
}

bool MdkDeclarativeObject::isStopped() const {
    return playbackState() == PlaybackState::StoppedState;
}

void MdkDeclarativeObject::setVideoSurfaceSize(int width, int height) {
    player->setVideoSurfaceSize(width, height);
}

void MdkDeclarativeObject::renderVideo() { player->renderVideo(); }
