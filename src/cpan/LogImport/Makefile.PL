use ExtUtils::MakeMaker;
# See lib/ExtUtils/MakeMaker.pm for details of how to influence
# the contents of the Makefile that is written.
WriteMakefile(
    NAME		=> 'PCP::LogImport',
    AUTHOR		=> 'Ken McDonell <kenj@internode.on.net>',
    VERSION_FROM	=> 'LogImport.pm', # finds $VERSION
    ABSTRACT_FROM	=> 'LogImport.pm', # retrieve abstract from module
    C			=> ['LogImport.c'],
    OPTIMIZE		=> '-g',
    XSPROTOARG		=> '-noprototypes',
    OBJECT		=> 'LogImport.o',
    DEFINE		=> '-DPERLIO_NOT_STDIO=0 -DPCP_VERSION -DPCP_DEBUG',
    LDFROM		=> "-L$ENV{PCP_TOPDIR}/src/libpcp/src -L$ENV{PCP_TOPDIR}/src/libpcp_import/src -L$ENV{PCP_DIR}\\local\\bin -lpcp_import -lpcp LogImport.o",
    INC			=> "-I$ENV{PCP_TOPDIR}/src/include -I/usr/include/pcp -I$ENV{PCP_DIR}\\include\\pcp -I$ENV{PCP_DIR}\\c\\include",
    LIBS		=> ["-L$ENV{PCP_DIR}\\local\\bin", '-lpcp_import', '-lpcp'],
    CC			=> $ENV{"CC"},
    LD			=> $ENV{"CC"},
);
