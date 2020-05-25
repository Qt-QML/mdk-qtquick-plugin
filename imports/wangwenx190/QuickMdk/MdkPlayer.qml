import QtQuick 2.14
import wangwenx190.QuickMdk 1.0

Item {
    id: mdkPlayer

    property alias source: mdkObject.source
    property alias fileName: mdkObject.fileName
    property alias path: mdkObject.path
    property alias position: mdkObject.position
    property alias duration: mdkObject.duration
    property alias videoSize: mdkObject.videoSize
    property alias volume: mdkObject.volume
    property alias mute: mdkObject.mute
    property alias seekable: mdkObject.seekable
    property alias playbackState: mdkObject.playbackState
    property alias mediaStatus: mdkObject.mediaStatus
    property alias logLevel: mdkObject.logLevel
    property alias playbackRate: mdkObject.playbackRate
    property alias aspectRatio: mdkObject.aspectRatio
    property alias snapshotDirectory: mdkObject.snapshotDirectory
    property alias snapshotFormat: mdkObject.snapshotFormat
    property alias snapshotTemplate: mdkObject.snapshotTemplate
    property alias videoSuffixes: mdkObject.videoSuffixes
    property alias audioSuffixes: mdkObject.audioSuffixes
    property alias subtitleSuffixes: mdkObject.subtitleSuffixes
    property alias mediaSuffixes: mdkObject.mediaSuffixes
    property alias videoMimeTypes: mdkObject.videoMimeTypes
    property alias audioMimeTypes: mdkObject.audioMimeTypes
    property alias mediaMimeTypes: mdkObject.mediaMimeTypes

    signal loaded
    signal playing
    signal paused
    signal stopped

    function open(url) {
        mdkObject.open(url);
    }
    function play() {
        mdkObject.play();
    }
    function pause() {
        mdkObject.pause();
    }
    function stop() {
        mdkObject.stop();
    }
    function seek(position) {
        mdkObject.seek(position);
    }
    function rotate(degree) {
        mdkObject.rotate(degree);
    }
    function scale(x, y) {
        mdkObject.scale(x, y);
    }
    function snapshot() {
        mdkObject.snapshot();
    }
    function isVideo(url) {
        return mdkObject.isVideo(url);
    }
    function isAudio(url) {
        return mdkObject.isAudio(url);
    }
    function isMedia(url) {
        return mdkObject.isMedia(url);
    }
    function isPlaying() {
        return mdkObject.playbackState === MdkObject.Playing;
    }
    function isPaused() {
        return mdkObject.playbackState === MdkObject.Paused;
    }
    function isStopped() {
        return mdkObject.playbackState === MdkObject.Stopped;
    }

    Rectangle {
        anchors.fill: mdkPlayer
        color: "black"
    }

    MdkObject {
        id: mdkObject
        anchors.fill: mdkPlayer
        onLoaded: mdkPlayer.loaded()
        onPlaying: mdkPlayer.playing()
        onPaused: mdkPlayer.paused()
        onStopped: mdkPlayer.stopped()
    }
}
