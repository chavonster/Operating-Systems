.PHONY = clean all
CXX = g++ -std=c++11
FLAGS = -Wall
TAR = tar
TAR_FLAGS = -cvf
TAR_NAME = ex5.tar
TAR_SRC = Makefile README whatsappClient.cpp whatsappServer.cpp whatsappServer.h whatsappClient.h
FILES_FOR_TAR = README whatsappClient.cpp whatsappServer.cpp Makefile
FILES_TO_CLEAN = whatsappServer whatsappClient ex5.tar

all: whatsappServer whatsappClient

whatsappServer: whatsappServer.o
	$(CXX) $(FLAGS) whatsappServer.o -o whatsappServer

whatsappClient: whatsappClient.o
	$(CXX) $(FLAGS) whatsappClient.o -o whatsappClient

whatsappServer.o: whatsappServer.cpp whatsappServer.h
	$(CXX) -c $(FLAGS) whatsappServer.cpp

whatsappClient.o: whatsappClient.cpp whatsappClient.h
	$(CXX) -c $(FLAGS) whatsappClient.cpp

tar: $(FILES_FOR_TAR)
	$(TAR) $(TAR_FLAGS) $(TAR_NAME) $(TAR_SRC) 

clean:
	rm *.o $(FILES_TO_CLEAN)
