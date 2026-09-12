@echo off
setlocal enabledelayedexpansion
REM ============================================
REM NEW CODE - ALBT multiplatform
REM Builds every platform this machine can reach, then merges them into ONE
REM dist\albt_full_plugin.dusk with symgen-verified ABI metadata.
REM
REM Reachable here today:  windows-amd64 (MSVC), linux-x86_64, linux-aarch64 (zig)
REM Needs more toolchain:  windows-arm64  -> VS "C++ ARM64 build tools" component
REM                        android-aarch64 -> Android NDK 29
REM                        macos-*/ios-*   -> a macOS runner (zig lld rejects
REM                                           -bundle_loader; Apple's linker does not)
REM Those four are what .github\workflows\build.yml already covers.
REM ============================================
set ROOT=%~dp0
set CMAKE="C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
set NINJA="C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe"
if not defined ZIG set ZIG=zig
set SYMGEN=%ROOT%tools\bin\symgen.exe

"%ZIG%" version >nul 2>&1
if errorlevel 1 (
  echo Zig is missing. Install Zig on PATH or set ZIG=path\to\zig.exe
  exit /b 1
)
if not exist "%SYMGEN%" (
  echo symgen is missing. Download symgen-windows-x86_64.exe to %SYMGEN%
  exit /b 1
)

echo === windows-amd64 ===
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
if not exist "%ROOT%build\build.ninja" (
  %CMAKE% -S "%ROOT%." -B "%ROOT%build" -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
  if errorlevel 1 exit /b 1
)
%CMAKE% --build "%ROOT%build"
if errorlevel 1 exit /b 1

call :zigbuild linux-x86_64  x86_64-linux-gnu   Linux x86_64
if errorlevel 1 exit /b 1
call :zigbuild linux-aarch64 aarch64-linux-gnu  Linux aarch64
if errorlevel 1 exit /b 1

echo === merge ===
python "%ROOT%tools\merge_mod.py" -o "%ROOT%dist\albt_full_plugin.dusk" --symgen "%SYMGEN%" ^
  "%ROOT%build\mods\albt_full_plugin.dusk" ^
  "%ROOT%build-linux-x86_64\mods\albt_full_plugin.dusk" ^
  "%ROOT%build-linux-aarch64\mods\albt_full_plugin.dusk"
if errorlevel 1 exit /b 1

if exist "%AppData%\TwilitRealm\Dusklight\mods\" (
  copy /Y "%ROOT%dist\albt_full_plugin.dusk" "%AppData%\TwilitRealm\Dusklight\mods\albt_full_plugin.dusk" >nul
  echo installed to %%AppData%%\TwilitRealm\Dusklight\mods\albt_full_plugin.dusk
)
exit /b 0

:zigbuild
echo === %1 (%2) ===
if not exist "%ROOT%build-%1\build.ninja" (
  %CMAKE% -S "%ROOT%." -B "%ROOT%build-%1" -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo ^
    -DCMAKE_MAKE_PROGRAM=%NINJA% -DCMAKE_TOOLCHAIN_FILE="%ROOT%cmake\zig-cross.cmake" ^
    -DZIG=%ZIG% -DDUSK_ZIG_TRIPLE=%2 -DDUSK_TARGET_SYSTEM=%3 -DDUSK_TARGET_PROCESSOR=%4
  if errorlevel 1 exit /b 1
)
%CMAKE% --build "%ROOT%build-%1"
exit /b %ERRORLEVEL%
