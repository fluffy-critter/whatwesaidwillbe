.PHONY: build

all: build whatwesaidwillbe

whatwesaidwillbe: build/src/whatwesaidwillbe
	cp $(^) $(@)

build:
	mkdir -p build && \
	cd build && \
	cmake .. && \
	make

clean:
	rm -rf build

