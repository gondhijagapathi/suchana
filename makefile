CC = gcc

# Define the D-Bus and Wayland flags and libraries
SRC_DIR = src
PROTOCOL_DIR = build/protocols
PKGCONFIG = pkg-config
DBUS_CFLAGS = $(shell $(PKGCONFIG) --cflags dbus-1 wayland-client cairo libsystemd) -I$(SRC_DIR) -I$(PROTOCOL_DIR)
DBUS_LIBS = $(shell $(PKGCONFIG) --libs dbus-1 wayland-client cairo libsystemd)

# Define the source files and output binary
BUILD_DIR = build
SRC = $(wildcard $(SRC_DIR)/*.c) $(wildcard $(PROTOCOL_DIR)/*.c)
OBJ = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SRC))
OUT = $(BUILD_DIR)/suchana

# Commands to generate protocol files
PROTOCOL_GENERATE = \
	wayland-scanner private-code /usr/share/wayland-protocols/stable/xdg-shell/xdg-shell.xml $(PROTOCOL_DIR)/xdg-shell-protocol.c && \
	wayland-scanner private-code /usr/share/wlr-protocols/unstable/wlr-layer-shell-unstable-v1.xml $(PROTOCOL_DIR)/wlr-layer-shell-v1-protocol.c && \
	wayland-scanner client-header /usr/share/wayland-protocols/stable/xdg-shell/xdg-shell.xml $(PROTOCOL_DIR)/xdg-shell-client-protocol.h && \
	wayland-scanner client-header /usr/share/wlr-protocols/unstable/wlr-layer-shell-unstable-v1.xml $(PROTOCOL_DIR)/wlr-layer-shell-v1-protocol.h

# Default target to build the project
all: build generate_protocols $(OUT)

# Rule to build the output binary
$(OUT): $(OBJ)
	$(CC) $(OBJ) -o $(OUT) $(DBUS_LIBS)

# Rule to build object files
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(DBUS_CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: $(PROTOCOL_DIR)/%.c
	$(CC) $(DBUS_CFLAGS) -c $< -o $@

# Rule to create the build directory and generate protocol files
build:
	mkdir -p $(BUILD_DIR) $(PROTOCOL_DIR)

generate_protocols:
	$(PROTOCOL_GENERATE)

# Rule to clean up build files
clean:
	rm -f $(BUILD_DIR)/*.o $(OUT)
	rm -f $(PROTOCOL_DIR)/*.c $(PROTOCOL_DIR)/*.h

.PHONY: all clean build generate_protocols
