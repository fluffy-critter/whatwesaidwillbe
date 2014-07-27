CXX = g++
CXXFLAGS = -g -Wall -Werror -O3 -std=c++11 $(shell pkg-config --cflags alsa)
LDFLAGS = $(shell pkg-config --libs alsa) -lboost_program_options

all: whatwesaidwillbe

clean:
	rm -f *.o

whatwesaidwillbe: main.o Drum.o Buffer.o
	$(CXX) -o $(@) $(^) $(LDFLAGS)

.cpp.o:
	$(CXX) $(CXXFLAGS) -c $(^)
