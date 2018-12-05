# Makefile 
#*********************************************************
#CC = gcc
#CFLAGS = -g 

#PSRC=Makefile readckt.c 

#POBJ    = readckt.o

#TARGET  = readckt

#*********************************************************


#$(TARGET) : $(POBJ)
	#gcc $(CFLAGS) $(POBJ) -o $(TARGET) -lm

readckt: readckt.o prigate.o
	gcc -o readckt -g readckt.o prigate.o -lm

readckt.o: readckt.c prigate.h type.h
	gcc -g -c readckt.c -lm

prigate.o: prigate.c prigate.h
	gcc -g -c -Wall prigate.c

clean: 
	rm -f *.o readckt

