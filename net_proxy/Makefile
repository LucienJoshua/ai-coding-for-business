CC = gcc
CFLAGS = -Wall -Wextra -g -O2 -pthread
TARGET = net_proxy
OBJS = net_proxy.o

.PHONY: all clean test

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS)

net_proxy.o: net_proxy.c net_proxy.h
	$(CC) $(CFLAGS) -c net_proxy.c

clean:
	rm -f $(TARGET) $(OBJS)

test: $(TARGET)
	@echo "Running basic tests..."
	@if [ -x ./$(TARGET) ]; then \
		echo "Binary exists and is executable"; \
	else \
		echo "Build failed"; \
		exit 1; \
	fi

run: $(TARGET)
	@echo "Starting net_proxy..."
	@echo "Press Ctrl+C to stop"
	./$(TARGET)

static-check:
	@echo "Running static analysis..."
	gcc -Wall -Wextra -pedantic -std=c11 -fanalyzer -o $(TARGET) net_proxy.c 2>&1 || true
	@echo "Note: fanalyzer may not be available on all systems"