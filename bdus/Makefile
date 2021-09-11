# ---------------------------------------------------------------------------- #

# version

VERSION := 0.0.9

# configuration

KBDUS_DEBUG  ?= 0 # valid values: 0, 1
KBDUS_SPARSE ?= 0 # valid values: 0, 1

KBDUS_KDIR        ?= /lib/modules/$(shell uname -r)/build
KBDUS_INSTALL_DIR ?= /lib/modules/$(shell uname -r)/kernel/drivers/block

LIBBDUS_HEADER_INSTALL_DIR ?= /usr/include
LIBBDUS_BINARY_INSTALL_DIR ?= /usr/lib

CMDBDUS_BINARY_INSTALL_DIR     ?= /usr/bin
CMDBDUS_COMPLETION_INSTALL_DIR ?= /etc/bash_completion.d

# ---------------------------------------------------------------------------- #

# version

VERSION_MAJOR := $(word 1,$(subst ., ,$(VERSION)))
VERSION_MINOR := $(word 2,$(subst ., ,$(VERSION)))
VERSION_PATCH := $(word 3,$(subst ., ,$(VERSION)))

VERSION_CFLAGS := \
-DBDUS_VERSION_MAJOR=$(VERSION_MAJOR) \
-DBDUS_VERSION_MINOR=$(VERSION_MINOR) \
-DBDUS_VERSION_PATCH=$(VERSION_PATCH)

# kbdus

KBDUS_FLAGS := \
M='$(realpath kbdus)' W=1 C=$(KBDUS_SPARSE) \
ADDITIONAL_CFLAGS='$(VERSION_CFLAGS) -DKBDUS_DEBUG=$(KBDUS_DEBUG)'

# libbdus

# TODO: use this definition when moving to 0.1.0
# LIBBDUS_SONAME := libbdus.so.$(VERSION_MAJOR).$(VERSION_MINOR)

LIBBDUS_SONAME := libbdus.so.$(VERSION_MAJOR).$(VERSION_MINOR).$(VERSION_PATCH)

LIBBDUS_CFLAGS := \
-std=c99 -Werror -Wall -Wextra -Wconversion -pedantic \
-O2 -march=native -g -pthread $(VERSION_CFLAGS) -fvisibility=hidden \
-shared -fPIC -Wl,--no-undefined -Wl,-soname,$(LIBBDUS_SONAME) \
-Ikbdus/include -Ilibbdus/include -Ilibbdus/include-private

LIBBDUS_SRCS := $(shell find libbdus/src -type f -name '*.c')

# cmdbdus

CMDBDUS_CFLAGS := \
-std=c99 -Werror -Wall -Wextra -Wconversion -pedantic \
-O2 -march=native -g $(VERSION_CFLAGS) -Ilibbdus/include

# ---------------------------------------------------------------------------- #
# all

.PHONY: all
all: kbdus.ko libbdus.so.$(VERSION) bdus

# ---------------------------------------------------------------------------- #
# build

.PHONY: kbdus.ko
kbdus.ko:
	$(MAKE) -C $(KBDUS_KDIR) $(KBDUS_FLAGS)
	mv kbdus/kbdus.ko .

.PHONY: libbdus.so.$(VERSION)
libbdus.so.$(VERSION):
	$(CC) $(LIBBDUS_CFLAGS) $(LIBBDUS_SRCS) -o $@

.PHONY: bdus
bdus: cmdbdus/bdus.c libbdus.so.$(VERSION)
	$(CC) $(CMDBDUS_CFLAGS) $^ -o $@

# ---------------------------------------------------------------------------- #
# clean

.PHONY: clean
clean:
	$(MAKE) -C $(KBDUS_KDIR) $(KBDUS_FLAGS) clean
	rm -f kbdus.ko libbdus.so.$(VERSION) bdus

# ---------------------------------------------------------------------------- #
# install

.PHONY: install
install: install-kbdus install-libbdus install-cmdbdus

.PHONY: install-kbdus
install-kbdus: kbdus.ko
	# fail only if kbdus is installed and loaded but can't remove it
	! modinfo kbdus > /dev/null 2> /dev/null || modprobe -r kbdus
	install -m 644 kbdus.ko $(KBDUS_INSTALL_DIR)/
	depmod # this may take some time

.PHONY: install-libbdus-nodev
install-libbdus-nodev: libbdus.so.$(VERSION)
	# TODO: uncomment this line when moving to 0.1.0
	# rm -f $(LIBBDUS_BINARY_INSTALL_DIR)/$(LIBBDUS_SONAME).*?
	install -m 644 libbdus.so.$(VERSION) $(LIBBDUS_BINARY_INSTALL_DIR)/
	ldconfig # this may take some time

.PHONY: install-libbdus
install-libbdus: install-libbdus-nodev
	install -m 644 libbdus/include/bdus.h $(LIBBDUS_HEADER_INSTALL_DIR)/
	ln -fs $(LIBBDUS_SONAME) $(LIBBDUS_BINARY_INSTALL_DIR)/libbdus.so

.PHONY: install-cmdbdus
install-cmdbdus: bdus
	install -m 755 bdus $(CMDBDUS_BINARY_INSTALL_DIR)/
	install -m 644 cmdbdus/bdus-completion.bash $(CMDBDUS_COMPLETION_INSTALL_DIR)/

# ---------------------------------------------------------------------------- #
# uninstall

.PHONY: uninstall
uninstall: uninstall-kbdus uninstall-libbdus uninstall-cmdbdus

.PHONY: uninstall-kbdus
uninstall-kbdus:
	# fail only if kbdus is installed and loaded but can't remove it
	! modinfo kbdus > /dev/null 2> /dev/null || modprobe -r kbdus
	rm -f $(KBDUS_INSTALL_DIR)/kbdus.ko
	depmod # this may take some time

.PHONY: uninstall-libbdus-nodev
uninstall-libbdus-nodev:
	rm -f $(LIBBDUS_BINARY_INSTALL_DIR)/libbdus.so.$(VERSION)
	ldconfig # this may take some time

.PHONY: uninstall-libbdus
uninstall-libbdus: uninstall-libbdus-nodev
	rm -f $(LIBBDUS_BINARY_INSTALL_DIR)/libbdus.so
	rm -f $(LIBBDUS_HEADER_INSTALL_DIR)/bdus.h

.PHONY: uninstall-cmdbdus
uninstall-cmdbdus:
	rm -f $(CMDBDUS_COMPLETION_INSTALL_DIR)/bdus-completion.bash
	rm -f $(CMDBDUS_BINARY_INSTALL_DIR)/bdus

# ---------------------------------------------------------------------------- #
