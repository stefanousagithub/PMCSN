# Analyze commands: Permits to execute different analysis about the system
all: 
	gcc -c -fPIC  ./controller/main.c -o obj1.o
	gcc -c -fPIC  ./controller/simulation.c -o obj2.o
	gcc -c -fPIC  ./util/rngs.c -o obj3.o
	gcc -c -fPIC  ./util/rvgs.c  -o obj4.o
	gcc -c -fPIC  ./util/rvms.c  -o obj5.o
	gcc -c -fPIC  ./util/calqueue.c  -o obj6.o
	ld -r -o demo.o obj1.o obj2.o obj3.o obj4.o obj5.o obj6.o
	gcc demo.o -lm -shared -o demo.so
	rm obj1.o obj2.o obj3.o obj4.o obj5.o obj6.o

run:
	python ./analyze/main.py	

clean:
	rm demo.so demo.o


# Single simulation execution: Runs 400 000 jobs and prints the final statistics
single:
	gcc ./controller/main.c ./controller/simulation.c ./util/rngs.c ./util/rvgs.c ./util/rvms.c ./util/calqueue.c  -lm -o demo

runSingle:
	./demo

cleanSingle:
	rm demo
