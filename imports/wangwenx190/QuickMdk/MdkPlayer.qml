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

import QtQuick 2.15
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
    property alias positionText: mdkObject.positionText
    property alias durationText: mdkObject.durationText
    property alias format: mdkObject.format
    property alias fileSize: mdkObject.fileSize
    property alias bitRate: mdkObject.bitRate
    property alias chapters: mdkObject.chapters
    property alias metaData: mdkObject.metaData
    property alias hardwareDecoding: mdkObject.hardwareDecoding
    property alias videoDecoders: mdkObject.videoDecoders
    property alias audioDecoders: mdkObject.audioDecoders
    property alias defaultVideoDecoders: mdkObject.defaultVideoDecoders
    property alias defaultAudioDecoders: mdkObject.defaultAudioDecoders
    property alias audioBackends: mdkObject.audioBackends
    property alias autoStart: mdkObject.autoStart
    property alias livePreview: mdkObject.livePreview
    property alias videoBackend: mdkObject.videoBackend
    property alias fillMode: mdkObject.fillMode

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
    function rotateImage(degree) {
        mdkObject.rotateImage(degree);
    }
    function scaleImage(x, y) {
        mdkObject.scaleImage(x, y);
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
    function isLoaded() {
        return mdkObject.isLoaded();
    }
    function isPlaying() {
        return mdkObject.isPlaying();
    }
    function isPaused() {
        return mdkObject.isPaused();
    }
    function isStopped() {
        return mdkObject.isStopped();
    }
    function currentIsVideo() {
        return mdkObject.currentIsVideo();
    }
    function currentIsAudio() {
        return mdkObject.currentIsAudio();
    }
    function currentIsMedia() {
        return mdkObject.currentIsMedia();
    }
    function startRecord(url, format) {
        mdkObject.startRecord(url, format);
    }
    function stopRecord() {
        mdkObject.stopRecord();
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
