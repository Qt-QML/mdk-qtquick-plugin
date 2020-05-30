# mdk-qtquick-plugin

MDK wrapper for Qt Quick. Can be used as a normal visual element in .qml files easily.

## Features

- Can be easily embeded into any Qt Quick GUI applications.
- Support both shared and static ways of compiling, linking and loading.
- Cross-platform support: Windows, Linux, macOS, Android, iOS, UWP, etc.

## TODO

- Support the *CMake + Ninja* build system.

  Note: Qt is dropping QMake support and is migrating to use CMake in the coming major release, Qt 6. However, CMake is still not quite usable when creating qml plugins in Qt 5 days, so let's migrate to CMake in Qt 6.

## Usage

Once you have installed this plugin successfully, you can use it like any other normal visual elements of Qt Quick in .qml files:

```qml
import QtQuick.Dialogs 1.3
import wangwenx190.QuickMdk 1.0

Shortcut {
    sequence: StandardKey.Open
    onActivated: fileDialog.open()
}

FileDialog {
    id: fileDialog

    title: qsTr("Please select a media file.")
    folder: shortcuts.movies
    nameFilters: [qsTr("Video files (%1)").arg(mdkPlayer.videoSuffixes.join(' ')), qsTr("Audio files (%1)").arg(mdkPlayer.audioSuffixes.join(' ')), qsTr("All files (*)")]

    onAccepted: mdkPlayer.source = fileDialog.fileUrl
}

MdkPlayer {
    id: mdkPlayer
    anchors.fill: parent

    source: "file:///D:/Videos/test.mkv" // playback will start immediately once the source url is changed
    logLevel: MdkObject.Debug
    volume: 0.8 // 0-1.0

    onPositionChanged: // do something
    onDurationChanged: // do something
    onVideoSizeChanged: // do something
    onPlaybackStateChanged: // do something
    onMediaStatusChanged: // do something
}
```

Notes

- `MdkPlayer` (defined in [*MdkPlayer.qml*](/imports/wangwenx190/QuickMdk/MdkPlayer.qml)) is just a simple wrapper of the QML type `MdkObject` (defined in [*mdkobject.h*](/mdkobject.h) and [*mdkobject.cpp*](/mdkobject.cpp)). You can also use `MdkObject` directly if you want. It's usage is exactly the same with `MdkPlayer`.
- `mdkPlayer.duration`, `mdkPlayer.position` and `mdkPlayer.seek(position)` use **MILLISECONDS** instead of seconds.
- `mdkPlayer.seek(position)` uses absolute position, not relative offset.
- You can use `mdkPlayer.open(url)` to load and play *url* directly, it is equivalent to `mdkPlayer.source = url` (no need to call `mdkPlayer.play()` manually, because the playback will start immediately once the source url is changed).
- You can also use `mdkPlayer.play()` to resume a paused playback, `mdkPlayer.pause()` to pause a playing playback, `mdkPlayer.stop()` to stop a loaded playback and `mdkPlayer.seek(position)` to jump to a different position.
- To get the current playback state, use `mdkPlayer.isPlaying()`, `mdkPlayer.isPaused()` and `mdkPlayer.isStopped()`.
- Qt will load the qml plugins automatically if you have installed them into their correct locations, you don't need to load them manually (and to be honest I don't know how to load them manually either).
- If you want to integrate it into your application rather than load it dynamically, the traditional `qmlRegisterType()` function is also supported.

For more information, please refer to [*MdkPlayer.qml*](/imports/wangwenx190/QuickMdk/MdkPlayer.qml).

## Compilation

Before doing anything else, please make sure you have a compiler that supports at least C++14 and a recent version of Qt.

1. Checkout source code:

   ```bash
   git clone https://github.com/wangwenx190/mdk-qtquick-plugin.git
   ```

   Note: Please remember to install *Git* yourself. Windows users can download it from: <https://git-scm.com/downloads>

2. Setup MDK SDK:

   Download the official MDK SDK from: <https://sourceforge.net/projects/mdk-sdk/files/>. Then extract it to anywhere you like.

   Once everything is ready, then write the following things to a text file named **.qmake.conf** and save it to this repository's directory:

   ```conf
   # You should replace the "D:/code/mdk-sdk" with your own path.
   # Better to use "/" instead of "\", even on Windows platform.
   isEmpty(MDK_SDK_DIR): MDK_SDK_DIR = D:/code/mdk-sdk
   ```

3. Create a directory for building:

   Linux:

   ```bash
   mkdir build
   cd build
   ```

   Windows (cmd):

   ```bat
   md build
   cd build
   ```

   Windows (PowerShell):

   ```powershell
   New-Item -Path "build" -ItemType "Directory"
   Set-Location -Path "build"
   ```

4. Build and Install:

   Linux:

   ```bash
   qmake
   make
   make install
   ```

   Windows:

   ```bat
   qmake
   jom/nmake/mingw32-make
   jom/nmake/mingw32-make install
   ```

   Note: If you are not using MinGW, then *JOM* is your best choice on Windows. Qt's official website to download *JOM*: <http://download.qt.io/official_releases/jom/>

## FAQ

- How to enable hardware decoding?

  ```qml
  MdkPlayer {
      // ...
      hardwareDecoding: true // default is false
      // ...
  }
  ```

  Hardware decoding is disabled by default, you have to enable it manually if you want to use it.

- Why is the playback process not smooth enough or even stuttering?

   If you can insure the video file itself isn't damaged, here are two possible reasons and their corresponding solutions:
   1. You are using **software decoding** instead of hardware decoding.

      MDK will not enable **hardware decoding** by default. You will have to enable it manually if you need it. Please refer to the previous topic to learn about how to enable it.
   2. You need a more powerful GPU, maybe even a better CPU. MDK is never designed to run on too crappy computers.

- How to set the log level of MDK?

    ```qml
   import wangwenx190.QuickMdk 1.0

   MdkPlayer {
       // ...
       logLevel: MdkObject.Debug // type: enum
       // ...
   }
   ```

   Note: For more log levels, please refer to [*MdkPlayer.qml*](/imports/wangwenx190/QuickMdk/MdkPlayer.qml).
- Why my application complaints about failed to create EGL context ... etc at startup and then crashed?

   ANGLE only supports OpenGL version <= 3.1. Please check whether you are using OpenGL newer than 3.1 through ANGLE or not.

   Desktop OpenGL doesn't have this limit. You can use any version you like. The default version that Qt uses is 2.0, which I think is kind of out-dated.

   Here is how to change the OpenGL version in Qt:

   ```cpp
   QSurfaceFormat surfaceFormat;
   // Here we use OpenGL version 4.6 for instance.
   // Don't use any versions newer than 3.1 if you are using ANGLE.
   surfaceFormat.setMajorVersion(4);
   surfaceFormat.setMinorVersion(6);
   // You can also use "QSurfaceFormat::CoreProfile" to disable the using of deprecated OpenGL APIs, however, some deprecated APIs will still be usable.
   surfaceFormat.setProfile(QSurfaceFormat::CompatibilityProfile);
   QSurfaceFormat::setDefaultFormat(surfaceFormat);
   ```

## License

[GNU Lesser General Public License version 3](/LICENSE.md)
