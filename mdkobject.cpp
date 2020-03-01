#include "mdkobject.h"

#include <QDebug>
#include <QDir>
#include <QOpenGLFramebufferObject>

class MdkRenderer : public QQuickFramebufferObject::Renderer {
    Q_DISABLE_COPY_MOVE(MdkRenderer)

public:
    MdkRenderer(MdkObject *mdkobject) : mdkobject(mdkobject) {
        Q_ASSERT(this->mdkobject != nullptr);
    }
    ~MdkRenderer() override = default;

    void render() override { mdkobject->renderVideo(); }

    QOpenGLFramebufferObject *
    createFramebufferObject(const QSize &size) override {
        mdkobject->setVideoSurfaceSize(size);
        return new QOpenGLFramebufferObject(size);
    }

private:
    MdkObject *mdkobject = nullptr;
};

MdkObject::MdkObject(QQuickItem *parent)
    : QQuickFramebufferObject(parent),
      m_player(std::make_unique<MDK_NS::Player>()),
      m_snapshotDirectory(
          QDir::toNativeSeparators(QCoreApplication::applicationDirPath())) {
    Q_ASSERT(m_player != nullptr);
#ifdef Q_OS_WINDOWS
    m_player->setVideoDecoders({"MFT:d3d=11", "MFT:d3d=9", "MFT", "D3D11",
                                "DXVA", "CUDA", "NVDEC", "FFmpeg"});
#elif defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
    m_player->setVideoDecoders({"VAAPI", "VDPAU", "CUDA", "NVDEC", "FFmpeg"});
#elif defined(Q_OS_MACOS)
    m_player->setVideoDecoders({"VT", "VideoToolbox", "FFmpeg"});
#endif
    m_player->setRenderCallback([this](void * /*unused*/) {
        QMetaObject::invokeMethod(this, "update");
    });
    // MUST set before setMedia() because setNextMedia() is called when media is
    // changed
    m_player->setPreloadImmediately(false);
    connect(this, &MdkObject::sourceChanged, this, &MdkObject::fileNameChanged);
    connect(this, &MdkObject::sourceChanged, this, &MdkObject::pathChanged);
    startTimer(50);
    processMdkEvents();
}

MdkObject::~MdkObject() = default;

QQuickFramebufferObject::Renderer *MdkObject::createRenderer() const {
    return new MdkRenderer(const_cast<MdkObject *>(this));
}

void MdkObject::renderVideo() { m_player->renderVideo(); }

void MdkObject::setVideoSurfaceSize(QSize size) {
    m_player->setVideoSurfaceSize(size.width(), size.height());
}

QUrl MdkObject::source() const { return isStopped() ? QUrl() : m_source; }

void MdkObject::setSource(const QUrl &value) {
    if (!value.isValid() || (value == m_source)) {
        return;
    }
    m_player->setNextMedia(nullptr, -1);
    m_player->setState(MDK_NS::PlaybackState::Stopped);
    m_player->waitFor(MDK_NS::PlaybackState::Stopped);
    // 1st url may be the same as current url
    m_player->setMedia(nullptr);
    m_player->setMedia(
        value.isLocalFile()
            ? qUtf8Printable(QDir::toNativeSeparators(value.toLocalFile()))
            : qUtf8Printable(value.url()));
    m_source = value;
    Q_EMIT sourceChanged();
    m_player->scale(1.0, -1.0);
    Q_EMIT positionChanged();
    m_player->prepare(0, [this](int64_t position, bool * /*unused*/) {
        Q_UNUSED(position)
        const auto &info = m_player->mediaInfo();
        m_hasVideo = !info.video.empty();
        m_hasAudio = !info.audio.empty();
        // m_hasSubtitle = ...
        Q_EMIT durationChanged();
        Q_EMIT seekableChanged();
        if (m_hasVideo) {
            Q_EMIT videoSizeChanged();
        }
        return true;
    });
    m_player->setState(MDK_NS::PlaybackState::Playing);
}

QString MdkObject::fileName() const {
    return (isStopped() || !m_source.isValid())
        ? QString()
        : (m_source.isLocalFile() ? m_source.fileName()
                                  : m_source.toDisplayString());
}

QString MdkObject::path() const {
    return (isStopped() || !m_source.isValid())
        ? QString()
        : (m_source.isLocalFile()
               ? QDir::toNativeSeparators(m_source.toLocalFile())
               : m_source.toDisplayString());
}

qint64 MdkObject::position() const {
    return isStopped() ? 0
                       : qBound(qint64(0), m_player->position(), duration());
}

void MdkObject::setPosition(qint64 value) {
    if (isStopped() || (value == position())) {
        return;
    }
    seek(qBound(qint64(0), value, duration()));
}

qint64 MdkObject::duration() const {
    return isStopped() ? 0 : qMax(m_player->mediaInfo().duration, qint64(0));
}

