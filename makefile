# Flags de compilation: 
#	-Wall: affichage des warnings
#	-ansi: Assurer le respect du standard ANSI
CFLAGS=-W -Wall -Wextra -pedantic -ansi -lpthread
#-ansi -g
SRC=src/
OBJ=obj/
BIN=bin/
TEST=test/
OPT = -l pthread
HEADER=include/
# Compilateur utilisé
CC=gcc -I$(HEADER) -g

all:
	clean runserver

$(OBJ)list.o: $(SRC)list.c $(HEADER)list.h
	$(CC) $(CFLAGS) -c $(SRC)list.c -o $(OBJ)list.o

server: $(BIN)server

$(BIN)server: $(SRC)server.c $(OBJ)list.o $(OBJ)tools.o $(HEADER)server.h
	$(CC) $(CFLAGS) $(SRC)server.c $(OBJ)tools.o $(OBJ)list.o -o $(BIN)server

runserver: server
	./${BIN}server

client: $(BIN)client

$(BIN)client: $(SRC)client.c $(OBJ)tools.o $(OBJ)locker.o $(OBJ)list.o $(HEADER)client.h
	$(CC) $(CFLAGS) $(OBJ)tools.o $(OBJ)locker.o $(OBJ)list.o $(SRC)client.c -o $(BIN)client

$(OBJ)tools.o: $(SRC)tools.c $(HEADER)tools.h
	$(CC) $(CFLAGS) -c $(SRC)tools.c -o $(OBJ)tools.o

$(OBJ)locker.o: $(SRC)locker.c $(HEADER)locker.h $(HEADER)list.h $(HEADER)tools.h
	$(CC) $(CFLAGS) -c $(SRC)locker.c -o $(OBJ)locker.o

runclient: client
	./${BIN}client

$(OBJ)file.o: $(SRC)file.c $(HEADER)file.h
	$(CC) $(CFLAGS) -c $(SRC)file.c -o $(OBJ)file.o

testFile: $(TEST)testFile.c $(OBJ)file.o $(OBJ)tools.o
	$(CC) $(CFLAGS) $(OBJ)file.o $(OBJ)tools.o $(TEST)testFile.c -o $(BIN)testFile $(OPT)

clean:
	rm -rf bin/* obj/*
