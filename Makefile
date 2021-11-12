#Akhil Vinta

default: client.c server.c
	gcc -Wall -Wextra client.c -lz -o client
	gcc -Wall -Wextra server.c -lz -o server

clean:
	rm -f client server

dist: default
	tar zcfv serverToClient.tar.gz client.c server.c README Makefile