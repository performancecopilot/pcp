/*
 * Copyright (c) 2013 Red Hat.
 *
 * Exercise basic user access control checking APIs.
 * (non-Win32 variant of the test, uses uid_t/gid_t)
 */

#include <pcp/pmapi.h>
#include "libpcp.h"
#if defined(HAVE_PWD_H)
#include <pwd.h>
#endif
#if defined(HAVE_GRP_H)
#include <grp.h>
#endif

gid_t		gid;
struct group	*grp, *groups;
int		ngroups;
uid_t		uid;
struct passwd	*usr, *users;
int		nusers;
unsigned int	op;

int
check_users(void)
{
    int		c, s, sts = 0;

    for (c = 0; c < nusers; c++) {
	usr = &users[c];
	if ((s = __pmAccAddUser(usr->pw_name, 1 << c, 1 << c, 0)) < 0) {
	    printf("cannot add user for op%d: %s\n", c, pmErrStr(s));
	    sts = s;
	}
    }
    if (sts < 0)
	return 1;

    putc('\n', stderr);
    __pmAccDumpUsers(stderr);
    putc('\n', stderr);

    for (c = 0; c < WORD_BIT; c++) {
	usr = &users[c % nusers];
	sts = __pmAccAddAccount(usr->pw_name, NULL, &op);
	if (sts < 0) {
	    printf("add user #%d (%s): %s\n", c, usr->pw_name, pmErrStr(sts));
	    continue;
	}
	else if (op != (1 << c))
	    printf("account %d: __pmAccAddAccount returns denyOpsResult 0x%x (expected 0x%x)\n",
		   c, op, 1 << c);
    }

    putc('\n', stderr);
    __pmAccDumpUsers(stderr);
    putc('\n', stderr);
    return 0;
}

int
check_groups(void)
{
    int		c, s, sts = 0;

    for (c = 0; c < ngroups; c++) {
	grp = &groups[c];
	if ((s = __pmAccAddGroup(grp->gr_name, 1 << c, 1 << c, 0)) < 0) {
	    printf("cannot add group for op%d: %s\n", c, pmErrStr(s));
	    sts = s;
	}
    }
    if (sts < 0)
	return 1;

    putc('\n', stderr);
    __pmAccDumpGroups(stderr);
    putc('\n', stderr);

    for (c = 0; c < WORD_BIT; c++) {
	grp = &groups[c % ngroups];
	sts = __pmAccAddAccount(NULL, grp->gr_name, &op);
	if (sts < 0) {
	    printf("add group #%d (%s): %s\n", c, grp->gr_name, pmErrStr(sts));
	    continue;
	}
	else if (op != (1 << c))
	    printf("account %d: __pmAccAddAccount returns denyOpsResult 0x%x (expected 0x%x)\n",
		   c, op, 1 << c);
    }

    putc('\n', stderr);
    __pmAccDumpGroups(stderr);
    putc('\n', stderr);
    return 0;
}

int
main(int argc, char **argv)
{
    int			c, sts;
    int			errflag = 0;
    char		*name;
    size_t		size;

    pmSetProgname(argv[0]);

    while ((c = getopt(argc, argv, "D:u:g:?")) != EOF) {
	switch (c) {
	case 'D':	/* debug options */
	    if ((sts = pmSetDebug(optarg)) < 0) {
		fprintf(stderr, "%s: unrecognized debug options specification (%s)\n",
		    pmGetProgname(), optarg);
		errflag++;
	    }
	    break;

	case 'g':	/* another group ID */
	    gid = atoi(optarg);
	    if ((grp = getgrgid(gid)) == NULL) {
		fprintf(stderr, "%s: getgrgid: unknown group identifier (%s)\n",
		    pmGetProgname(), optarg);
		errflag++;
		break;
	    }
	    size = (ngroups + 1) * sizeof(*grp);
	    if ((groups = realloc(groups, size)) == NULL)
		pmNoMem("gid realloc", size, PM_FATAL_ERR);
	    groups[ngroups] = *grp;
	    for (c = 0, name = grp->gr_mem[0]; name; c++, name++)
		groups[ngroups].gr_mem[c] = strdup(name);
	    ngroups++;
	    break;

	case 'u':	/* another user ID */
	    uid = atoi(optarg);
	    if ((usr = getpwuid(uid)) == NULL) {
		fprintf(stderr, "%s: getpwuid: unknown user identifier (%s)\n",
		    pmGetProgname(), optarg);
		errflag++;
		break;
	    }
	    size = (nusers + 1) * sizeof(*usr);
	    if ((users = realloc(users, size)) == NULL)
		pmNoMem("uid realloc", size, PM_FATAL_ERR);
	    memset(&users[nusers], 0, sizeof(*usr));
	    users[nusers].pw_name = strdup(usr->pw_name);
	    users[nusers].pw_uid = usr->pw_uid;
	    users[nusers].pw_gid = usr->pw_gid;
	    nusers++;
	    break;

	case '?':
	default:
	    errflag++;
	    break;
	}
    }

    if (errflag) {
	fprintf(stderr,
"Usage: %s [options] uid ...\n\
\n\
Options:\n\
  -D debugspec   set debugging diagnostic options\n\
  -u uid         add numeric user ID to set used in testing\n\
  -g gid         add numeric group ID to set used in testing\n",
                pmGetProgname());
	return 1;
    }

    sts = 0;
    for (op = 0; op < WORD_BIT; op++) {
	if ((c = __pmAccAddOp(1 << op)) < 0) {
	    printf("Bad op %d: %s\n", op, pmErrStr(c));
	    sts = c;
	}
	if ((c = __pmAccAddOp(1 << op)) >= 0) {
	    printf("duplicate op test failed for op %d\n", op);
	    sts = -EINVAL;
	}
    }
    if (sts < 0)
	return 1;

    if (nusers)
	if ((sts = check_users()) != 0)
	    return sts;

    if (ngroups)
	if ((sts = check_groups()) != 0)
	    return sts;

    return 0;
}
