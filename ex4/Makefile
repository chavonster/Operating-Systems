.PHONY = clean all
CXX = g++ -std=c++11
FLAGS = -Wall
TAR = tar
TAR_FLAGS = -cvf
TAR_NAME = ex4.tar
TAR_SRC = Makefile README Answers.pdf CacheFS.cpp Cache.cpp CacheBlock.cpp Cache.h CacheBlock.h
FILES_FOR_TAR = Makefile README Answers.pdf CacheFS.cpp Cache.cpp CacheBlock.cpp
FILES_TO_CLEAN = CacheFS.a CacheFS.o Cache.o CacheBlock.o

all: CacheFS.a

CacheFS.a: CacheFS.cpp Cache.cpp CacheBlock.cpp 
	$(CXX) -c $(FLAGS) CacheFS.cpp -o CacheFS.o
	$(CXX) -c $(FLAGS) Cache.cpp -o Cache.o
	$(CXX) -c $(FLAGS) CacheBlock.cpp -o CacheBlock.o
	ar rcs CacheFS.a CacheFS.o Cache.o CacheBlock.o 


tar: $(FILES_FOR_TAR)
	$(TAR) $(TAR_FLAGS) $(TAR_NAME) $(TAR_SRC) 


clean:
	rm $(FILES_TO_CLEAN)
