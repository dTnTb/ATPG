#include <stdio.h>
#include "prigate.h"

int or(int i, int j){
	return i|j;
}
int xor(int i, int j){
	return (i+j)%2;
}
int nor(int i, int j){
	return !(i|j);
}
int nand(int i, int j){
	return !(i&&j);
}
int and(int i, int j){
	return i&&j;
}
int not(int i){
	return !i;
}


