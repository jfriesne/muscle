TOPTARGETS := all clean

SUBDIRS := $(dir $(wildcard */Makefile))

$(TOPTARGETS): $(SUBDIRS)
$(SUBDIRS): libmuscle.a
	$(MAKE) -C $@ $(MAKECMDGOALS)

libmuscle.a: 
	$(MAKE) -C ../../../server $(MAKECMDGOALS)

.PHONY: $(TOPTARGETS) $(SUBDIRS)
