REM To lazy to learn CMake, setup symbolic links for external libraries, replace paths when setting up on a new computer
mkdir .\libraries
mklink /d .\libraries\SDL2 F:\Data\Libraries\SDL-release-2.26.2
mklink /d .\libraries\stb-master F:\Data\Libraries\stb-master