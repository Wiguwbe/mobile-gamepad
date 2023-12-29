
CFLAGS := -std=gnu99 $(shell pkg-config --cflags libevdev)
LIBS := $(shell pkg-config --libs libevdev)

server: server.o mongoose.o
	gcc -o $@ $^ $(LIBS)

server.o: server.c
	gcc -c $< $(CFLAGS)

mongoose.o: mongoose.c
	gcc -c $< $(CFLAGS)
