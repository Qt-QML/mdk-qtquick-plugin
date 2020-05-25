#pragma once

#include <QQuickFramebufferObject>
#include <QUrl>
#include <mdk/Player.h>

class MdkObject : public QQuickFramebufferObject {
    Q_OBJECT
    QML_ELEMENT
    Q_DISABLE_COPY_MOVE(MdkObject)

    Q_PROPERTY(QUrl source READ source WRITE setSource NOTIFY sourceChanged)
    Q_PROPERTY(QString fileName READ fileName NOTIFY fileNameChanged)
    Q_PROPERTY(QString path READ path NOTIFY pathChanged)
    Q_PROPERTY(
        qint64 position READ position WRITE setPosition NOTIFY positionChanged)
    Q_PROPERTY(qint64 duration READ duration NOTIFY durationChanged)
    Q_PROPERTY(QSize videoSize READ videoSize NOTIFY videoSizeChanged)
    Q_PROPERTY(qreal volume READ volume WRITE setVolume NOTIFY volumeChanged)
    Q_PROPERTY(bool mute READ mute WRITE setMute NOTIFY muteChanged)
    Q_PROPERTY(bool seekable READ seekable NOTIFY seekableChanged)
    Q_PROPERTY(MdkObject::PlaybackState playbackState READ playbackState WRITE
                   setPlaybackState NOTIFY playbackStateChanged)
    Q_PROPERTY(MdkObject::MediaStatus mediaStatus READ mediaStatus NOTIFY
                   mediaStatusChanged)
    Q_PROPERTY(MdkObject::LogLevel logLevel READ logLevel WRITE setLogLevel
                   NOTIFY logLevelChanged)
    Q_PROPERTY(qreal playbackRate READ playbackRate WRITE setPlaybackRate NOTIFY
                   playbackRateChanged)
    Q_PROPERTY(qreal aspectRatio READ aspectRatio WRITE setAspectRatio NOTIFY
                   aspectRatioChanged)
    Q_PROPERTY(QString snapshotDirectory READ snapshotDirectory WRITE
                   setSnapshotDirectory NOTIFY snapshotDirectoryChanged)
    Q_PROPERTY(QString snapshotFormat READ snapshotFormat WRITE
                   setSnapshotFormat NOTIFY snapshotFormatChanged)
    Q_PROPERTY(QString snapshotTemplate READ snapshotTemplate WRITE
                   setSnapshotTemplate NOTIFY snapshotTemplateChanged)
    Q_PROPERTY(QStringList videoSuffixes READ videoSuffixes CONSTANT)
    Q_PROPERTY(QStringList audioSuffixes READ audioSuffixes CONSTANT)
    Q_PROPERTY(QStringList subtitleSuffixes READ subtitleSuffixes CONSTANT)
    Q_PROPERTY(QStringList mediaSuffixes READ mediaSuffixes CONSTANT)
    Q_PROPERTY(QStringList videoMimeTypes READ videoMimeTypes CONSTANT)
    Q_PROPERTY(QStringList audioMimeTypes READ audioMimeTypes CONSTANT)
    Q_PROPERTY(QStringList mediaMimeTypes READ mediaMimeTypes CONSTANT)

public:
    enum class PlaybackState { Stopped, Playing, Paused };
    Q_ENUM(PlaybackState)

    enum class MediaStatus {
        Unknown,
        NoMedia,
        Loading,
        Loaded,
        Stalled,
        Buffering,
        Buffered,
        End,
        Invalid
    };
    Q_ENUM(MediaStatus)

    enum class LogLevel { Off, Debug, Warning, Critical, Fatal, Info };
    Q_ENUM(LogLevel)

    explicit MdkObject(QQuickItem *parent = nullptr);
    ~MdkObject() override;

    Renderer *createRenderer() const override;

    void renderVideo();
    void setVideoSurfaceSize(QSize size);

    QUrl source() const;
    void setSource(const QUrl &value);

    QString fileName() const;

    QString path() const;

    qint64 position() const;
    void setPosition(qint64 value);

