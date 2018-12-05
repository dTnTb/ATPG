/*=======================================================================
  A simple parser for "self" format

  The circuit format (called "self" format) is based on outputs of
  a ISCAS 85 format translator written by Dr. Sandeep Gupta.
  The format uses only integers to represent circuit information.
  The format is as follows:

1        2        3        4           5           6 ...
------   -------  -------  ---------   --------    --------
0 GATE   outline  0 IPT    #_of_fout   #_of_fin    inlines
                  1 BRCH
                  2 XOR(currently not implemented)
                  3 OR
                  4 NOR
                  5 NOT
                  6 NAND
                  7 AND

1 PI     outline  0        #_of_fout   0

2 FB     outline  1 BRCH   inline

3 PO     outline  2 - 7    0           #_of_fin    inlines




                                    Author: Chihang Chen
                                    Date: 9/16/94

=======================================================================*/

/*=======================================================================
  - Write your program as a subroutine under main().
    The following is an example to add another command 'lev' under main()

enum e_com {READ, PC, HELP, QUIT, LEV};
#define NUMFUNCS 5
int cread(), pc(), quit(), lev();
struct cmdstruc command[NUMFUNCS] = {
   {"READ", cread, EXEC},
   {"PC", pc, CKTLD},
   {"HELP", help, EXEC},
   {"QUIT", quit, EXEC},
   {"LEV", lev, CKTLD},
};

lev()
{
   ...
}
=======================================================================*/

#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <math.h>
#include "type.h"
#include "prigate.h"

#define MAXLINE 81               /* Input buffer size */
#define MAXNAME 31               /* File name size */

#define Upcase(x) ((isalpha(x) && islower(x))? toupper(x) : (x))
#define Lowcase(x) ((isalpha(x) && isupper(x))? tolower(x) : (x))

enum e_com {READ, PC, HELP, QUIT, LEV , LOGIC};
enum e_state {EXEC, CKTLD};         /* Gstate values */
enum e_ntype {GATE, PI, FB, PO};    /* column 1 of circuit format */
enum e_gtype {IPT, BRCH, XOR, OR, NOR, NOT, NAND, AND};  /* gate types */

struct cmdstruc {
   char name[MAXNAME];        /* command syntax */
   int (*fptr)();             /* function pointer of the commands */
   enum e_state state;        /* execution state sequence */
};

typedef struct n_struc {
   unsigned indx;             /* node index(from 0 to NumOfLine - 1 */
   unsigned num;              /* line number(May be different from indx */
   enum e_gtype type;         /* gate type */
   unsigned fin;              /* number of fanins */
   unsigned fout;             /* number of fanouts */
   struct n_struc **unodes;   /* pointer to array of up nodes */
   struct n_struc **dnodes;   /* pointer to array of down nodes */
   int level;                 /* LI:level of the gate output */
   int val;				  /* LI: vaule of line -1 is unkown */
} NSTRUC;                     

/*----------------- new function        ----------------------------------*/
int calval(int type,int i, int j);
int getlev(NSTRUC *np);
void initFArr();
void setNodelev();


/*----------------- Command definitions ----------------------------------*/
#define NUMFUNCS 6
int cread(), pc(), help(), quit(), lev(), logic();
struct cmdstruc command[NUMFUNCS] = {
   {"READ", cread, EXEC},
   {"PC", pc, CKTLD},
   {"HELP", help, EXEC},
   {"QUIT", quit, EXEC},
   {"LEV", lev, CKTLD},
   {"LOGIC",logic,CKTLD},
};

/*------------------------------------------------------------------------*/
enum e_state Gstate = EXEC;     /* global exectution sequence */
NSTRUC *Node;                   /* dynamic array of nodes */
NSTRUC **Pinput;                /* pointer to array of primary inputs */
NSTRUC **Poutput;               /* pointer to array of primary outputs */
int Nnodes;                     /* number of nodes */
int Npi;                        /* number of primary inputs */
int Npo;                        /* number of primary outputs */
int Done = 0;                   /* status bit to terminate program */



