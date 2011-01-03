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

$(BIN)server: $(SRC)server.c $(OBJ)list.o $(OBJ)tools.o $(OBJ)file.o $(HEADER)server.h
	$(CC) $(CFLAGS) $(SRC)server.c $(OBJ)tools.o $(OBJ)list.o $(OBJ)file.o -o $(BIN)server

runserver: server
	./${BIN}server 10000 files.txt 10001 rep1

client: $(BIN)client

$(BIN)client: $(SRC)client.c $(OBJ)tools.o $(OBJ)file.o $(OBJ)locker.o $(OBJ)list.o $(HEADER)client.h
	$(CC) $(CFLAGS) $(OBJ)tools.o $(OBJ)file.o $(OBJ)locker.o $(OBJ)list.o $(SRC)client.c -o $(BIN)client

$(OBJ)tools.o: $(SRC)tools.c $(HEADER)tools.h
	$(CC) $(CFLAGS) -c $(SRC)tools.c -o $(OBJ)tools.o

$(OBJ)locker.o: $(SRC)locker.c $(HEADER)locker.h $(HEADER)list.h $(HEADER)tools.h
	$(CC) $(CFLAGS) -c $(SRC)locker.c -o $(OBJ)locker.o $(OPT)

runclient1: client
	./${BIN}client rep2 10000 "localhost" 20000

runclient2: client
	./${BIN}client rep3 10000 "localhost" 20001

$(OBJ)file.o: $(SRC)file.c $(HEADER)file.h
	$(CC) $(CFLAGS) -c $(SRC)file.c -o $(OBJ)file.o

# test

$(OBJ)testTools.o : $(SRC)testTools.c $(HEADER)testTools.h
	$(CC) $(CFLAGS) -c $(SRC)testTools.c -o $(OBJ)testTools.o

testFile: $(BIN)testFile

$(BIN)testFile: $(TEST)testFile.c $(OBJ)file.o $(OBJ)tools.o
	$(CC) $(CFLAGS) $(OBJ)file.o $(OBJ)tools.o $(TEST)testFile.c -o $(BIN)testFile

testLocker: $(BIN)testLocker

$(BIN)testLocker: $(TEST)testLocker.c $(OBJ)tools.o $(OBJ)file.o $(OBJ)list.o $(OBJ)testTools.o $(OBJ)locker.o
	$(CC) $(CFLAGS) $(OBJ)list.o $(OBJ)tools.o $(OBJ)file.o $(OBJ)testTools.o $(OBJ)locker.o $(TEST)testLocker.c -o $(BIN)testLocker

clean:
	rm -rf bin/* obj/* rep1/* rep2/* rep3/*
	touch rep2/1.txt rep2/2.txt rep2/3.txt
	touch rep3/1.txt rep3/2.txt rep3/3.txt 
