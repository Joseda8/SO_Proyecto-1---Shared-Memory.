all: build

build:
	gcc initializer.c -o init -pthread
	gcc finisher.c -o finish -pthread
	gcc writer.c -o writer -pthread -lm
	gcc reader.c -o reader -pthread -lm
