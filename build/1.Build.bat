@echo off

REM Set the working folder to the folder this batch file is in just to be sure
%~d0
cd %~dp0

time /T

if not exist "SetEnvVars.bat" (
	copy "SetEnvVars.bat.template" "SetEnvVars.bat"
	
	call exec ".\bin\fregex.exe" "r|VS10PATH=.*$|VS10PATH=$r|HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\VisualStudio\10.0\InstallDir|" -i "SetEnvVars.bat" -o "SetEnvVars.bat"
	if not %ERRORLEVEL%==0 exit /B %ERRORLEVEL%
)

call SetEnvVars.bat

if not exist "%VS10PATH%" (
	echo Error: VS10PATH does not exist: "%VS10PATH%"
	echo Please edit your SetEnvVars.bat file to enter the correct location for your Visual Studio 2008 installation
	goto ConfigIsWrong
)

if exist devenv.log del devenv.log

if not exist .\Temp mkdir .\Temp

:Update-VersionNumbers
echo ######################## Update-VersionNumbers ########################

	REM Use the rev of the repo for the version
	git rev-list HEAD --count > .\Temp\version.txt
	if not %ERRORLEVEL%==0 exit /B %ERRORLEVEL%

	set /p VERSION_D=<.\Temp\version.txt
	call SetVersion.bat
	echo Version is %VERSION%
	del .\Temp\version.txt

    REM back up files that hold versions so they can be restored after the build
    REM  (this is just so that these files don't keep showing up in the svn commit dialog)
    call exec copy /Y "..\ScanChannelsBDA.rc" ".\Temp\ScanChannelsBDA.rc"
	if not %ERRORLEVEL%==0 exit /B %ERRORLEVEL%

    REM Replace the version numbers
	call exec ".\bin\fregex.exe" "s/FILEVERSION .*$/FILEVERSION %VERSION_A%,%VERSION_B%,%VERSION_C%,%VERSION_D%/" "s/FileVersion\".*$/FileVersion\", \"%VERSION%\"/" -i "..\ScanChannelsBDA.rc" -o "..\ScanChannelsBDA.rc"
	if not %ERRORLEVEL%==0 exit /B %ERRORLEVEL%
	call exec ".\bin\fregex.exe" "s/PRODUCTVERSION .*$/PRODUCTVERSION %VERSION_A%,%VERSION_B%,%VERSION_C%,%VERSION_D%/" "s/ProductVersion\".*$/ProductVersion\", \"%VERSION%\"/" -i "..\ScanChannelsBDA.rc" -o "..\ScanChannelsBDA.rc"
	if not %ERRORLEVEL%==0 exit /B %ERRORLEVEL%
    
:End-VersionNumbers

:Build-ScanChannelsBDA
echo ######################## Build-ScanChannelsBDA ########################

	call exec "%VS10PATH%\devenv.exe" "..\ScanChannelsBDA.sln" /rebuild "Release|Win32" /out devenv.log
	if not %ERRORLEVEL%==0 exit /B %ERRORLEVEL%

    REM restore resource files to original (unaltered version number)
    copy /Y ".\Temp\ScanChannelsBDA.rc" "..\ScanChannelsBDA.rc"
	if not %ERRORLEVEL%==0 exit /B %ERRORLEVEL%
    
:CreateBuildOutput

    if not exist "..\builds" mkdir "..\builds"
	if not %ERRORLEVEL%==0 exit /B %ERRORLEVEL%
    
    if not exist "..\builds\ScanChannelsBDA-%VERSION%" mkdir "..\builds\ScanChannelsBDA-%VERSION%"
	if not %ERRORLEVEL%==0 exit /B %ERRORLEVEL%
    
    copy /Y "..\bin\*.exe" "..\builds\ScanChannelsBDA-%VERSION%"
    
    
GOTO end

:ConfigIsWrong
	echo -------------------
	echo The settings file will now be opened in notepad.
	echo Please confirm that each of the path variables is pointing to the correct location.
	echo -------------------
	pause
	start notepad "SetEnvVars.bat"
	exit /B 1

:end
echo Build Successful
time /T
