@echo off
python "%~dp0tools\kart_assets\toolchain.py" open --blend "%~dp0build\kart-tools\starter\kart_authoring.blend"
if errorlevel 1 pause
