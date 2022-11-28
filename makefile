all:
	g++ -std=c++11 -O3 -g -Wall -fmessage-length=0 -o threes threes.cpp
	weights_size="65536,65536,65536,65536,65536,65536,65536,65536"
stats: 
	./threes --total=100000 --block=1000 --limit=1000 --slide="init=$weights_size save=weights.bin"
clean:
	rm threes
