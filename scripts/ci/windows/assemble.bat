@echo off
cmake -P cmake\AsterboundVersion.cmake
if errorlevel 1 exit /b %errorlevel%

set client=client_distribution
if exist %client% rmdir %client% /S /Q

mkdir %client%
mkdir %client%\storage
mkdir %client%\mods
mkdir %client%\logs
mkdir %client%\assets
mkdir %client%\win
echo 211820 > %client%\win\steam_appid.txt

set server=server_distribution
if exist %server% rmdir %server% /S /Q
xcopy %client% %server% /E /I

.\dist\asset_packer.exe -c scripts\packing.config assets\asterbound %client%\assets\asterbound.pak
.\dist\asset_packer.exe -c scripts\packing.config assets\asterbound-compat %client%\assets\asterbound-compat.pak

for /f "delims=" %%f in (scripts\ci\windows\files_client.txt) do (
    xcopy "%%f" "%client%\win\" /Y
)

.\dist\asset_packer.exe -c scripts\packing.config -s assets\asterbound %server%\assets\asterbound.pak
.\dist\asset_packer.exe -c scripts\packing.config -s assets\asterbound-compat %server%\assets\asterbound-compat.pak

for /f "delims=" %%f in (scripts\ci\windows\files_server.txt) do (
    xcopy "%%f" "%server%\win\" /Y
)

set win=windows
if exist %win% rmdir %win% /S /Q
xcopy %client% %win% /E /I /Y
xcopy %server% %win% /E /I /Y

cmake -E tar cf client.zip --format=zip "%client%"
cmake -E tar cf server.zip --format=zip "%server%"
