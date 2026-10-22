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