/*------------------------LI:new variable ----------------------...----------*/
int Nbr; 						/* numer of branch */
int lev_max = 0;                /* max level in circuit */
int *input;                     /* input */
NSTRUC **Nodelev;               /* pointer to array of gates sorted by level */
NSTRUC **Pbrput;				/* pointer to array of branch*/
struct fList *Fchead;	/*collasped list*/
struct fault *FArr; /*original Farr*/
struct fault **Fcp;

/*------------------------------------------------------------------------*/

/*-----------------------------------------------------------------------
input: nothing
output: nothing
called by: shell
description:
  This is the main program of the simulator. It displays the prompt, reads
  and parses the user command, and calls the corresponding routines.
  Commands not reconized by the parser are passed along to the shell.
  The command is executed according to some pre-determined sequence.
  For example, we have to read in the circuit description file before any
  action commands.  The code uses "Gstate" to check the execution
  sequence.
  Pointers to functions are used to make function calls which makes the
  code short and clean.
-----------------------------------------------------------------------*/
main()
{
   enum e_com com;
   char cline[MAXLINE], wstr[MAXLINE], *cp;

   while(!Done) {
      printf("\nCommand>");
      fgets(cline, MAXLINE, stdin);
      if(sscanf(cline, "%s", wstr) != 1) continue;
      cp = wstr;
      while(*cp){
	*cp= Upcase(*cp);
	cp++;
      }
      cp = cline + strlen(wstr);
      com = READ;
      while(com < NUMFUNCS && strcmp(wstr, command[com].name)) com++;
      if(com < NUMFUNCS) {
         if(command[com].state <= Gstate) (*command[com].fptr)(cp);
         else printf("Execution out of sequence!\n");
      }
      else system(cline);
   }
}

/*-----------------------------------------------------------------------
input: circuit description file name
output: nothing
called by: main
description:
  This routine reads in the circuit description file and set up all the
  required data structure. It first checks if the file exists, then it
  sets up a mapping table, determines the number of nodes, PI's and PO's,
  allocates dynamic data arrays, and fills in the structural information
  of the circuit. In the ISCAS circuit description format, only upstream
  nodes are specified. Downstream nodes are implied. However, to facilitate
  forward implication, they are also built up in the data structure.
  To have the maximal flexibility, three passes through the circuit file
  are required: the first pass to determine the size of the mapping table
  , the second to fill in the mapping table, and the third to actually
  set up the circuit information. These procedures may be simplified in
  the future.
-----------------------------------------------------------------------*/
cread(cp)
char *cp;
{
   char buf[MAXLINE];
   int ntbl, *tbl, i, j, k, nd, tp, fo, fi, ni = 0, no = 0;
   int nb = 0;
   FILE *fd;
   NSTRUC *np;

   sscanf(cp, "%s", buf);
   if((fd = fopen(buf,"r")) == NULL) {
      printf("File %s does not exist!\n", buf);
      return;
   }
   if(Gstate >= CKTLD) clear();
   Nnodes = Npi = Npo = ntbl = 0;
   Nbr = 0; /* Nbr reset */
   while(fgets(buf, MAXLINE, fd) != NULL) {
      if(sscanf(buf,"%d %d", &tp, &nd) == 2) {
         if(ntbl < nd) ntbl = nd;
         Nnodes ++;
         if(tp == PI) Npi++;
         else if(tp == PO) Npo++;
		 else if(tp == FB) Nbr++;  /*Li:branch array count*/
      }
   }
   tbl = (int *) malloc(++ntbl * sizeof(int));

   fseek(fd, 0L, 0);
   i = 0;
   while(fgets(buf, MAXLINE, fd) != NULL) {
      if(sscanf(buf,"%d %d", &tp, &nd) == 2) tbl[nd] = i++;
   }
   allocate();

   fseek(fd, 0L, 0);
   while(fscanf(fd, "%d %d", &tp, &nd) != EOF) {
      np = &Node[tbl[nd]];
      np->num = nd;
	  np->level = -1; //LI
      np->val = 0; //LI
      if(tp == PI) Pinput[ni++] = np;
      else if(tp == PO) Poutput[no++] = np;
	  else if(tp == FB) Pbrput[nb++] = np; /*Li: add branh*/

      switch(tp) {
         case PI:
         case PO:
         case GATE:
            fscanf(fd, "%d %d %d", &np->type, &np->fout, &np->fin);
            break;
         
         case FB:
            np->fout = np->fin = 1;
            fscanf(fd, "%d", &np->type);
            break;

         default:
            printf("Unknown node type!\n");
            exit(-1);
         }
      np->unodes = (NSTRUC **) malloc(np->fin * sizeof(NSTRUC *));
      np->dnodes = (NSTRUC **) malloc(np->fout * sizeof(NSTRUC *));
      for(i = 0; i < np->fin; i++) {
         fscanf(fd, "%d", &nd);
         np->unodes[i] = &Node[tbl[nd]];
         }
      for(i = 0; i < np->fout; np->dnodes[i++] = NULL);
      }
   	
   for(i = 0; i < Nnodes; i++) {
      for(j = 0; j < Node[i].fin; j++) {
         np = Node[i].unodes[j];
         k = 0;
         while(np->dnodes[k] != NULL) k++;
         np->dnodes[k] = &Node[i];
         }
      }

   input = (int *) malloc(ni * sizeof(int)); /* LI */
   for(i = 0;i<Npi;i++) input[i] = 0; /* LI : inaite the input */
   initFArr(); /* L:get original fault list */
   fclose(fd);
   Gstate = CKTLD;
   printf("==> OK\n");
}

