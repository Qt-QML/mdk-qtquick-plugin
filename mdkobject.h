/*
 * MIT License
 *
 * Copyright (C) 2020 by wangwenx190 (Yuhang Zhao)
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

#pragma once

#include "mdk/global.h"
#include <QHash>
#include <QList>
#include <QLoggingCategory>
#include <QQuickItem>
#include <QUrl>

Q_DECLARE_LOGGING_CATEGORY(lcMdk)
Q_DECLARE_LOGGING_CATEGORY(lcMdkLog)
Q_DECLARE_LOGGING_CATEGORY(lcMdkRenderer)
#ifdef Q_OS_WINDOWS
Q_DECLARE_LOGGING_CATEGORY(lcMdkD3D12Renderer)
Q_DECLARE_LOGGING_CATEGORY(lcMdkD3D11Renderer)
#endif
Q_DECLARE_LOGGING_CATEGORY(lcMdkVulkanRenderer)
#ifdef Q_OS_MACOS
Q_DECLARE_LOGGING_CATEGORY(lcMdkMetalRenderer)
#endif
Q_DECLARE_LOGGING_CATEGORY(lcMdkOpenGLRenderer)
Q_DECLARE_LOGGING_CATEGORY(lcMdkPlayback)
Q_DECLARE_LOGGING_CATEGORY(lcMdkMisc)

namespace MDK_NS {
QT_FORWARD_DECLARE_CLASS(Player)
} // namespace MDK_NS

QT_FORWARD_DECLARE_CLASS(VideoTextureNode)

class MdkObject : public QQuickItem
{
    Q_OBJECT
    QML_ELEMENT
    Q_DISABLE_COPY_MOVE(MdkObject)

    Q_PROPERTY(QUrl url READ url WRITE setUrl NOTIFY urlChanged)
    Q_PROPERTY(QList<QUrl> urls READ urls WRITE setUrls NOTIFY urlsChanged)
    Q_PROPERTY(QString fileName READ fileName NOTIFY fileNameChanged)
    Q_PROPERTY(QString path READ path NOTIFY pathChanged)
    Q_PROPERTY(qint64 position READ position WRITE setPosition NOTIFY positionChanged)
    Q_PROPERTY(qint64 duration READ duration NOTIFY durationChanged)
    Q_PROPERTY(QSize videoSize READ videoSize NOTIFY videoSizeChanged)
    Q_PROPERTY(qreal volume READ volume WRITE setVolume NOTIFY volumeChanged)
    Q_PROPERTY(bool mute READ mute WRITE setMute NOTIFY muteChanged)
    Q_PROPERTY(bool seekable READ seekable NOTIFY seekableChanged)
    Q_PROPERTY(PlaybackState playbackState READ playbackState WRITE setPlaybackState NOTIFY
                   playbackStateChanged)
    Q_PROPERTY(MediaStatus mediaStatus READ mediaStatus NOTIFY mediaStatusChanged)
    Q_PROPERTY(LogLevel logLevel READ logLevel WRITE setLogLevel NOTIFY logLevelChanged)
    Q_PROPERTY(qreal playbackRate READ playbackRate WRITE setPlaybackRate NOTIFY playbackRateChanged)
    Q_PROPERTY(qreal aspectRatio READ aspectRatio WRITE setAspectRatio NOTIFY aspectRatioChanged)
    Q_PROPERTY(QString snapshotDirectory READ snapshotDirectory WRITE setSnapshotDirectory NOTIFY
                   snapshotDirectoryChanged)
    Q_PROPERTY(QString snapshotFormat READ snapshotFormat WRITE setSnapshotFormat NOTIFY
                   snapshotFormatChanged)
    Q_PROPERTY(QString snapshotTemplate READ snapshotTemplate WRITE setSnapshotTemplate NOTIFY
                   snapshotTemplateChanged)
    Q_PROPERTY(QStringList videoSuffixes READ videoSuffixes CONSTANT)
    Q_PROPERTY(QStringList audioSuffixes READ audioSuffixes CONSTANT)
    Q_PROPERTY(QStringList subtitleSuffixes READ subtitleSuffixes CONSTANT)
    Q_PROPERTY(QStringList mediaSuffixes READ mediaSuffixes CONSTANT)
    Q_PROPERTY(QStringList videoMimeTypes READ videoMimeTypes CONSTANT)
    Q_PROPERTY(QStringList audioMimeTypes READ audioMimeTypes CONSTANT)
    Q_PROPERTY(QStringList mediaMimeTypes READ mediaMimeTypes CONSTANT)
    Q_PROPERTY(QString positionText READ positionText NOTIFY positionTextChanged)
    Q_PROPERTY(QString durationText READ durationText NOTIFY durationTextChanged)
    Q_PROPERTY(bool hardwareDecoding READ hardwareDecoding WRITE setHardwareDecoding NOTIFY
                   hardwareDecodingChanged)
    Q_PROPERTY(QStringList videoDecoders READ videoDecoders WRITE setVideoDecoders NOTIFY
                   videoDecodersChanged)
    Q_PROPERTY(QStringList audioDecoders READ audioDecoders WRITE setAudioDecoders NOTIFY
                   audioDecodersChanged)
    Q_PROPERTY(QStringList defaultVideoDecoders READ defaultVideoDecoders CONSTANT)
    Q_PROPERTY(QStringList defaultAudioDecoders READ defaultAudioDecoders CONSTANT)
    Q_PROPERTY(QStringList audioBackends READ audioBackends WRITE setAudioBackends NOTIFY
                   audioBackendsChanged)
    Q_PROPERTY(bool autoStart READ autoStart WRITE setAutoStart NOTIFY autoStartChanged)
    Q_PROPERTY(bool livePreview READ livePreview WRITE setLivePreview NOTIFY livePreviewChanged)
    Q_PROPERTY(VideoBackend videoBackend READ videoBackend CONSTANT)
    Q_PROPERTY(FillMode fillMode READ fillMode WRITE setFillMode NOTIFY fillModeChanged)
    Q_PROPERTY(MediaInfo mediaInfo READ mediaInfo NOTIFY mediaInfoChanged)
    Q_PROPERTY(bool loop READ loop WRITE setLoop NOTIFY loopChanged)

public:
    enum class PlaybackState { Stopped, Playing, Paused };
    Q_ENUM(PlaybackState)

    enum class MediaStatus {
        Unknown,
        NoMedia,
        Unloaded,
        Loading,
        Loaded,
        Prepared,
        Stalled,
        Buffering,
        Buffered,
        End,
        Seeking,
        Invalid
    };
    Q_ENUM(MediaStatus)

    enum class LogLevel { Off, Debug, Warning, Critical, Fatal, Info };
    Q_ENUM(LogLevel)

    struct ChapterInfo
    {
        qint64 beginTime = 0, endTime = 0;
        QString title = QString();
    };
    using Chapters = QList<ChapterInfo>;

    using MetaData = QHash<QString, QString>;

    enum class VideoBackend { Auto, D3D12, D3D11, Vulkan, Metal, OpenGL };
    Q_ENUM(VideoBackend)

    enum class FillMode { PreserveAspectFit, PreserveAspectCrop, Stretch };
    Q_ENUM(FillMode)

    struct VideoStreamInfo
    {
        int index = 0;
        qint64 startTime = 0;
        qint64 duration = 0;
        QString codec = QString();
        qint64 bitRate = 0;
        qreal frameRate = 0.0;
        QString format = QString();
        int width = 0;
        int height = 0;
        MetaData metaData = {};
    };

    struct AudioStreamInfo
    {
        int index = 0;
        qint64 startTime = 0;
        qint64 duration = 0;
        QString codec = QString();
        qint64 bitRate = 0;
        qreal frameRate = 0.0;
        int channels = 0;
        int sampleRate = 0;
        MetaData metaData = {};
    };

    struct MediaInfo
    {
        qint64 startTime = 0;
        qint64 duration = 0;
        qint64 bitRate = 0;
        qint64 fileSize = 0;
        QString format = QString();
        int streamCount = 0;
        Chapters chapters = {};
        MetaData metaData = {};
        QList<VideoStreamInfo> videoStreams = {};
        QList<AudioStreamInfo> audioStreams = {};
    };

    explicit MdkObject(QQuickItem *parent = nullptr);
    ~MdkObject() override;

    QUrl url() const;
    void setUrl(const QUrl &value);

    QList<QUrl> urls() const;
    void setUrls(const QList<QUrl> &value);

    QString fileName() const;

    QString path() const;

    qint64 position() const;
    void setPosition(const qint64 value);

    qint64 duration() const;

    QSize videoSize() const;

    qreal volume() const;
    void setVolume(const qreal value);

    bool mute() const;
    void setMute(const bool value);

    bool seekable() const;

    PlaybackState playbackState() const;
    void setPlaybackState(const PlaybackState value);

    MediaStatus mediaStatus() const;

    LogLevel logLevel() const;
    void setLogLevel(const LogLevel value);

    qreal playbackRate() const;
    void setPlaybackRate(const qreal value);

    qreal aspectRatio() const;
    void setAspectRatio(const qreal value);

    QString snapshotDirectory() const;
    void setSnapshotDirectory(const QString &value);

    QString snapshotFormat() const;
    void setSnapshotFormat(const QString &value);

    QString snapshotTemplate() const;
    void setSnapshotTemplate(const QString &value);

    static QStringList videoSuffixes()
    {
        return QStringList{QString::fromUtf8("*.3g2"),   QString::fromUtf8("*.3ga"),
                           QString::fromUtf8("*.3gp"),   QString::fromUtf8("*.3gp2"),
                           QString::fromUtf8("*.3gpp"),  QString::fromUtf8("*.amv"),
                           QString::fromUtf8("*.asf"),   QString::fromUtf8("*.asx"),
                           QString::fromUtf8("*.avf"),   QString::fromUtf8("*.avi"),
                           QString::fromUtf8("*.bdm"),   QString::fromUtf8("*.bdmv"),
                           QString::fromUtf8("*.bik"),   QString::fromUtf8("*.clpi"),
                           QString::fromUtf8("*.cpi"),   QString::fromUtf8("*.dat"),
                           QString::fromUtf8("*.divx"),  QString::fromUtf8("*.drc"),
                           QString::fromUtf8("*.dv"),    QString::fromUtf8("*.dvr-ms"),
                           QString::fromUtf8("*.f4v"),   QString::fromUtf8("*.flv"),
                           QString::fromUtf8("*.gvi"),   QString::fromUtf8("*.gxf"),
                           QString::fromUtf8("*.hdmov"), QString::fromUtf8("*.hlv"),
                           QString::fromUtf8("*.iso"),   QString::fromUtf8("*.letv"),
                           QString::fromUtf8("*.lrv"),   QString::fromUtf8("*.m1v"),
                           QString::fromUtf8("*.m2p"),   QString::fromUtf8("*.m2t"),
                           QString::fromUtf8("*.m2ts"),  QString::fromUtf8("*.m2v"),
                           QString::fromUtf8("*.m3u"),   QString::fromUtf8("*.m3u8"),
                           QString::fromUtf8("*.m4v"),   QString::fromUtf8("*.mkv"),
                           QString::fromUtf8("*.moov"),  QString::fromUtf8("*.mov"),
                           QString::fromUtf8("*.mp2"),   QString::fromUtf8("*.mp2v"),
                           QString::fromUtf8("*.mp4"),   QString::fromUtf8("*.mp4v"),
                           QString::fromUtf8("*.mpe"),   QString::fromUtf8("*.mpeg"),
                           QString::fromUtf8("*.mpeg1"), QString::fromUtf8("*.mpeg2"),
                           QString::fromUtf8("*.mpeg4"), QString::fromUtf8("*.mpg"),
                           QString::fromUtf8("*.mpl"),   QString::fromUtf8("*.mpls"),
                           QString::fromUtf8("*.mpv"),   QString::fromUtf8("*.mpv2"),
                           QString::fromUtf8("*.mqv"),   QString::fromUtf8("*.mts"),
                           QString::fromUtf8("*.mtv"),   QString::fromUtf8("*.mxf"),
                           QString::fromUtf8("*.mxg"),   QString::fromUtf8("*.nsv"),
                           QString::fromUtf8("*.nuv"),   QString::fromUtf8("*.ogm"),
                           QString::fromUtf8("*.ogv"),   QString::fromUtf8("*.ogx"),
                           QString::fromUtf8("*.ps"),    QString::fromUtf8("*.qt"),
                           QString::fromUtf8("*.qtvr"),  QString::fromUtf8("*.ram"),
                           QString::fromUtf8("*.rec"),   QString::fromUtf8("*.rm"),
                           QString::fromUtf8("*.rmj"),   QString::fromUtf8("*.rmm"),
                           QString::fromUtf8("*.rms"),   QString::fromUtf8("*.rmvb"),
                           QString::fromUtf8("*.rmx"),   QString::fromUtf8("*.rp"),
                           QString::fromUtf8("*.rpl"),   QString::fromUtf8("*.rv"),
                           QString::fromUtf8("*.rvx"),   QString::fromUtf8("*.thp"),
                           QString::fromUtf8("*.tod"),   QString::fromUtf8("*.tp"),
                           QString::fromUtf8("*.trp"),   QString::fromUtf8("*.ts"),
                           QString::fromUtf8("*.tts"),   QString::fromUtf8("*.txd"),
                           QString::fromUtf8("*.vcd"),   QString::fromUtf8("*.vdr"),
                           QString::fromUtf8("*.vob"),   QString::fromUtf8("*.vp8"),
                           QString::fromUtf8("*.vro"),   QString::fromUtf8("*.webm"),
                           QString::fromUtf8("*.wm"),    QString::fromUtf8("*.wmv"),
                           QString::fromUtf8("*.wtv"),   QString::fromUtf8("*.xesc"),
                           QString::fromUtf8("*.xspf")};
    }

    static QStringList audioSuffixes()
    {
        return QStringList{QString::fromUtf8("*.mp3"),
                           QString::fromUtf8("*.aac"),
                           QString::fromUtf8("*.mka"),
                           QString::fromUtf8("*.dts"),
                           QString::fromUtf8("*.flac"),
                           QString::fromUtf8("*.ogg"),
                           QString::fromUtf8("*.m4a"),
                           QString::fromUtf8("*.ac3"),
                           QString::fromUtf8("*.opus"),
                           QString::fromUtf8("*.wav"),
                           QString::fromUtf8("*.wv")};
    }

    static QStringList subtitleSuffixes()
    {
        return QStringList{QString::fromUtf8("*.utf"),
                           QString::fromUtf8("*.utf8"),
                           QString::fromUtf8("*.utf-8"),
                           QString::fromUtf8("*.idx"),
                           QString::fromUtf8("*.sub"),
                           QString::fromUtf8("*.srt"),
                           QString::fromUtf8("*.rt"),
                           QString::fromUtf8("*.ssa"),
                           QString::fromUtf8("*.ass"),
                           QString::fromUtf8("*.mks"),
                           QString::fromUtf8("*.vtt"),
                           QString::fromUtf8("*.sup"),
                           QString::fromUtf8("*.scc"),
                           QString::fromUtf8("*.smi")};
    }

    static QStringList mediaSuffixes()
    {
        QStringList suffixes{};
        suffixes.append(videoSuffixes());
        suffixes.append(audioSuffixes());
        return suffixes;
    }

    static QStringList videoMimeTypes();

    static QStringList audioMimeTypes();

    static QStringList mediaMimeTypes()
    {
        QStringList mimeTypes{};
        mimeTypes.append(videoMimeTypes());
        mimeTypes.append(audioMimeTypes());
        return mimeTypes;
    }

    QString positionText() const;

    QString durationText() const;

    bool hardwareDecoding() const;
    void setHardwareDecoding(const bool value);

    QStringList videoDecoders() const;
    void setVideoDecoders(const QStringList &value);

    QStringList audioDecoders() const;
    void setAudioDecoders(const QStringList &value);

    // The order is important. Only FFmpeg is software decoding.
    QStringList defaultVideoDecoders() const
    {
#ifdef Q_OS_WINDOWS
        return QStringList{QString::fromUtf8("MFT:d3d=11"),
                           QString::fromUtf8("MFT:d3d=9"),
                           QString::fromUtf8("MFT"),
                           QString::fromUtf8("D3D11"),
                           QString::fromUtf8("DXVA"),
                           QString::fromUtf8("CUDA"),
                           QString::fromUtf8("NVDEC"),
                           QString::fromUtf8("FFmpeg")};
#elif defined(Q_OS_LINUX)
#ifdef Q_OS_ANDROID
        return QStringList{QString::fromUtf8("AMediaCodec"), QString::fromUtf8("FFmpeg")};
#else
        return QStringList{QString::fromUtf8("VAAPI"),
                           QString::fromUtf8("VDPAU"),
                           QString::fromUtf8("CUDA"),
                           QString::fromUtf8("NVDEC"),
                           QString::fromUtf8("FFmpeg")};
#endif
#elif defined(Q_OS_DARWIN)
        return QStringList{QString::fromUtf8("VT"),
                           QString::fromUtf8("VideoToolbox"),
                           QString::fromUtf8("FFmpeg")};
#endif
    }

    QStringList defaultAudioDecoders() const { return {}; }

    QStringList audioBackends() const;
    // Available audio backends: XAudio2 (Windows only), ALSA (Linux only),
    // AudioQueue (Apple only), OpenSL (Android only), OpenAL
    void setAudioBackends(const QStringList &value);

    bool autoStart() const;
    void setAutoStart(const bool value);

    bool livePreview() const;
    void setLivePreview(const bool value);

    // The video backend can't be changed at run-time.
    VideoBackend videoBackend() const;

    FillMode fillMode() const;
    void setFillMode(const FillMode value);

    MediaInfo mediaInfo() const;

    bool loop() const;
    void setLoop(const bool value);

public Q_SLOTS:
    void open(const QUrl &value);
    void play();
    void play(const QUrl &value);
    void pause();
    void stop();
    // Key frame seeking is the fastest seeking but it's not accurate, for example,
    // if you want to jump to the 235 frame, the player may end up at the 248 frame
    // because the 248 frame is the nearest key frame.
    // To avoid this inaccuracy, you should not use key frame seeking, however,
    // accurate seeking will take more time because the player needs to decode the
    // media to get the image data to show (if you are not jumping to a key frame).
    void seek(const qint64 value, const bool keyFrame = true);
    // Avoid naming conflicts of QQuickItem's own functions.
    void rotateImage(const int value);
    void scaleImage(const qreal x, const qreal y);
    void snapshot();
    bool isLoaded() const;
    bool isPlaying() const;
    bool isPaused() const;
    bool isStopped() const;
    static bool isVideo(const QUrl &value);
    static bool isAudio(const QUrl &value);
    static bool isMedia(const QUrl &value);
    bool currentIsVideo() const;
    bool currentIsAudio() const;
    bool currentIsMedia() const;
    void startRecording(const QUrl &value, const QString &format = QString());
    void stopRecording();
    void seekBackward(const int value = 5000);
    void seekForward(const int value = 5000);
    void playPrevious();
    void playNext();

protected:
    void timerEvent(QTimerEvent *event) override;
    QSGNode *updatePaintNode(QSGNode *node, UpdatePaintNodeData *data) override;
    void geometryChanged(const QRectF &newGeometry, const QRectF &oldGeometry) override;

private Q_SLOTS:
    void invalidateSceneGraph();

private:
    void releaseResources() override;
    void initMdkHandlers();
    void resetInternalData();
    void advance();
    void advance(const QUrl &value);

Q_SIGNALS:
    void loaded();
    void playing();
    void paused();
    void stopped();
    void urlChanged();
    void urlsChanged();
    void fileNameChanged();
    void pathChanged();
    void positionChanged();
    void durationChanged();
    void videoSizeChanged();
    void volumeChanged();
    void muteChanged();
    void seekableChanged();
    void playbackStateChanged();
    void mediaStatusChanged();
    void logLevelChanged();
    void playbackRateChanged();
    void aspectRatioChanged();
    void snapshotDirectoryChanged();
    void snapshotFormatChanged();
    void snapshotTemplateChanged();
    void positionTextChanged();
    void durationTextChanged();
    void hardwareDecodingChanged();
    void videoDecodersChanged();
    void audioDecodersChanged();
    void audioBackendsChanged();
    void autoStartChanged();
    void livePreviewChanged();
    void fillModeChanged();
    void mediaInfoChanged();
    void loopChanged();
    void newHistory(const QUrl &param1, const qint64 param2);

private:
    friend class VideoTextureNode;
    VideoTextureNode *m_node = nullptr;
    QList<QUrl> m_urls = {};
    QList<QUrl>::const_iterator m_next_it = nullptr;
    QSharedPointer<MDK_NS::Player> m_player;
    qreal m_volume = 1.0;
    bool m_mute = false, m_hasVideo = false, m_hasAudio = false, m_hasSubtitle = false,
         m_hasChapters = false, m_hardwareDecoding = false, m_autoStart = true,
         m_livePreview = false, m_loop = false;
    QString m_snapshotDirectory = QString(), m_snapshotFormat = QString::fromUtf8("jpg"),
            m_snapshotTemplate = QString();
    QStringList m_videoDecoders = {}, m_audioDecoders = {}, m_audioBackends = {};
    FillMode m_fillMode = FillMode::PreserveAspectFit;
    MediaInfo m_mediaInfo = {};
    MDK_NS::MediaStatus m_mediaStatus = MDK_NS::MediaStatus::NoMedia;
};

Q_DECLARE_METATYPE(MdkObject::ChapterInfo)
Q_DECLARE_METATYPE(MdkObject::VideoStreamInfo)
Q_DECLARE_METATYPE(MdkObject::AudioStreamInfo)
Q_DECLARE_METATYPE(MdkObject::MediaInfo)
