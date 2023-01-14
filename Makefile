CC = g++
CFLAGS = -g `pkg-config --libs --cflags fuse` -pthread --std=c++17
LDFLAGS = $(shell pkg-config --libs fuse)

procfs: main.cpp
	$(CC) main.cpp -o procfs  $(CFLAGS) $(LDFLAGS) 

clean:
	rm -rf procfs foldertest.txt output.txt
