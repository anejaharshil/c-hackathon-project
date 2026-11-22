CC = gcc
CFLAGS = -std=c17 -Wall -Wextra -Wpedantic -I.
TARGET = parking
SOURCES = main.c parking.c storage.c
TEST = tests/test_parking_system

.PHONY: all run test asan test-asan clean

all: $(TARGET)

$(TARGET): $(SOURCES) parking.h
	$(CC) $(CFLAGS) $(SOURCES) -o $@

$(TEST): tests/test_parking_system.c parking.c storage.c parking.h
	$(CC) $(CFLAGS) tests/test_parking_system.c parking.c storage.c -o $@

run: $(TARGET)
	mkdir -p data
	./$(TARGET)

test: $(TEST)
	./$(TEST)

asan:
	$(CC) $(CFLAGS) -g -fsanitize=address,undefined -fno-omit-frame-pointer \
		$(SOURCES) -o parking_asan

test-asan:
	$(CC) $(CFLAGS) -g -fsanitize=address,undefined -fno-omit-frame-pointer \
		tests/test_parking_system.c parking.c storage.c \
		-o tests/test_parking_system_asan
	./tests/test_parking_system_asan

clean:
	rm -f $(TARGET) parking_asan $(TEST) tests/test_parking_system_asan *.o
