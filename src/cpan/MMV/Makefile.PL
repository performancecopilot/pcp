use ExtUtils::MakeMaker;
# See lib/ExtUtils/MakeMaker.pm for details of how to influence
# the contents of the Makefile that is written.
WriteMakefile(
    NAME		=> 'PCP::MMV',
    AUTHOR		=> 'Nathan Scott <nathans@debian.org>',
    VERSION_FROM	=> 'MMV.pm', # finds $VERSION
    ABSTRACT_FROM	=> 'MMV.pm', # retrieve abstract from module
    C			=> ['MMV.c'],
    OPTIMIZE		=> '-g',
    XSPROTOARG		=> '-noprototypes',
    OBJECT		=> 'MMV.o',
    DEFINE		=> '-DPERLIO_NOT_STDIO=0 -DPCP_VERSION -DPCP_DEBUG',
    LDFROM		=> "-L$ENV{PCP_TOPDIR}/src/libpcp/src -L$ENV{PCP_TOPDIR}/src/libpcp_mmv/src -L$ENV{PCP_DIR}\\local\\bin -lpcp_mmv -lpcp MMV.o",
    INC			=> "-I$ENV{PCP_TOPDIR}/src/include -I/usr/include/pcp -I$ENV{PCP_DIR}\\include\\pcp -I$ENV{PCP_DIR}\\c\\include",
    LIBS		=> ["-L$ENV{PCP_DIR}\\local\\bin", '-lpcp_mmv', '-lpcp'],
    CC			=> $ENV{"CC"},
    LD			=> $ENV{"CC"},
);
