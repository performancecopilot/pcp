use ExtUtils::MakeMaker;
# See lib/ExtUtils/MakeMaker.pm for details of how to influence
# the contents of the Makefile that is written.
WriteMakefile(
    NAME		=> 'PCP::PMDA',
    AUTHOR		=> 'Nathan Scott <nathans@debian.org>',
    VERSION_FROM	=> 'PMDA.pm', # finds $VERSION
    ABSTRACT_FROM	=> 'PMDA.pm', # retrieve abstract from module
    LIBS		=> ['-lpcp_pmda -lpcp'],
    C			=> ['local.c', 'PMDA.c'],
    OBJECT		=> 'local.o PMDA.o',
    INC			=> '-I../../include -I/usr/include/pcp',
    DEFINE		=> '-DPERLIO_NOT_STDIO=0',     # e.g., '-DPCP_DEBUG'
    XSPROTOARG		=> '-noprototypes',
);
