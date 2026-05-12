# Compiler
CXX 				= g++ -std=c++17
CXXFLAGS 			= -O3  -Wall -Wextra
INCLUDES	  		= -I. -I./include

SOURCES            	= $(wildcard *.cpp)
TARGET             	= $(SOURCES:.cpp=)


.PHONY: all clean cleanall 


all: $(TARGETS)

%: %.cpp
	$(CXX) $(INCLUDES) $(CXXFLAGS) -o $@ $< $(LIBS)


clean: 
	-rm -fr *.o *~ *.txt

cleanall: clean
	-rm -fr $(TARGET)