# Makefile 
#*********************************************************
#CC = gcc
#CFLAGS = -g 

#PSRC=Makefile readckt.c 

#POBJ    = readckt.o

#TARGET  = readckt

# Author: zhenyu LI
# group 7
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
	rm -f *.o readckt prigate Group-7.zip
	rm -f fault_collapse.txt fault_original.txt output.txt dal_failed.txt Dal.txt

zip:
	zip Group-7.zip *.c *.h Makefile ReadMe.txt


