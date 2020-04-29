main: main.c
	gcc main.c -o main

clean:
	rm -rf main

run:
	sudo ./main
