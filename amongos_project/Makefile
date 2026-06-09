CC := gcc
CFLAGS := -Wall -Wextra -O2 -g -std=c11
TARGET := amongos
SRC := src/amongos.c

.PHONY: all clean run demo observe

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)

run: $(TARGET)
	./$(TARGET) --sabotage combo

demo: $(TARGET)
	bash scripts/run_demo.sh

observe:
	bash scripts/observe.sh

clean:
	rm -f $(TARGET) *.o .amongos_*.txt .amongos_*.pid core vgcore.*
