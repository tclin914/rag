all: rwg

rwg: rwg.c fifo.o
	gcc -g $< -o $@ fifo.o

fifo.o: fifo.c
	gcc $< -c -o $@

clean: 
	rm rwg
	rm fifo.o
