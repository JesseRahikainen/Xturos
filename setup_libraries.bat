REM To lazy to learn CMake, setup symbolic links for external libraries, replace paths when setting up on a new computer
mkdir .\libraries
mklink /d .\libraries\SDL3 F:\Data\Libraries\SDL3-3.2.0
mklink /d .\libraries\stb-master F:\Data\Libraries\stb-master

REM Powershell script files have to be unblocked so they can be run
powershell -Command {Unblock-File -Path ./src/ProjectToolScripts/addNewState.ps1}