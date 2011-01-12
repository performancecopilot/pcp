@rem = '--*-PCP-*--
@echo off
if NOT "%COMSPEC%" == "%SystemRoot%\system32\cmd.exe" goto endofpcp

set PCP_DIR=C:\Glider
set PCP_CONF=%PCP_DIR%\etc\pcp.conf
set PATH=%PCP_DIR%\bin;%PCP_DIR%\local\bin;%PATH%

sc stop pcp > nul
sc stop pmie > nul
sc stop pmproxy > nul

@rem poor mans sleep
ping -n 1 localhost > nul
@rem then use a bigger hammer:

taskkill /F /IM pmcd.exe /T 2> nul
taskkill /F /IM pmie.exe /T 2> nul
taskkill /F /IM pmproxy.exe /T 2> nul
taskkill /F /IM pmlogger.exe /T 2> nul
taskkill /F /IM pmchart.exe /T 2> nul
taskkill /F /IM pmtime.exe /T 2> nul

sc delete pcp > nul
sc delete pmie > nul
sc delete pmproxy > nul

:endofpcp
