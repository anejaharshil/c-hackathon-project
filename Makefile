CC = gcc
CFLAGS = -std=c17 -Wall -Wextra -Wpedantic -I.
TARGET = parking
SOURCES = Codes/main.c Codes/parking.c Codes/storage.c Codes/registration.c Codes/map.c
LIB_SOURCES = Codes/parking.c Codes/storage.c Codes/registration.c Codes/map.c
TEST = tests/test_parking_system

.PHONY: all run test asan test-asan clean

all: $(TARGET)

$(TARGET): $(SOURCES)
	$(CC) $(CFLAGS) $(SOURCES) -o $@

$(TEST): tests/test_parking_system.c $(LIB_SOURCES)
	$(CC) $(CFLAGS) tests/test_parking_system.c $(LIB_SOURCES) -o $@

asan:
	$(CC) $(CFLAGS) -g -fsanitize=address,undefined -fno-omit-frame-pointer \
		$(SOURCES) -o parking_asan

test-asan:
	$(CC) $(CFLAGS) -g -fsanitize=address,undefined -fno-omit-frame-pointer \
		tests/test_parking_system.c $(LIB_SOURCES) \
		-o tests/test_parking_system_asan
	./tests/test_parking_system_asan

run: $(TARGET)
	mkdir -p data
	./$(TARGET)

test: $(TEST)
	./$(TEST)

clean:
	rm -f $(TARGET) parking_asan $(TEST) tests/test_parking_system_asan *.o