/*-----------------------------------------------------------------------
input: nothing
output: nothing
called by: main
description:
  The routine prints out the circuit description from previous READ command.
-----------------------------------------------------------------------*/
pc(cp)
char *cp;
{
   int i, j;
   NSTRUC *np;
   char *gname();
   
   printf(" Node   Type \tIn     \t\t\tOut    \tLevel\n");
   printf("------ ------\t-------\t\t\t-------\t-----\n");
   for(i = 0; i<Nnodes; i++) {
      np = &Node[i];
      printf("\t\t\t\t\t");
      for(j = 0; j<np->fout; j++) printf("%d ",np->dnodes[j]->num);
	  printf("\t%5d",np->level);
      printf("\r%5d  %s\t", np->num, gname(np->type));
      for(j = 0; j<np->fin; j++) printf("%d ",np->unodes[j]->num);
      printf("\n");
   }
   printf("Primary inputs:  ");
   for(i = 0; i<Npi; i++) printf("%d ",Pinput[i]->num);
   printf("\n");
   printf("Primary outputs: ");
   for(i = 0; i<Npo; i++) printf("%d ",Poutput[i]->num);
   printf("\n\n");
   printf("Number of nodes = %d\n", Nnodes);
   printf("Number of primary inputs = %d\n", Npi);
   printf("Number of primary outputs = %d\n", Npo);
   //printf("Nbr = %d\n",Nbr);
   /*for(i=0;i<2*(Npi+Nbr);i++){
		printf("line = %d ; type = %d; lev = %d\n",Fcp[i]->fnum,Fcp[i]->fval, Fcp[i]->Np->level);
	}//show orginal fault list
	printf("colla\n");
	struct fList * br = Fchead->next;
	while(br){
		printf("line = %d ; type = %d; lev = %d\n", br->fp->fnum,br->fp->fval, br->fp->Np->level);
		br = br->next;	
	}*/
}

/*-----------------------------------------------------------------------
input: nothing
output: nothing
called by: main 
description:
  The routine prints ot help inormation for each command.
-----------------------------------------------------------------------*/
help()
{
   printf("READ filename - ");
   printf("read in circuit file and creat all data structures\n");
   printf("PC - ");
   printf("print circuit information\n");
   printf("HELP - ");
   printf("print this help information\n");
   printf("LEV - ");
   printf("levelize the circuit\n");
   printf("QUIT - ");
   printf("stop and exit\n");
}

/*-----------------------------------------------------------------------
input: nothing
output: nothing
called by: main 
description:
  Set Done to 1 which will terminates the program.
-----------------------------------------------------------------------*/
quit()
{
   Done = 1;
}

/*======================================================================*/

