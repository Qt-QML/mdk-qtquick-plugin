#include "mdkdeclarativeobject.h"
#include <QDebug>
#include <QDir>
#include <QOpenGLFramebufferObject>

class MdkRenderer : public QQuickFramebufferObject::Renderer {
    Q_DISABLE_COPY_MOVE(MdkRenderer)

public:
    MdkRenderer(MdkDeclarativeObject *mdkdeclarativeobject)
        : mdkdeclarativeobject(mdkdeclarativeobject) {
        Q_ASSERT(this->mdkdeclarativeobject != nullptr);
    }
    ~MdkRenderer() override = default;

    void render() override { mdkdeclarativeobject->renderVideo(); }

    QOpenGLFramebufferObject *
    createFramebufferObject(const QSize &size) override {
        mdkdeclarativeobject->setVideoSurfaceSize(size);
        return new QOpenGLFramebufferObject(size);
    }

private:
    MdkDeclarativeObject *mdkdeclarativeobject = nullptr;
};

MdkDeclarativeObject::MdkDeclarativeObject(QQuickItem *parent)
    : QQuickFramebufferObject(parent), player(std::make_unique<mdk::Player>()) {
    Q_ASSERT(player != nullptr);
#ifdef Q_OS_WINDOWS
    player->setVideoDecoders({"MFT:d3d=11", "MFT:d3d=9", "MFT", "D3D11", "DXVA",
                              "CUDA", "NVDEC", "FFmpeg"});
#elif defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
    player->setVideoDecoders({"VAAPI", "VDPAU", "CUDA", "NVDEC", "FFmpeg"});
#elif defined(Q_OS_MACOS)
    player->setVideoDecoders({"VT", "VideoToolbox", "FFmpeg"});
#endif
    player->setRenderCallback([this](void * /*unused*/) {
        QMetaObject::invokeMethod(this, "update");
    });
    // MUST set before setMedia() because setNextMedia() is called when media is
    // changed
    player->setPreloadImmediately(false);
    connect(this, &MdkDeclarativeObject::startWatchingProperties, this,
            [this] { timer.start(500); });
    connect(this, &MdkDeclarativeObject::stopWatchingProperties, this,
            [this] { timer.stop(); });
    connect(&timer, &QTimer::timeout, this, &MdkDeclarativeObject::notify);
    connect(this, &MdkDeclarativeObject::sourceChanged, this,
            &MdkDeclarativeObject::fileNameChanged);
    connect(this, &MdkDeclarativeObject::sourceChanged, this,
            &MdkDeclarativeObject::pathChanged);
    processMdkEvents();
}

QQuickFramebufferObject::Renderer *
MdkDeclarativeObject::createRenderer() const {
    return new MdkRenderer(const_cast<MdkDeclarativeObject *>(this));
}

void MdkDeclarativeObject::renderVideo() { player->renderVideo(); }

void MdkDeclarativeObject::setVideoSurfaceSize(QSize size) {
    player->setVideoSurfaceSize(size.width(), size.height());
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
    // 1st url may be the same as current url
    player->setMedia(nullptr);
    player->setMedia(value.isLocalFile() ? qPrintable(value.toLocalFile())
                                         : qPrintable(value.url()));
    m_source = value;
    Q_EMIT sourceChanged();
    player->scale(1.0F, -1.0F);
    Q_EMIT positionChanged();
    player->waitFor(mdk::PlaybackState::Stopped);
    player->prepare(0, [this](int64_t position, bool * /*unused*/) {
        Q_UNUSED(position)
        const auto &info = player->mediaInfo();
        hasVideo = !info.video.empty();
        hasAudio = !info.audio.empty();
        Q_EMIT durationChanged();
        Q_EMIT seekableChanged();
        if (hasVideo) {
            Q_EMIT videoSizeChanged();
        }
    });
    player->setState(mdk::PlaybackState::Playing);
}

QString MdkDeclarativeObject::fileName() const {
    return (isStopped() || !m_source.isValid())
        ? QString()
        : (m_source.isLocalFile() ? m_source.fileName() : m_source.url());
}

QString MdkDeclarativeObject::path() const {
    return (isStopped() || !m_source.isValid())
        ? QString()
        : (m_source.isLocalFile()
               ? QDir::toNativeSeparators(m_source.toLocalFile())
               : m_source.url());
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
    return (isStopped() || !hasVideo)
        ? QSize(0, 0)
        : QSize(qMax(player->mediaInfo().video.at(0).codec.width, 0),
                qMax(player->mediaInfo().video.at(0).codec.height, 0));
}

float MdkDeclarativeObject::volume() const {
    return qMin(qMax(m_volume, 0.0F), 1.0F);
}

void MdkDeclarativeObject::setVolume(float value) {
    if (value == volume()) {
        return;
    }
    player->setVolume(qMin(qMax(value, 0.0F), 1.0F));
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
        return PlaybackState::Playing;
    case mdk::PlaybackState::Paused:
        return PlaybackState::Paused;
    case mdk::PlaybackState::Stopped:
        return PlaybackState::Stopped;
    }
    return PlaybackState::Stopped;
}

