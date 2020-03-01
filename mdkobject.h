#pragma once

#include <QQuickFramebufferObject>
#include <QUrl>
#include <mdk/Player.h>
#include <memory>

class MdkObject : public QQuickFramebufferObject {
    Q_OBJECT
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

    // By default, the registered QML type's name is the class or namespace
    // name, if you want to register a different name to the QML engine, use
    // "QML_NAMED_ELEMENT(name)" instead.
    QML_ELEMENT

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

protected:
    void timerEvent(QTimerEvent *event) override;

private:
    void processMdkEvents();

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
    std::unique_ptr<MDK_NS::Player> m_player;
    qreal m_volume = 1.0;
    bool m_mute = false, m_hasVideo = false, m_hasAudio = false,
         m_hasSubtitle = false;
    QString m_snapshotDirectory, m_snapshotFormat = QString::fromUtf8("png"),
                                 m_snapshotTemplate = QString();
};
