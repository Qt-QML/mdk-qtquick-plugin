#include "mdkobject.h"

#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QMimeDatabase>
#include <QMimeType>
#include <QOpenGLFramebufferObject>

namespace {

QStringList suffixesToMimeTypes(const QStringList &suffixes) {
    QStringList mimeTypes{};
    const QMimeDatabase db;
    for (auto &&suffix : qAsConst(suffixes)) {
        const QList<QMimeType> typeList = db.mimeTypesForFileName(suffix);
        if (!typeList.isEmpty()) {
            for (auto &&mimeType : qAsConst(typeList)) {
                mimeTypes.append(mimeType.name());
            }
        }
    }
    if (!mimeTypes.isEmpty()) {
        mimeTypes.removeDuplicates();
    }
    return mimeTypes;
}

} // namespace

class MdkRenderer : public QQuickFramebufferObject::Renderer {
    Q_DISABLE_COPY_MOVE(MdkRenderer)

public:
    MdkRenderer(MdkObject *obj) : m_mdkobject(obj) { Q_ASSERT(m_mdkobject); }
    ~MdkRenderer() override = default;

    void render() override { m_mdkobject->renderVideo(); }

    QOpenGLFramebufferObject *
    createFramebufferObject(const QSize &size) override {
        m_mdkobject->setVideoSurfaceSize(size);
        return new QOpenGLFramebufferObject(size);
    }

private:
    MdkObject *m_mdkobject = nullptr;
};

MdkObject::MdkObject(QQuickItem *parent)
    : QQuickFramebufferObject(parent),
      m_snapshotDirectory(
          QDir::toNativeSeparators(QCoreApplication::applicationDirPath())) {
    Q_ASSERT(m_player);
    // Disable status messages as they are quite annoying.
    qputenv("MDK_LOG_STATUS", "0");
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
    initMdkHandlers();
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
    if (value.isEmpty() || !value.isValid() || (value == m_source) ||
        !isMedia(value)) {
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
    return m_source.isLocalFile() ? m_source.fileName()
                                  : m_source.toDisplayString();
}

QString MdkObject::path() const {
    return m_source.isLocalFile()
        ? QDir::toNativeSeparators(m_source.toLocalFile())
        : m_source.toDisplayString();
}

qint64 MdkObject::position() const {
    return isStopped() ? 0 : m_player->position();
}

void MdkObject::setPosition(qint64 value) {
    if (isStopped() || (value == position())) {
        return;
    }
    seek(value);
}

qint64 MdkObject::duration() const {
    return isStopped() ? 0 : m_player->mediaInfo().duration;
}

QSize MdkObject::videoSize() const {
    const auto &codec = m_player->mediaInfo().video.at(0).codec;
    return QSize(codec.width, codec.height);
}

qreal MdkObject::volume() const { return m_volume; }

void MdkObject::setVolume(qreal value) {
    if (qFuzzyCompare(value, volume())) {
        return;
    }
    m_player->setVolume(value);
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
    return isStopped() ? 1.0 : static_cast<qreal>(m_player->playbackRate());
}

void MdkObject::setPlaybackRate(qreal value) {
    if (isStopped()) {
        return;
    }
    m_player->setPlaybackRate(value);
    Q_EMIT playbackRateChanged();
}

qreal MdkObject::aspectRatio() const { return static_cast<qreal>(16.0 / 9.0); }

void MdkObject::setAspectRatio(qreal value) {
    if (isStopped()) {
        return;
    }
    m_player->setAspectRatio(value);
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

QStringList MdkObject::videoMimeTypes() const {
    return suffixesToMimeTypes(videoSuffixes());
}

QStringList MdkObject::audioMimeTypes() const {
    return suffixesToMimeTypes(audioSuffixes());
}

void MdkObject::open(const QUrl &value) {
    if (value.isEmpty() || !value.isValid()) {
        return;
    }
    if ((value != m_source) && isMedia(value)) {
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
    if (value.isEmpty() || !value.isValid()) {
        return;
    }
    if ((value == m_source) && !isPlaying()) {
        play();
    }
    if ((value != m_source) && isMedia(value)) {
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
    m_player->seek(value);
}

void MdkObject::rotate(int value) {
    if (isStopped()) {
        return;
    }
    m_player->rotate(value);
}

void MdkObject::scale(qreal x, qreal y) {
    if (isStopped()) {
        return;
    }
    m_player->scale(x, y);
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

bool MdkObject::isVideo(const QUrl &value) const {
    if (!value.isEmpty() && value.isValid()) {
        return videoSuffixes().contains(
            QString::fromUtf8("*.") + QFileInfo(value.fileName()).suffix(),
            Qt::CaseInsensitive);
    }
    return false;
}

bool MdkObject::isAudio(const QUrl &value) const {
    if (!value.isEmpty() && value.isValid()) {
        return audioSuffixes().contains(
            QString::fromUtf8("*.") + QFileInfo(value.fileName()).suffix(),
            Qt::CaseInsensitive);
    }
    return false;
}

bool MdkObject::isMedia(const QUrl &value) const {
    return (isVideo(value) || isAudio(value));
}

void MdkObject::timerEvent(QTimerEvent *event) {
    Q_UNUSED(event)
    Q_EMIT positionChanged();
}

void MdkObject::initMdkHandlers() {
    MDK_NS::setLogHandler([](MDK_NS::LogLevel level, const char *msg) {
        switch (level) {
        case MDK_NS::LogLevel::Info:
            qInfo().noquote() << msg;
            break;
        case MDK_NS::LogLevel::All:
        case MDK_NS::LogLevel::Debug:
            qDebug().noquote() << msg;
            break;
        case MDK_NS::LogLevel::Warning:
            qWarning().noquote() << msg;
            break;
        case MDK_NS::LogLevel::Error:
            qCritical().noquote() << msg;
            break;
        default:
            break;
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

bool MdkObject::isLoaded() const { return !isStopped(); }

bool MdkObject::isPlaying() const {
    return m_player->state() == MDK_NS::PlaybackState::Playing;
}

bool MdkObject::isPaused() const {
    return m_player->state() == MDK_NS::PlaybackState::Paused;
}

bool MdkObject::isStopped() const {
    return m_player->state() == MDK_NS::PlaybackState::Stopped;
}