void MdkDeclarativeObject::setPlaybackState(
    MdkDeclarativeObject::PlaybackState value) {
    if (isStopped() || (value == playbackState())) {
        return;
    }
    switch (value) {
    case PlaybackState::Playing:
        player->setState(mdk::PlaybackState::Playing);
        break;
    case PlaybackState::Paused:
        player->setState(mdk::PlaybackState::Paused);
        break;
    case PlaybackState::Stopped:
        player->setState(mdk::PlaybackState::Stopped);
        break;
    }
}

MdkDeclarativeObject::MediaStatus MdkDeclarativeObject::mediaStatus() const {
    switch (player->mediaStatus()) {
    case mdk::MediaStatus::NoMedia:
        return MediaStatus::NoMedia;
    case mdk::MediaStatus::Loading:
        return MediaStatus::Loading;
    case mdk::MediaStatus::Loaded:
        return MediaStatus::Loaded;
    case mdk::MediaStatus::Stalled:
        return MediaStatus::Stalled;
    case mdk::MediaStatus::Buffering:
        return MediaStatus::Buffering;
    case mdk::MediaStatus::Buffered:
        return MediaStatus::Buffered;
    case mdk::MediaStatus::End:
        return MediaStatus::End;
    case mdk::MediaStatus::Invalid:
        return MediaStatus::Invalid;
    default:
        return MediaStatus::Unknown;
    }
}

MdkDeclarativeObject::LogLevel MdkDeclarativeObject::logLevel() const {
    switch (mdk::logLevel()) {
    case mdk::LogLevel::Off:
        return LogLevel::Off;
    case mdk::LogLevel::Debug:
        return LogLevel::Debug;
    case mdk::LogLevel::Warning:
        return LogLevel::Warning;
    case mdk::LogLevel::Error:
        return LogLevel::Critical;
    case mdk::LogLevel::Info:
        return LogLevel::Info;
    default:
        return LogLevel::Debug;
    }
}

void MdkDeclarativeObject::setLogLevel(MdkDeclarativeObject::LogLevel value) {
    if (value == logLevel()) {
        return;
    }
    switch (value) {
    case LogLevel::Off:
        mdk::setLogLevel(mdk::LogLevel::Off);
        break;
    case LogLevel::Debug:
        mdk::setLogLevel(mdk::LogLevel::Debug);
        break;
    case LogLevel::Warning:
        mdk::setLogLevel(mdk::LogLevel::Warning);
        break;
    case LogLevel::Critical:
    case LogLevel::Fatal:
        mdk::setLogLevel(mdk::LogLevel::Error);
        break;
    case LogLevel::Info:
        mdk::setLogLevel(mdk::LogLevel::Info);
        break;
    }
    Q_EMIT logLevelChanged();
}

float MdkDeclarativeObject::playbackRate() const {
    return isStopped() ? 0.0F : qMax(player->playbackRate(), 0.0F);
}

void MdkDeclarativeObject::setPlaybackRate(float value) {
    if (isStopped()) {
        return;
    }
    player->setPlaybackRate(qMax(value, 0.0F));
    Q_EMIT playbackRateChanged();
}

float MdkDeclarativeObject::aspectRatio() const {
    return 1.7777F; // 16:9
}

void MdkDeclarativeObject::setAspectRatio(float value) {
    if (isStopped()) {
        return;
    }
    player->setAspectRatio(qMax(value, 0.0F));
    Q_EMIT aspectRatioChanged();
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

void MdkDeclarativeObject::rotate(int value) {
    if (isStopped()) {
        return;
    }
    player->rotate(qMin(qMax(value, 0), 359));
}

void MdkDeclarativeObject::scale(float x, float y) {
    if (isStopped()) {
        return;
    }
    player->scale(qMax(x, 0.0F), qMax(y, 0.0F));
}

void MdkDeclarativeObject::processMdkEvents() {
    player->currentMediaChanged([this] {
        qDebug().noquote() << "Current media changed:" << player->url();
        Q_EMIT sourceChanged();
    });
    player->onMediaStatusChanged([this](mdk::MediaStatus s) {
        Q_UNUSED(s)
        Q_EMIT mediaStatusChanged();
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
        Q_EMIT playbackStateChanged();
        if (isPlaying()) {
            Q_EMIT startWatchingProperties();
            Q_EMIT playing();
        }
        if (isPaused()) {
            Q_EMIT stopWatchingProperties();
            Q_EMIT paused();
        }
        if (isStopped()) {
            Q_EMIT stopWatchingProperties();
            Q_EMIT stopped();
        }
    });
}

void MdkDeclarativeObject::notify() { Q_EMIT positionChanged(); }

bool MdkDeclarativeObject::isLoaded() const { return true; }

bool MdkDeclarativeObject::isPlaying() const {
    return player->state() == mdk::PlaybackState::Playing;
}

bool MdkDeclarativeObject::isPaused() const {
    return player->state() == mdk::PlaybackState::Paused;
}

bool MdkDeclarativeObject::isStopped() const {
    return player->state() == mdk::PlaybackState::Stopped;
}
