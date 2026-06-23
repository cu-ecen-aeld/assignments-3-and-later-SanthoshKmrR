CC = $(CROSS_COMPILE)gcc
LD = $(CROSS_COMPILE)ld
AR = $(CROSS_COMPILE)ld

$(info cc used : $(CC))

all: main.o

main.o: writer.c
	$(CC) writer.c -o writer
		
.PHONY:	
clean:
	rm -rf writer *.o

