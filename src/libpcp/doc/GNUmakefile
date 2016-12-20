#
# create .dot and .png files from source files using mk.cgraph
#

PNGFILES = access.png access_part.png util.png AF.png desc \
	   derive.png derive_part.png desc.png store.png avahi.png \
	   probe.png probe_part.png access_prune.png probe_prune.png

default: $(PNGFILES)

.PRECIOUS: $(PNGFILES:.png=.dot)

$(PNGFILES) $(PNGFILES:.png=.dot): mk.cgraph \
	     glibc.safe glibc.unsafe other.safe other.unsafe posix.unsafe

%.png:	%.dot
	dot -Tpng <$< >$@

%.dot:	../src/%.c
	mk.cgraph -w -f $< >$@

util.dot:	../src/config.c

access_part.png:	access_part.dot
access_part.dot:	../src/access.c
	mk.cgraph -w -s __pmNotifyErr -f ../src/access.c >access_part.dot

access_prune.png:	access_part.dot
access_prune.dot:	access.dot
	cp access_part.dot access_prune.dot
	prune.cgraph access_prune.dot

derive_part.png:	derive_part.dot
derive_part.dot:	../src/derive.c
	mk.cgraph -w -s __pmNotifyErr -f ../src/derive.c >derive_part.dot

probe_part.png:	probe_part.dot
probe_part.dot:	../src/probe.c
	mk.cgraph -w -s __pmNotifyErr -f ../src/probe.c >probe_part.dot

probe_prune.png:	probe_part.dot
probe_prune.dot:	probe.dot
	cp probe_part.dot probe_prune.dot
	prune.cgraph probe_prune.dot


