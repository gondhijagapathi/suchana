CC = gcc

# Define the D-Bus flags and libraries
SRC_DIR = src
PKGCONFIG = pkg-config
DBUS_CFLAGS = $(shell $(PKGCONFIG) --cflags dbus-1) -I$(SRC_DIR)
DBUS_LIBS = $(shell $(PKGCONFIG) --libs dbus-1)

# Define the source files and output binary
BUILD_DIR = build
SRC = $(wildcard $(SRC_DIR)/*.c)
OBJ = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SRC))
OUT = $(BUILD_DIR)/suchana

# Default target to build the project
all: build $(OUT)

# Rule to build the output binary
$(OUT): $(OBJ)
	$(CC) $(OBJ) -o $(OUT) $(DBUS_LIBS)

# Rule to build object files
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(DBUS_CFLAGS) -c $< -o $@

# Rule to create the build directory
build:
	mkdir -p $(BUILD_DIR)

# Rule to clean up build files
clean:
	rm -f $(BUILD_DIR)/*.o $(OUT)

.PHONY: all clean build