/*-----------------------------------------------------------------------
input: nothing
output: nothing
called by: cread
description:
  This routine clears the memory space occupied by the previous circuit
  before reading in new one. It frees up the dynamic arrays Node.unodes,
  Node.dnodes, Node.flist, Node, Pinput, Poutput, and Tap.

-----------------------------------------------------------------------*/

NSTRUC **Nodelev;               /* pointer to array of gates sorted by level */
NSTRUC **Pbrput;				/* pointer to array of branch*/
struct fList *Fchead;	/*collasped list*/
struct fault *FArr; /*original Farr*/
struct fault **Fcp;

clear()
{
   int i;

   for(i = 0; i<Nnodes; i++) {
      free(Node[i].unodes);
      free(Node[i].dnodes);
   }
   free(Node);
   free(Pinput);
   free(Poutput);
   /* Li  free memory*/
   free(Pbrput);
   free(Fcp);
   free(Nodelev);
   free(FArr);
   free(Fchead);
   /* Li  end*/
   Gstate = EXEC;
}

/*-----------------------------------------------------------------------
input: nothing
output: nothing
called by: cread
description:
  This routine allocatess the memory space required by the circuit
  description data structure. It allocates the dynamic arrays Node,
  Node.flist, Node, Pinput, Poutput, and Tap. It also set the default
  tap selection and the fanin and fanout to 0.
-----------------------------------------------------------------------*/
allocate()
{
   int i;

   Node = (NSTRUC *) malloc(Nnodes * sizeof(NSTRUC));\
   Fchead = (struct fList*) malloc(sizeof(struct fList));
   FArr = (struct fault *) malloc(2 * Nnodes * sizeof(struct fault)); /*LI: fault */
   Fcp = (struct fault **) malloc(2 * (Nbr + Npi) * sizeof(struct fault *)); 
   Pbrput = (NSTRUC **) malloc(Nbr * sizeof(NSTRUC *));
   Pinput = (NSTRUC **) malloc(Npi * sizeof(NSTRUC *));
   Poutput = (NSTRUC **) malloc(Npo * sizeof(NSTRUC *));
   for(i = 0; i<Nnodes; i++) {
      Node[i].indx = i;
      Node[i].fin = Node[i].fout = 0;
   }
}

/*-----------------------------------------------------------------------
input: gate type
output: string of the gate type
called by: pc
description:
  The routine receive an integer gate type and return the gate type in
  character string.
-----------------------------------------------------------------------*/
char *gname(tp)
int tp;
{
   switch(tp) {
      case 0: return("PI");
      case 1: return("BRANCH");
      case 2: return("XOR");
      case 3: return("OR");
      case 4: return("NOR");
      case 5: return("NOT");
      case 6: return("NAND");
      case 7: return("AND");
   }
}
/*-----------------------------------------------------------------------
input: None
output: level
called by: user
description:functions achieve leveliziation 
author: Li
-----------------------------------------------------------------------*/

int getlev(NSTRUC *np){
	if(np->type == 0){
		np->level = 0;
		return 0;
	}
	if(np->level != -1)
		return np->level;
	int lev[np->fin];
	int max;
	int i;
	for(i = 0;i<np->fin;i++){
		lev[i]=getlev(np->unodes[i]);
	}
	max = lev[0];
	for(i = 0;i<np->fin;i++){
		if(lev[i]>max)
			max = lev[i];
	}
	np->level = max + 1;
	return max+1;
}



lev() /* set the gate level and Nodelev() */
{
	int i;
	for(i = 0; i<Npo; i++){
    	getlev(Poutput[i]);
		if(lev_max<Poutput[i]->level)
			lev_max = Poutput[i]->level;
   	} 
   setNodelev();
}

/*-----------------------------------------------------------------------
input: None
output: level
called by: user
description: logic simulation logic()
author: Li
-----------------------------------------------------------------------*/


void setinput(int i){
	int j,s = 0;
	for(j = 0;j<Npi;j++){
		input[j] = i%2;
		i = i/2;
	}
}


void readinput(){
	int i;
	for(i = 0;i<Npi;i++){
		Pinput[i]->val = input[i];
	}
}

