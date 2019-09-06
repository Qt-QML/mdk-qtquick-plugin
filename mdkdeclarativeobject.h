#pragma once

#ifndef _MDKDECLARATIVEOBJECT_H
#define _MDKDECLARATIVEOBJECT_H

#include <QQuickFramebufferObject>
#include <QTimer>
#include <QUrl>
#include <mdk/Player.h>
#include <memory>

class MdkDeclarativeObject : public QQuickFramebufferObject {
    Q_OBJECT

    Q_PROPERTY(QUrl source READ source WRITE setSource NOTIFY sourceChanged)
    Q_PROPERTY(QString fileName READ fileName NOTIFY fileNameChanged)
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
    Q_PROPERTY(float playbackRate READ playbackRate WRITE setPlaybackRate NOTIFY
                   playbackRateChanged)
    Q_PROPERTY(float aspectRatio READ aspectRatio WRITE setAspectRatio NOTIFY
                   aspectRatioChanged)

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

    [[nodiscard]] Renderer *createRenderer() const override;

    Q_INVOKABLE void renderVideo();
    Q_INVOKABLE void setVideoSurfaceSize(QSize size);

    [[nodiscard]] QUrl source() const;
    void setSource(const QUrl &value);

    [[nodiscard]] QString fileName() const;

    [[nodiscard]] qint64 position() const;
    void setPosition(qint64 value);

    [[nodiscard]] qint64 duration() const;

    [[nodiscard]] QSize videoSize() const;

    [[nodiscard]] float volume() const;
    void setVolume(float value);

    [[nodiscard]] bool mute() const;
    void setMute(bool value);

    [[nodiscard]] bool seekable() const;

    [[nodiscard]] MdkDeclarativeObject::PlaybackState playbackState() const;
    void setPlaybackState(MdkDeclarativeObject::PlaybackState value);

    [[nodiscard]] MdkDeclarativeObject::MediaStatus mediaStatus() const;

    [[nodiscard]] MdkDeclarativeObject::LogLevel logLevel() const;
    void setLogLevel(MdkDeclarativeObject::LogLevel value);

    [[nodiscard]] float playbackRate() const;
    void setPlaybackRate(float value);

    [[nodiscard]] float aspectRatio() const;
    void setAspectRatio(float value);

    Q_INVOKABLE void open(const QUrl &value);
    Q_INVOKABLE void play();
    Q_INVOKABLE void play(const QUrl &value);
    Q_INVOKABLE void pause();
    Q_INVOKABLE void stop();
    Q_INVOKABLE void seek(qint64 value);
    Q_INVOKABLE void rotate(int value);
    Q_INVOKABLE void scale(float x, float y);

private:
    void processMdkEvents();
    void notify();

private:
    [[nodiscard]] bool isLoaded() const;
    [[nodiscard]] bool isPlaying() const;
    [[nodiscard]] bool isPaused() const;
    [[nodiscard]] bool isStopped() const;

Q_SIGNALS:
    void startWatchingProperties();
    void stopWatchingProperties();

    void loaded();
    void playing();
    void paused();
    void stopped();

    void sourceChanged();
    void fileNameChanged();
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

private:
    QUrl m_source = QUrl();
    std::unique_ptr<mdk::Player> player;
    float m_volume = 1.0F;
    bool m_mute = false, hasVideo = false, hasAudio = false;
    QTimer timer;
};

#endif
