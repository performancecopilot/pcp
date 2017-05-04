#
# create .dot and .png files from source files using mk.cgraph
#

PNGFILES = access.png accounts.png AF.png auxconnect.png \
	   auxserver.png avahi.png checksum.png config.png \
	   connect.png connectlocal.png context.png \
	   derive_parser.png derive_fetch.png desc.png discovery.png \
	   endian.png err.png events.png fault.png \
	   fetch.png fetchgroup.png fetchlocal.png freeresult.png \
	   getdate.tab.png getopt.png hash.png help.png instance.png \
	   interp.png ipc.png lock.png logconnect.png \
	   logcontrol.png logmeta.png logportmap.png logutil.png \
	   optfetch.png pdubuf.png pdu.png pmns.png probe.png profile.png \
	   p_attr.png p_creds.png p_desc.png p_error.png p_fetch.png \
	   p_instance.png p_lcontrol.png p_lrequest.png p_lstatus.png \
	   p_pmns.png p_profile.png p_result.png p_text.png \
	   rtime.png secureconnect.png secureserver.png sortinst.png spec.png \
	   store.png stuffvalue.png tv.png tz.png \
	   units.png util.png win32.png \
	   access_part.png access_prune.png \
	   derive_parser_part.png logutil_part.png \
	   probe_part.png probe_prune.png \
	   tz_part.png

default: $(PNGFILES)

clean:
	rm -f $(PNGFILES:.png=.dot)
	rm -f $(PNGFILES)

.PRECIOUS: $(PNGFILES:.png=.dot)

$(PNGFILES) $(PNGFILES:.png=.dot): mk.cgraph \
	     glibc.safe glibc.unsafe other.safe other.unsafe posix.unsafe \
	     nolock.calls

%.png:	%.dot
	dot -Tpng <$< >$@

%.dot:	../src/%.c
	mk.cgraph -w -f $< >$@

%.dot:	../src/%.y
	mk.cgraph -w -f $< >$@

util.dot:	../src/config.c

access_part.png:	access_part.dot
access_part.dot:	../src/access.c
	mk.cgraph -w -s __pmNotifyErr -f ../src/access.c >access_part.dot

access_prune.png:	access_part.dot
access_prune.dot:	access.dot
	cp access_part.dot access_prune.dot
	prune.cgraph access_prune.dot

derive_parser_part.png:	derive_parser_part.dot
derive_parser_part.dot:	../src/derive_parser.y
	mk.cgraph -w -s __pmNotifyErr -f ../src/derive_parser.y >derive_parser_part.dot

probe_part.png:	probe_part.dot
probe_part.dot:	../src/probe.c
	mk.cgraph -w -s __pmNotifyErr -f ../src/probe.c >probe_part.dot

probe_prune.png:	probe_part.dot
probe_prune.dot:	probe.dot
	cp probe_part.dot probe_prune.dot
	prune.cgraph probe_prune.dot

tz_part.png:	tz_part.dot
tz_part.dot:	../src/tz.c
	mk.cgraph -w -s pmLookupName,pmFetch,pmFreeResult,__pmNoMem -f ../src/tz.c >tz_part.dot

logutil_part.png:	logutil_part.dot
logutil_part.dot:	../src/logutil.c
	mk.cgraph -w -s __pmNotifyErr -f ../src/logutil.c >logutil_part.dot

