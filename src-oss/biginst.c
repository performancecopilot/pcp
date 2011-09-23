#include <pcp/pmapi.h>
#include <pcp/impl.h>
#include <pcp/pmda.h>

static int histo[128];

int
main()
{
    int		key[3];
    int		inst;
    pmInDom	indom;
    int		i;
    int		j;
    int		k;
    int		s;
    int		c;
    int		n = 0;
    char	name[40];

    indom = pmInDom_build(42, 42);

    for (i = 0; i < 255; i++) {
	for (j = 1; j < 255; ) {
	    if (j == 0)
		key[0] = i;
	    else if (j == 1)
		key[0] = (i << 16) | j;
	    else if (j == 2)
		key[0] = (i << 24) | (i << 16) | j;
	    else
		key[0] = (i << 24) | (i << 16) | (j << 8) | j;
	    for (k = 0; k < 3; k++) {
		switch (k) {
		    case 0:
			snprintf(name, sizeof(name), "%08x", key[0]);
			break;
		    case 1:
			key[1] = key[0];
			snprintf(name, sizeof(name), "%08x-%08x", key[0], key[1]);
			break;
		    case 2:
			key[2] = key[1] = key[0];
			snprintf(name, sizeof(name), "%08x-%08x-%08x", key[0], key[1], key[2]);
			break;
		    case 3:
			key[2] = key[1] = key[0];
			key[3]++;
			snprintf(name, sizeof(name), "%08x-%08x-%08x-%08x", key[0], key[1], key[2], key[3]);
			break;
		}

		inst = pmdaCacheStoreInst(indom, PMDA_CACHE_ADD, name, k+1, key, NULL);
		if (inst < 0) {
		    printf("pmdaCacheStoreInst failed: %s\n", pmErrStr(inst));
		    exit(1);
		}
		printf("%d <- %s\n", inst, name);
		histo[(int)(inst/(0x7fffffff/128))]++;
		n++;
	    }
	    if (j < 32)
		j++;
	    else
		j = 2*j + 1;
	}
    }

    pmdaCacheStoreInst(PM_INDOM_NULL, 0, NULL, 0, NULL, NULL);

    for (s = 1; s <= 64; s *= 2) {
	printf("\nInstances distribution across %d bins\n", 128/s);
	c = 0;
	for (j = 0; j < 128; j += s) {
	    i = 0;
	    for (k = 0; k < s; k++)
		i += histo[j+k];
	    printf("%.5f ", (double)i/n);
	    c++;
	    if (c == 10) {
		putchar('\n');
		c = 0;
	    }
	}
	putchar('\n');
    }

    return 0;
}