/* get a array ordered by the gate lev Time complex N*N BAD*/
void setNodelev(){
	Nodelev = (NSTRUC **) malloc(Nnodes * sizeof(NSTRUC *));
	int n = 0,i,j;
	for(i = 0;i<=lev_max;i++){
		for(j =0;j<Nnodes;j++){
			if(Node[j].level == i){
				Nodelev[n]= &Node[j];
				n++;
			}
		}
	}
}

/*calval(Nodelev[i]->type,Nodelev[i]->unodes[0]->val,Nodelev[i]->unodes[1]->val); */
int calval(int type,int i, int j){
	switch(type){
		case 2: return xor(i,j);
		case 3: return or(i,j);
		case 4: return nor(i,j);
		case 5: return not(i);
		case 6: return nand(i,j);
		case 7: return and(i,j); 
	}
}

int getval(NSTRUC* n,int lo, int high){
	if((high-lo+1)== 1){
		return n->unodes[lo]->val;
	}else{
		int mid =lo+(high-lo-1)/2;
		return calval(n->type,getval(n,lo,mid),getval(n,mid+1,high));	
	}
}

void levsim(){
	int i,j;
	for(i = Npi; i<Nnodes;i++){
		if(Nodelev[i]->type !=1){
			Nodelev[i]->val = getval(Nodelev[i],0,Nodelev[i]->fin-1); /*get val according the input number*/
		}
		else Nodelev[i]->val = Nodelev[i]->unodes[0]->val; /* branch, just get upnode val */
	}
}


logic(){
	FILE *fp = fopen("output.txt","w");
	int i,j;
	fputs("Primary Inputs: ",fp);
	fputs("->>>>>>>>>>>\t\t\t\t\tPrimary outputs:\n",fp);	
   	for(j = 0;j<pow(2,Npi)&&j<1000;j++){		
		setinput(j);
		readinput();
		for(i = Npi-1; i>=0; i--) fputc(Pinput[i]->val+'0',fp);
		levsim();
		fputs("\t\t\t\t\t\t\t",fp);	
   		for(i = 0; i<Npo; i++) fputc(Poutput[i]->val+'0',fp);
		fputs("\n",fp);
	}
   	printf("=>logic simualtion done, check output.txt file");	
	fclose(fp);	
}
/*-----------------------------------------------------------------------
input: None
output:
called by: user
description: fault collaspe
author: Li
-----------------------------------------------------------------------*/
/*addFList(struct fList *head, struct fault *new){
		
}*/

/* 0 is no dom eq, 1 have dom and eq, need delete */
int checkeq(struct n_struc* ne,int f1,struct n_struc* nc,int f2){
		int type;
		if(ne->dnodes[0]->num != nc->dnodes[0]->num)
			return 0;
		else 
			type = ne->dnodes[0]->type;
		switch(type){
		case 2: return 0;
		case 3: if(f1 == 1 && f2 == 1) return 1;
				else return 1; 
		case 4: if(f1 == 1 && f2 == 1) return 1;
				else return 0;
		case 5: return 0;
		case 6: if(f1 == 0 && f2 == 0) return 1;
				else return 0;
		case 7: if(f1 == 0 && f2 == 0) return 1;
				else return 0; 
	}
}

int checkeqd(struct n_struc* ne,int f1,struct n_struc* nc,int f2){
		int type;
		if(ne->dnodes[0]->num != nc->dnodes[0]->num)
			return 0;
		else 
			type = ne->dnodes[0]->type;
		switch(type){
		case 2: return 0;
		case 3: if(f1 == 1 && f2 == 1) return 1;
				else return 1; 
		case 4: if(f1 == 1 && f2 == 0) return 1;
				else return 0;
		case 5: return 0;
		case 6: if(f1 == 0 && f2 == 1) return 1;
				else return 0;
		case 7: if(f1 == 0 && f2 == 0) return 1;
				else return 0; 
	}
}



int checkdom(struct n_struc* ne,int f1,struct n_struc* nc,int f2){
		int type = nc->type;
		switch(type){
		case 2: return 0;
		case 3: if(f1 == 1 && f2 == 1) return 1;
				else return 0; 
		case 4: if(f1 == 1 && f2 == 0) return 1;
				else return 0;
		case 5: return 0;
		case 6: if(f1 ==0 && f2== 1) return 1;
				else return 0;
		case 7: if(f1 ==0 && f2 == 0) return 1;
				else return 0; 
		}
}

