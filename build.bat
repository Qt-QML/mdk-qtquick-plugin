@echo off
title Building MdkDeclarativeWrapper
setlocal
if exist "%~dp0build.user.bat" call "%~dp0build.user.bat"
set vcvarsallpath=
set vs2017path=
for /f "delims=" %%A in ('"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -property installationPath -latest -requires Microsoft.Component.MSBuild Microsoft.VisualStudio.Component.VC.Tools.x86.x64') do set vs2017path=%%A
set vcvarsallpath=%vs2017path%\VC\Auxiliary\Build\vcvarsall.bat
if not exist "%vcvarsallpath%" (
    echo Cannot find [vcvarsall.bat], if you did't install Visual Studio to it's default location, please change this script and re-run it.
    endlocal
    pause
    exit /b
)
if not defined apparch set apparch=x64
call "%vcvarsallpath%" %apparch%
cd /d "%~dp0"
if exist doc rd /s /q doc
if exist build rd /s /q build
md build
cd build
if not defined qtspec set qtspec=win32-msvc
if not defined prjcfg set prjcfg=silent
qmake "%~dp0mdkdeclarativewrapper.pro" -spec %qtspec% "CONFIG+=release qtquickcompiler %prjcfg%"
set maketool=jom
where %maketool%
if %ERRORLEVEL% neq 0 set maketool=nmake
%maketool% qmake_all
%maketool%
%maketool% qmltypes
%maketool% install
cd "%~dp0"
endlocal
rd /s /q build
exit /b
