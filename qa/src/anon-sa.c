/*
 * Anonymise binary sa data files: rewrite sa files
 * from users that are suitable for sharing/qa'ing.
 *
 * When sniffing out starting offsets of hostnames,
 * something like this command can prove handy:
 *   dd if=rhel5-sa20.bin bs=1 skip=109 count=100
 */

#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#define SA_UTSNAME_LEN	65

long
seek_magic_and_reposition_for_hostname(int fd)
{
    int i;
    unsigned short sa_magic;

    struct magic {
	unsigned short	magic_offset;

	/* check magic - not endian neutral in sa ...  :| */
	unsigned short	numeric;
	unsigned short	swabbed;

	/* fortunately, all same length so far (65 bytes) */
	unsigned long	utsname_offset;
    } magic[] = {
	{  0, 0xd596, 0xd596,  89 },	/* sysstat-9.x & -10.x */
	{ 40, 0x216e, 0x6e21,  113 },	/* sysstat-8.x */
	{ 36, 0x2169, 0x6921,  109 },	/* sysstat-7.x */
    };

    for (i = 0; i < sizeof(magic) / sizeof(struct magic); i++) {
	if ((lseek(fd, magic[i].magic_offset, SEEK_SET)) < 0) {
	    perror("lseek magic");
	    continue;
	}
	if (read(fd, &sa_magic, sizeof(sa_magic)) < 0) {
	    perror("read magic");
	    continue;
	}
	if (sa_magic == magic[i].numeric || sa_magic == magic[i].swabbed) {
	    if ((lseek(fd, magic[i].utsname_offset, SEEK_SET)) < 0) {
		perror("hostname lseek");
		return -1;
	    }
	    return magic[i].utsname_offset;	/* all good */
	}
    }
    printf("  non-sa data file - cannot find any sa magic - ignored\n");
    return -1;
}

int main(int argc, char **argv)
{
    int count, fixit = 0, updates = 0;

    for (count = 1; count < argc; count++) {
	char hostname[SA_UTSNAME_LEN] = { 0 };
	char *file = argv[count];
	int fd, bytes, length;
	long offset;

	if (strcmp(file, "-f") == 0) {
	    fixit = 1;
	    continue;
	}
	printf("Filename: %s\n", file);
	if ((fd = open(file, O_RDWR)) < 0) {
	    perror("open");
	    continue;
	}
	if ((offset = seek_magic_and_reposition_for_hostname(fd)) < 0) {
	    close(fd);
	    continue;
	}
	if ((bytes = read(fd, hostname, sizeof(hostname))) < 0) {
	    perror("read");
	    close(fd);
	    continue;
	}
	hostname[sizeof(hostname)-1] = '\0';
	length = strlen(hostname);
	printf("  hostname: %s (%d bytes)\n", hostname, length);

	if (fixit) {
	    memset(hostname, 0, length);	// clear old name
	    strcpy(hostname, "pcp.qa.org");	// reset it
	    if ((lseek(fd, offset, SEEK_SET)) < 0) {
		perror("lseek2");
		close(fd);
		continue;
	    }
	    if (write(fd, hostname, length) < 0) {
		perror("write");
		close(fd);
		continue;
	    }
	    updates++;
	}
	close(fd);
    }
    printf("%d files scanned, %d files updated\n", count-1-fixit, updates);
    return 0;
}
