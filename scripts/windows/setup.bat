@echo off
setlocal
cd /d "%~dp0\..\.."

if not defined XCLANG_ROOT (
  cmake "-DXCLANG_INSTALL_DIR=%CD%\build\xclang-sdk" -P scripts\ci\install-xclang.cmake
  if errorlevel 1 exit /b 1
  set "XCLANG_ROOT=%CD%\build\xclang-sdk\xclang"
)
set "PATH=%XCLANG_ROOT%\bin;%PATH%"
if not exist dist mkdir dist
copy /Y lib\windows\*.dll dist\
copy /Y scripts\windows\sbinit.config dist\
pushd source
cmake --preset windows-release
set "result=%ERRORLEVEL%"
popd
exit /b %result%
