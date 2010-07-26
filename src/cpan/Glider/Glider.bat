@rem = '--*-PCP-*--
@echo off
if NOT "%COMSPEC%" == "%SystemRoot%\system32\cmd.exe" goto endofpcp

set PCP_DIR=C:\Glider
set PCP_CONF=%PCP_DIR%\etc\pcp.conf
set PCP_CONFIG=%PCP_DIR%\local\bin\pmconfig.exe
set PATH=%PCP_DIR%\bin;%PCP_DIR%\local\bin;%PATH%

set TMP=D:\tmp
set TEMP=D:\tmp
set PACKAGE=D:\packages
copy pcp.fstab %PACKAGE%
copy pcpsh.bat %PACKAGE%
copy prerm.bat %PACKAGE%
copy postinst.bat %PACKAGE%
copy pcp.profile %PACKAGE%
copy pcp.ico %PACKAGE%
copy chart.ico %PACKAGE%
copy pcp.ico %PACKAGE%\pcpsh.ico
copy pcp.ico %PACKAGE%\GliderPCP.ico
copy chart.ico %PACKAGE%\pmchart.ico
copy GliderBanner.bmp %PACKAGE%
copy GliderDialog.bmp %PACKAGE%

copy Glider.pm C:\Strawberry\perl\site\lib\Perl\Dist\Glider.pm
copy LaunchExe.pm C:\Strawberry\perl\site\lib\Perl\Dist\Glider\LaunchExe.pm
copy LaunchScript.pm C:\Strawberry\perl\site\lib\Perl\Dist\Glider\LaunchScript.pm
copy pcp.ico C:\Strawberry\perl\site\lib\auto\share\dist\Perl-Dist-Strawberry\icons\pcpsh.ico
copy chart.ico C:\Strawberry\perl\site\lib\auto\share\dist\Perl-Dist-Strawberry\icons\pmchart.ico

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
