@echo off
REM ==============================================
REM  YOLO Detection Platform - Windows Build
REM  One-click build for MSVC + Qt5 + OpenCV
REM ==============================================

cd /d %~dp0

echo [1/2] Configuring CMake...
cmake -S . -B build-win -G "Visual Studio 18 2026" -A x64 ^
  -DQt5_DIR="C:/Qt/5.15.2/msvc2019_64/lib/cmake/Qt5"

if %ERRORLEVEL% NEQ 0 (
    echo ERROR: CMake configure failed
    pause
    exit /b 1
)

echo.
echo [2/2] Building Release...
cmake --build build-win --config Release

if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Build failed
    pause
    exit /b 1
)

echo.
echo ==============================================
echo  Build successful!
echo  Run: .\build-win\Release\yolo_win.exe
echo  MJPEG: http://localhost:9093/stream
echo ==============================================
pause
