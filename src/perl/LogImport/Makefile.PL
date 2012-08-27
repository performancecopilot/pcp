use ExtUtils::MakeMaker;
# See lib/ExtUtils/MakeMaker.pm for details of how to influence
# the contents of the Makefile that is written.
my $ldfrom;
my $inc;
my $libs;
my $lddlflags;
my $cccdlflags;

if ($ENV{TARGET_OS} eq "mingw") {
    $ldfrom = "-L$ENV{PCP_TOPDIR}/src/libpcp/src -L$ENV{PCP_TOPDIR}/src/libpcp_import/src -L$ENV{PCP_DIR}\\local\\bin -lpcp_import -lpcp LogImport.o",
    $inc = "-I$ENV{PCP_TOPDIR}/src/include/pcp -I/usr/include/pcp -I$ENV{PCP_DIR}\\include\\pcp -I$ENV{PCP_DIR}\\c\\include";
    $libs = ["-L$ENV{PCP_DIR}\\local\\bin", '-lpcp_import', '-lpcp'];
}
else {
    $ldfrom = "LogImport.o",
    $inc = "-I$ENV{PCP_TOPDIR}/src/include/pcp -I/usr/include/pcp";
    $libs = ["-L$ENV{PCP_TOPDIR}/src/libpcp/src -L$ENV{PCP_TOPDIR}/src/libpcp_import/src -lpcp_import -lpcp"];
}
if ($ENV{TARGET_OS} eq "darwin") {
    # standard ones, minus -arch ppc
    $lddlflags = "-arch x86_64 -arch i386 -bundle -undefined dynamic_lookup";
}
else {
    $lddlflags = "-shared \$(OPTIMIZE) \$(LDFLAGS)";
}
if ($ENV{TARGET_OS} eq "solaris") {
    # for OpenSolaris Makefile ends up with -KPIC instead of -fPIC otherwise
    $cccdlflags = "-fPIC"
} 

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
    LDFROM		=> $ldfrom,
    LDDLFLAGS		=> $lddlflags,
    CCCDLFLAGS		=> $cccdlflags,
    INC			=> $inc,
    LIBS		=> $libs,
    CC			=> $ENV{"CC"},
    LD			=> $ENV{"CC"},
);
