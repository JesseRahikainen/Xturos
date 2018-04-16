rmdir libs /s /q
rmdir gen /s /q
rmdir obj /s /q
rmdir assets /s /q
xcopy ..\..\bin\data assets /S /EXCLUDE:copy.exclusion /I
rm -v assets\snd.cfg
ndk-build
ant debug
ant debug install