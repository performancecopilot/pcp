TOPDIR = ../..
include $(TOPDIR)/src/include/builddefs

LSRCFILES = $(SRCARCH) $(subst 0,meta,$(SRCARCH)) $(subst 0,index,$(SRCARCH))

SRCARCH = chartqa1.0 viewqa1.0 viewqa2.0 viewqa3.0 rattle.0 oview-short.0 \
	  20071023.08:06:36.0 
MKARCH = gap.0 gap2.0

LDIRT = $(MKARCH) $(subst 0,meta,$(MKARCH)) $(subst 0,index,$(MKARCH)) \
	pmlogger.log
LDIRDIRT = tmp

default install: $(MKARCH)

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
	pmlogger -s 10 -c tmp/A.config tmp/A
	sleep 5
	pmlogger -s 10 -c tmp/B.config tmp/B
	sleep 5
	pmlogger -s 10 -c tmp/C.config tmp/C
	rm -f gap.index gap.meta gap.0
	pmlogextract tmp/A tmp/B gap
	rm -f gap2.index gap2.meta gap2.0
	pmlogextract tmp/A tmp/B tmp/C gap2
	rm -rf tmp
