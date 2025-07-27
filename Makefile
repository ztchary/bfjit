CC=cc
CFLAGS=-Wall -Werror

bf: bfjit.c
	$(CC) $(CFLAGS) -o bf bfjit.c
