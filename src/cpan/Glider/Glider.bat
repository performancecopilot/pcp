@rem = '--*-PCP-*--
@echo off
if NOT "%COMSPEC%" == "%SystemRoot%\system32\cmd.exe" goto endofpcp

call setup.bat

set TMP=D:\tmp
set TEMP=D:\tmp
set PACKAGE=D:\packages
copy pcp.fstab %PACKAGE%
copy pcpsh.bat %PACKAGE%
copy setup.bat %PACKAGE%
copy prerm.bat %PACKAGE%
copy postinst.bat %PACKAGE%
copy pcp.profile %PACKAGE%
copy ..\images\pcp.ico %PACKAGE%
copy ..\images\chart.ico %PACKAGE%
copy ..\images\pcp.ico %PACKAGE%\pcpsh.ico
copy ..\images\pcp.ico %PACKAGE%\GliderPCP.ico
copy ..\images\chart.ico %PACKAGE%\pmchart.ico
copy ..\images\GliderBanner.bmp %PACKAGE%
copy ..\images\GliderDialog.bmp %PACKAGE%

copy Glider.pm C:\Strawberry\perl\site\lib\Perl\Dist\Glider.pm
copy LaunchExe.pm C:\Strawberry\perl\site\lib\Perl\Dist\Glider\LaunchExe.pm
copy LaunchScript.pm C:\Strawberry\perl\site\lib\Perl\Dist\Glider\LaunchScript.pm
copy ..\images\pcp.ico C:\Strawberry\perl\site\lib\auto\share\dist\Perl-Dist-Strawberry\icons\pcpsh.ico
copy ..\images\chart.ico C:\Strawberry\perl\site\lib\auto\share\dist\Perl-Dist-Strawberry\icons\pmchart.ico

set TOOLKIT=C:\strawberry
set PATH=%TOOLKIT%\c\bin;%TOOLKIT%\perl\bin;%PATH%

if "%OS%" == "Windows_NT" goto WinNT
perl -w -I. -mPerl::Dist::Glider -e "(new Perl::Dist::Glider)->run();"
goto endofpcp
:WinNT
perl -w -I. -mPerl::Dist::Glider -e "(new Perl::Dist::Glider)->run();"
if %errorlevel% == 9009 echo You do not have perl in your PATH.
if errorlevel 1 goto script_failed_so_exit_with_non_zero_val 2>nul
goto endofpcp
@rem ';
:endofpcp
