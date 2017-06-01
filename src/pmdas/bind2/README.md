#bind2 Performance Metrics Domain Agent (PMDA) for Performance Co-Pilot (PCP)

This PMDA enables the PCP to collect a lot of Bind server statistics (more than the previous PMDA was able to get). It is written in Perl, requires a few Perl common modules (LWP and XML::LibXML). It is written with intention to be placed on the server hosting the Bind server itself, so with emphasis on the CPU impact and resistance to overload.

## Main features

* the bind2 PMDA enables the PCP to collect most of the statistics metrics from the Bind server version 9 which includes:
  - boot-time
  * overall memory statistics
  * overall per-query statistics (general queries, EDNS/truncated responses, Update/Notify/AXFR/IXFR messages)
  * overall error statistics (Rejected, SERVFAIL, Update/XFR failures ...)
  * overall statistics per transport protocol, EDNS and per version of IP protocol
  * resolver statistics (successes, errors, round-trip times in several ranges)
  * detailed per-socket statistics with respect to the transport protcol and IP version including errors
  * detailed per-file-descriptor statistics including errors
* per-second collection of the whole data set (148 metrics on the test environment) with modest requirements (2% CPU usage on Intel i7-4700MQ @2.4 GHz, cca 30 MB RAM)
* if more than 1 requests/sec are performed, the memoized values are being used so that the statistics interface of the Bind server does not get overloaded
* PCP and PMDA traces can be enabled for debugging in the %cfg hash

The per-socket and per-memory-context statistics that can be retrieved from Bind are intentionally not accessible but could be added if needed.

##Installation

* Install the required modules using your distribution packages (preferably) or using CPAN. E.g. for RHEL/CentOS, install perl-libwww-perl, perl-XML-LibXML and perl-Time-HiRes.
* Copy this code to /var/lib/pcp/pmdas/bind2
* Copy the bind2-example.conf file to bind2.conf and adapt the contents. Currently it is only the host line is needed to be changed.
* Configure the Bind server to give all the interesting information via HTTP in named.conf (addr and port must match the configuration in hosts):

  * statistics-channels { inet <addr> port <port> allow { any }; }
  * options { zone-statistics yes; }

   Note that you may need to allow the statistics queries/acls on the Bind server depending on the security settings of the Bind server as well. See the Bind server Administration Reference Manual e.g. on https://ftp.isc.org/isc/ appropriate to the version of Bind server you use for details.

* Install the PMDA to the PMCD as usual using the ./Install script and select it to be Collector or Both
* Check it to be present with pcp command:

   $ pcp
   ...
     pmda: root pmcd proc ... bind2 ...
   ...

* Check if all the metrics were successfully autoconfigured (this depends on the zone stats being enabled or not):

