# Define the compiler
CC = gcc

# Define the D-Bus flags and libraries
PKGCONFIG = pkg-config
DBUS_CFLAGS = $(shell $(PKGCONFIG) --cflags dbus-1)
DBUS_LIBS = $(shell $(PKGCONFIG) --libs dbus-1)

# Define the source file and output binary
SRC = main.c
OUT = suchana

# Default target to build the project
all: $(OUT)

# Rule to build the output binary
$(OUT): $(SRC)
	$(CC) $(DBUS_CFLAGS) $(SRC) -o $(OUT) $(DBUS_LIBS)

# Rule to clean up build files
clean:
	rm -f $(OUT)
