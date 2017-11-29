#include <pcp/pmapi.h>

typedef struct {
	unsigned int	a:16;
	unsigned int	b:7;
	unsigned int	c:3;
	unsigned int	d:3;
	unsigned int	e:3;
	unsigned int	f:4;
	unsigned int	g:12;
} ext_bits_t;

typedef struct {
	unsigned int	a:16;
	unsigned int	b:7;
	unsigned int	c:3;
	unsigned int	d:3;
	unsigned int	e:3;
	unsigned int	f:4;
	unsigned int	g:12;
} bits_t;

bits_t	outbits = { 0xdead, 0x5f, 0x3, 0x5, 0x7, 0xc, 0xafe };

int
main(int argc, char *argv[])
{
    bits_t	inbits;
    ext_bits_t	extbits;
    __uint32_t	*ip;
    int		i;
    int		sts;

    fprintf(stderr, "sizeof(bits_t): %d\n", (int)sizeof(bits_t));

    if (strcmp(basename(argv[0]), "read-bf") == 0) {
	sts = read(0, &extbits, sizeof(ext_bits_t));
	if (sts != sizeof(ext_bits_t)) {
	    fprintf(stderr, "read() returns %d not %d as expected\n", sts, (int)sizeof(ext_bits_t));
	    exit(1);
	}

	fprintf(stderr, "read: ");
	for (i = 0, ip = (__uint32_t *)&extbits; i < sizeof(ext_bits_t); i += sizeof(*ip), ip++)
	    fprintf(stderr, " %08x", *ip);
	fputc('\n', stderr);

	/*
	 * swap bytes, then assign fields
	 */
	for (i = 0, ip = (__uint32_t *)&extbits; i < sizeof(ext_bits_t); i += sizeof(*ip), ip++)
	    *ip = ntohl(*ip);

	inbits.a = extbits.a;
	inbits.b = extbits.b;
	inbits.c = extbits.c;
	inbits.d = extbits.d;
	inbits.e = extbits.e;
	inbits.f = extbits.f;
	inbits.g = extbits.g;

	if (inbits.a != outbits.a)
	    fprintf(stderr, "Error: \"a\" got %x expected %x\n", inbits.a, outbits.a);
	if (inbits.b != outbits.b)
	    fprintf(stderr, "Error: \"b\" got %x expected %x\n", inbits.b, outbits.b);
	if (inbits.c != outbits.c)
	    fprintf(stderr, "Error: \"c\" got %x expected %x\n", inbits.c, outbits.c);
	if (inbits.d != outbits.d)
	    fprintf(stderr, "Error: \"d\" got %x expected %x\n", inbits.d, outbits.d);
	if (inbits.e != outbits.e)
	    fprintf(stderr, "Error: \"e\" got %x expected %x\n", inbits.e, outbits.e);
	if (inbits.f != outbits.f)
	    fprintf(stderr, "Error: \"f\" got %x expected %x\n", inbits.f, outbits.f);
	if (inbits.g != outbits.g)
	    fprintf(stderr, "Error: \"g\" got %x expected %x\n", inbits.g, outbits.g);
    }
    else {
	/*
	 * assign fields, then swap bytes
	 */
	memset(&extbits, 0, sizeof(ext_bits_t));
	extbits.a = outbits.a;
	extbits.b = outbits.b;
	extbits.c = outbits.c;
	extbits.d = outbits.d;
	extbits.e = outbits.e;
	extbits.f = outbits.f;
	extbits.g = outbits.g;

	for (i = 0, ip = (__uint32_t *)&extbits; i < sizeof(ext_bits_t); i += sizeof(*ip), ip++)
	    *ip = htonl(*ip);

	fprintf(stderr, "write: ");
	for (i = 0, ip = (__uint32_t *)&extbits; i < sizeof(ext_bits_t); i += sizeof(*ip), ip++)
	    fprintf(stderr, " %08x", *ip);
	fputc('\n', stderr);

	sts = write(1, &extbits, sizeof(bits_t));
	if (sts != sizeof(bits_t)) {
	    fprintf(stderr, "write() returns %d not %d as expected\n", sts, (int)sizeof(bits_t));
	    exit(1);
	}
    }
    exit(0);
}
