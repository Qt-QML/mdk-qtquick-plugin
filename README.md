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

FileDialog {
    id: fileDialog

    title: qsTr("Please select a media file.")
    folder: shortcuts.movies
    nameFilters: [qsTr("Video files (*.avi *.mkv* *.mp4)"), qsTr("Audio files (*.mp3 *.flac)"), qsTr("All files (*)")

    onAccepted: mdkPlayer.source = fileDialog.fileUrl
}

MdkPlayer {
    id: mdkPlayer

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

- `MdkPlayer` (defined in [*MdkPlayer.qml*](/imports/quickmdk/MdkPlayer.qml)) is just a simple wrapper of the QML type `MdkObject` (defined in [*mdkdeclarativeobject.h*](/mdkdeclarativeobject.h) and [*mdkdeclarativeobject.cpp*](/mdkdeclarativeobject.cpp)). You can also use `MdkObject` directly if you want. It's usage is exactly the same with `MdkPlayer`.
- `mdkPlayer.duration`, `mdkPlayer.position` and `mdkPlayer.seek(position)` use **MILLISECONDS** instead of seconds.
- `mdkPlayer.seek(position)` uses absolute position, not relative offset.
- You can use `mdkPlayer.open(url)` to load and play *url* directly, it is equivalent to `mdkPlayer.source = url` (no need to call `mdkPlayer.play()` manually, because the playback will start immediately once the source url is changed).
- You can also use `mdkPlayer.play()` to resume a paused playback, `mdkPlayer.pause()` to pause a playing playback, `mdkPlayer.stop()` to stop a loaded playback and `mdkPlayer.seek(position)` to jump to a different position.
- To get the current playback state, use `mdkPlayer.isPlaying()`, `mdkPlayer.isPaused()` and `mdkPlayer.isStopped()`.
- Qt will load the qml plugins automatically if you have installed them into their correct locations, you don't need to load them manually (and to be honest I don't know how to load them manually either).

For more information, please refer to [*MdkPlayer.qml*](/imports/quickmdk/MdkPlayer.qml).

## Examples

- [**Quick Player**](https://github.com/wangwenx190/quickplayer-test/tree/mdk) (the *mdk* branch) - A simple multimedia player written mostly in QML by myself. It uses this plugin to play media contents. Obviously, it is based on the technology of Qt Quick (UI) and MDK (multimedia core). Actually, it's a personal experimental repository, just to test some concepts and prototypes.

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

  ```cpp
  #ifdef Q_OS_WINDOWS
      player->setVideoDecoders({"MFT:d3d=11", "MFT:d3d=9", "MFT", "D3D11", "DXVA", "CUDA", "NVDEC", "FFmpeg"});
  #elif defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
      player->setVideoDecoders({"VAAPI", "VDPAU", "CUDA", "NVDEC", "FFmpeg"});
  #elif defined(Q_OS_MACOS)
      player->setVideoDecoders({"VT", "VideoToolbox", "FFmpeg"});
  #endif
  ```

  In most cases, only the `FFmpeg` decoder uses software decoding. You can enable hardware decoding by switching to any decoders except for it.

- Why is the playback process not smooth enough or even stuttering?

   If you can insure the video file itself isn't damaged, then here are three possible reasons and their corresponding solutions:
   1. You are using **desktop OpenGL** or **Mesa llvmpipe** instead of ANGLE.

      Only by using **ANGLE** will your application gets the best performance. There are two official ways to let Qt use ANGLE as it's default backend:
      1. Set the environment variable `QT_OPENGL` to `angle`, case sensitive:

         Linux:

         ```bash
         export QT_OPENGL=angle
         ```

         Windows (cmd):

         ```bat
         set QT_OPENGL=angle
         ```

         Windows (PowerShell):

         ```powershell
         $env:QT_OPENGL="angle"
         ```

      2. Enable the Qt attribute `Qt::AA_UseOpenGLES` for `Q(Core|Gui)Application`:

         ```qt
         QCoreApplication::setAttribute(Qt::AA_UseOpenGLES);
         // or: QGuiApplication::setAttribute(Qt::AA_UseOpenGLES);
         ```

         Note: You **MUST** do this **BEFORE** the construction of `Q(Core|Gui)Application`!!!
   2. You are using **software decoding** instead of hardware decoding.

      MDK will not enable **hardware decoding** by default. You will have to enable it manually if you need it. Please refer to the previous topic to learn about how to enable it.
   3. You need a more powerful GPU, maybe even a better CPU. MDK is never designed to run on too crappy computers.

- How to set the log level of MDK?

    ```qml
   import wangwenx190.QuickMdk 1.0

   MdkPlayer {
       // ...
       logLevel: MdkObject.Debug // type: enum
       // ...
   }
   ```

   Note: For more log levels, please refer to [*MdkPlayer.qml*](/MdkPlayer.qml).
- Why my application complaints about failed to create EGL context ... etc at startup and then crashed?

   ANGLE only supports OpenGL version <= 3.1. Please check whether you are using OpenGL newer than 3.1 through ANGLE or not.

   Desktop OpenGL doesn't have this limit. You can use any version you like. The default version that Qt uses is 2.0, which I think is kind of out-dated.

   Here is how to change the OpenGL version in Qt:

   ```qt
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
