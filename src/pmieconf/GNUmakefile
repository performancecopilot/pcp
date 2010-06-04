# This program is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the
# Free Software Foundation; either version 2 of the License, or (at your
# option) any later version.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
# or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
# for more details.
#
# You should have received a copy of the GNU General Public License along
# with this program; if not, write to the Free Software Foundation, Inc.,
# 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA

TOPDIR = ../..
include	$(TOPDIR)/src/include/builddefs

MKFILE_SUBDIRS = cpu filesys memory percpu pernetif

SUBDIRS	= global $(MKFILE_SUBDIRS)

TARGET = pmieconf
CFILES = pmieconf.c rules.c io.c
HFILES = rules.h

LSRCFILES = GNUmakefile.rules check-rules pmie_email xtractnames $(RFILES)

LLDLIBS = -lpcp
LDIRT = rules local $(TARGET) rate-syscalls \
	pmlogger.tmp pmlogger.config \
	cpu/GNUmakefile filesys/GNUmakefile memory/GNUmakefile \
	percpu/GNUmakefile pernetif/GNUmakefile

default_pcp: $(TARGET) makefiles rules pmlogger.config

# for src-link-pcp target from buildrules
$(SUBDIRS):	makefiles

$(TARGET) : $(OBJECTS)

pmieconf.o rules.o:    rules.h

makefiles:
	@for d in $(MKFILE_SUBDIRS); do \
	    rm -f $$d/GNUmakefile; \
	    cp GNUmakefile.rules $$d/GNUmakefile; \
	done

# setting PATH below before running ./pmieconf is needed in the MinGW world
# as an alternative to LD_LIBRARY_PATH
#
rules: $(SUBDIRS)
	rm -fr rules local; mkdir rules; exit 0
	$(SUBDIRS_MAKERULE)
	LD_LIBRARY_PATH=../libpcp/src PATH=$$PATH:../libpcp/src ./pmieconf -F -r rules -f local

pmlogger.config: $(SUBDIRS)
	@rm -f pmlogger.config
	@echo "# extracted from pmieconf rules in source tree" >pmlogger.config
	@echo >>pmlogger.config
	@echo "log advisory on default {" >>pmlogger.config
	$(SUBDIRS_MAKERULE) | grep -v '===' >pmlogger.tmp
	@sort -u pmlogger.tmp | sed -e 's/^/    /' >>pmlogger.config
	@echo "}" >>pmlogger.config

install_pcp: default_pcp $(SUBDIRS)
	$(SUBDIRS_MAKERULE)
	$(INSTALL) -m 755 $(TARGET) $(PCP_BIN_DIR)/$(TARGET)
	$(INSTALL) -m 755 pmie_email $(PCP_BINADM_DIR)/pmie_email
	$(INSTALL) -m 644 pmlogger.config $(PCP_VAR_DIR)/config/pmlogger/config.pmieconf

include $(BUILDRULES)
