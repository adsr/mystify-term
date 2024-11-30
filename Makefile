all: mystify-term

mystify-term: mystify-term.c
	$(CC) -Wall -Werror -pedantic -g -Ofast -I. $< -o $@ -lm

clean:
	rm -f mystify-term

.PHONY: all clean
