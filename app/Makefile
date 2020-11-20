TARGET = dirtycow
LDLIBS = -pthread
CFLAGS = -Wall -Wextra -Wpedantic

.PHONY: all run clean debug debug_compile

all: $(TARGET)

run: $(TARGET)
	./$(TARGET)

clean:
	$(RM) $(TARGET)

debug: clean debug_compile

debug_compile: CFLAGS += -Og -g
debug_compile: $(TARGET)

