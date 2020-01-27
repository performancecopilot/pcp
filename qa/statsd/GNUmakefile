TOPDIR = ../..
include $(TOPDIR)/src/include/builddefs

TESTDIR = $(PCP_VAR_DIR)/testsuite/statsd
TESTSRCDIR = src
TESTFILES = $(shell find $(TESTSRCDIR) -type f)

ifeq "$(PMDA_STATSD)" "true"
default setup default_pcp:

install install_pcp: default $(TESTFILES)
	$(INSTALL) -m 755 -d $(TESTDIR)
	# I wish I knew how to extract such listing from $(TESTFILES), not sure why this is needed yet
	$(INSTALL) -m 644 src/cases/01.py $(TESTDIR)/src/cases/01.py
	$(INSTALL) -m 644 src/cases/02.py $(TESTDIR)/src/cases/02.py
	$(INSTALL) -m 644 src/cases/03.py $(TESTDIR)/src/cases/03.py
	$(INSTALL) -m 644 src/cases/04.py $(TESTDIR)/src/cases/04.py
	$(INSTALL) -m 644 src/cases/05.py $(TESTDIR)/src/cases/05.py
	$(INSTALL) -m 644 src/cases/06.py $(TESTDIR)/src/cases/06.py
	$(INSTALL) -m 644 src/cases/07.py $(TESTDIR)/src/cases/07.py
	$(INSTALL) -m 644 src/cases/08.py $(TESTDIR)/src/cases/08.py
	$(INSTALL) -m 644 src/cases/09.py $(TESTDIR)/src/cases/09.py
	$(INSTALL) -m 644 src/cases/10.py $(TESTDIR)/src/cases/10.py
	$(INSTALL) -m 644 src/cases/11.py $(TESTDIR)/src/cases/11.py
	$(INSTALL) -m 644 src/cases/12.py $(TESTDIR)/src/cases/12.py
	$(INSTALL) -m 644 src/cases/13.py $(TESTDIR)/src/cases/13.py
	$(INSTALL) -m 644 src/configs/complex/0/pmdastatsd.ini $(TESTDIR)/src/configs/complex/0/pmdastatsd.ini
	$(INSTALL) -m 644 src/configs/complex/1/pmdastatsd.ini $(TESTDIR)/src/configs/complex/1/pmdastatsd.ini
	$(INSTALL) -m 644 src/configs/single/debug_output_filename/0/pmdastatsd.ini $(TESTDIR)/src/configs/single/debug_output_filename/0/pmdastatsd.ini
	$(INSTALL) -m 644 src/configs/single/debug_output_filename/1/pmdastatsd.ini $(TESTDIR)/src/configs/single/debug_output_filename/1/pmdastatsd.ini
	$(INSTALL) -m 644 src/configs/single/duration_aggregation_type/0/pmdastatsd.ini $(TESTDIR)/src/configs/single/duration_aggregation_type/0/pmdastatsd.ini
	$(INSTALL) -m 644 src/configs/single/duration_aggregation_type/1/pmdastatsd.ini $(TESTDIR)/src/configs/single/duration_aggregation_type/1/pmdastatsd.ini
	$(INSTALL) -m 644 src/configs/single/max_udp_packet_size/0/pmdastatsd.ini $(TESTDIR)/src/configs/single/max_udp_packet_size/0/pmdastatsd.ini
	$(INSTALL) -m 644 src/configs/single/max_udp_packet_size/1/pmdastatsd.ini $(TESTDIR)/src/configs/single/max_udp_packet_size/1/pmdastatsd.ini
	$(INSTALL) -m 644 src/configs/single/max_udp_packet_size/2/pmdastatsd.ini $(TESTDIR)/src/configs/single/max_udp_packet_size/2/pmdastatsd.ini
	$(INSTALL) -m 644 src/configs/single/max_unprocessed_packets/0/pmdastatsd.ini $(TESTDIR)/src/configs/single/max_unprocessed_packets/0/pmdastatsd.ini
	$(INSTALL) -m 644 src/configs/single/max_unprocessed_packets/1/pmdastatsd.ini $(TESTDIR)/src/configs/single/max_unprocessed_packets/1/pmdastatsd.ini
	$(INSTALL) -m 644 src/configs/single/parser_type/0/pmdastatsd.ini $(TESTDIR)/src/configs/single/parser_type/0/pmdastatsd.ini
	$(INSTALL) -m 644 src/configs/single/parser_type/1/pmdastatsd.ini $(TESTDIR)/src/configs/single/parser_type/1/pmdastatsd.ini
	$(INSTALL) -m 644 src/configs/single/port/0/pmdastatsd.ini $(TESTDIR)/src/configs/single/port/0/pmdastatsd.ini
	$(INSTALL) -m 644 src/configs/single/port/1/pmdastatsd.ini $(TESTDIR)/src/configs/single/port/1/pmdastatsd.ini
	$(INSTALL) -m 644 src/configs/single/verbose/0/pmdastatsd.ini $(TESTDIR)/src/configs/single/verbose/0/pmdastatsd.ini
	$(INSTALL) -m 644 src/configs/single/verbose/1/pmdastatsd.ini $(TESTDIR)/src/configs/single/verbose/1/pmdastatsd.ini
	$(INSTALL) -m 644 src/configs/single/verbose/2/pmdastatsd.ini $(TESTDIR)/src/configs/single/verbose/2/pmdastatsd.ini
	$(INSTALL) -m 644 src/utils/pmdastatsd_test_utils.py $(TESTDIR)/src/utils/pmdastatsd_test_utils.py
	
	$(INSTALL) -m 644 GNUmakefile.install $(TESTDIR)/GNUmakefile
else
default setup default_pcp:
install install_pcp:
endif

include $(BUILDRULES)
