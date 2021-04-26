/*
 * MIT License
 *
 * Copyright (C) 2021 by wangwenx190 (Yuhang Zhao)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "mdkplayer.h"
#include "videotexturenode.h"
#include <QtCore/qdebug.h>
#include <QtCore/qdir.h>
#include <QtCore/qfileinfo.h>
#include <QtCore/qmimedatabase.h>
#include <QtCore/qmimetype.h>
#include <QtCore/qstandardpaths.h>
#include <QtCore/qdatetime.h>
#include <QtCore/qmath.h>
#include <QtQuick/qquickwindow.h>
#include <mdk/Player.h>

#ifndef QT_NO_DEBUG_STREAM
QDebug operator<<(QDebug d, const MDKPLAYER_PREPEND_NAMESPACE(MDKPlayer)::Chapters &chapters)
{
    QDebugStateSaver saver(d);
    d.nospace();
    d.noquote();
    QString chaptersStr = {};
    for (auto &&chapter : qAsConst(chapters)) {
        chaptersStr.append(QStringLiteral("(title: %1, beginTime: %2, endTime: %3)").arg(
              chapter.title, QString::number(chapter.beginTime), QString::number(chapter.endTime)));
    }
    d << "QList(" << chaptersStr << ')';
    return d;
}
#endif

static inline QStringList suffixesToMimeTypes(const QStringList &suffixes)
{
    QStringList mimeTypes{};
    const QMimeDatabase db;
    for (auto &&suffix : qAsConst(suffixes)) {
        const QList<QMimeType> typeList = db.mimeTypesForFileName(suffix);
        if (!typeList.isEmpty()) {
            for (auto &&mimeType : qAsConst(typeList)) {
                const QString name = mimeType.name();
                if (!name.isEmpty()) {
                    mimeTypes.append(name);
                }
            }
        }
    }
    if (!mimeTypes.isEmpty()) {
        mimeTypes.removeDuplicates();
    }
    return mimeTypes;
}

static inline QString timeToString(const qint64 ms, const bool isAudio = false)
{
    return QTime(0, 0).addMSecs(ms).toString(isAudio ? QStringLiteral("mm:ss") : QStringLiteral("hh:mm:ss"));
}

static inline std::vector<std::string> qStringListToStdStringVector(const QStringList &stringList)
{
    if (stringList.isEmpty()) {
        return {};
    }
    std::vector<std::string> result = {};
    for (auto &&string : qAsConst(stringList)) {
        result.push_back(string.toStdString());
    }
    return result;
}

static inline QString urlToString(const QUrl &value, const bool display = false)
{
    if (!value.isValid()) {
        return {};
    }
    return (value.isLocalFile() ? QDir::toNativeSeparators(value.toLocalFile())
                                : (display ? value.toDisplayString() : value.url()));
}

static inline MDK_NS_PREPEND(LogLevel) _MDKPlayer_MDK_LogLevel()
{
    return static_cast<MDK_NS_PREPEND(LogLevel)>(MDK_logLevel());
}

MDKPLAYER_BEGIN_NAMESPACE

VideoTextureNode *createNodePublic(MDKPlayer *item);
VideoTextureNode *createNodePrivate(MDKPlayer *item);

MDKPlayer::MDKPlayer(QQuickItem *parent) : QQuickItem(parent)
{
    setFlag(ItemHasContents);
    qRegisterMetaType<ChapterInfo>();
    qRegisterMetaType<Chapters>();
    qRegisterMetaType<MetaData>();
    qRegisterMetaType<VideoStreamInfo>();
    qRegisterMetaType<VideoStreams>();
    qRegisterMetaType<AudioStreamInfo>();
    qRegisterMetaType<AudioStreams>();
    qRegisterMetaType<MediaInfo>();
    m_player.reset(new MDK_NS_PREPEND(Player));
    if (!m_livePreview) {
        qDebug() << "Player created.";
    }
    m_player->setRenderCallback([this](void *){
        QMetaObject::invokeMethod(this, "update");
    });
    m_snapshotDirectory = QDir::toNativeSeparators(QStandardPaths::writableLocation(QStandardPaths::PicturesLocation));
    connect(this, &MDKPlayer::urlChanged, this, &MDKPlayer::fileNameChanged);
    connect(this, &MDKPlayer::urlChanged, this, &MDKPlayer::filePathChanged);
    connect(this, &MDKPlayer::positionChanged, this, &MDKPlayer::positionTextChanged);
    connect(this, &MDKPlayer::durationChanged, this, &MDKPlayer::durationTextChanged);
    initMdkHandlers();
    startTimer(50);
}

MDKPlayer::~MDKPlayer()
{
    if (!m_livePreview) {
        qDebug() << "Player destroyed.";
    }
}

// The beauty of using a true QSGNode: no need for complicated cleanup
// arrangements, unlike in other examples like metalunderqml, because the
// scenegraph will handle destroying the node at the appropriate time.
// Called on the render thread when the scenegraph is invalidated.
void MDKPlayer::invalidateSceneGraph()
{
    m_node = nullptr;
}

// Called on the gui thread if the item is removed from scene.
void MDKPlayer::releaseResources()
{
    m_node = nullptr;
}

QSGNode *MDKPlayer::updatePaintNode(QSGNode *node, UpdatePaintNodeData *data)
{
    Q_UNUSED(data);
    auto n = static_cast<VideoTextureNode *>(node);
    if (!n && ((width() <= 0) || (height() <= 0))) {
        return nullptr;
    }
    if (!n) {
        m_node = createNodePublic(this);
        n = m_node;
    }
    m_node->sync();
    // Ensure getting to beforeRendering() at some point.
    window()->update();
    return n;
}

#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
void MDKPlayer::geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry)
#else
void MDKPlayer::geometryChanged(const QRectF &newGeometry, const QRectF &oldGeometry)
#endif
{
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
    QQuickItem::geometryChange(newGeometry, oldGeometry);
#else
    QQuickItem::geometryChanged(newGeometry, oldGeometry);
#endif
    if (newGeometry.size() != oldGeometry.size()) {
        update();
    }
}

QUrl MDKPlayer::url() const
{
    // ### TODO: isStopped() ?
    if (!m_player->url()) {
        return {};
    }
    return QUrl::fromUserInput(QString::fromUtf8(m_player->url()),
                               QCoreApplication::applicationDirPath(),
                               QUrl::AssumeLocalFile);
}

void MDKPlayer::setUrl(const QUrl &value)
{
    const QUrl now = url();
    if (now.isValid() && (value != now)) {
        Q_EMIT newHistory(now, position());
    }
    const auto realStop = [this]() -> void {
        m_player->setNextMedia(nullptr);
        m_player->setState(MDK_NS_PREPEND(PlaybackState)::Stopped);
        m_player->waitFor(MDK_NS_PREPEND(PlaybackState)::Stopped);
    };
    if (value.isEmpty()) {
        realStop();
        return;
    }
    if (!value.isValid() || (value == url())) {
        return;
    }
    realStop();
    //advance(value);
    // The first url may be the same as current url.
    m_player->setMedia(nullptr);
    m_player->setMedia(qUtf8Printable(urlToString(value)));
    Q_EMIT urlChanged();
    m_player->prepare();
    if (autoStart() && !livePreview()) {
        m_player->setState(MDK_NS_PREPEND(PlaybackState)::Playing);
    }
}

void MDKPlayer::setUrls(const QList<QUrl> &value)
{
    m_player->setNextMedia(nullptr);
    if (value.isEmpty()) {
        m_urls.clear();
        Q_EMIT urlsChanged();
        m_next_it = nullptr;
        stop();
        return;
    }
    const QUrl now = url();
    const QUrl first = value.constFirst();
    if (m_urls == value) {
        if (!isPlaying()) {
            if (now.isValid()) {
                play();
            } else {
                play(first);
            }
        }
    } else {
        m_urls = value;
        Q_EMIT urlsChanged();
        if (!now.isValid()) {
            play(first);
            return;
        }
        m_next_it = std::find(m_urls.cbegin(), m_urls.cend(), now);
        if (m_next_it != m_urls.cbegin()) {
            play(first);
            return;
        }
        //advance(now);
    }
}

QList<QUrl> MDKPlayer::urls() const
{
    return m_urls;
}

bool MDKPlayer::loop() const
{
    return m_loop;
}

void MDKPlayer::setLoop(const bool value)
{
    if (m_loop != value) {
        m_loop = value;
        Q_EMIT loopChanged();
    }
}

QString MDKPlayer::fileName() const
{
    const QUrl source = url();
    return source.isValid() ? (source.isLocalFile() ? source.fileName() : source.toDisplayString())
                            : QString{};
}

QString MDKPlayer::filePath() const
{
    const QUrl source = url();
    return source.isValid() ? urlToString(source, true) : QString{};
}

qint64 MDKPlayer::position() const
{
    return isStopped() ? 0 : m_player->position();
}

void MDKPlayer::setPosition(const qint64 value)
{
    if (isStopped() || (value == position())) {
        return;
    }
    seek(value);
}

qint64 MDKPlayer::duration() const
{
    return m_mediaInfo.duration;
}

QSizeF MDKPlayer::videoSize() const
{
    const auto &vs = m_mediaInfo.videoStreams;
    if (vs.isEmpty()) {
        return {};
    }
    const auto &vsf = vs.constFirst();
    return {static_cast<qreal>(vsf.width), static_cast<qreal>(vsf.height)};
}

qreal MDKPlayer::volume() const
{
    return m_volume;
}

void MDKPlayer::setVolume(const qreal value)
{
    if (qFuzzyCompare(value, m_volume)) {
        return;
    }
    m_volume = value;
    m_player->setVolume(m_volume);
    Q_EMIT volumeChanged();
    if (!m_livePreview) {
        qDebug() << "Volume -->" << m_volume;
    }
}

bool MDKPlayer::mute() const
{
    return m_mute;
}

void MDKPlayer::setMute(const bool value)
{
    if (value == m_mute) {
        return;
    }
    m_mute = value;
    m_player->setMute(m_mute);
    Q_EMIT muteChanged();
    if (!m_livePreview) {
        qDebug() << "Mute -->" << m_mute;
    }
}

bool MDKPlayer::seekable() const
{
    // Local files are always seekable, in theory.
    return (isLoaded() && url().isLocalFile());
}

MDKPlayer::PlaybackState MDKPlayer::playbackState() const
{
    switch (m_player->state()) {
    case MDK_NS_PREPEND(PlaybackState)::Playing:
        return PlaybackState::Playing;
    case MDK_NS_PREPEND(PlaybackState)::Paused:
        return PlaybackState::Paused;
    case MDK_NS_PREPEND(PlaybackState)::Stopped:
        return PlaybackState::Stopped;
    }
    return PlaybackState::Stopped;
}

void MDKPlayer::setPlaybackState(const MDKPlayer::PlaybackState value)
{
    if (isStopped() || (value == playbackState())) {
        return;
    }
    switch (value) {
    case PlaybackState::Playing:
        m_player->setState(MDK_NS_PREPEND(PlaybackState)::Playing);
        break;
    case PlaybackState::Paused:
        m_player->setState(MDK_NS_PREPEND(PlaybackState)::Paused);
        break;
    case PlaybackState::Stopped:
        m_player->setState(MDK_NS_PREPEND(PlaybackState)::Stopped);
        break;
    }
}

MDKPlayer::MediaStatus MDKPlayer::mediaStatus() const
{
    const auto ms = m_player->mediaStatus();
    if (MDK_NS_PREPEND(test_flag)(ms & MDK_NS_PREPEND(MediaStatus)::NoMedia)) {
        return MediaStatus::NoMedia;
    }
    if (MDK_NS_PREPEND(test_flag)(ms & MDK_NS_PREPEND(MediaStatus)::Unloaded)) {
        return MediaStatus::Unloaded;
    }
    if (MDK_NS_PREPEND(test_flag)(ms & MDK_NS_PREPEND(MediaStatus)::Loading)) {
        return MediaStatus::Loading;
    }
    if (MDK_NS_PREPEND(test_flag)(ms & MDK_NS_PREPEND(MediaStatus)::Loaded)) {
        return MediaStatus::Loaded;
    }
    if (MDK_NS_PREPEND(test_flag)(ms & MDK_NS_PREPEND(MediaStatus)::Prepared)) {
        return MediaStatus::Prepared;
    }
    if (MDK_NS_PREPEND(test_flag)(ms & MDK_NS_PREPEND(MediaStatus)::Stalled)) {
        return MediaStatus::Stalled;
    }
    if (MDK_NS_PREPEND(test_flag)(ms & MDK_NS_PREPEND(MediaStatus)::Buffering)) {
        return MediaStatus::Buffering;
    }
    if (MDK_NS_PREPEND(test_flag)(ms & MDK_NS_PREPEND(MediaStatus)::Buffered)) {
        return MediaStatus::Buffered;
    }
    if (MDK_NS_PREPEND(test_flag)(ms & MDK_NS_PREPEND(MediaStatus)::End)) {
        return MediaStatus::End;
    }
    if (MDK_NS_PREPEND(test_flag)(ms & MDK_NS_PREPEND(MediaStatus)::Seeking)) {
        return MediaStatus::Seeking;
    }
    if (MDK_NS_PREPEND(test_flag)(ms & MDK_NS_PREPEND(MediaStatus)::Invalid)) {
        return MediaStatus::Invalid;
    }
    return MediaStatus::Unknown;
}

MDKPlayer::LogLevel MDKPlayer::logLevel() const
{
    switch (_MDKPlayer_MDK_LogLevel()) {
    case MDK_NS_PREPEND(LogLevel)::Off:
        return LogLevel::Off;
    case MDK_NS_PREPEND(LogLevel)::Debug:
        return LogLevel::Debug;
    case MDK_NS_PREPEND(LogLevel)::Warning:
        return LogLevel::Warning;
    case MDK_NS_PREPEND(LogLevel)::Error:
        return LogLevel::Critical;
    case MDK_NS_PREPEND(LogLevel)::Info:
        return LogLevel::Info;
    default:
        return LogLevel::Debug;
    }
}

void MDKPlayer::setLogLevel(const MDKPlayer::LogLevel value)
{
    if (value == logLevel()) {
        return;
    }
    MDK_NS_PREPEND(LogLevel) logLv = MDK_NS_PREPEND(LogLevel)::Debug;
    switch (value) {
    case LogLevel::Off:
        logLv = MDK_NS_PREPEND(LogLevel)::Off;
        break;
    case LogLevel::Debug:
        logLv = MDK_NS_PREPEND(LogLevel)::Debug;
        break;
    case LogLevel::Warning:
        logLv = MDK_NS_PREPEND(LogLevel)::Warning;
        break;
    case LogLevel::Critical:
    case LogLevel::Fatal:
        logLv = MDK_NS_PREPEND(LogLevel)::Error;
        break;
    case LogLevel::Info:
        logLv = MDK_NS_PREPEND(LogLevel)::Info;
        break;
    }
    MDK_NS_PREPEND(SetGlobalOption)("logLevel", logLv);
    Q_EMIT logLevelChanged();
    if (!m_livePreview) {
        qDebug() << "Log level -->" << value;
    }
}

qreal MDKPlayer::playbackRate() const
{
    return static_cast<qreal>(m_player->playbackRate());
}

void MDKPlayer::setPlaybackRate(const qreal value)
{
    if (isStopped() || (value == playbackRate())) {
        return;
    }
    m_player->setPlaybackRate(value);
    Q_EMIT playbackRateChanged();
    if (!m_livePreview) {
        qDebug() << "Playback rate -->" << value;
    }
}

qreal MDKPlayer::aspectRatio() const
{
    const QSizeF vs = videoSize();
    return (vs.width() / vs.height());
}

void MDKPlayer::setAspectRatio(const qreal value)
{
    if (isStopped() || (value == aspectRatio())) {
        return;
    }
    m_player->setAspectRatio(value);
    Q_EMIT aspectRatioChanged();
    if (!m_livePreview) {
        qDebug() << "Aspect ratio -->" << value;
    }
}

QString MDKPlayer::snapshotDirectory() const
{
    return QDir::toNativeSeparators(m_snapshotDirectory);
}

void MDKPlayer::setSnapshotDirectory(const QString &value)
{
    if (value.isEmpty() || (value == snapshotDirectory())) {
        return;
    }
    const QString val = QDir::toNativeSeparators(value);
    if (val == snapshotDirectory()) {
        return;
    }
    m_snapshotDirectory = val;
    Q_EMIT snapshotDirectoryChanged();
    if (!m_livePreview) {
        qDebug() << "Snapshot directory -->" << m_snapshotDirectory;
    }
}

QString MDKPlayer::snapshotFormat() const
{
    return m_snapshotFormat;
}

void MDKPlayer::setSnapshotFormat(const QString &value)
{
    if (value.isEmpty() || (value == m_snapshotFormat)) {
        return;
    }
    m_snapshotFormat = value;
    Q_EMIT snapshotFormatChanged();
    if (!m_livePreview) {
        qDebug() << "Snapshot format -->" << m_snapshotFormat;
    }
}

QString MDKPlayer::snapshotTemplate() const
{
    return m_snapshotTemplate;
}

void MDKPlayer::setSnapshotTemplate(const QString &value)
{
    if (value.isEmpty() || (value == m_snapshotTemplate)) {
        return;
    }
    m_snapshotTemplate = value;
    Q_EMIT snapshotTemplateChanged();
    if (!m_livePreview) {
        qDebug() << "Snapshot template -->" << m_snapshotTemplate;
    }
}

QStringList MDKPlayer::videoMimeTypes()
{
    return suffixesToMimeTypes(videoSuffixes());
}

QStringList MDKPlayer::audioMimeTypes()
{
    return suffixesToMimeTypes(audioSuffixes());
}

QString MDKPlayer::positionText() const
{
    return isStopped() ? QString{} : timeToString(position(), currentIsAudio());
}

QString MDKPlayer::durationText() const
{
    return isStopped() ? QString{} : timeToString(duration(), currentIsAudio());
}

bool MDKPlayer::hardwareDecoding() const
{
    return m_hardwareDecoding;
}

void MDKPlayer::setHardwareDecoding(const bool value)
{
    if (m_hardwareDecoding != value) {
        m_hardwareDecoding = value;
        if (m_hardwareDecoding) {
            setVideoDecoders(defaultVideoDecoders());
        } else {
            setVideoDecoders({QStringLiteral("FFmpeg")});
        }
        Q_EMIT hardwareDecodingChanged();
        if (!m_livePreview) {
            qDebug() << "Hardware decoding -->" << m_hardwareDecoding;
        }
    }
}

QStringList MDKPlayer::videoDecoders() const
{
    return m_videoDecoders;
}

void MDKPlayer::setVideoDecoders(const QStringList &value)
{
    if (m_videoDecoders != value) {
        m_videoDecoders = value.isEmpty() ? QStringList{QStringLiteral("FFmpeg")} : value;
        m_player->setDecoders(MDK_NS_PREPEND(MediaType)::Video, qStringListToStdStringVector(m_videoDecoders));
        Q_EMIT videoDecodersChanged();
        if (!m_livePreview) {
            qDebug() << "Video decoders -->" << m_videoDecoders;
        }
    }
}

QStringList MDKPlayer::audioDecoders() const
{
    return m_audioDecoders;
}

void MDKPlayer::setAudioDecoders(const QStringList &value)
{
    if (m_audioDecoders != value) {
        // ### FIXME: value.isEmpty() ?
        m_audioDecoders = value;
        m_player->setDecoders(MDK_NS_PREPEND(MediaType)::Audio, qStringListToStdStringVector(m_audioDecoders));
        Q_EMIT audioDecodersChanged();
        if (!m_livePreview) {
            qDebug() << "Audio decoders -->" << m_audioDecoders;
        }
    }
}

QStringList MDKPlayer::audioBackends() const
{
    return m_audioBackends;
}

void MDKPlayer::setAudioBackends(const QStringList &value)
{
    if (m_audioBackends != value) {
        // ### FIXME: value.isEmpty() ?
        m_audioBackends = value;
        m_player->setAudioBackends(qStringListToStdStringVector(m_audioBackends));
        Q_EMIT audioBackendsChanged();
        if (!m_livePreview) {
            qDebug() << "Audio backends -->" << m_audioBackends;
        }
    }
}

bool MDKPlayer::autoStart() const
{
    return m_autoStart;
}

void MDKPlayer::setAutoStart(const bool value)
{
    if (m_autoStart != value) {
        m_autoStart = value;
        Q_EMIT autoStartChanged();
        if (!m_livePreview) {
            qDebug() << "Auto start -->" << m_autoStart;
        }
    }
}

bool MDKPlayer::livePreview() const
{
    return m_livePreview;
}

void MDKPlayer::setLivePreview(const bool value)
{
    if (m_livePreview != value) {
        m_livePreview = value;
        if (m_livePreview) {
            // Disable log output, otherwise they'll mix up with the real
            // player.
            MDK_NS_PREPEND(SetGlobalOption)("logLevel", MDK_NS_PREPEND(LogLevel)::Off);
            // We only need static images.
            m_player->setState(MDK_NS_PREPEND(PlaybackState)::Paused);
            // We don't want the preview window play sound.
            m_player->setMute(true);
            // Decode as soon as possible when media data received. It also
            // ensures the maximum delay of rendered video is one second and no
            // accumulated delay.
            m_player->setBufferRange(0, 1000, true);
            // Prevent player stop playing after EOF is reached.
            m_player->setProperty("continue_at_end", "1");
            // And don't forget to use accurate seek.
        } else {
            // Restore everything to default.
            m_player->setBufferRange(1000, 2000, false);
            m_player->setMute(m_mute);
            m_player->setProperty("continue_at_end", "0");
            //MDK_NS_PREPEND(SetGlobalOption)("logLevel", MDK_NS_PREPEND(LogLevel)::Debug);
        }
        Q_EMIT livePreviewChanged();
    }
}

MDKPlayer::FillMode MDKPlayer::fillMode() const
{
    return m_fillMode;
}

void MDKPlayer::setFillMode(const MDKPlayer::FillMode value)
{
    if (m_fillMode != value) {
        m_fillMode = value;
        switch (m_fillMode) {
        case FillMode::PreserveAspectFit:
            m_player->setAspectRatio(MDK_NS_PREPEND(KeepAspectRatio));
            break;
        case FillMode::PreserveAspectCrop:
            m_player->setAspectRatio(MDK_NS_PREPEND(KeepAspectRatioCrop));
            break;
        case FillMode::Stretch:
            m_player->setAspectRatio(MDK_NS_PREPEND(IgnoreAspectRatio));
            break;
        }
        Q_EMIT fillModeChanged();
        if (!m_livePreview) {
            qDebug() << "Fill mode -->" << m_fillMode;
        }
    }
}

MDKPlayer::MediaInfo MDKPlayer::mediaInfo() const
{
    return m_mediaInfo;
}

void MDKPlayer::open(const QUrl &value)
{
    if (!value.isValid()) {
        return;
    }
    if (value != url()) {
        setUrl(value);
    }
    if (!isPlaying()) {
        play();
    }
}

void MDKPlayer::play()
{
    if (!isPaused() || !url().isValid()) {
        return;
    }
    m_player->setState(MDK_NS_PREPEND(PlaybackState)::Playing);
}

void MDKPlayer::play(const QUrl &value)
{
    if (!value.isValid()) {
        return;
    }
    const QUrl source = url();
    if ((value == source) && !isPlaying()) {
        play();
    }
    if (value != source) {
        open(value);
    }
}

void MDKPlayer::pause()
{
    if (!isPlaying()) {
        return;
    }
    m_player->setState(MDK_NS_PREPEND(PlaybackState)::Paused);
}

void MDKPlayer::stop()
{
    if (isStopped()) {
        return;
    }
    m_player->setNextMedia(nullptr);
    m_player->setState(MDK_NS_PREPEND(PlaybackState)::Stopped);
    m_player->waitFor(MDK_NS_PREPEND(PlaybackState)::Stopped);
}

void MDKPlayer::seek(const qint64 value, const bool keyFrame)
{
    if (isStopped() || (value == position())) {
        return;
    }
    // We have to seek accurately when we are in live preview mode.
    m_player->seek(qBound(qint64(0), value, duration()),
                   (!keyFrame || m_livePreview) ? MDK_NS_PREPEND(SeekFlag)::FromStart
                                                : MDK_NS_PREPEND(SeekFlag)::Default);
    if (!m_livePreview) {
        qDebug()
            << "Seek -->" << value << '='
            << qRound((static_cast<qreal>(value) / static_cast<qreal>(duration())) * 100) << '%';
    }
}

void MDKPlayer::rotateImage(const int value)
{
    if (isStopped()) {
        return;
    }
    m_player->rotate(value);
    if (!m_livePreview) {
        qDebug() << "Rotate -->" << value;
    }
}

void MDKPlayer::scaleImage(const qreal x, const qreal y)
{
    if (isStopped()) {
        return;
    }
    m_player->scale(x, y);
    if (!m_livePreview) {
        qDebug() << "Scale -->" << QSizeF{x, y};
    }
}

void MDKPlayer::snapshot()
{
    if (isStopped()) {
        return;
    }
    MDK_NS_PREPEND(Player)::SnapshotRequest snapshotRequest = {};
    m_player->snapshot(&snapshotRequest,
                       [this](MDK_NS_PREPEND(Player)::SnapshotRequest *ret, qreal frameTime) {
                           Q_UNUSED(ret);
                           const QString path = QStringLiteral("%1%2%3_%4.%5")
                                                    .arg(snapshotDirectory(),
                                                         QDir::separator(),
                                                         fileName(),
                                                         QString::number(frameTime),
                                                         snapshotFormat());
                           if (!m_livePreview) {
                               qDebug() << "Taking snapshot -->" << path;
                           }
                           return qUtf8Printable(path);
                       });
}

bool MDKPlayer::isVideo(const QUrl &value)
{
    if (value.isValid()) {
        return videoSuffixes().contains(QStringLiteral("*.")
                                            + QFileInfo(value.fileName()).suffix(),
                                        Qt::CaseInsensitive);
    }
    return false;
}

bool MDKPlayer::isAudio(const QUrl &value)
{
    if (value.isValid()) {
        return audioSuffixes().contains(QStringLiteral("*.")
                                            + QFileInfo(value.fileName()).suffix(),
                                        Qt::CaseInsensitive);
    }
    return false;
}

bool MDKPlayer::currentIsVideo() const
{
    if (isStopped()) {
        return false;
    }
    return isVideo(url());
}

bool MDKPlayer::currentIsAudio() const
{
    if (isStopped()) {
        return false;
    }
    return isAudio(url());
}

void MDKPlayer::seekBackward(const int value)
{
    if (isStopped()) {
        return;
    }
    seek(position() - qAbs(value), false);
}

void MDKPlayer::seekForward(const int value)
{
    if (isStopped()) {
        return;
    }
    seek(position() + qAbs(value), false);
}

void MDKPlayer::playPrevious()
{
    if (isStopped() || (m_urls.count() < 2)) {
        return;
    }
    auto it = std::find(m_urls.cbegin(), m_urls.cend(), url());
    if (it == m_urls.cbegin()) {
        it = m_urls.cend();
    }
    --it;
    play(*it);
}

void MDKPlayer::playNext()
{
    if (isStopped() || (m_urls.count() < 2)) {
        return;
    }
    auto it = std::find(m_urls.cbegin(), m_urls.cend(), url());
    if (it != m_urls.cend()) {
        ++it;
    }
    if (it == m_urls.cend()) {
        it = m_urls.cbegin();
    }
    play(*it);
}

void MDKPlayer::startRecording(const QUrl &value, const QString &format)
{
    if (value.isValid() && value.isLocalFile()) {
        // If media is not loaded, recorder will start when playback starts.
        const QString path = urlToString(value);
        m_player->record(qUtf8Printable(path), format.isEmpty() ? nullptr : qUtf8Printable(format));
        if (!m_livePreview) {
            qDebug() << "Start recording -->" << path;
        }
    }
}

void MDKPlayer::stopRecording()
{
    m_player->record();
    if (!m_livePreview) {
        qDebug() << "Recording stopped.";
    }
}

void MDKPlayer::timerEvent(QTimerEvent *event)
{
    QQuickItem::timerEvent(event);
    if (!isStopped()) {
        Q_EMIT positionChanged();
    }
}

void MDKPlayer::initMdkHandlers()
{
    MDK_NS_PREPEND(setLogHandler)([this](MDK_NS_PREPEND(LogLevel) level, const char *msg) {
        if (m_livePreview) {
            return;
        }
        switch (level) {
        case MDK_NS_PREPEND(LogLevel)::Info:
            qInfo() << msg;
            break;
        case MDK_NS_PREPEND(LogLevel)::All:
        case MDK_NS_PREPEND(LogLevel)::Debug:
            qDebug() << msg;
            break;
        case MDK_NS_PREPEND(LogLevel)::Warning:
            qWarning() << msg;
            break;
        case MDK_NS_PREPEND(LogLevel)::Error:
            qCritical() << msg;
            break;
        default:
            break;
        }
    });
    m_player->currentMediaChanged([this] {
        const QUrl now = url();
        if (!now.isValid()) {
            return;
        }
        advance(now);
        if (!m_livePreview) {
            qDebug() << "Current media -->" << urlToString(now, true);
        }
        Q_EMIT urlChanged();
    });
    m_player->onMediaStatusChanged([this](MDK_NS_PREPEND(MediaStatus) ms) {
        if (MDK_NS_PREPEND(flags_added)(m_mediaStatus, ms, MDK_NS_PREPEND(MediaStatus)::Loaded)) {
            const auto &info = m_player->mediaInfo();
            m_mediaInfo.startTime = info.start_time;
            m_mediaInfo.duration = info.duration;
            m_mediaInfo.bitRate = info.bit_rate;
            m_mediaInfo.fileSize = info.size;
            m_mediaInfo.format = QString::fromUtf8(info.format);
            m_mediaInfo.streamCount = info.streams;
            m_hasVideo = !info.video.empty();
            if (m_hasVideo) {
                m_mediaInfo.videoStreams.clear();
                for (auto &&vsi : qAsConst(info.video)) {
                    VideoStreamInfo vsinfo = {};
                    vsinfo.index = vsi.index;
                    vsinfo.startTime = vsi.start_time;
                    vsinfo.duration = vsi.duration;
                    const auto &codec = vsi.codec;
                    vsinfo.codec = QString::fromUtf8(codec.codec);
                    vsinfo.bitRate = codec.bit_rate;
                    vsinfo.frameRate = codec.frame_rate;
                    vsinfo.format = QString::fromUtf8(codec.format_name);
                    vsinfo.width = codec.width;
                    vsinfo.height = codec.height;
                    const auto &metaData = vsi.metadata;
                    if (!metaData.empty()) {
                        for (auto &&data : qAsConst(metaData)) {
                            vsinfo.metaData.insert(QString::fromStdString(data.first),
                                                   QString::fromStdString(data.second));
                        }
                    }
                    m_mediaInfo.videoStreams.append(vsinfo);
                }
                Q_EMIT videoSizeChanged();
            }
            m_hasAudio = !info.audio.empty();
            if (m_hasAudio) {
                m_mediaInfo.audioStreams.clear();
                for (auto &&asi : qAsConst(info.audio)) {
                    AudioStreamInfo asinfo = {};
                    asinfo.index = asi.index;
                    asinfo.startTime = asi.start_time;
                    asinfo.duration = asi.duration;
                    const auto &codec = asi.codec;
                    asinfo.codec = QString::fromUtf8(codec.codec);
                    asinfo.bitRate = codec.bit_rate;
                    asinfo.frameRate = codec.frame_rate;
                    asinfo.channels = codec.channels;
                    asinfo.sampleRate = codec.sample_rate;
                    const auto &metaData = asi.metadata;
                    if (!metaData.empty()) {
                        for (auto &&data : qAsConst(metaData)) {
                            asinfo.metaData.insert(QString::fromStdString(data.first),
                                                   QString::fromStdString(data.second));
                        }
                    }
                    m_mediaInfo.audioStreams.append(asinfo);
                }
            }
            // ### TODO: m_hasSubtitle = ...
            // ### TODO: if (m_hasSubtitle) { ... }
            m_hasChapters = !info.chapters.empty();
            if (m_hasChapters) {
                m_mediaInfo.chapters.clear();
                for (auto &&chapter : qAsConst(info.chapters)) {
                    ChapterInfo cpinfo = {};
                    cpinfo.beginTime = chapter.start_time;
                    cpinfo.endTime = chapter.end_time;
                    cpinfo.title = QString::fromStdString(chapter.title);
                    m_mediaInfo.chapters.append(cpinfo);
                }
            }
            if (!info.metadata.empty()) {
                m_mediaInfo.metaData.clear();
                for (auto &&data : qAsConst(info.metadata)) {
                    m_mediaInfo.metaData.insert(QString::fromStdString(data.first),
                                                QString::fromStdString(data.second));
                }
            }
            Q_EMIT positionChanged();
            Q_EMIT durationChanged();
            Q_EMIT seekableChanged();
            Q_EMIT mediaInfoChanged();
            Q_EMIT loaded();
            if (!m_livePreview) {
                qDebug() << "Media loaded.";
            }
        }
        m_mediaStatus = ms;
        Q_EMIT mediaStatusChanged();
        return true;
    });
    m_player->onEvent([this](const MDK_NS_PREPEND(MediaEvent) &me) {
        if (!m_livePreview) {
            qDebug() << "MDK event:" << me.category.data() << me.detail.data();
        }
        return false;
    });
    m_player->onLoop([this](int count) {
        if (!m_livePreview) {
            qDebug() << "loop:" << count;
        }
        return false;
    });
    m_player->onStateChanged([this](MDK_NS_PREPEND(PlaybackState) pbs) {
        Q_EMIT playbackStateChanged();
        if (pbs == MDK_NS_PREPEND(PlaybackState)::Playing) {
            Q_EMIT playing();
            if (!m_livePreview) {
                qDebug() << "Start playing.";
            }
        }
        if (pbs == MDK_NS_PREPEND(PlaybackState)::Paused) {
            Q_EMIT paused();
            if (!m_livePreview) {
                qDebug() << "Paused.";
            }
        }
        if (pbs == MDK_NS_PREPEND(PlaybackState)::Stopped) {
            resetInternalData();
            Q_EMIT stopped();
            if (!m_livePreview) {
                qDebug() << "Stopped.";
            }
        }
    });
}

void MDKPlayer::resetInternalData()
{
    // Make sure MDKPlayer::url() returns empty.
    m_player->setMedia(nullptr);
#if 0
    advance();
    if (m_next_it == nullptr) {
        m_urls.clear();
        Q_EMIT urlsChanged();
    } else {
        if (m_next_it == m_urls.cbegin()) {
            m_next_it = m_urls.cend();
        }
        --m_next_it;
    }
#endif
    // ----------------
    m_hasVideo = false;
    m_hasAudio = false;
    m_hasSubtitle = false;
    m_hasChapters = false;
    //m_loop = false;
    m_mediaInfo = {};
    m_mediaStatus = MDK_NS_PREPEND(MediaStatus)::NoMedia;
    Q_EMIT urlChanged();
    Q_EMIT positionChanged();
    Q_EMIT durationChanged();
    Q_EMIT seekableChanged();
    Q_EMIT mediaInfoChanged();
    //Q_EMIT loopChanged();
    Q_EMIT mediaStatusChanged();
}

void MDKPlayer::advance()
{
    if (m_next_it == nullptr) {
        return;
    }
    if (m_next_it != m_urls.cend()) {
        ++m_next_it;
    }
    if (m_next_it == m_urls.cend()) {
        if (m_loop) {
            m_next_it = m_urls.cbegin();
        } else {
            m_next_it = nullptr;
        }
    }
}

void MDKPlayer::advance(const QUrl &value)
{
    if (value.isValid()) {
        m_next_it = std::find(m_urls.cbegin(), m_urls.cend(), value);
        advance();
    }
    m_player->setNextMedia(nullptr);
    if (m_next_it == nullptr) {
        return;
    }
    const QUrl nextUrl = *m_next_it;
    if (nextUrl.isValid()) {
        m_player->setNextMedia(qUtf8Printable(urlToString(nextUrl)));
    }
    advance();
}

bool MDKPlayer::isLoaded() const
{
    return !isStopped();
}

bool MDKPlayer::isPlaying() const
{
    return (m_player->state() == MDK_NS_PREPEND(PlaybackState)::Playing);
}

bool MDKPlayer::isPaused() const
{
    return (m_player->state() == MDK_NS_PREPEND(PlaybackState)::Paused);
}

bool MDKPlayer::isStopped() const
{
    return (m_player->state() == MDK_NS_PREPEND(PlaybackState)::Stopped);
}

MDKPLAYER_END_NAMESPACE
