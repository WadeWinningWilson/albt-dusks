@echo off
setlocal
set CMAKE="C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
set NINJA="C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe"
set ROOT=%~dp0
set BUILD=%ROOT%build-linux
if not defined ZIG set ZIG=zig
"%ZIG%" version >nul 2>&1
if errorlevel 1 (
  echo Zig is missing. Install Zig on PATH or set ZIG=path\to\zig.exe
  exit /b 1
)
%CMAKE% -S "%ROOT%." -B "%BUILD%" -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_MAKE_PROGRAM=%NINJA% -DCMAKE_TOOLCHAIN_FILE="%ROOT%cmake\linux-x86_64-zig.cmake" -DZIG="%ZIG%"
if errorlevel 1 exit /b 1
%CMAKE% --build "%BUILD%"
exit /b %ERRORLEVEL%
