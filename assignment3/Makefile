CXX		= g++ -std=c++20
INCLUDES	= -I . -I miniz
CXXFLAGS  	+= -Wall 

LDFLAGS 	= -pthread -fopenmp -lz 
OPTFLAGS	= -O3 -ffast-math -DNDEBUG

TARGETS		= minizseq minizpar

.PHONY: all clean cleanall
.SUFFIXES: .cpp 


%: %.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(OPTFLAGS) -o $@ $< ./miniz/miniz.c $(LDFLAGS)

all		: $(TARGETS)

minizseq	: minizseq.cpp cmdline.hpp utility.hpp
minizpar 	: minizpar.cpp cmdline.hpp utility.hpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(OPTFLAGS) -o $@ $< ./miniz/miniz.c $(LDFLAGS)

clean		: 
	rm -f $(TARGETS) 
cleanall	: clean
	\rm -f *.o *.zip *~ output.txt output2.txt



