@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
cd /d "%~dp0"
if not exist build\build.ninja (
  cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
  if errorlevel 1 exit /b 1
)
cmake --build build
if errorlevel 1 exit /b 1
python tools\pack_dist.py
if errorlevel 1 exit /b 1
if exist "%AppData%\TwilitRealm\Dusklight\mods\" (
  copy /Y "dist\albt_full_plugin.dusk" "%AppData%\TwilitRealm\Dusklight\mods\albt_full_plugin.dusk" >nul
  echo installed to %%AppData%%\TwilitRealm\Dusklight\mods\albt_full_plugin.dusk
)
exit /b %ERRORLEVEL%
