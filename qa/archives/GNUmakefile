TOPDIR = ../..
include $(TOPDIR)/src/include/builddefs

SRCARCH = chartqa1.0 viewqa1.0 viewqa2.0 viewqa3.0 rattle.0 oview-short.0 \
	  snort-disks.0 vldb-disks.0 20071023.08\:06\:36.0 moomba.pmkstat.0
SRCEXTRAS = $(subst 0,meta,$(SRCARCH)) $(subst 0,index,$(SRCARCH))
LSRCFILES = $(SRCARCH) $(SRCEXTRAS)
TESTDIR = $(PCP_VAR_DIR)/../pcp-gui/testsuite/archives

MKARCH = gap.0 gap2.0
MKEXTRAS = $(subst 0,meta,$(MKARCH)) $(subst 0,index,$(MKARCH))
LDIRT = $(MKARCH) $(MKEXTRAS) pmlogger.log
LDIRDIRT = tmp

default:

install: $(SRCARCH)
	$(INSTALL) -m 755 -d $(TESTDIR)
	$(INSTALL) -m 755 $(SRCARCH) $(TESTDIR)

arch: $(MKARCH)

include $(BUILDRULES)

# gap: 10 sec data, 5 sec gap, 10 sec data
# gap2: 10 sec data, 5 sec gap, 10 sec data, 5 sec gap, 10 sec data
# sample.bin instances change across gaps
gap.0 gap2.0:
	rm -rf tmp
	mkdir tmp
	echo 'log mandatory on 1sec { pmcd.pdu_in,pmcd.numagents,hinv.ncpu,sample.bin["bin-100","bin-200","bin-400"] }' >tmp/A.config
	echo 'log mandatory on 1sec { pmcd.pdu_in,pmcd.numagents,hinv.ncpu,sample.bin["bin-100","bin-300","bin-400"] }' >tmp/B.config
	echo 'log mandatory on 1sec { pmcd.pdu_in,pmcd.numagents,hinv.ncpu,sample.bin["bin-100","bin-400"] }' >tmp/C.config
	$(PCP_BINADM_DIR)/pmlogger -s 10 -c tmp/A.config tmp/A
	sleep 5
	$(PCP_BINADM_DIR)/pmlogger -s 10 -c tmp/B.config tmp/B
	sleep 5
	$(PCP_BINADM_DIR)/pmlogger -s 10 -c tmp/C.config tmp/C
	rm -f gap.index gap.meta gap.0
	$(PCP_BINADM_DIR)/pmlogextract tmp/A tmp/B gap
	rm -f gap2.index gap2.meta gap2.0
	$(PCP_BINADM_DIR)/pmlogextract tmp/A tmp/B tmp/C gap2
	rm -rf tmp
