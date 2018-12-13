/***********************
Author: zhenyu LI
Group 7
************************/

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

unsigned por(unsigned i, unsigned j){
	return i|j;
}
unsigned pxor(unsigned i, unsigned j){
	return i^j;
}
unsigned pnor(unsigned i, unsigned j){
	return ~(i|j);
}
unsigned pnand(unsigned i, unsigned j){
	return ~(i&j);
}
unsigned pand(unsigned i, unsigned j){
	return i&j;
}
unsigned pnot(unsigned i){
	return ~i;
}
