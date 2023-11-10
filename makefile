all: clean janksh

janksh:
	gcc janksh.c -o janksh

clean:
	rm -f janksh