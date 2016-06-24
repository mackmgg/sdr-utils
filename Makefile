CC = gcc

CFLAGS = -Wall -lm -lc
CFLAGSSDR = -lliquid -lhackrf

TARGET = hackrf_rssi

all: $(TARGET)

$(TARGET):
		$(CC) $(CFLAGS) $(CFLAGSSDR) -o $(TARGET) $(TARGET).c

clean:
	-rm $(TARGET)
