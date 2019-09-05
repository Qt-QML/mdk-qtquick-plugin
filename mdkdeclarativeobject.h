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
    Q_PROPERTY(
        qint64 position READ position WRITE setPosition NOTIFY positionChanged)
    Q_PROPERTY(qint64 duration READ duration NOTIFY durationChanged)
    Q_PROPERTY(QSize videoSize READ videoSize NOTIFY videoSizeChanged)
    Q_PROPERTY(float volume READ volume WRITE setVolume NOTIFY volumeChanged)
    Q_PROPERTY(bool mute READ mute WRITE setMute NOTIFY muteChanged)
    Q_PROPERTY(bool seekable READ seekable NOTIFY seekableChanged)
    Q_PROPERTY(
        MdkDeclarativeObject::PlaybackState playbackState READ playbackState
            WRITE setPlaybackState NOTIFY playbackStateChanged)
    Q_PROPERTY(MdkDeclarativeObject::MediaStatus mediaStatus READ mediaStatus
                   NOTIFY mediaStatusChanged)
    Q_PROPERTY(MdkDeclarativeObject::LogLevel logLevel READ logLevel WRITE
                   setLogLevel NOTIFY logLevelChanged)

public:
    enum PlaybackState { StoppedState, PlayingState, PausedState };
    Q_ENUM(PlaybackState)

    enum MediaStatus {
        UnknownMediaStatus,
        NoMedia,
        LoadingMedia,
        LoadedMedia,
        StalledMedia,
        BufferingMedia,
        BufferedMedia,
        EndOfMedia,
        InvalidMedia
    };
    Q_ENUM(MediaStatus)

    enum LogLevel {
        NoLog,
        DebugLevel,
        WarningLevel,
        CriticalLevel,
        FatalLevel,
        InfoLevel
    };
    Q_ENUM(LogLevel)

    explicit MdkDeclarativeObject(QQuickItem *parent = nullptr);

    Renderer *createRenderer() const override;

    Q_INVOKABLE void renderVideo();
    Q_INVOKABLE void setVideoSurfaceSize(int width, int height);

    QUrl source() const;
    void setSource(const QUrl &value);

    qint64 position() const;
    void setPosition(qint64 value);

    qint64 duration() const;

    QSize videoSize() const;

    float volume() const;
    void setVolume(float value);

    bool mute() const;
    void setMute(bool value);

    bool seekable() const;

    MdkDeclarativeObject::PlaybackState playbackState() const;
    void setPlaybackState(MdkDeclarativeObject::PlaybackState value);

    MdkDeclarativeObject::MediaStatus mediaStatus() const;

    MdkDeclarativeObject::LogLevel logLevel() const;
    void setLogLevel(MdkDeclarativeObject::LogLevel value);

    Q_INVOKABLE void open(const QUrl &value);
    Q_INVOKABLE void play();
    Q_INVOKABLE void play(const QUrl &value);
    Q_INVOKABLE void pause();
    Q_INVOKABLE void stop();
    Q_INVOKABLE void seek(qint64 value);

private:
    void processMdkEvents();

private:
    bool isLoaded() const;
    bool isPlaying() const;
    bool isPaused() const;
    bool isStopped() const;

Q_SIGNALS:
    void initFinished();

    void loaded();
    void playing();
    void paused();
    void stopped();

    void sourceChanged();
    void positionChanged();
    void durationChanged();
    void videoSizeChanged();
    void volumeChanged();
    void muteChanged();
    void seekableChanged();
    void playbackStateChanged();
    void mediaStatusChanged();
    void logLevelChanged();

private:
    QUrl m_source = QUrl();
    std::unique_ptr<mdk::Player> player;
    float m_volume = 1.0;
    bool m_mute = false;
};

#endif
