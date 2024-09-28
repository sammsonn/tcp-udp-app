server:
	gcc -Wall -o server server.c func.c -lm

subscriber:
	gcc -Wall -o subscriber subscriber.c func.c -lm

.PHONY: clean
clean:
	rm -f server subscriber
