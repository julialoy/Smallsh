smallsh: smallsh.c
				gcc -std=c99 -o smallsh smallsh.c

clean:
	rm -rf *.o smallsh
