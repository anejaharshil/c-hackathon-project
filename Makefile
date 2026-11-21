# Smart Campus Parking Management System - Sprint 0 build.
#
# Compiles the skeleton with:
#   gcc -std=c17 -Wall -Wextra -Wpedantic -Iinclude src/*.c -o parking

CC      = gcc
CFLAGS  = -std=c17 -Wall -Wextra -Wpedantic -Iinclude
SRCDIR  = src
SRCS    = $(wildcard $(SRCDIR)/*.c)
TARGET  = parking

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) $^ -o $@

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)
