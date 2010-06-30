@rem = '--*-PCP-*--
@echo off
if NOT "%COMSPEC%" == "%SystemRoot%\system32\cmd.exe" goto endofpcp

call setup.bat

sc stop pcp > nul
sc stop pmie > nul
sc stop pmproxy > nul

@rem poor mans sleep
ping -n 1 localhost > nul
@rem then use a bigger hammer:

taskkill /F /IM pmcd.exe /T > nul
taskkill /F /IM pmie.exe /T > nul
taskkill /F /IM pmproxy.exe /T > nul
@rem and pick up a few of our friends:
taskkill /F /IM pmlogger.exe /T > nul
taskkill /F /IM pmchart.exe /T > nul
taskkill /F /IM pmtime.exe /T > nul

sc delete pcp > nul
sc delete pmie > nul
sc delete pmproxy > nul

:endofpcp
