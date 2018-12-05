// fault 
struct fault{
	int fval; // fault type 0: SA0, 1:SA1
	int fnum; // line num
	int Neq;
	int Ndom;
	struct n_struc *Np;
};
// fault list
struct fList{
	struct fault *fp; // fault node
	struct fList *next; // next node
};

// input vector data structure
struct ipList{
	int *Nip;  // array of input 
 	struct fault *fp; // fault detect by this input	
	struct ipList *next; 
};

//note: how malloc
//struct fault *new = (struct fault *)malloc(sizeof(struct fault));
// free(new) to delete.


