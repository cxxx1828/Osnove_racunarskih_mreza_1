# To "make"       run in terminal: "make client" or "make server"
# To "make clean" run in terminal: "make client_clean" or "make server_clean"

SERVER_C = server.c
CLIENT_C = client.c

client:$(CLIENT_C)
	make -f makefile.client
client_clean:
	make clean -f makefile.client
server:$(SERVER_C)
	make -f makefile.server
server_clean:
	make clean -f makefile.server