.PHONY = clean all
CXX = g++ -std=c++11
FLAGS = -Wall -lpthread
TAR=tar
TARFLAGS = -cvf
TARNAME = ex3.tar
FILES_TO_CREATE = Search tar
FILES_TO_CLEAN = *.o  MapReduceFramework.a Search
TARSRCS = MapReduceFramework.cpp Search.cpp Makefile README
FILES_FOR_SEARCH = Search.cpp  MapReduceFramework.h MapReduceClient.h
FILES_FOR_FRAME = MapReduceFramework.cpp 

Search: MapReduceFramework.a Search.o 
	$(CXX) -lpthread Search.o -L. MapReduceFramework.a -o Search

MapReduceFramework.a:  MapReduceFramework.o 
	ar rcs MapReduceFramework.a  MapReduceFramework.o 

MapReduceFramework.o: $(FILES_FOR_FRAME)  
	$(CXX) -c $(FLAGS) MapReduceFramework.cpp

Search.o: $(FILES_FOR_SEARCH)
	$(CXX) -c $(FLAGS) Search.cpp

tar: $(TARSRCS)
	$(TAR) $(TARFLAGS) $(TARNAME) $(TARSRCS)

all: $(TARSRCS)

clean:
	 rm $(FILES_TO_CLEAN)
