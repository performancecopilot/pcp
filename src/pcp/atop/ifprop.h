struct ifprop	{
	char		name[16];	/* name of interface  		*/
	long int	speed;		/* in megabits per second	*/
	char		fullduplex;	/* boolean			*/
};

int 	getifprop(struct ifprop *);
void 	initifprop(void);
