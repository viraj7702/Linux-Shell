shell: shell.c
	gcc -g -Wall -Wvla -fsanitize=address -o $@ $^
all: shell
clean:
	rm -f shell