    qint64 duration() const;

    QSize videoSize() const;

    qreal volume() const;
    void setVolume(qreal value);

    bool mute() const;
    void setMute(bool value);

    bool seekable() const;

    MdkObject::PlaybackState playbackState() const;
    void setPlaybackState(MdkObject::PlaybackState value);

    MdkObject::MediaStatus mediaStatus() const;

    MdkObject::LogLevel logLevel() const;
    void setLogLevel(MdkObject::LogLevel value);

    qreal playbackRate() const;
    void setPlaybackRate(qreal value);

    qreal aspectRatio() const;
    void setAspectRatio(qreal value);

    QString snapshotDirectory() const;
    void setSnapshotDirectory(const QString &value);

    QString snapshotFormat() const;
    void setSnapshotFormat(const QString &value);

    QString snapshotTemplate() const;
    void setSnapshotTemplate(const QString &value);

    QStringList videoSuffixes() const {
        return QStringList{
            QString::fromUtf8("*.3g2"),   QString::fromUtf8("*.3ga"),
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

    QStringList audioSuffixes() const {
        return QStringList{
            QString::fromUtf8("*.mp3"),  QString::fromUtf8("*.aac"),
            QString::fromUtf8("*.mka"),  QString::fromUtf8("*.dts"),
            QString::fromUtf8("*.flac"), QString::fromUtf8("*.ogg"),
            QString::fromUtf8("*.m4a"),  QString::fromUtf8("*.ac3"),
            QString::fromUtf8("*.opus"), QString::fromUtf8("*.wav"),
            QString::fromUtf8("*.wv")};
    }

    QStringList subtitleSuffixes() const {
        return QStringList{
            QString::fromUtf8("*.utf"),   QString::fromUtf8("*.utf8"),
            QString::fromUtf8("*.utf-8"), QString::fromUtf8("*.idx"),
            QString::fromUtf8("*.sub"),   QString::fromUtf8("*.srt"),
            QString::fromUtf8("*.rt"),    QString::fromUtf8("*.ssa"),
            QString::fromUtf8("*.ass"),   QString::fromUtf8("*.mks"),
            QString::fromUtf8("*.vtt"),   QString::fromUtf8("*.sup"),
            QString::fromUtf8("*.scc"),   QString::fromUtf8("*.smi")};
    }

    QStringList mediaSuffixes() const {
        QStringList suffixes{};
        suffixes.append(videoSuffixes());
        suffixes.append(audioSuffixes());
        return suffixes;
    }

    QStringList videoMimeTypes() const;

    QStringList audioMimeTypes() const;

    QStringList mediaMimeTypes() const {
        QStringList mimeTypes{};
        mimeTypes.append(videoMimeTypes());
        mimeTypes.append(audioMimeTypes());
        return mimeTypes;
    }

public Q_SLOTS:
    void open(const QUrl &value);
    void play();
    void play(const QUrl &value);
    void pause();
    void stop();
    void seek(qint64 value);
    void rotate(int value);
    void scale(qreal x, qreal y);
    void snapshot();
    bool isVideo(const QUrl &value) const;
    bool isAudio(const QUrl &value) const;
    bool isMedia(const QUrl &value) const;

protected:
    void timerEvent(QTimerEvent *event) override;

private:
    void initMdkHandlers();

private:
    bool isLoaded() const;
    bool isPlaying() const;
    bool isPaused() const;
    bool isStopped() const;

Q_SIGNALS:
    void loaded();
    void playing();
    void paused();
    void stopped();

    void sourceChanged();
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

private:
    QUrl m_source = QUrl();
    QScopedPointer<MDK_NS::Player> m_player{new MDK_NS::Player};
    qreal m_volume = 1.0;
    bool m_mute = false, m_hasVideo = false, m_hasAudio = false,
         m_hasSubtitle = false;
    QString m_snapshotDirectory = QString(),
            m_snapshotFormat = QString::fromUtf8("png"),
            m_snapshotTemplate = QString();
};
