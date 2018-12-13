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

enum e_com {READ, PC, HELP, QUIT, LEV, LOGIC, DFS ,PFS,DAL,PODEM};
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
   int val;				  /* Li: vaule of line -1 is unkown */
   struct fList *head;    /* li: for DFS */ 
   unsigned sa1;
   unsigned sa0;
   int pval;

} NSTRUC;                     

/*----------------- new function        ----------------------------------*/
int calval(int type,int i, int j);
int getlev(NSTRUC *np);
void initFArr();
void setNodelev();
void setinput(); /* load input into line node */
void levsim();


#define NUMFUNCS 10
int cread(), pc(), help(), quit(), lev(), logic(), DFS_client(),PFS_client(),D_client(),podemS();
struct cmdstruc command[NUMFUNCS] = {
   {"READ", cread, EXEC},
   {"PC", pc, CKTLD},
   {"HELP", help, EXEC},
   {"QUIT", quit, EXEC},
   {"LEV", lev, CKTLD},
   {"LOGIC",logic,CKTLD},
   {"DFS",DFS_client,CKTLD},
   {"PFS",PFS_client,CKTLD},
   {"DAL",D_client,CKTLD},
   {"PODEM",podemS,CKTLD},
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
//NSTRUC **Pbrput;				/* pointer to array of branch*/
struct fList *Fchead;	/*collasped list*/
struct fault *FArr; /*original Farr*/
struct fault **Fcp;

int snum = 0;
struct ipList *siphead = NULL;
int fnum = 0;
struct ipList *fiphead = NULL;

int psnum = 0;
int pfnum = 0;
int dfnum = 0;
int dsnum = 0;

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
      /* Li init */
	  np->level = -1; 
      np->val = 0;
	  np->pval = 0;
      np->head = (struct fList *)malloc(sizeof(struct fList));
	  np->head->fp = NULL; 
	  np->head->next = NULL;
      /* Li init */
      if(tp == PI) Pinput[ni++] = np;
      else if(tp == PO) Poutput[no++] = np;
	  //else if(tp == FB) Pbrput[nb++] = np; /*Li: add branh*/

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
   /* L the folloing code print the collapse fault list */
   /*
    printf("Nbr = %d\n",Nbr);
    for(i=0;i<2*(Npi+Nbr);i++){
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
clear()
{
   int i;

   for(i = 0; i<Nnodes; i++) {
      free(Node[i].unodes);
      free(Node[i].dnodes);
	  free(Node[i].head);
   }
   free(Node);
   free(Pinput);
   free(Poutput);
   /* Li  free memory*/
   //free(Pbrput);
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
   //Pbrput = (NSTRUC **) malloc(Nbr * sizeof(NSTRUC *));
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

/* give a dec, change to binary and save into input array */
void DectobinInput(int i){
	int j,s = 0;
	for(j = 0;j<Npi;j++){
		input[j] = i%2;
		i = i/2;
	}
}

/* read input array into nodes value */
void setinput(){
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
	if((high-lo+1)== 1){  // only 1 input for primary gate or this node is primary input
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
		DectobinInput(j);
		setinput();
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
		//printf("%d  val = 1\n",ne->num);
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
    /* get orignal fault list , write into file */
	for(i=0;i<2*Nnodes;i++){
		FArr[i].fval = j;
		FArr[i].Np = Nodelev[i/2]; /* use nodelev, the Farr will be sorted by level */
		FArr[i].fnum = (FArr[i].Np)->num;
		if(j == 0) FArr[i].Np->sa0 = i;
		else FArr[i].Np->sa1 = i;
		j = (j+1)%2;
		fprintf(fp,"Line: %d, Fault: %d \n",FArr[i].fnum,FArr[i].fval,fp);
        /* get the collapsed fault by check point theory */
		if(FArr[i].Np->type == 1 || FArr[i].Np->type == 0) Fcp[nc++]=&FArr[i];
	}
	fclose(fp);
    struct fList *br = Fchead;
    struct fList *new;
    /* store the fcp sorted by gate level large to small order */
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
		/* check dom */
		if(br->fp->Np->type != 0)
			if(check(br->fp->Np,br->fp->fval))
				flag = 1;
		/* check equal if no dom */
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
    /* write file */
	br = Fchead->next;
	fp = fopen("fault_collapse.txt","w");
	while(br){
		fprintf(fp,"Line: %d, Fault: %d \n",br->fp->fnum,br->fp->fval);
		br = br->next;	
	}
	fclose(fp);
	printf("======> fault collapse done, check fault_collapse.txt and fault_original.txt \n");
}

/*-----------------------------------------------------------------------
input: None
output:
called by: user
description: DFS
author: Li
-----------------------------------------------------------------------*/
struct fList* copyfList(struct fList *l1){
	struct fList* head = (struct fList*)malloc(sizeof(struct fList));
	struct fList* new;
	struct fList* br = l1->next;
	struct fList* brr = head;
	while(br){
		new = (struct fList*)malloc(sizeof(struct fList));
		new->fp = br->fp;
		new->next = NULL;
		brr->next = new;
		brr = brr->next;
		br = br->next;
	}
	return head;
}

void addfList(struct fList* head,struct fault *fp){
	struct fList* br = head;
	while(br->next) br = br->next;
	struct fList* new = (struct fList*)malloc(sizeof(struct fList));
	br->next = new;
	new->fp = fp;
	new->next = NULL;
}

void mergefList(struct fList* head,struct fList* l1){
	struct fList* br = head;
	while(br->next) br = br->next;
	br->next = copyfList(l1)->next;
}

int getconval(int type){
	switch(type){
		case 3: return 1;
		case 4: return 1;
		case 6: return 0;
		case 7: return 0; 
	}
}

int checkconval(struct n_struc *Np, int *num){
	int i,j = 0;
	if(Np->type == 1 || Np->type == 2 || Np->type == 5)
		return 0;
	int c = getconval(Np->type);
	for(i = 0; i<Np->fin;i++){
		if(c == Np->unodes[i]->val){
			j++;
			*num = i;
		}
	}
	return j;
}

struct fList* DFSs(int *Nip)
{
	input = Nip;
    /* get logic sim */
	setinput();
	levsim();
    int i,j;
    int index;
    int* num;
    struct fList* fee;
	for(i = 0;i<Nnodes;i++){
		if(Nodelev[i]->head->next){
			fee = Nodelev[i]->head->next;
			Nodelev[i]->head->next = NULL;
			free(fee);
		}
			
	}
    for(i = 0;i<Nnodes;i++)
	{
		//printf("%d\n",Nodelev[i]->val);
		//printf("%d\n",Nodelev[i]->num);
		if(Nodelev[i]->val == 0) addfList(Nodelev[i]->head,&FArr[Nodelev[i]->sa1]);
		else addfList(Nodelev[i]->head,&FArr[Nodelev[i]->sa0]);
        num = (int*)malloc(sizeof(int));
		index = checkconval(Nodelev[i],num);
		if(Nodelev[i]->type != 0)
		{
			if(index == 1) mergefList(Nodelev[i]->head,Nodelev[i]->unodes[*num]->head);
			else if(index == 0)
				for(j = 0; j<Nodelev[i]->fin;j++)
					mergefList(Nodelev[i]->head,Nodelev[i]->unodes[j]->head);	
		}
	}
	struct fList* head = (struct fList*) malloc(sizeof(struct fList));
	head->next = NULL;
	for(i = 0; i<Npo; i++){
		mergefList(head,Poutput[i]->head);
	}
	free(num);
	return head;
}

int DFS_client()
{
	/*DectobinInput(0);
	struct fList* head = DFSs(input);
	struct fList* br = head->next;
	while(br){
		printf("line = %d ; type = %d; lev = %d\n", br->fp->fnum,br->fp->fval, br->fp->Np->level);
		br = br->next;	
	}
	return 0;*/
	if(siphead == NULL){
		printf("Do the DAL command first");
		return 0;
	}
    dsnum = dfnum = 0;
	struct ipList* brr = siphead->next;
	struct fList* head;
	struct fList* br;
	while(brr){
	 	head = DFSs(brr->Nip);
	 	br	= head->next;
		int flag = 0;
		while(br){
				if(br->fp == brr->fp) flag = 1;
				br = br->next;
		}
		if(flag == 1){ dsnum++;}
		else{ //printf("Test vector for line = %d fault %d fail\n",brr->fp->fnum,brr->fp->fval);
			dfnum++;
		}
		brr = brr->next;
	}
	printf("----------------------------------------------------\n");
	printf("Fault coverage  = %0.2f%% \n", (dsnum+0.0)*100/(snum+fnum));
	printf("Total test vector = %d\nRight test vector = %d\nWrong test vector = %d",snum,dsnum,dfnum);

	return 0;
}

/*-----------------------------------------------------------------------
input: None
output: level
called by: user
description: PFS
author: vinay
-----------------------------------------------------------------------*/
/* for print binary */
void printb(unsigned n){
	if(!n) printf("0");		
	while (n) {
		if (n%2 == 1)
		    printf("1");
		else
		    printf("0");

		n = n/2;
	}
	 //printf("\n");
}

void printmask(unsigned* ormk, unsigned *andmk)
{
	int i;	
	for(i = 0;i<Nnodes;i++)
    {
		printf("line %d \n",Node[i].num);
		printf("andmask = ");
		printb(andmk[i]);
		printf("\n");
		printf("ormask  = ");
		printb(ormk[i]);
		printf("\n");
	}
}

/*set mask  */

void setbinary(unsigned* mk, int i, int val, int index)
{
	if(val == 0)
		mk[index] = mk[index] - pow(2,i);
	else 
		mk[index] = mk[index] + pow(2,i); 
}


void setmask(unsigned* ormk, unsigned *andmk,int lo, int hi)
{
	int j;
	int val, num , index;
	for(j = lo;j<hi;j++){
		val = FArr[j].fval;
		num = FArr[j].fnum;
		index = FArr[j].Np->indx;
		if(val == 0) setbinary(andmk,j-lo,val,index);
		else setbinary(ormk,j-lo,val,index); 
	}
}

void resetmask(unsigned* ormk, unsigned *andmk)
{
	int i;
	for(i = 0 ; i<Nnodes; i++)
    {
		andmk[i] = 0xFFFFFFFF;
		ormk[i] = 0x00000000;
	}
}

/* cal val */
void psetinput()
{
	int i;
	for(i = 0;i<Npi;i++)
	{
		if(input[i])
			Pinput[i]->pval = 0xFFFFFFFF;
		else 
			Pinput[i]->pval = 0;
	}
}

int calpval(int type,unsigned i,unsigned j){
	switch(type){
		case 2: return pxor(i,j);
		case 3: return por(i,j);
		case 4: return pnor(i,j);
		case 5: return pnot(i);
		case 6: return pnand(i,j);
		case 7: return pand(i,j); 
	}
}

int getpval(NSTRUC* n,int lo, int high){
	if((high-lo+1)== 1){  // only 1 input for primary gate or this node is primary input
		return n->unodes[lo]->pval;
	}else{
		int mid =lo+(high-lo-1)/2;
		return calpval(n->type,getpval(n,lo,mid),getpval(n,mid+1,high));	
	}
}

void parsim(unsigned* ormk, unsigned *andmk){
	int i,j;
	int index = 0;
	for(i = 0; i<Nnodes;i++){
		index = Nodelev[i]->indx;
		if(Nodelev[i]->type > 1){
			Nodelev[i]->pval = getpval(Nodelev[i],0,Nodelev[i]->fin-1); /*get val according the input number*/
		}else if(Nodelev[i]->type == 1){
			Nodelev[i]->pval = Nodelev[i]->unodes[0]->pval; 
		}
		/*printf("-----------------------\n");
		printf("andmk = ");
		printb(andmk[index]);
		printf("\n");
		printf("ormk = ");
		printb(ormk[index]);
		printf("\n");
		printf("num: %d value: ",Nodelev[i]->num);		
		printb(Nodelev[i]->pval);
		printf("\n");*/

		Nodelev[i]->pval = ((Nodelev[i]->pval)&andmk[index])|ormk[index];
		
		/*printf("Nodelev[i]-> num: %d value: ",Nodelev[i]->num);		
		printb(Nodelev[i]->pval);
		printf("\n");*/
	}
}

void dectobinary(unsigned num,int *res,int lo,int hi){
	int i;	
	for(i = lo;i<hi;i++){
		res[i-lo] = num%2;
		num = num/2;
	}
}


struct fList* PFSs(int *Nip)
{
	int bit = 32;
	input = Nip;
	setinput();
	levsim();
	unsigned ormk[Nnodes];
	unsigned andmk[Nnodes];
	int i,f,h;
	int num = (2*Nnodes+bit-1)/bit;
	int j = 2*Nnodes;
	struct fList* head = (struct fList*)malloc(sizeof(struct fList));
	head->next = NULL;
	head->fp = NULL;
	for(i = 0; i< num; i++)
	{
		/* set mask */		
		resetmask(ormk, andmk);
		//printmask(ormk,andmk);
		if(j >= bit) setmask(ormk,andmk,i*bit,(i+1)*bit);
		else setmask(ormk,andmk,i*bit,i*bit+j);
		/* cal mask */
		psetinput();
		parsim(ormk,andmk);
		int res[bit];
		for(f = 0; f<Npo; f++)
		{
			if(j >= bit)
			{ 
				dectobinary(Poutput[f]->pval,res,i*bit,(i+1)*bit);
				for(h = 0;h<bit;h++){
					if(res[h] != Poutput[f]->val)
					{
						 addfList(head,&FArr[h+i*bit]);		
					}	
				}
			}else
			{ 
				dectobinary(Poutput[f]->pval,res,i*bit,i*bit+j);
				for(h = 0;h<j;h++){
					if(res[h] != Poutput[f]->val)
						 addfList(head,&FArr[h+i*bit]);		
				}
			}
		}
		j = j - bit;
	}
	/*struct fList* br = head->next;
	while(br){
		//printf("line num = %d , type = %d\n", br->fp->fnum,br->fp->fval);
		br = br->next;
	}*/
	return head;
}
	
int PFS_client()
{
	/*DectobinInput(12);
	struct fList* head = PFSs(input);
	return 0; */
	if(siphead == NULL){
		printf("Do the DAL command first");
		return 0;
	}
	struct ipList* brr = siphead->next;
	struct fList* head;
	struct fList* br;
	int flag;
	psnum = pfnum = 0;
	while(brr){
	 	head = PFSs(brr->Nip);
	 	br	= head->next;
		flag = 0;
		while(br){
			//if(brr->fp->fnum == 1)
				//printf("line num = %d , type = %d\n", br->fp->fnum,br->fp->fval);
			if(br->fp == brr->fp) flag = 1;
			br = br->next;	
		}
		if(flag == 1){ psnum++;}
		else{ 
			//printf("Test vector for line = %d fault %d fail\n",brr->fp->fnum,brr->fp->fval);
			pfnum++;
		}
		brr = brr->next;
	}
	printf("----------------------------------------------------\n");
	printf("Fault coverage  = %0.2f%%\n", (psnum+0.0)*100/(snum+fnum));
	printf("Total test vector = %d\nRight test vector = %d\nWrong test vector = %d",snum,psnum,pfnum);
	return 0;
}


int D_client(){
   printf("fail\n");

   return 0;
}


int podemS(){
	printf("fail\n");
	return 0;

}
/*========================= End of program ============================*/

