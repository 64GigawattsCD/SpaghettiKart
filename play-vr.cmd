@echo off
pushd "%~dp0build\x64\Debug" || exit /b 1
start "" Spaghettify.exe --vr
popd
