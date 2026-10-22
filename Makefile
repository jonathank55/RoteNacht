CC = clang
CFLAGS = -O2 -Wall -Wextra
FRAMEWORKS = -framework CoreGraphics -framework CoreFoundation -framework IOKit
BIN_DIR = $(HOME)/bin
TARGET = build/rotenacht

all: $(TARGET)

$(TARGET): src/rotenacht.c
	@mkdir -p build
	$(CC) $(CFLAGS) src/rotenacht.c $(FRAMEWORKS) -o $(TARGET)
	@echo "✓ Build erfolgreich: $(TARGET)"

install: $(TARGET)
	@mkdir -p $(BIN_DIR)
	@if [ -f $(HOME)/Library/LaunchAgents/com.rotenacht.daemon.plist ]; then launchctl unload $(HOME)/Library/LaunchAgents/com.rotenacht.daemon.plist 2>/dev/null || true; fi
	@pkill -9 nachtrot 2>/dev/null || true
	@sleep 0.5
	@cp $(TARGET) $(BIN_DIR)/nachtrot
	@chmod 755 $(BIN_DIR)/nachtrot
	@ln -sf $(BIN_DIR)/nachtrot $(BIN_DIR)/rotenacht
	@echo "✓ Binaries installiert in $(BIN_DIR)/nachtrot und $(BIN_DIR)/rotenacht"
	@$(BIN_DIR)/nachtrot install

uninstall:
	@if [ -x $(BIN_DIR)/nachtrot ]; then $(BIN_DIR)/nachtrot uninstall; fi
	@rm -f $(BIN_DIR)/nachtrot $(BIN_DIR)/rotenacht
	@echo "✓ RoteNacht vollständig deinstalliert."

clean:
	@rm -rf build
	@echo "✓ Bereinigt."

.PHONY: all install uninstall clean