QSize MdkObject::videoSize() const {
    return (isStopped() || !m_hasVideo)
        ? QSize(0, 0)
        : QSize(qMax(m_player->mediaInfo().video.at(0).codec.width, 0),
                qMax(m_player->mediaInfo().video.at(0).codec.height, 0));
}

qreal MdkObject::volume() const { return qBound(0.0, m_volume, 1.0); }

void MdkObject::setVolume(qreal value) {
    if (qFuzzyCompare(value, volume())) {
        return;
    }
    m_player->setVolume(qBound(0.0, value, 1.0));
    m_volume = value;
    Q_EMIT volumeChanged();
}

bool MdkObject::mute() const { return m_mute; }

void MdkObject::setMute(bool value) {
    if (value == mute()) {
        return;
    }
    m_player->setMute(value);
    m_mute = value;
    Q_EMIT muteChanged();
}

bool MdkObject::seekable() const {
    // Local files are always seekable, in theory.
    return true;
}

MdkObject::PlaybackState MdkObject::playbackState() const {
    switch (m_player->state()) {
    case MDK_NS::PlaybackState::Playing:
        return PlaybackState::Playing;
    case MDK_NS::PlaybackState::Paused:
        return PlaybackState::Paused;
    case MDK_NS::PlaybackState::Stopped:
        return PlaybackState::Stopped;
    }
    return PlaybackState::Stopped;
}

void MdkObject::setPlaybackState(MdkObject::PlaybackState value) {
    if (isStopped() || (value == playbackState())) {
        return;
    }
    switch (value) {
    case PlaybackState::Playing:
        m_player->setState(MDK_NS::PlaybackState::Playing);
        break;
    case PlaybackState::Paused:
        m_player->setState(MDK_NS::PlaybackState::Paused);
        break;
    case PlaybackState::Stopped:
        m_player->setState(MDK_NS::PlaybackState::Stopped);
        break;
    }
}

MdkObject::MediaStatus MdkObject::mediaStatus() const {
    switch (m_player->mediaStatus()) {
    case MDK_NS::MediaStatus::NoMedia:
        return MediaStatus::NoMedia;
    case MDK_NS::MediaStatus::Loading:
        return MediaStatus::Loading;
    case MDK_NS::MediaStatus::Loaded:
        return MediaStatus::Loaded;
    case MDK_NS::MediaStatus::Stalled:
        return MediaStatus::Stalled;
    case MDK_NS::MediaStatus::Buffering:
        return MediaStatus::Buffering;
    case MDK_NS::MediaStatus::Buffered:
        return MediaStatus::Buffered;
    case MDK_NS::MediaStatus::End:
        return MediaStatus::End;
    case MDK_NS::MediaStatus::Invalid:
        return MediaStatus::Invalid;
    default:
        return MediaStatus::Unknown;
    }
}

MdkObject::LogLevel MdkObject::logLevel() const {
    switch (MDK_NS::logLevel()) {
    case MDK_NS::LogLevel::Off:
        return LogLevel::Off;
    case MDK_NS::LogLevel::Debug:
        return LogLevel::Debug;
    case MDK_NS::LogLevel::Warning:
        return LogLevel::Warning;
    case MDK_NS::LogLevel::Error:
        return LogLevel::Critical;
    case MDK_NS::LogLevel::Info:
        return LogLevel::Info;
    default:
        return LogLevel::Debug;
    }
}

void MdkObject::setLogLevel(MdkObject::LogLevel value) {
    if (value == logLevel()) {
        return;
    }
    switch (value) {
    case LogLevel::Off:
        MDK_NS::setLogLevel(MDK_NS::LogLevel::Off);
        break;
    case LogLevel::Debug:
        MDK_NS::setLogLevel(MDK_NS::LogLevel::Debug);
        break;
    case LogLevel::Warning:
        MDK_NS::setLogLevel(MDK_NS::LogLevel::Warning);
        break;
    case LogLevel::Critical:
    case LogLevel::Fatal:
        MDK_NS::setLogLevel(MDK_NS::LogLevel::Error);
        break;
    case LogLevel::Info:
        MDK_NS::setLogLevel(MDK_NS::LogLevel::Info);
        break;
    }
    Q_EMIT logLevelChanged();
}

qreal MdkObject::playbackRate() const {
    return isStopped()
        ? 0.0
        : qMax(static_cast<qreal>(m_player->playbackRate()), 0.0);
}

void MdkObject::setPlaybackRate(qreal value) {
    if (isStopped()) {
        return;
    }
    m_player->setPlaybackRate(qMax(value, 0.0));
    Q_EMIT playbackRateChanged();
}

qreal MdkObject::aspectRatio() const {
    return 1.7777; // 16:9
}

void MdkObject::setAspectRatio(qreal value) {
    if (isStopped()) {
        return;
    }
    m_player->setAspectRatio(qMax(value, 0.0));
    Q_EMIT aspectRatioChanged();
}

QString MdkObject::snapshotDirectory() const {
    return QDir::toNativeSeparators(m_snapshotDirectory);
}