int check(struct n_struc *nc, int fval){
    NSTRUC *nd = nc->dnodes[0];
	NSTRUC *ne;
	if(nd->fin == 1) return 0;
	if(nd->unodes[0] == nc) ne = nd->unodes[1];
	else ne = nd->unodes[0];
	int i;
	NSTRUC *ni;
	if(checkeq(ne,0,nc,fval)){
		/*printf("%d  val = 0\n",ne->num);*/
		if(ne->type == 0)	return 1;
		else 
			for(i = 0; i<ne->fin;i++){
				ni = ne->unodes[i];
				if(ni->type == 0)	return 1;
				if(checkeqd(ni,0,ne,0))	 
					if(check(ni, 0))
						return 1;
				if(checkeqd(ni,1,ne,0))	 
					if(check(ni, 1))
						return 1;
			}				
	}else if(checkeq(ne,1,nc,fval)){
		printf("%d  val = 1\n",ne->num);
		if(ne->type == 0)	return 1;
		else 
			for(i = 0; i<ne->fin;i++){
				ni = ne->unodes[i];
				if(ni->type == 0)	return 1;
				if(checkeqd(ni,0,ne,1))	 
					if(check(ni, 0))
						return 1;
				if(checkeqd(ni,1,ne,1))	 
					if(check(ni, 1))
						return 1;
			}	
	}
	if(nc->type >1 && nc->type < 8 && nc->type != 5){
		int i;
		NSTRUC *ni;
		for(i = 0; i< nc->fin;i++){
			ni = nc->unodes[i];
			if(checkdom(ni,0,nc,fval))
				if(check(ni,0)) return 1;
			else if(checkdom(ni,1,nc,fval))
				if(check(ni,1)) return 1;
		}
	}		
	return 0;
}

void initFArr(){
	lev();
	int i,j=0;
	int nc = 0;
	FILE *fp = fopen("fault_original.txt","w");
	for(i=0;i<2*Nnodes;i++){
		FArr[i].fval = j;
		FArr[i].Np = Nodelev[i/2]; /* use nodelev, the Farr will be sorted by level */
		FArr[i].fnum = (FArr[i].Np)->num;
		j = (j+1)%2;
		fprintf(fp,"Line: %d, Fault: %d \n",FArr[i].fnum,FArr[i].fval,fp);
		if(FArr[i].Np->type == 1 || FArr[i].Np->type == 0) Fcp[nc++]=&FArr[i];
	}
	fclose(fp);
    struct fList *br = Fchead;
    struct fList *new;
    /* store the fcp large to small for gate level */
	for(i = 2*(Npi+Nbr)-1;i>=0;i--){
		new = (struct fList*)malloc(sizeof(struct fList));
		new->fp = Fcp[i];
		new->next = NULL;
		br->next = new;
		br = br->next; 	
	}
	struct fList *pre = Fchead;
	struct fList *brr;
	br = Fchead->next;
	while(br){
		//printf("%d %d\n",br->fp->Np->num,br->fp->fval);
		int flag = 0;
		if(br->fp->Np->type != 0)
			if(check(br->fp->Np,br->fp->fval))
				flag = 1;
		if(flag == 0){
			brr = br->next;
			while(brr){
				if(checkeq(brr->fp->Np,0,br->fp->Np,br->fp->fval))
					flag = 1;
				else if(checkeq(brr->fp->Np,1,br->fp->Np,br->fp->fval))
					flag = 1;
				brr = brr->next;			
			}
		}	
		if(flag == 1)			
				pre->next = br->next;
			else 
				pre = pre->next;	
		br = br->next;
	}
	br = Fchead->next;
	fp = fopen("fault_collapse.txt","w");
	while(br){
		fprintf(fp,"Line: %d, Fault: %d \n",br->fp->fnum,br->fp->fval);
		br = br->next;	
	}
	fclose(fp);
	
}







/*========================= End of program ============================*/

