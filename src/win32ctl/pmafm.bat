@echo off
if "%OS%" == "Windows_NT" goto WinNT
%PCP_DIR%\bin\sh.exe pmafm.sh %1 %2 %3 %4 %5 %6 %7 %8 %9
goto endofbash
:WinNT
%PCP_DIR%\bin\sh.exe pmafm.sh %*
if NOT "%COMSPEC%" == "%SystemRoot%\system32\cmd.exe" goto endofbash
if %errorlevel% == 9009 echo You do not have sh.exe in your PCP_DIR.
if errorlevel 1 goto script_failed_so_exit_with_non_zero_val 2>nul
:endofbash
