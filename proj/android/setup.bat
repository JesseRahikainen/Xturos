REM Setup symbolic links
mklink /d .\app\jni\nuklear F:\Data\Libraries\nuklear-master
mklink /d .\app\jni\SDL2 F:\Data\Libraries\SDL-release-2.26.2
REM mklink /d .\jni\stb F:\Data\Libraries\stb-master
mklink /d .\app\jni\src\src ..\..\..\..\..\src\Game
mklink /d .\app\src\main\assets ..\..\..\..\..\bin\data