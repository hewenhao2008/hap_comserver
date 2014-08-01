#CC=mips-openwrt-linux-gcc 
CC=gcc
OUTPUT=hap_comserver

all:  comserverd.c 
	$(CC) -o $(OUTPUT) -Wall -pedantic comserverd.c -lpthread -std=gnu99  -ldl -lrt $(CFLAGS)

.PHONY: clean

clean:
	rm -f $(OUTPUT) 
