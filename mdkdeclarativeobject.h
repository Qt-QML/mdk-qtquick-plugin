#pragma once

#ifndef _MDKDECLARATIVEOBJECT_H
#define _MDKDECLARATIVEOBJECT_H

#include <QQuickFramebufferObject>
#include <QUrl>
#include <mdk/Player.h>
#include <memory>

class MdkDeclarativeObject : public QQuickFramebufferObject {
    Q_OBJECT

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
    Q_PROPERTY(
        MdkDeclarativeObject::PlaybackState playbackState READ playbackState
            WRITE setPlaybackState NOTIFY playbackStateChanged)
    Q_PROPERTY(MdkDeclarativeObject::MediaStatus mediaStatus READ mediaStatus
                   NOTIFY mediaStatusChanged)
    Q_PROPERTY(MdkDeclarativeObject::LogLevel logLevel READ logLevel WRITE
                   setLogLevel NOTIFY logLevelChanged)
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

    explicit MdkDeclarativeObject(QQuickItem *parent = nullptr);

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

    MdkDeclarativeObject::PlaybackState playbackState() const;
    void setPlaybackState(MdkDeclarativeObject::PlaybackState value);

    MdkDeclarativeObject::MediaStatus mediaStatus() const;

    MdkDeclarativeObject::LogLevel logLevel() const;
    void setLogLevel(MdkDeclarativeObject::LogLevel value);

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

#endif