```
   $ pminfo bind2
   bind2.boot-time
   bind2.current-time
   bind2.memory.total.BlockSize
   bind2.memory.total.ContextSize
   bind2.memory.total.InUse
   bind2.memory.total.Lost
   bind2.memory.total.TotalUse
   bind2.nsstat.AuthQryRej
   bind2.nsstat.QryAuthAns
   bind2.nsstat.QryDropped
   bind2.nsstat.QryDuplicate
   bind2.nsstat.QryFailure
   bind2.nsstat.QryFORMERR
   bind2.nsstat.QryNoauthAns
   bind2.nsstat.QryNXDOMAIN
   bind2.nsstat.QryNxrrset
   bind2.nsstat.QryRecursion
   bind2.nsstat.QryReferral
   bind2.nsstat.QrySERVFAIL
   bind2.nsstat.QrySuccess
   bind2.nsstat.RateDropped
   bind2.nsstat.RateSlipped
   bind2.nsstat.RecQryRej
   bind2.nsstat.ReqBadEDNSVer
   bind2.nsstat.ReqBadSIG
   bind2.nsstat.ReqEdns0
   bind2.nsstat.ReqSIG0
   bind2.nsstat.ReqTCP
   bind2.nsstat.ReqTSIG
   bind2.nsstat.Requestv4
   bind2.nsstat.Requestv6
   bind2.nsstat.RespEDNS0
   bind2.nsstat.Response
   bind2.nsstat.RespSIG0
   bind2.nsstat.RespTSIG
   bind2.nsstat.TruncatedResp
   bind2.nsstat.UpdateBadPrereq
   bind2.nsstat.UpdateDone
   bind2.nsstat.UpdateFail
   bind2.nsstat.UpdateFwdFail
   bind2.nsstat.UpdateRej
   bind2.nsstat.UpdateReqFwd
   bind2.nsstat.UpdateRespFwd
   bind2.nsstat.XfrRej
   bind2.nsstat.XfrReqDone
   bind2.queries.in.A
   bind2.queries.in.AXFR
   bind2.queries.in.IXFR
   bind2.queries.in.SOA
   bind2.resolver.total.EDNS0Fail
   bind2.resolver.total.FORMERR
   bind2.resolver.total.GlueFetchv4
   bind2.resolver.total.GlueFetchv4Fail
   bind2.resolver.total.GlueFetchv6
   bind2.resolver.total.GlueFetchv6Fail
   bind2.resolver.total.Lame
   bind2.resolver.total.Mismatch
   bind2.resolver.total.NXDOMAIN
   bind2.resolver.total.OtherError
   bind2.resolver.total.QryRTT10
   bind2.resolver.total.QryRTT100
   bind2.resolver.total.QryRTT1600
   bind2.resolver.total.QryRTT1600+
   bind2.resolver.total.QryRTT500
   bind2.resolver.total.QryRTT800
   bind2.resolver.total.QueryAbort
   bind2.resolver.total.QuerySockFail
   bind2.resolver.total.QueryTimeout
   bind2.resolver.total.Queryv4
   bind2.resolver.total.Queryv6
   bind2.resolver.total.Responsev4
   bind2.resolver.total.Responsev6
   bind2.resolver.total.Retry
   bind2.resolver.total.SERVFAIL
   bind2.resolver.total.Truncated
   bind2.resolver.total.ValAttempt
   bind2.resolver.total.ValFail
   bind2.resolver.total.ValNegOk
   bind2.resolver.total.ValOk
   bind2.sockstat.FdwatchBindFail
   bind2.sockstat.FDWatchClose
   bind2.sockstat.FDwatchConn
   bind2.sockstat.FDwatchConnFail
   bind2.sockstat.FDwatchRecvErr
   bind2.sockstat.FDwatchSendErr
   bind2.sockstat.TCP4Accept
   bind2.sockstat.TCP4AcceptFail
   bind2.sockstat.TCP4BindFail
   bind2.sockstat.TCP4Close
   bind2.sockstat.TCP4Conn
   bind2.sockstat.TCP4ConnFail
   bind2.sockstat.TCP4Open
   bind2.sockstat.TCP4OpenFail
   bind2.sockstat.TCP4RecvErr
   bind2.sockstat.TCP4SendErr
   bind2.sockstat.TCP6Accept
   bind2.sockstat.TCP6AcceptFail
   bind2.sockstat.TCP6BindFail
   bind2.sockstat.TCP6Close
   bind2.sockstat.TCP6Conn
   bind2.sockstat.TCP6ConnFail
   bind2.sockstat.TCP6Open
   bind2.sockstat.TCP6OpenFail
   bind2.sockstat.TCP6RecvErr
   bind2.sockstat.TCP6SendErr
   bind2.sockstat.UDP4BindFail
   bind2.sockstat.UDP4Close
   bind2.sockstat.UDP4Conn
   bind2.sockstat.UDP4ConnFail
   bind2.sockstat.UDP4Open
   bind2.sockstat.UDP4OpenFail
   bind2.sockstat.UDP4RecvErr
   bind2.sockstat.UDP4SendErr
   bind2.sockstat.UDP6BindFail
   bind2.sockstat.UDP6Close
   bind2.sockstat.UDP6Conn
   bind2.sockstat.UDP6ConnFail
   bind2.sockstat.UDP6Open
   bind2.sockstat.UDP6OpenFail
   bind2.sockstat.UDP6RecvErr
   bind2.sockstat.UDP6SendErr
   bind2.sockstat.UnixAccept
   bind2.sockstat.UnixAcceptFail
   bind2.sockstat.UnixBindFail
   bind2.sockstat.UnixClose
   bind2.sockstat.UnixConn
   bind2.sockstat.UnixConnFail
   bind2.sockstat.UnixOpen
   bind2.sockstat.UnixOpenFail
   bind2.sockstat.UnixRecvErr
   bind2.sockstat.UnixSendErr
   bind2.total.queries.out.NOTIFY
   bind2.total.queries.out.QUERY
   bind2.total.queries.out.UPDATE
   bind2.zones.serial
   bind2.zonestat.AXFRReqv4
   bind2.zonestat.AXFRReqv6
   bind2.zonestat.IXFRReqv4
   bind2.zonestat.IXFRReqv6
   bind2.zonestat.NotifyInv4
   bind2.zonestat.NotifyInv6
   bind2.zonestat.NotifyOutv4
   bind2.zonestat.NotifyOutv6
   bind2.zonestat.NotifyRej
   bind2.zonestat.SOAOutv4
   bind2.zonestat.SOAOutv6
   bind2.zonestat.XfrFail
   bind2.zonestat.XfrSuccess
```

* Check if you can get the values of it:

```
   $ pminfo -f bind2


   bind2.boot-time
      value "2017-01-04T08:29:26Z"

   ...
```
