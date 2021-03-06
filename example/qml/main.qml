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

import QtQuick 2.9
import QtQuick.Window 2.9
import QtQuick.Controls 2.9
import Qt.labs.platform 1.1
import wangwenx190.MDKWrapper 1.0

Window {
    id: window
    visible: true
    width: 800
    height: 600
    title: (mdkPlayer.filePath.length > 0) ? (qsTr("Current playing: ") + mdkPlayer.filePath) : qsTr("MDKPlayer demo application")

    function gfxApi() {
        switch (GraphicsInfo.api) {
        case GraphicsInfo.Direct3D11:
            return "Direct3D11";
        case GraphicsInfo.Vulkan:
            return "Vulkan";
        case GraphicsInfo.Metal:
            return "Metal";
        case GraphicsInfo.OpenGL:
            return "OpenGL";
        case GraphicsInfo.Null:
            return "Null";
        case GraphicsInfo.Direct3D11Rhi:
            return "Direct3D11 Rhi";
        case GraphicsInfo.VulkanRhi:
            return "Vulkan Rhi";
        case GraphicsInfo.MetalRhi:
            return "Metal Rhi";
        case GraphicsInfo.OpenGLRhi:
            return "OpenGL Rhi";
        case GraphicsInfo.Software:
            return "Software";
        case GraphicsInfo.NullRhi:
            return "Null Rhi";
        case GraphicsInfo.Unknown:
            return "Unknown";
        }
        return "What?";
    }

    Shortcut {
        sequence: StandardKey.Open
        onActivated: fileDialog.open()
    }

    MDKPlayer {
        id: mdkPlayer
        anchors.fill: parent
        url: fileDialog.file
        logLevel: MDKPlayer.LogLevel.Info
        hardwareDecoding: true
        onVideoSizeChanged: {
            if (window.visibility === Window.Windowed) {
                window.width = mdkPlayer.videoSize.width;
                window.height = mdkPlayer.videoSize.height;
            }
        }
    }

    FileDialog {
        id: fileDialog
        currentFile: mdkPlayer.url
        folder: StandardPaths.writableLocation(StandardPaths.MoviesLocation)
        nameFilters: [
            qsTr("Video files (%1)").arg(mdkPlayer.videoSuffixes.join(' ')),
            qsTr("Audio files (%1)").arg(mdkPlayer.audioSuffixes.join(' ')),
            qsTr("All files (*)")
        ]
        options: FileDialog.ReadOnly
    }

    Label {
        anchors {
            top: parent.top
            left: parent.left
        }
        text: qsTr("Current RHI backend: ") + gfxApi()
        color: "white"
        font {
            bold: true
            pointSize: 15
        }
    }

    CheckBox {
        id: checkbox
        anchors {
            top: parent.top
            right: parent.right
        }
        text: qsTr("Hardware decoding")
        checked: mdkPlayer.hardwareDecoding
        font {
            bold: true
            pointSize: 15
        }
        onCheckedChanged: mdkPlayer.hardwareDecoding = checkbox.checked

        contentItem: Text {
            text: checkbox.text
            font: checkbox.font
            opacity: checkbox.enabled ? 1.0 : 0.3
            color: checkbox.down ? "#17a81a" : "#21be2b"
            verticalAlignment: Text.AlignVCenter
            leftPadding: checkbox.indicator.width + checkbox.spacing
        }
    }

    Button {
        visible: mdkPlayer.playbackState === MDKPlayer.PlaybackState.Stopped
        anchors.centerIn: parent
        width: 180
        height: 80
        text: qsTr("Open file")
        font.pointSize: 14
        onClicked: fileDialog.open()
    }

    Slider {
        id: slider
        anchors {
            left: parent.left
            right: parent.right
            bottom: parent.bottom
            bottomMargin: 20
        }
        visible: mdkPlayer.playbackState !== MDKPlayer.PlaybackState.Stopped
        to: mdkPlayer.duration
        value: mdkPlayer.position
        onMoved: mdkPlayer.seek(slider.value)

        background: Rectangle {
            id: slider_background
            x: slider.leftPadding
            y: slider.topPadding + slider.availableHeight / 2 - height / 2
            implicitWidth: 200
            implicitHeight: 7
            width: slider.availableWidth
            height: implicitHeight
            radius: 2
            color: "#bdbebf"

            Rectangle {
                width: slider.visualPosition * parent.width
                height: parent.height
                color: "#21be2b"
                radius: 2
            }

            MouseArea {
                hoverEnabled: true
                anchors.fill: parent
                onEntered: preview.visible = true
                onExited: preview.visible = false
                onPositionChanged: {
                    var minX = 0;
                    var maxX = window.width - preview.width;
                    var curX = mouseX - (preview.width / 2);
                    var newX = (curX < minX) ? minX : ((curX > maxX) ? maxX : curX);
                    preview.x = newX;
                    var newPos = (mouseX - slider_background.x) / slider_background.width;
                    preview.seek(newPos * mdkPlayer.duration, false);
                }
            }
        }

        handle: Rectangle {
            x: slider.leftPadding + slider.visualPosition * (slider.availableWidth - width)
            y: slider.topPadding + slider.availableHeight / 2 - height / 2
            implicitWidth: 20
            implicitHeight: 20
            radius: 10
            color: slider.pressed ? "#f0f0f0" : "#f6f6f6"
            border.color: "#bdbebf"
        }
    }

    MDKPlayer {
        id: preview
        visible: false
        url: mdkPlayer.url
        livePreview: true
        hardwareDecoding: true
        width: 300
        height: 168.75
        y: window.height - preview.height - 50;

        Label {
            anchors {
                horizontalCenter: parent.horizontalCenter
                bottom: parent.bottom
                bottomMargin: 5
            }
            color: "red"
            font {
                bold: true
                pointSize: 15
            }
            text: preview.positionText
        }
    }
}
