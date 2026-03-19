# Makefile
CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -g -D_POSIX_C_SOURCE=200809L
TARGET = dbms-linux-bin
SRCS = main.c core.c systab.c
OBJS = $(SRCS:.c=.o)
HEADER = prefunc.h structures.h

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $(TARGET)

%.o: %.c $(HEADER)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)
	rm -f *.tbl *.csv

run: $(TARGET)
	./$(TARGET)

.PHONY: all clean run
