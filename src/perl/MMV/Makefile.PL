use ExtUtils::MakeMaker;
# See lib/ExtUtils/MakeMaker.pm for details of how to influence
# the contents of the Makefile that is written.
my $ldfrom;
my $inc;
my $libs;
my $lddlflags;
my $cccdlflags;

if ($ENV{TARGET_OS} eq "mingw") {
    $ldfrom = "-L$ENV{PCP_TOPDIR}/src/libpcp/src -L$ENV{PCP_TOPDIR}/src/libpcp_mmv/src -L$ENV{PCP_DIR}\\local\\bin -lpcp_mmv -lpcp MMV.o";
    $inc = "-I$ENV{PCP_TOPDIR}/src/include/pcp -I/usr/include/pcp -I$ENV{PCP_DIR}\\include\\pcp -I$ENV{PCP_DIR}\\c\\include";
    $libs = ["-L$ENV{PCP_DIR}\\local\\bin", '-lpcp_mmv', '-lpcp'];
}
else {
    $ldfrom = "MMV.o";
    $inc = "-I$ENV{PCP_TOPDIR}/src/include/pcp -I/usr/include/pcp";
    $libs = ["-L$ENV{PCP_TOPDIR}/src/libpcp_mmv/src -L$ENV{PCP_TOPDIR}/src/libpcp/src -lpcp_mmv -lpcp"];
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
    NAME		=> 'PCP::MMV',
    AUTHOR		=> 'Nathan Scott <nathans@debian.org>',
    VERSION_FROM	=> 'MMV.pm', # finds $VERSION
    ABSTRACT_FROM	=> 'MMV.pm', # retrieve abstract from module
    C			=> ['MMV.c'],
    OPTIMIZE		=> '-g',
    XSPROTOARG		=> '-noprototypes',
    OBJECT		=> 'MMV.o',
    DEFINE		=> '-DPERLIO_NOT_STDIO=0 -DPCP_VERSION -DPCP_DEBUG',
    LDFROM		=> $ldfrom,
    LDDLFLAGS		=> $lddlflags,
    CCCDLFLAGS		=> $cccdlflags,
    INC			=> $inc,
    LIBS		=> $libs,
    CC			=> $ENV{"CC"},
    LD			=> $ENV{"CC"},
);
