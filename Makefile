CXX = g++
CXXFLAGS = -g -Wall -Werror $(shell pkg-config --cflags alsa)
LDFLAGS = $(shell pkg-config --libs alsa) -lboost_program_options

all: whatwesaidwillbe

clean:
	rm -f *.o

whatwesaidwillbe: whatwesaidwillbe.o
	$(CXX) -o $(@) $(^) $(LDFLAGS)

.cpp.o:
	$(CXX) $(CXXFLAGS) -c $(^)
