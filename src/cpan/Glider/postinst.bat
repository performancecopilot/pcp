@rem = '--*-PCP-*--
@echo off
if NOT "%COMSPEC%" == "%SystemRoot%\system32\cmd.exe" goto endofpcp

set PCP_DIR=C:\Glider
set PCP_CONF=%PCP_DIR%\etc\pcp.conf
set PCP_CONFIG=%PCP_DIR%\local\bin\pmconfig.exe
set PATH=%PCP_DIR%\bin;%PCP_DIR%\local\bin;%PATH%

sc stop   pcp > nul
sc stop   pmie > nul
sc stop   pmproxy > nul

@rem poor mans sleep
ping -n 1 localhost > nul
@rem then use a bigger hammer:

taskkill /F /IM pmcd.exe /T > nul
taskkill /F /IM pmie.exe /T > nul
taskkill /F /IM pmproxy.exe /T > nul

sc delete pcp > nul
sc create pcp binPath= "%PCP_DIR%\local\bin\pcp-services pcp %PCP_DIR%" start= auto DisplayName= "PCP Collector Processes" 
sc description pcp "Provides metric collection and logging services for Performance Co-Pilot."
sc start  pcp

sc delete pmie > nul
sc create pmie binPath= "%PCP_DIR%\local\bin\pcp-services pmie %PCP_DIR%" start= demand DisplayName= "PCP Inference Engines" 
sc description pmie "Provides performance rule evaluation services for Performance Co-Pilot."

sc delete pmproxy > nul
sc create pmproxy binPath= "%PCP_DIR%\local\bin\pcp-services pmproxy %PCP_DIR%" start= demand DisplayName= "PCP Collector Proxy" 
sc description pmproxy "Provides proxied access to Performance Co-Pilot collector processes."

:endofpcp
