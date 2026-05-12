# Compiler
CXX 				= g++ -std=c++17
MPICXX              = mpicxx -std=c++20 
CXXFLAGS 			= -O3  -Wall -lpthread
INCLUDES	  		= -I. -I./fastflow 
TARGETS             = ffMS mpiMS

.PHONY: all clean cleanall 

all: $(TARGETS)

ffMS: ffMS.cpp 
	$(CXX) $(INCLUDES) $(CXXFLAGS) -o $@ $< $(LIBS)

mpiMS: mpiMS.cpp 
	$(MPICXX) $(INCLUDES) $(CXXFLAGS) -o $@ $< $(LIBS)

%: %.cpp
	$(CXX) $(INCLUDES) $(CXXFLAGS) -o $@ $< $(LIBS)

clean: 
	-rm -fr *.o *~ *.txt

cleanall: clean
	-rm -fr $(TARGETS) valgrind.log