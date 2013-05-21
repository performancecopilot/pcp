TOPDIR = ../../..
include $(TOPDIR)/src/include/builddefs

IAM = user-administrator-guide
LDIRDIRT = Common_Content
LDIRT = $(IAM).xml $(IAM).fo

default build.done: $(IAM).xml

include $(BUILDRULES)

$(IAM).xml:
	$(LN_S) ../$(IAM).xml $(IAM).xml

install:
