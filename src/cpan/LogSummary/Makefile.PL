use ExtUtils::MakeMaker;
# See lib/ExtUtils/MakeMaker.pm for details of how to influence
# the contents of the Makefile that is written.
WriteMakefile(
    NAME              => 'PCP::LogSummary',
    AUTHOR            => 'Nathan Scott <nathans@debian.org>',
    VERSION_FROM      => 'LogSummary.pm', # finds $VERSION
    ABSTRACT_FROM     => 'LogSummary.pm', # retrieve abstract from module
);
