ALL:
	g++ ashell.cpp -o ashell.out
	
clean:
	rm -rf ashell.out

r:
	./ashell.out

d:
	gdb ashell.out
