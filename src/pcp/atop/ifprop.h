#define MAXINTNM	32
struct ifprop	{
	char		name[MAXINTNM];	/* name of interface  		*/
	long int	speed;		/* in megabits per second	*/
	char		fullduplex;	/* boolean			*/
	char		type;		/* type: 'e' (ethernet)		*/
					/*       'w' (wireless)         */
};

int 	getifprop(struct ifprop *);
void 	initifprop(void);
