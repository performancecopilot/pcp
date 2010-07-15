@rem = '--*-PCP-*--
@echo off
if NOT "%COMSPEC%" == "%SystemRoot%\system32\cmd.exe" goto endofpcp
set PCP_WIN=C:\Glider
chdir %PCP_DIR_BAT%
if "%OS%" == "Windows_NT" goto WinNT
start %PCP_WIN%\msys.bat "%0" %1 %2 %3 %4 %5 %6 %7 %8 %9
goto endofpcp
:WinNT
start %PCP_WIN%\msys.bat %0 %*
if %errorlevel% == 9009 echo You do not have msys.bat in your PATH.
if errorlevel 1 goto script_failed_so_exit_with_non_zero_val 2>nul
goto endofpcp
@rem ';
:endofpcp
