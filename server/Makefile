
CC := $(CROSS_COMPILE)gcc
CFLAGS := -Wall -Werror -pthread
LDFLAGS := -pthread

all: aesdsocket
build: aesdsocket
default: aesdsocket

aesdsocket: aesdsocket.c
	$(CC) $(CFLAGS) aesdsocket.c -o aesdsocket

clean:
	rm -f aesdsocket 