void MdkObject::setSnapshotDirectory(const QString &value) {
    if (value.isEmpty()) {
        return;
    }
    const QString val = QDir::toNativeSeparators(value);
    if (val == snapshotDirectory()) {
        return;
    }
    m_snapshotDirectory = val;
    Q_EMIT snapshotDirectoryChanged();
}

QString MdkObject::snapshotFormat() const { return m_snapshotFormat; }

void MdkObject::setSnapshotFormat(const QString &value) {
    if (value.isEmpty() || (value == snapshotFormat())) {
        return;
    }
    m_snapshotFormat = value;
    Q_EMIT snapshotFormatChanged();
}

QString MdkObject::snapshotTemplate() const { return m_snapshotTemplate; }

void MdkObject::setSnapshotTemplate(const QString &value) {
    if (value.isEmpty() || (value == snapshotTemplate())) {
        return;
    }
    m_snapshotTemplate = value;
    Q_EMIT snapshotTemplateChanged();
}

void MdkObject::open(const QUrl &value) {
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

void MdkObject::play() {
    if (!isPaused() || !m_source.isValid()) {
        return;
    }
    m_player->setState(MDK_NS::PlaybackState::Playing);
}

void MdkObject::play(const QUrl &value) {
    if (!value.isValid()) {
        return;
    }
    if ((value == m_source) && !isPlaying()) {
        play();
    } else {
        open(value);
    }
}

void MdkObject::pause() {
    if (!isPlaying()) {
        return;
    }
    m_player->setState(MDK_NS::PlaybackState::Paused);
}

void MdkObject::stop() {
    if (isStopped()) {
        return;
    }
    m_player->setState(MDK_NS::PlaybackState::Stopped);
    m_source.clear();
}

void MdkObject::seek(qint64 value) {
    if (isStopped() || (value == position())) {
        return;
    }
    m_player->seek(qBound(qint64(0), value, duration()));
}

void MdkObject::rotate(int value) {
    if (isStopped()) {
        return;
    }
    m_player->rotate(qBound(0, value, 359));
}

void MdkObject::scale(qreal x, qreal y) {
    if (isStopped()) {
        return;
    }
    m_player->scale(qMax(x, 0.0), qMax(y, 0.0));
}

void MdkObject::snapshot() {
    if (isStopped()) {
        return;
    }
    MDK_NS::Player::SnapshotRequest snapshotRequest{};
    m_player->snapshot(
        &snapshotRequest,
        [this](MDK_NS::Player::SnapshotRequest *ret, double frameTime) {
            Q_UNUSED(ret)
            const QString path =
                QString::fromUtf8("%1%2%3.%4")
                    .arg(snapshotDirectory(), QDir::separator(),
                         QString::number(frameTime), snapshotFormat());
            qDebug().noquote() << "Taking snapshot:" << path;
            return path.toStdString();
        });
}

void MdkObject::timerEvent(QTimerEvent *event) {
    Q_UNUSED(event)
    Q_EMIT positionChanged();
}

void MdkObject::processMdkEvents() {
    MDK_NS::setLogHandler([](MDK_NS::LogLevel level, const char *msg) {
        if (level >= std::underlying_type<MDK_NS::LogLevel>::type(
                         MDK_NS::LogLevel::Info)) {
            qDebug().noquote() << msg;
        } else if (level >= std::underlying_type<MDK_NS::LogLevel>::type(
                                MDK_NS::LogLevel::Warning)) {
            qWarning().noquote() << msg;
        }
    });
    m_player->currentMediaChanged([this] {
        qDebug().noquote() << "Current media changed:" << m_player->url();
        Q_EMIT sourceChanged();
    });
    m_player->onMediaStatusChanged([this](MDK_NS::MediaStatus s) {
        Q_UNUSED(s)
        Q_EMIT mediaStatusChanged();
        return true;
    });
    m_player->onEvent([](const MDK_NS::MediaEvent &e) {
        qDebug().noquote() << "Media event:" << e.category.data()
                           << e.detail.data();
        return false;
    });
    m_player->onLoop([](int count) {
        qDebug().noquote() << "onLoop:" << count;
        return false;
    });
    m_player->onStateChanged([this](MDK_NS::PlaybackState s) {
        Q_UNUSED(s)
        Q_EMIT playbackStateChanged();
        if (isPlaying()) {
            Q_EMIT playing();
        }
        if (isPaused()) {
            Q_EMIT paused();
        }
        if (isStopped()) {
            Q_EMIT stopped();
        }
    });
}

bool MdkObject::isLoaded() const { return true; }

bool MdkObject::isPlaying() const {
    return m_player->state() == MDK_NS::PlaybackState::Playing;
}

bool MdkObject::isPaused() const {
    return m_player->state() == MDK_NS::PlaybackState::Paused;
}

bool MdkObject::isStopped() const {
    return m_player->state() == MDK_NS::PlaybackState::Stopped;
}
