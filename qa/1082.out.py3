QA output created by 1082
== archives/procpid-encode
000001 /usr/lib/systemd/systemd --switched-root --system --deserialize 23
000002 (kthreadd)
000003 (ksoftirqd/0)
000005 (kworker/0:0H)
000007 (rcu_sched)
000008 (rcu_bh)
000009 (rcuos/0)
000010 (rcuob/0)
000011 (migration/0)
000012 (watchdog/0)
000013 (watchdog/1)
000014 (migration/1)
000015 (ksoftirqd/1)
000017 (kworker/1:0H)
000018 (rcuos/1)
000019 (rcuob/1)
000020 (watchdog/2)
000021 (migration/2)
000022 (ksoftirqd/2)
000024 (kworker/2:0H)
000025 (rcuos/2)
000026 (rcuob/2)
000027 (watchdog/3)
000028 (migration/3)
000029 (ksoftirqd/3)
000031 (kworker/3:0H)
000032 (rcuos/3)
000033 (rcuob/3)
000034 (khelper)
000035 (kdevtmpfs)
000036 (netns)
000037 (perf)
000038 (writeback)
000039 (ksmd)
000040 (khugepaged)
000041 (crypto)
000042 (kintegrityd)
000043 (bioset)
000044 (kblockd)
000045 (ata_sff)
000046 (md)
000047 (devfreq_wq)
000073 (kswapd0)
000074 (fsnotify_mark)
000084 (kthrotld)
000085 (acpi_thermal_pm)
000086 (scsi_eh_0)
000087 (scsi_tmf_0)
000088 (scsi_eh_1)
000089 (scsi_tmf_1)
000092 (kpsmoused)
000094 (dm_bufio_cache)
000095 (ipv6_addrconf)
000097 (deferwq)
000127 (kauditd)
000215 (kworker/3:1H)
000216 (kworker/0:1H)
000222 (kworker/1:1H)
000235 (jbd2/sda7-8)
000236 (ext4-rsv-conver)
000237 (kworker/2:1H)
000313 /usr/lib/systemd/systemd-journald
000333 /usr/sbin/lvmetad -f
000341 /usr/lib/systemd/systemd-udevd
000388 /sbin/auditd -n
000401 (irq/46-mei_me)
000437 (hd-audio0)
000438 (hd-audio1)
000444 (kmemstick)
000462 (rtsx_usb_ms_1)
000468 /sbin/audispd
000471 /usr/sbin/sedispatch
000474 (cfg80211)
000476 (rpciod)
000482 /usr/sbin/alsactl -s -n 19 -c -E ALSA_CONFIG_PATH=/etc/alsa/alsactl.conf --initfile=/lib/alsa/init/00main rdaemon
000483 /usr/bin/python -Es /usr/sbin/firewalld --nofork --nopid
000486 /usr/libexec/accounts-daemon
000489 /usr/libexec/rtkit-daemon
000492 /usr/sbin/chronyd
000495 /sbin/rsyslogd -n
000496 /usr/sbin/ModemManager
000499 (kvm-irqfd-clean)
000500 /usr/sbin/abrtd -d -s
000509 /usr/bin/abrt-watch-log -F BUG: WARNING: at WARNING: CPU: INFO: possible recursive locking detected ernel BUG at list_del corruption list_add corruption do_IRQ: stack overflow: ear stack overflow (cur: eneral protection fault nable to handle kernel ouble fault: RTNL: assertion failed eek! page_mapcount(page) went negative! adness at NETDEV WATCHDOG ysctl table check failed : nobody cared IRQ handler type mismatch Machine Check Exception: Machine check events logged divide error: bounds: coprocessor segment overrun: invalid TSS: segment not present: invalid opcode: alignment check: stack segment: fpu exception: simd exception: iret exception: /var/log/messages -- /usr/bin/abrt-dump-oops -xtD
000510 (kworker/u17:0)
000511 (hci0)
000512 (hci0)
000513 (kworker/u17:1)
000519 /sbin/rngd -f
000520 /sbin/rpcbind -w
000530 /usr/sbin/irqbalance --foreground
000531 /usr/bin/abrt-watch-log -F Backtrace /var/log/Xorg.0.log -- /usr/bin/abrt-dump-xorg -xD
000534 /usr/sbin/smartd -n -q never
000538 avahi-daemon: running [slick.local]
000539 /usr/lib/systemd/systemd-logind
000542 /bin/dbus-daemon --system --address=systemd: --nofork --nopidfile --systemd-activation
000548 /usr/sbin/crond -n
000549 /usr/sbin/atd -f
000551 /usr/sbin/gdm
000552 /usr/libexec/bluetooth/bluetoothd
000556 java -agentpath:/usr/lib64/libabrt-java-connector.so=abrt=on -Xms256m -Xmx1g -Djava.awt.headless=true -XX:+UseParNewGC -XX:+UseConcMarkSweepGC -XX:CMSInitiatingOccupancyFraction=75 -XX:+UseCMSInitiatingOccupancyOnly -XX:+HeapDumpOnOutOfMemoryError -XX:+DisableExplicitGC -Dfile.encoding=UTF-8 -Delasticsearch -Des.pidfile=/var/run/elasticsearch/elasticsearch.pid -Des.path.home=/usr/share/elasticsearch -cp :/usr/share/elasticsearch/lib/elasticsearch-1.5.1.jar:/usr/share/elasticsearch/lib/*:/usr/share/elasticsearch/lib/sigar/* -Des.default.config=/etc/elasticsearch/elasticsearch.yml -Des.default.path.home=/usr/share/elasticsearch -Des.default.path.logs=/var/log/elasticsearch -Des.default.path.data=/var/lib/elasticsearch -Des.default.path.work=/tmp/elasticsearch -Des.default.path.conf=/etc/elasticsearch org.elasticsearch.bootstrap.Elasticsearch
000643 /usr/sbin/NetworkManager --no-daemon
000647 /usr/libexec/gdm-simple-slave --display-id /org/gnome/DisplayManager/Displays/_0
000672 avahi-daemon: chroot helper
000877 /usr/bin/Xorg :0 -background none -verbose -auth /run/gdm/auth-for-gdm-FuRaSs/database -seat seat0 -nolisten tcp vt1
000955 /usr/sbin/mcelog --ignorenodev --daemon --foreground
000956 /usr/sbin/libvirtd
000957 /usr/bin/docker -d --selinux-enabled --group wheel
000959 /usr/bin/memcached -u memcached -p 11211 -m 64 -c 1024
000972 /usr/sbin/sshd -D
000983 /sbin/rpc.statd
001013 /usr/bin/postgres -D /var/lib/pgsql/data -p 5432
001162 (loop0)
001163 (loop1)
001164 (kdmflush)
001175 (bioset)
001193 /sbin/dhclient -d -sf /usr/libexec/nm-dhcp-helper -pf /var/run/dhclient-p2p1.pid -lf /var/lib/NetworkManager/dhclient-c1a891b2-a297-4d35-bcf5-d3771822e95e-p2p1.lease -cf /var/lib/NetworkManager/dhclient-p2p1.conf p2p1
001194 (kcopyd)
001195 (bioset)
001196 (dm-thin)
001197 (bioset)
001236 postgres: logger process   
001245 postgres: checkpointer process   
001246 postgres: writer process   
001247 postgres: wal writer process   
001248 postgres: autovacuum launcher process   
001249 postgres: stats collector process   
001430 /usr/lib/polkit-1/polkitd --no-debug
003774 sudo su - pcpqa
003821 /usr/sbin/httpd -DFOREGROUND
004079 /usr/libexec/pcp/bin/pmwebd -l pmwebd.log -R /usr/share/pcp/webapps -A /var/log/pcp -G -v -M4
004507 bash
005628 /usr/lib/systemd/systemd --user
005631 (sd-pam)
005756 /usr/libexec/upowerd
005797 /usr/libexec/colord
005801 /usr/sbin/pcscd --foreground --auto-exit
006007 gdm-session-worker [pam/gdm-password]
006014 /usr/lib/systemd/systemd --user
006020 (sd-pam)
006035 /usr/bin/gnome-keyring-daemon --daemonize --login
006066 gnome-session
006074 dbus-launch --sh-syntax --exit-with-session
006075 /bin/dbus-daemon --fork --print-pid 4 --print-address 6 --session
006140 /usr/libexec/gvfsd
006160 /usr/libexec//gvfsd-fuse /run/user/1000/gvfs -f -o big_writes
006237 /usr/libexec/at-spi-bus-launcher
006241 /bin/dbus-daemon --config-file=/etc/at-spi2/accessibility.conf --nofork --print-address 3
006245 /usr/libexec/at-spi2-registryd --use-gnome-session
006258 /usr/bin/pulseaudio --start
006265 /usr/libexec/gnome-settings-daemon
006290 (krfcommd)
006297 /usr/libexec/gvfs-udisks2-volume-monitor
006299 /usr/lib/udisks2/udisksd --no-debug
006308 /usr/libexec/gvfs-afc-volume-monitor
006314 /usr/libexec/gvfs-goa-volume-monitor
006317 /usr/libexec/goa-daemon
006323 /usr/libexec/mission-control-5
006327 /usr/libexec/gvfs-mtp-volume-monitor
006340 /usr/libexec/gvfs-gphoto2-volume-monitor
006354 /usr/bin/gnome-shell
006371 /usr/sbin/cupsd -f
006378 /usr/libexec/gsd-printer
006381 /usr/libexec/dconf-service
006398 /usr/libexec/gnome-shell-calendar-server
006410 /usr/bin/ibus-daemon --replace --xim --panel disable
006414 /usr/libexec/evolution-source-registry
006418 /usr/libexec/ibus-dconf
006420 /usr/libexec/ibus-x11 --kill-daemon
006469 /usr/libexec/ibus-engine-simple
006483 /usr/libexec/evolution/3.10/evolution-alarm-notify
006525 /usr/libexec/gvfsd-metadata
006531 /usr/libexec/evolution-calendar-factory
006592 /usr/libexec/tracker-miner-fs
006596 /usr/bin/python /usr/share/system-config-printer/applet.py
006630 /usr/bin/seapplet
006656 /usr/libexec/tracker-store
006682 abrt-applet
006685 /usr/libexec/bluetooth/obexd
006687 /usr/libexec/telepathy-logger
006688 /usr/libexec/deja-dup/deja-dup-monitor
006693 /usr/libexec/gvfsd-burn --spawner :1.3 /org/gtk/gvfs/exec_spaw/1
006706 /usr/libexec/vino-server
006971 xchat
007014 /usr/lib64/firefox/firefox
007524 /usr/lib/systemd/systemd --user
007529 (sd-pam)
008026 /usr/lib/systemd/systemd --user
008033 (sd-pam)
008201 /usr/libexec/nm-vpnc-service
008209 /usr/sbin/vpnc --non-inter --no-detach -
008421 bash
011371 bash
014186 su - pcpqa
014187 -bash
014253 /usr/libexec/pcp/bin/pmcd -T 3
014256 /var/lib/pcp/pmdas/root/pmdaroot -d 1
014257 /var/lib/pcp/pmdas/proc/pmdaproc -d 3
014258 /var/lib/pcp/pmdas/xfs/pmdaxfs -d 11
014259 /var/lib/pcp/pmdas/shping/pmdashping -I 30 -t 10 /var/lib/pcp/config/shping/shping.conf
014262 /var/lib/pcp/pmdas/sample/pmdasample -d 29
014264 /var/lib/pcp/pmdas/linux/pmdalinux -DATTR
014266 perl /var/lib/pcp/pmdas/nfsclient/pmdanfsclient.pl
014273 perl /var/lib/pcp/pmdas/mysql/pmdamysql.pl
014287 /var/lib/pcp/pmdas/apache/pmdaapache -d 68
014288 /var/lib/pcp/pmdas/mounts/pmdamounts -d 72
014289 perl /var/lib/pcp/pmdas/elasticsearch/pmdaelasticsearch.pl
014290 perl /var/lib/pcp/pmdas/postgresql/pmdapostgresql.pl
014291 perl /var/lib/pcp/pmdas/nginx/pmdanginx.pl
014297 /var/lib/pcp/pmdas/rpm/pmdarpm -d 123
014299 python3 /var/lib/pcp/pmdas/zswap/pmdazswap.python
014314 /var/lib/pcp/pmdas/perfevent/pmdaperfevent -d 127
014316 python3 /var/lib/pcp/pmdas/json/pmdajson.python
014320 /var/lib/pcp/pmdas/simple/pmdasimple -d 253
016569 sshd: nathans [priv]
016582 sshd: nathans@pts/4
016591 -bash
016672 sshd: nathans [priv]
016677 sshd: nathans@pts/5
016680 -bash
017141 postgres: postgres postgres [local] idle
018628 /usr/libexec/pcp/bin/pmlogger -P -r -T24h10m -c config.default -m pmlogger_check 20150925.12.46
021036 sudo su - pcpqa
021037 su - pcpqa
021038 -bash
021354 bash
021388 (kworker/0:2)
021551 (kworker/3:2)
021737 /usr/libexec/gnome-terminal-server
021740 gnome-pty-helper
022288 bash
022779 /usr/libexec/gconfd-2
026776 (kworker/2:2)
027181 (kworker/1:0)
027881 (kworker/3:0)
028160 bash
028842 (kworker/1:3)
029079 (kworker/u16:2)
029334 sshd: nathans [priv]
029375 sshd: nathans@pts/6
029378 -bash
030059 (kworker/0:1)
030631 (kworker/2:1)
030730 (nfsiod)
030736 (nfsv4.0-svc)
031020 (kworker/u16:1)
031191 (kworker/0:0)
031252 (kworker/3:1)
031816 /tmp/1µsec 60
031857 pmlogger -c /tmp/config.proc procpid-encode -T1sec
032671 /usr/sbin/httpd -DFOREGROUND
032672 /usr/sbin/httpd -DFOREGROUND
032673 /usr/sbin/httpd -DFOREGROUND
032674 /usr/sbin/httpd -DFOREGROUND
032675 /usr/sbin/httpd -DFOREGROUND
032676 /usr/sbin/httpd -DFOREGROUND

== archives/procpid-encode2
000001 /sbin/init
000002 (kthreadd)
000003 (migration/0)
000004 (ksoftirqd/0)
000005 (stopper/0)
000006 (watchdog/0)
000007 (migration/1)
000008 (stopper/1)
000009 (ksoftirqd/1)
000010 (watchdog/1)
000011 (migration/2)
000012 (stopper/2)
000013 (ksoftirqd/2)
000014 (watchdog/2)
000015 (migration/3)
000016 (stopper/3)
000017 (ksoftirqd/3)
000018 (watchdog/3)
000019 (migration/4)
000020 (stopper/4)
000021 (ksoftirqd/4)
000022 (watchdog/4)
000023 (migration/5)
000024 (stopper/5)
000025 (ksoftirqd/5)
000026 (watchdog/5)
000027 (migration/6)
000028 (stopper/6)
000029 (ksoftirqd/6)
000030 (watchdog/6)
000031 (migration/7)
000032 (stopper/7)
000033 (ksoftirqd/7)
000034 (watchdog/7)
000035 (migration/8)
000036 (stopper/8)
000037 (ksoftirqd/8)
000038 (watchdog/8)
000039 (migration/9)
000040 (stopper/9)
000041 (ksoftirqd/9)
000042 (watchdog/9)
000043 (migration/10)
000044 (stopper/10)
000045 (ksoftirqd/10)
000046 (watchdog/10)
000047 (migration/11)
000048 (stopper/11)
000049 (ksoftirqd/11)
000050 (watchdog/11)
000051 (migration/12)
000052 (stopper/12)
000053 (ksoftirqd/12)
000054 (watchdog/12)
000055 (migration/13)
000056 (stopper/13)
000057 (ksoftirqd/13)
000058 (watchdog/13)
000059 (migration/14)
000060 (stopper/14)
000061 (ksoftirqd/14)
000062 (watchdog/14)
000063 (migration/15)
000064 (stopper/15)
000065 (ksoftirqd/15)
000066 (watchdog/15)
000067 (events/0)
000068 (events/1)
000069 (events/2)
000070 (events/3)
000071 (events/4)
000072 (events/5)
000073 (events/6)
000074 (events/7)
000075 (events/8)
000076 (events/9)
000077 (events/10)
000078 (events/11)
000079 (events/12)
000080 (events/13)
000081 (events/14)
000082 (events/15)
000083 (cgroup)
000084 (khelper)
000085 (netns)
000086 (async/mgr)
000087 (pm)
000088 (sync_supers)
000089 (bdi-default)
000090 (kintegrityd/0)
000091 (kintegrityd/1)
000092 (kintegrityd/2)
000093 (kintegrityd/3)
000094 (kintegrityd/4)
000095 (kintegrityd/5)
000096 (kintegrityd/6)
000097 (kintegrityd/7)
000098 (kintegrityd/8)
000099 (kintegrityd/9)
000100 (kintegrityd/10)
000101 (kintegrityd/11)
000102 (kintegrityd/12)
000103 (kintegrityd/13)
000104 (kintegrityd/14)
000105 (kintegrityd/15)
000106 (kblockd/0)
000107 (kblockd/1)
000108 (kblockd/2)
000109 (kblockd/3)
000110 (kblockd/4)
000111 (kblockd/5)
000112 (kblockd/6)
000113 (kblockd/7)
000114 (kblockd/8)
000115 (kblockd/9)
000116 (kblockd/10)
000117 (kblockd/11)
000118 (kblockd/12)
000119 (kblockd/13)
000120 (kblockd/14)
000121 (kblockd/15)
000122 (kacpid)
000123 (kacpi_notify)
000124 (kacpi_hotplug)
000125 (ata_aux)
000126 (ata_sff/0)
000127 (ata_sff/1)
000128 (ata_sff/2)
000129 (ata_sff/3)
000130 (ata_sff/4)
000131 (ata_sff/5)
000132 (ata_sff/6)
000133 (ata_sff/7)
000134 (ata_sff/8)
000135 (ata_sff/9)
000136 (ata_sff/10)
000137 (ata_sff/11)
000138 (ata_sff/12)
000139 (ata_sff/13)
000140 (ata_sff/14)
000141 (ata_sff/15)
000142 (ksuspend_usbd)
000143 (khubd)
000144 (kseriod)
000145 (md/0)
000146 (md/1)
000147 (md/2)
000148 (md/3)
000149 (md/4)
000150 (md/5)
000151 (md/6)
000152 (md/7)
000153 (md/8)
000154 (md/9)
000155 (md/10)
000156 (md/11)
000157 (md/12)
000158 (md/13)
000159 (md/14)
000160 (md/15)
000161 (md_misc/0)
000162 (md_misc/1)
000163 (md_misc/2)
000164 (md_misc/3)
000165 (md_misc/4)
000166 (md_misc/5)
000167 (md_misc/6)
000168 (md_misc/7)
000169 (md_misc/8)
000170 (md_misc/9)
000171 (md_misc/10)
000172 (md_misc/11)
000173 (md_misc/12)
000174 (md_misc/13)
000175 (md_misc/14)
000176 (md_misc/15)
000177 (linkwatch)
000195 (khungtaskd)
000196 (kswapd0)
000197 (kswapd1)
000198 (ksmd)
000199 (khugepaged)
000200 (aio/0)
000201 (aio/1)
000202 (aio/2)
000203 (aio/3)
000204 (aio/4)
000205 (aio/5)
000206 (aio/6)
000207 (aio/7)
000208 (aio/8)
000209 (aio/9)
000210 (aio/10)
000211 (aio/11)
000212 (aio/12)
000213 (aio/13)
000214 (aio/14)
000215 (aio/15)
000216 (crypto/0)
000217 (crypto/1)
000218 (crypto/2)
000219 (crypto/3)
000220 (crypto/4)
000221 (crypto/5)
000222 (crypto/6)
000223 (crypto/7)
000224 (crypto/8)
000225 (crypto/9)
000226 (crypto/10)
000227 (crypto/11)
000228 (crypto/12)
000229 (crypto/13)
000230 (crypto/14)
000231 (crypto/15)
000239 (kthrotld/0)
000240 (kthrotld/1)
000241 (kthrotld/2)
000242 (kthrotld/3)
000243 (kthrotld/4)
000244 (kthrotld/5)
000245 (kthrotld/6)
000246 (kthrotld/7)
000247 (kthrotld/8)
000248 (kthrotld/9)
000249 (kthrotld/10)
000250 (kthrotld/11)
000251 (kthrotld/12)
000252 (kthrotld/13)
000253 (kthrotld/14)
000254 (kthrotld/15)
000272 (kpsmoused)
000273 (usbhid_resumer)
000274 (deferwq)
000306 (kdmremove)
000307 (kstriped)
000417 (scsi_eh_0)
000453 (scsi_eh_1)
000454 (scsi_eh_2)
000455 (scsi_eh_3)
000456 (scsi_eh_4)
000457 (scsi_eh_5)
000458 (scsi_eh_6)
000528 (mlx4)
000548 (ib_addr)
000549 (infiniband/0)
000550 (infiniband/1)
000551 (infiniband/2)
000552 (infiniband/3)
000553 (infiniband/4)
000554 (infiniband/5)
000555 (infiniband/6)
000556 (infiniband/7)
000557 (infiniband/8)
000558 (infiniband/9)
000559 (infiniband/10)
000560 (infiniband/11)
000561 (infiniband/12)
000562 (infiniband/13)
000563 (infiniband/14)
000564 (infiniband/15)
000565 (ib_mcast)
000566 (mlx4_ib)
000567 (mlx4_ib_mcg)
000568 (ib_mad1)
000649 (flush-8:0)
000701 (jbd2/sda2-8)
000702 (ext4-dio-unwrit)
000790 /sbin/udevd -d
001053 /usr/sbin/anacron -s
001383 (edac-poller)
002353 (jbd2/sda3-8)
002354 (ext4-dio-unwrit)
002355 (jbd2/sda6-8)
002356 (ext4-dio-unwrit)
002357 (jbd2/sda5-8)
002358 (ext4-dio-unwrit)
002399 (kauditd)
002411 /bin/bash /usr/bin/run-parts /etc/cron.daily
002431 /bin/bash /etc/cron.daily/makewhatis.cron
002432 awk -v progname=/etc/cron.daily/makewhatis.cron progname {
				   print progname ":\n"
				   progname="";
			       }
			       { print; }
002439 /bin/bash /usr/sbin/makewhatis -U -w
002454 (iw_cm_wq)
002458 (ib_cm/0)
002460 (ib_cm/1)
002461 (ib_cm/2)
002462 (ib_cm/3)
002463 (ib_cm/4)
002464 (ib_cm/5)
002465 (ib_cm/6)
002466 (ib_cm/7)
002467 (ib_cm/8)
002468 (ib_cm/9)
002469 (ib_cm/10)
002470 (ib_cm/11)
002471 (ib_cm/12)
002472 (ib_cm/13)
002473 (ib_cm/14)
002474 (ib_cm/15)
002478 (rdma_cm)
002495 (ipoib_flush)
002496 (ipoib_wq)
002759 auditd
002780 /sbin/rsyslogd -i /var/run/syslogd.pid -c 5
002793 irqbalance --pid=/var/run/irqbalance.pid
002809 rpcbind
002824 /usr/sbin/sssd -f -D
002825 /usr/libexec/sssd/sssd_be --domain CCR --debug-to-files
002826 /usr/libexec/sssd/sssd_nss --debug-to-files
002827 /usr/libexec/sssd/sssd_pam --debug-to-files
002846 rpc.statd
008086 find /usr/share/man/man3 -name * -xtype f -size +0 -cnewer /var/cache/man/whatis
008087 /bin/awk 

	    function readline() {
              if (use_zcat || use_bzcat || use_lzcat) {
		result = (pipe_cmd | getline);
		if (result < 0) {
		  print "Pipe error: " pipe_cmd " " ERRNO > "/dev/stderr";
		}
	      } else {
		result = (getline < filename);
		if (result < 0) {
		  print "Read file error: " filename " " ERRNO > "/dev/stderr";
		}
	      }
	      return result;
	    }
	    
	    function closeline() {
              if (use_zcat || use_bzcat || use_lzcat) {
		return close(pipe_cmd);
	      } else {
		return close(filename);
	      }
	    }
	    
	    function do_one() {
	      insh = 0; thisjoin = 1; done = 0;
	      entire_line = "";

	      if (verbose) {
		print "adding " filename > "/dev/stderr"
	      }
	      
	      use_zcat = match(filename,"\\.Z$") ||
			 match(filename,"\\.z$") || match(filename,"\\.gz$");
	      if (!use_zcat)
		use_bzcat = match(filename,"\\.bz2");
              if(!use_bzcat && !use_zcat)
                use_lzcat = match(filename,"\\.lzma");
              if (use_zcat || use_bzcat || use_lzcat ) {
		filename_no_gz = substr(filename, 0, RSTART - 1);
	      } else {
		filename_no_gz = filename;
	      }
	      match(filename_no_gz, "/[^/]+$");
	      progname = substr(filename, RSTART + 1, RLENGTH - 1);
	      if (match(progname, "\\." section "[A-Za-z]+")) {
		actual_section = substr(progname, RSTART + 1, RLENGTH - 1);
	      } else {
		actual_section = section;
	      }
	      sub(/\..*/, "", progname);
              if (use_zcat || use_bzcat || use_lzcat) {
		if (use_zcat) {
		  pipe_cmd = "zcat \"" filename "\" 2>/dev/null";
                } else if (use_bzcat) {
		  pipe_cmd = "bzcat \"" filename "\" 2>/dev/null";
                } else {
                  pipe_cmd = "lzcat \"" filename "\" 2>/dev/null";
                }
                # Chuck output unless it is utf-8
                pipe_cmd = pipe_cmd " |iconv -f utf-8 -t utf-8 2>/dev/null"
		# try to avoid suspicious stuff
		if (filename ~ /[;&|`$(]/) {
		  print "ignored strange file name " filename " in " curdir > "/dev/stderr";
		  return;
		}
	      }
	    
	      while (!done && readline() > 0) {
		gsub(/.\b/, "");
		if (($1 ~ /^\.[Ss][Hh]/ &&
		  ($2 ~ /[Nn][Aa][Mm][Ee]/ ||
		   $2 ~ /^JMNO/ || $2 ~ /^NAVN/ || $2 ~ /^NUME/ ||
		   $2 ~ /^BEZEICHNUNG/ || $2 ~ /^NOMBRE/ ||
		   $2 ~ /^NIMI/ || $2 ~ /^NOM/ || $2 ~ /^IME/ ||
		   $2 ~ /^N[E]V/ || $2 ~ /^NAMA/ || $2 ~ /^̾/ ||
		   $2 ~ /^̾/ || $2 ~ /^̸/ || $2 ~ /^NAZWA/ ||
		   $2 ~ /^/ || $2 ~ /^/ || $2 ~ /^W/ ||
		   $2 ~ /^NOME/ || $2 ~ /^NAAM/ || $2 ~ /^/)) ||
		  (pages == "cat" && $1 ~ /^NAME/)) {
		    if (!insh) {
		      insh = 1;
		    } else {
		      done = 1;
		    }
		} else if (insh) {
		  if ($1 ~ /^\.[Ss][HhYS]/ ||
		    (pages == "cat" &&
		    ($1 ~ /^S[yYeE]/ || $1 ~ /^DESCRIPTION/ ||
		     $1 ~ /^COMMAND/ || $1 ~ /^OVERVIEW/ ||
		     $1 ~ /^STRUCTURES/ || $1 ~ /^INTRODUCTION/ ||
		     $0 ~ /^[^ ]/))) {
		      # end insh for Synopsis, Syntax, but also for
		      # DESCRIPTION (e.g., XFree86.1x),
		      # 
010198 dbus-daemon --system
010214 /usr/sbin/ibacm
010243 /usr/sbin/acpid
010280 /usr/sbin/munged
010298 /usr/sbin/sshd
010307 xinetd -stayalive -pidfile /var/run/xinetd.pid
010340 /usr/lpp/mmfs/bin/mmksh /usr/lpp/mmfs/bin/mmccrmonitor 15
013386 /usr/lpp/mmfs/bin/mmksh /usr/lpp/mmfs/bin/mmccrmonitor 15
016372 slurmstepd: [3929549]
016376 /bin/bash /var/spool/slurmd/job3929549/slurm_script
016465 /bin/bash /util/academic/namd/NAMD_2.9_Source-MPI/charmsrun +p 16 ++remote-shell /usr/bin/ssh ++nodelist tmp.16376 /util/academic/namd/NAMD_2.9_Source-MPI/namd2 nofield4.namd
016572 srun --nodes=1 --ntasks-per-node=16 --cpus-per-task=1 /util/academic/namd/NAMD_2.9_Source-MPI/namd2 nofield4.namd
016573 srun --nodes=1 --ntasks-per-node=16 --cpus-per-task=1 /util/academic/namd/NAMD_2.9_Source-MPI/namd2 nofield4.namd
016580 slurmstepd: [3929549.2]
016585 /util/academic/namd/NAMD_2.9_Source-MPI/namd2 nofield4.namd
016586 /util/academic/namd/NAMD_2.9_Source-MPI/namd2 nofield4.namd
016587 /util/academic/namd/NAMD_2.9_Source-MPI/namd2 nofield4.namd
016588 /util/academic/namd/NAMD_2.9_Source-MPI/namd2 nofield4.namd
016589 /util/academic/namd/NAMD_2.9_Source-MPI/namd2 nofield4.namd
016590 /util/academic/namd/NAMD_2.9_Source-MPI/namd2 nofield4.namd
016591 /util/academic/namd/NAMD_2.9_Source-MPI/namd2 nofield4.namd
016592 /util/academic/namd/NAMD_2.9_Source-MPI/namd2 nofield4.namd
016593 /util/academic/namd/NAMD_2.9_Source-MPI/namd2 nofield4.namd
016594 /util/academic/namd/NAMD_2.9_Source-MPI/namd2 nofield4.namd
016595 /util/academic/namd/NAMD_2.9_Source-MPI/namd2 nofield4.namd
016596 /util/academic/namd/NAMD_2.9_Source-MPI/namd2 nofield4.namd
016597 /util/academic/namd/NAMD_2.9_Source-MPI/namd2 nofield4.namd
016598 /util/academic/namd/NAMD_2.9_Source-MPI/namd2 nofield4.namd
016599 /util/academic/namd/NAMD_2.9_Source-MPI/namd2 nofield4.namd
016600 /util/academic/namd/NAMD_2.9_Source-MPI/namd2 nofield4.namd
018875 ntpd -u ntp:ntp -p /var/run/ntpd.pid -g
018909 /usr/sbin/nrpe -c /etc/nagios/nrpe.cfg -d
018986 /usr/libexec/postfix/master
019008 crond
019022 /usr/sbin/slurmd
019092 /usr/sbin/atd
019567 /usr/libexec/pcp/bin/pmcd
019570 /var/lib/pcp/pmdas/root/pmdaroot
019573 /var/lib/pcp/pmdas/proc/pmdaproc -d 3 -A
019575 qmgr -l -t fifo -u
019576 /var/lib/pcp/pmdas/xfs/pmdaxfs -d 11
019584 perl /var/lib/pcp/pmdas/slurm/pmdaslurm.pl
019623 /var/lib/pcp/pmdas/linux/pmdalinux
019631 perl /var/lib/pcp/pmdas/nfsclient/pmdanfsclient.pl
019634 /var/lib/pcp/pmdas/infiniband/pmdaib -d 91
019644 /var/lib/pcp/pmdas/nvidia/pmdanvidia -d 120
019649 /var/lib/pcp/pmdas/perfevent/pmdaperfevent -d 127
019663 perl /var/lib/pcp/pmdas/gpfs/pmdagpfs.pl
019879 /usr/libexec/pcp/bin/pmproxy
019964 /opt/dell/srvadmin/sbin/dsm_sa_datamgrd
020257 /opt/dell/srvadmin/sbin/dsm_sa_eventmgrd
020276 /sbin/mingetty /dev/tty1
020278 /sbin/mingetty /dev/tty2
020280 /sbin/mingetty /dev/tty3
020282 /sbin/mingetty /dev/tty4
020284 /sbin/mingetty /dev/tty5
020286 /sbin/mingetty /dev/tty6
020288 /sbin/udevd -d
020289 /sbin/udevd -d
020802 CROND
020812 (pmlogger_daily)
020982 /usr/lpp/mmfs/bin/mmksh /usr/lpp/mmfs/bin/runmmfs
021036 (gpfs_aio/0)
021037 (gpfs_aio/1)
021038 (gpfs_aio/2)
021039 (gpfs_aio/3)
021040 (gpfs_aio/4)
021041 (gpfs_aio/5)
021042 (gpfs_aio/6)
021043 (gpfs_aio/7)
021044 (gpfs_aio/8)
021045 (gpfs_aio/9)
021046 (gpfs_aio/10)
021047 (gpfs_aio/11)
021048 (gpfs_aio/12)
021049 (gpfs_aio/13)
021050 (gpfs_aio/14)
021051 (gpfs_aio/15)
021055 (gpfsSwapdKproc)
021205 pmlogger -r -m pmlogger_daily -P -l pmlogger.log -c /etc/pcp/pmlogger/pmlogger-config.ubccr 20150603.00.10
021238 /usr/lpp/mmfs/bin/mmfsd
021311 (mmkproc)
021312 (mmkproc)
021313 (mmkproc)
021415 (nfsWatchKproc)
031499 pickup -l -t fifo -u

