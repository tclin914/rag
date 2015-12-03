all: ras

ras: ras.c fifo.o
	gcc -g $< -o $@ fifo.o

fifo.o: fifo.c
	gcc $< -c -o $@

clean: 
	rm ../ras
