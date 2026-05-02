# Makefile for pam_dock_check
#
# Usage:
#   make              - build the .so
#   make install      - install to /usr/lib64/security/ (Fedora default)
#   make uninstall    - remove from system
#   make clean        - remove build artefacts

CC      := gcc
CFLAGS  := -shared -fPIC -Wall -Wextra -O2 -D_GNU_SOURCE
LDFLAGS := -lpam
TARGET  := pam_dock_check.so
SRC     := pam_dock_check.c

# Fedora stores PAM modules in /usr/lib64/security
PAM_DIR := /usr/lib64/security

.PHONY: all install uninstall clean

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)
	@echo "Built: $(TARGET)"

install: $(TARGET)
	@echo "Installing $(TARGET) to $(PAM_DIR)/"
	install -m 755 -o root -g root $(TARGET) $(PAM_DIR)/$(TARGET)
	@echo "Done."

uninstall:
	@echo "Removing $(PAM_DIR)/$(TARGET)"
	rm -f $(PAM_DIR)/$(TARGET)

clean:
	rm -f $(TARGET)
