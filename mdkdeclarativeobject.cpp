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
    connect(this, &MdkDeclarativeObject::startWatchingProperties, this,
            [this] { m_timer.start(500); });
    connect(this, &MdkDeclarativeObject::stopWatchingProperties, this,
            [this] { m_timer.stop(); });
    connect(&m_timer, &QTimer::timeout, this, &MdkDeclarativeObject::notify);
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

void MdkDeclarativeObject::renderVideo() { m_player->renderVideo(); }

void MdkDeclarativeObject::setVideoSurfaceSize(QSize size) {
    m_player->setVideoSurfaceSize(size.width(), size.height());
}

QUrl MdkDeclarativeObject::source() const {
    return isStopped() ? QUrl() : m_source;
}

void MdkDeclarativeObject::setSource(const QUrl &value) {
    if (!value.isValid() || (value == m_source)) {
        return;
    }
    m_player->setNextMedia(nullptr);
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
    m_player->scale(1.0F, -1.0F);
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

QString MdkDeclarativeObject::fileName() const {
    return (isStopped() || !m_source.isValid())
        ? QString()
        : (m_source.isLocalFile() ? m_source.fileName()
                                  : m_source.toDisplayString());
}

QString MdkDeclarativeObject::path() const {
    return (isStopped() || !m_source.isValid())
        ? QString()
        : (m_source.isLocalFile()
               ? QDir::toNativeSeparators(m_source.toLocalFile())
               : m_source.toDisplayString());
}

qint64 MdkDeclarativeObject::position() const {
    return isStopped() ? 0
                       : qBound(qint64(0), m_player->position(), duration());
}

void MdkDeclarativeObject::setPosition(qint64 value) {
    if (isStopped() || (value == position())) {
        return;
    }
    seek(qBound(qint64(0), value, duration()));
}

qint64 MdkDeclarativeObject::duration() const {
    return isStopped() ? 0 : qMax(m_player->mediaInfo().duration, qint64(0));
}

QSize MdkDeclarativeObject::videoSize() const {
    return (isStopped() || !m_hasVideo)
        ? QSize(0, 0)
        : QSize(qMax(m_player->mediaInfo().video.at(0).codec.width, 0),
                qMax(m_player->mediaInfo().video.at(0).codec.height, 0));
}

float MdkDeclarativeObject::volume() const {
    return qBound(0.0F, m_volume, 1.0F);
}

void MdkDeclarativeObject::setVolume(float value) {
    if (qFuzzyCompare(value, volume())) {
        return;
    }
    m_player->setVolume(qBound(0.0F, value, 1.0F));
    m_volume = value;
    Q_EMIT volumeChanged();
}

bool MdkDeclarativeObject::mute() const { return m_mute; }

void MdkDeclarativeObject::setMute(bool value) {
    if (value == mute()) {
        return;
    }
    m_player->setMute(value);
    m_mute = value;
    Q_EMIT muteChanged();
}

bool MdkDeclarativeObject::seekable() const {
    // Local files are always seekable, in theory.
    return true;
}

MdkDeclarativeObject::PlaybackState
MdkDeclarativeObject::playbackState() const {
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

void MdkDeclarativeObject::setPlaybackState(
    MdkDeclarativeObject::PlaybackState value) {
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

MdkDeclarativeObject::MediaStatus MdkDeclarativeObject::mediaStatus() const {
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

MdkDeclarativeObject::LogLevel MdkDeclarativeObject::logLevel() const {
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

void MdkDeclarativeObject::setLogLevel(MdkDeclarativeObject::LogLevel value) {
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

float MdkDeclarativeObject::playbackRate() const {
    return isStopped() ? 0.0F : qMax(m_player->playbackRate(), 0.0F);
}

void MdkDeclarativeObject::setPlaybackRate(float value) {
    if (isStopped()) {
        return;
    }
    m_player->setPlaybackRate(qMax(value, 0.0F));
    Q_EMIT playbackRateChanged();
}

float MdkDeclarativeObject::aspectRatio() const {
    return 1.7777F; // 16:9
}

void MdkDeclarativeObject::setAspectRatio(float value) {
    if (isStopped()) {
        return;
    }
    m_player->setAspectRatio(qMax(value, 0.0F));
    Q_EMIT aspectRatioChanged();
}

QString MdkDeclarativeObject::snapshotDirectory() const {
    return QDir::toNativeSeparators(m_snapshotDirectory);
}

void MdkDeclarativeObject::setSnapshotDirectory(const QString &value) {
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

QString MdkDeclarativeObject::snapshotFormat() const {
    return m_snapshotFormat;
}

void MdkDeclarativeObject::setSnapshotFormat(const QString &value) {
    if (value.isEmpty() || (value == snapshotFormat())) {
        return;
    }
    m_snapshotFormat = value;
    Q_EMIT snapshotFormatChanged();
}

QString MdkDeclarativeObject::snapshotTemplate() const {
    return m_snapshotTemplate;
}

void MdkDeclarativeObject::setSnapshotTemplate(const QString &value) {
    if (value.isEmpty() || (value == snapshotTemplate())) {
        return;
    }
    m_snapshotTemplate = value;
    Q_EMIT snapshotTemplateChanged();
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
    m_player->setState(MDK_NS::PlaybackState::Playing);
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
    m_player->setState(MDK_NS::PlaybackState::Paused);
}

void MdkDeclarativeObject::stop() {
    if (isStopped()) {
        return;
    }
    m_player->setState(MDK_NS::PlaybackState::Stopped);
    m_source.clear();
}

void MdkDeclarativeObject::seek(qint64 value) {
    if (isStopped() || (value == position())) {
        return;
    }
    m_player->seek(qBound(qint64(0), value, duration()));
}

void MdkDeclarativeObject::rotate(int value) {
    if (isStopped()) {
        return;
    }
    m_player->rotate(qBound(0, value, 359));
}

void MdkDeclarativeObject::scale(float x, float y) {
    if (isStopped()) {
        return;
    }
    m_player->scale(qMax(x, 0.0F), qMax(y, 0.0F));
}

void MdkDeclarativeObject::snapshot() {
    if (isStopped()) {
        return;
    }
    MDK_NS::Player::SnapshotRequest snapshotRequest{};
    m_player->snapshot(
        &snapshotRequest,
        [this](MDK_NS::Player::SnapshotRequest *ret, double frameTime) {
            Q_UNUSED(ret)
            const QString path =
                QLatin1String("%1%2%3.%4")
                    .arg(snapshotDirectory(), QDir::separator(),
                         QString::number(frameTime), snapshotFormat());
            qDebug().noquote() << "Taking snapshot:" << path;
            return path.toStdString();
        });
}

void MdkDeclarativeObject::processMdkEvents() {
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
    return m_player->state() == MDK_NS::PlaybackState::Playing;
}

bool MdkDeclarativeObject::isPaused() const {
    return m_player->state() == MDK_NS::PlaybackState::Paused;
}

bool MdkDeclarativeObject::isStopped() const {
    return m_player->state() == MDK_NS::PlaybackState::Stopped;
}
