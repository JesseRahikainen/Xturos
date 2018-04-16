REM Setup symbolic links
mklink /d .\jni\nuklear F:\Data\Libraries\nuklear-master
mklink /d .\jni\SDL2 F:\Data\Libraries\SDL2-2.05_src
mklink /d .\jni\spine_c F:\Data\Libraries\spine-runtimes-master\spine-c
mklink /d .\jni\stb F:\Data\Libraries\stb-master
mklink /d .\jni\game\src ..\..\..\..\src

REM Setup path
SET PATH=%PATH%;F:\Data\android_sdl_dev\android-ndk-r13b;F:\Data\android_sdl_dev\apache-ant-1.10.0\bin;F:\Data\android_sdl_dev\platform-tools
SET ANDROID_HOME=F:\Data\android_sdl_dev
SET JAVA_HOME=C:\Program Files\Java\jdk1.8.0_73

%ANDROID_HOME%\tools\monitor
