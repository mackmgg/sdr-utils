CC = gcc

CFLAGS = -Wall -lm -lc
CFLAGSSDR = -lliquid -lhackrf
LIBDIR = -L/opt/homebrew/lib
INCLUDEDIR = -I/opt/homebrew/include

TARGET = hackrf_rssi

all: $(TARGET)

$(TARGET):
		$(CC) $(CFLAGS) $(CFLAGSSDR) $(LIBDIR) $(INCLUDEDIR) -o $(TARGET) $(TARGET).c

clean:
	-rm $(TARGET)
