@echo off
set MSYSTEM=
set MSYS=
call C:\Espressif\frameworks\esp-idf-v5.2.1\export.bat
if errorlevel 1 exit /b 1
cd /d %~dp0
idf.py %*
