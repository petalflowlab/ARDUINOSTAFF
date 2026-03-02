@echo off
setlocal

set "PYTHON_EXE=C:\Users\Admin\AppData\Local\Arduino15\packages\esp8266\tools\python3\3.7.2-post1\python.exe"
set "HACK_SCRIPT=C:\Users\Admin\Documents\Arduino\C3Staff\C3STAFFMAIN\ARDUINOSTAFF\espota_hack.py"
set "BIN_FILE=C:\Users\Admin\Documents\Arduino\PurpleSword\build\esp32.esp32.XIAO_ESP32C3\PurpleSword.ino.bin"

if not exist "%PYTHON_EXE%" (
    echo [ERROR] Bundled Python not found at %PYTHON_EXE%
    pause
    exit /b 1
)

if not exist "%BIN_FILE%" (
    echo [ERROR] Binary not found. Please export it again.
    pause
    exit /b 1
)

echo Recovering OTA firmware via hacked script and bundled Python...
"%PYTHON_EXE%" "%HACK_SCRIPT%" -d -i 192.168.0.22 -p 3232 -a sword -m -f "%BIN_FILE%"

if errorlevel 1 (
    echo [ERROR] Recovery upload failed!
) else (
    echo [SUCCESS] Recovery upload complete!
)
pause
