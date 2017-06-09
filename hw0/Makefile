
main: main.o map.o wc.o         
	gcc -g main.o -o main
	gcc -g map.o -o map
	gcc -g wc.o -o wc  
main.o: main.c             
	gcc -c main.c 
map.o: map.c
	gcc -c map.c
wc.o: wc.c
	gcc -c wc.c         
clean:                 
	rm -rf main.o map.o wc.o



