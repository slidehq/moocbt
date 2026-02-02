# SPDX-License-Identifier: GPL-2.0-only

BINARY_NAME = moo
INSTALLDIR = $(PREFIX)/bin
SOURCES = moo.c

.PHONY: shared static install-static install uninstall clean

shared:
	$(CC) $(CCFLAGS) -o $(BINARY_NAME) -L $(BASE_DIR)/lib $(SOURCES) -lmoocbt

static:
	$(CC) $(CCFLAGS) -o $(BINARY_NAME) $(SOURCES) $(BASE_DIR)/lib/libmoocbt.a

install-static: static
	mkdir -p $(INSTALLDIR)
	install $(BINARY_NAME) $(INSTALLDIR)

install: shared
	mkdir -p $(INSTALLDIR)
	install $(BINARY_NAME) $(INSTALLDIR)

uninstall:
	$(RM) $(INSTALLDIR)/$(BINARY_NAME)

clean:
	$(RM) $(BINARY_NAME)
