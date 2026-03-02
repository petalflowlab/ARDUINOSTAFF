@echo off
setlocal
set "espota=C:\Users\Admin\AppData\Local\Arduino15\packages\esp32\hardware\esp32\3.3.3\tools\espota.exe"
if not exist "%espota%" (
    echo [ERROR] Could not find espota.exe at %espota%
    pause
    exit /b 1
)

echo Finding compiled binary in C:\Users\Admin\Documents\Arduino\PurpleSword\build\esp32.esp32.XIAO_ESP32C3...
set "binfile="
for %%F in ("C:\Users\Admin\Documents\Arduino\PurpleSword\build\esp32.esp32.XIAO_ESP32C3\*.bin") do (
    echo "%%F" | findstr /v "bootloader" | findstr /v "partitions" >nul
    if not errorlevel 1 (
        set "binfile=%%F"
        goto :found_bin
    )
)

:found_bin
if not defined binfile (
    echo [ERROR] Could not find compiled .bin file.
    echo Please go to Arduino IDE and click: Sketch -^> Export compiled Binary
    pause
    exit /b 1
)

echo Using: %espota%
echo Uploading: %binfile% to 192.168.0.22...

"%espota%" -d -i 192.168.0.22 -p 3232 -a sword -f "%binfile%"

if errorlevel 1 (
    echo [ERROR] Upload failed!
) else (
    echo [SUCCESS] Upload complete!
)
pause
