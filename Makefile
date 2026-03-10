CC=gcc
CFLAGS=-Wall -Wextra -Werror -O2 -Iinclude

SRC=src/main.c src/recipe.c src/builder.c
OBJ=$(SRC:.c=.o)

flappycook: $(OBJ)
	$(CC) $(OBJ) -o flappycook

clean:
	rm -f src/*.o flappycook
