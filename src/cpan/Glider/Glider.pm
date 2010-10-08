package Perl::Dist::Glider;
# Ref: http://strawberryperl.com/documentation/building.html

=pod

=head1 NAME

PCP::Glider - Windows runtime package for Performance Co-Pilot

=head1 DESCRIPTION

Glider is a binary distribution of PCP for the Windows (Win32) operating
system.  It includes a bundled compiler, Perl runtime (and pre-installed
modules that offer the ability to install XS CPAN modules directly from
CPAN), POSIX shell environment, GNU utilities and the Performance Co-Pilot
toolkit.

The purpose of the PCP::Glider package is to provide a practical Win32
environment for performance engineers experienced with either a POSIX
platform or the Win32 environment to run the Performance Co-Pilot toolkit
on Windows.  It provides both the PCP monitor and collector components.

PCP::Glider includes:

=over

=item *

Mingw GCC C/C++ compiler ( http://www.mingw.org )

=item *

MSYS Core System ( http://www.mingw.org/wiki/msys )

=item *

GNU Win32 debugger ( http://www.mingw.org/wiki/gdb )

=item *

Perl and CPAN support ( http://www.strawberryperl.com )

=item *

Qt4 toolkit runtime ( http://www.qtsoftware.com )

=item *

Performance Co-Pilot ( http://oss.sgi.com/projects/pcp )

=back

=cut

use strict;
use warnings;
use File::Spec::Functions qw( catfile catdir );
use base qw( Perl::Dist::Strawberry );
use vars qw( $VERSION @ISA );
use vars qw( $PKGCPAN $PKGNAME $PKGURLS );
use Readonly qw( Readonly );
#use Perl::Dist::WiX::Service;
use Perl::Dist::Glider::LaunchExe;
use Perl::Dist::Glider::LaunchScript;

BEGIN {
	$PKGNAME = 'pcp-glider';
	$PKGURLS = 'file://D|/packages/',
	$PKGCPAN = 'file://D|/minicpan/',
	$VERSION = '0.9.8';
	@ISA     = 'Perl::Dist::Strawberry';
}

sub new {
	shift->SUPER::new(
		app_id            => 'pcp',
		app_name          => 'PCP Glider',
		app_publisher     => 'Performance Co-Pilot Project',
		app_publisher_url => 'http://oss.sgi.com/projects/pcp',
		image_dir         => 'C:\\Glider',
		output_dir        => 'D:\\msi',
		download_dir      => 'C:\\packages',
		temp_dir          => 'D:\\tmp',
		offline           => 1,
		cpan              => new URI($PKGCPAN),
		perl_version      => '5120',
		perl_config_cf_email => 'pcp@oss.sgi.com',
		force             => 1,
		trace             => 1,

		build_number      => 1,
		beta_number       => 0,

		zip               => 0,
		msi               => 1,
		msi_help_url      => 'http://oss.sgi.com/projects/pcp',
		msi_banner_top    => 'C:\\packages\\GliderBanner.bmp',
		msi_banner_side   => 'C:\\packages\\GliderDialog.bmp',
		msi_product_icon  => 'C:\\packages\\GliderPCP.ico',

		msm_to_use => 'file://D|/packages/strawberry-perl-5.12.0.1.msm',
		msm_zip    => 'file://D|/packages/strawberry-perl-5.12.0.1.zip',
		msm_code   => 'BC4B680E-4871-31E7-9883-3E2C74EA4F3C',
		fileid_perl => 'F_exe_MzA1Mjk2NjIyOQ',
		fileid_relocation_pl => 'F_lp_NDIwNjE2MjkyNw',
		relocation => 1,

		# Tasks to complete to create Glider
		tasklist => [
			'final_initialization',
			'initialize_using_msm',
			'install_glider_toolchain',
#			'install_glider_modules_1',
#			'install_glider_modules_2',
			'install_glider_extras',
			'install_win32_extras',
			'install_strawberry_extras',
			'remove_waste',
			'install_relocatable',
			'regenerate_fragments',
			'find_relocatable_fields',
			'write',
		],
	);
}


sub output_base_filename {
	$PKGNAME . '-' . $VERSION;
}

my %PKG = (
	'gdb'		=> 'PCP-gdb-6.8-mingw-3.tar.gz',
	'msys'		=> 'PCP-msysCORE-1.0.11-20080826.tar.gz',
	'bison'		=> 'PCP-bison-2.3-MSYS-1.0.11-1.tar.gz',
	'coreutils'	=> 'PCP-coreutils-5.97-MSYS-1.0.11-1.tar.gz',
	'flex'		=> 'PCP-flex-2.5.33-MSYS-1.0.11-1.tar.gz',
	'minires'	=> 'PCP-minires-1.01-1-MSYS-1.0.11-1.tar.gz',
	'openssh'	=> 'PCP-openssh-4.7p1-MSYS-1.0.11-1-bin.tar.gz',
	'openssl'	=> 'PCP-openssl-0.9.8g-1-MSYS-1.0.11-2-dll098.tar.gz',
	'regex'		=> 'PCP-regex-0.12-MSYS-1.0.11-1.tar.gz',
	'tar'		=> 'PCP-tar-1.19.90-MSYS-1.0.11-1-bin.tar.gz',
	'vim'		=> 'PCP-vim-7.1-MSYS-1.0.11-1-bin.tar.gz',
	'zlib'		=> 'PCP-zlib-1.2.3-MSYS-1.0.11-1.tar.gz',

	'qt'		=> 'PCP-qt-4.6.3-lib.tar.gz',
	'coin'		=> 'PCP-coin-2.5.0-lib.tar.gz',

	'pcp'		=> 'pcp-3.4.1-1.tar.gz',
	'pcp_gui'	=> 'pcp-gui-1.5.0.tar.gz',
);

sub output_fragment_name
{
    my $name = shift;
    $name =~ s{\s}{_}msxg;
    return $name;
}

sub custom_binary
{
    my $self	= shift;
    my $name	= shift;
    my $instto	= shift;

    my $filelist = $self->install_binary(
		name       => $name,
		url        => $PKGURLS . $PKG{$name},
		install_to => $instto,
    );
    print "custom_binary: $name\n" . $filelist->as_string() . "\n";
    $self->insert_fragment(output_fragment_name($name), $filelist);
    return 1;
}

sub custom_file
{
    my $self	= shift;
    my $name	= shift;
    my $file    = shift;
    my $tofile	= shift;

    my $filelist = $self->install_file(
		name       => $name,
		url        => $PKGURLS . $file,
		install_to => $tofile,
    );
    print "custom_file: $name\n" . $filelist->as_string() . "\n";
    $self->insert_fragment(output_fragment_name($name), $filelist);
    return 1;
}

sub install_script_launcher
{
    my $self = shift;
    my $launcher = Perl::Dist::WiX::Asset::LaunchScript->new(
	parent	=> $self,
	@_,
    );
    $launcher->install();
    return $self;
}

sub install_exe_launcher
{
    my $self = shift;
    my $launcher = Perl::Dist::WiX::Asset::LaunchExe->new(
	parent	=> $self,
	@_,
    );
    $launcher->install();
    return $self;
}

sub install_menu_items
{
    my $self = shift;

    $self->trace_line(1, "Installing menu items\n");

    $self->custom_file('Filesystem Table', 'pcp.fstab', 'etc/fstab');
    $self->custom_file('Shell Profile', 'pcp.profile', 'etc/profile.d/pcp.sh');
    $self->custom_file('POSIX Shell', 'pcpsh.bat', 'scripts/pcpsh.bat');
    $self->custom_file('Website Icon', 'pcp.ico', 'icons/pcp.ico');
    $self->custom_file('Charts Icon', 'chart.ico', 'icons/chart.ico');
    $self->custom_file('Shell Icon', 'pcpsh.ico', 'icons/pcpsh.ico');
    $self->custom_file('PmChart Icon', 'pmchart.ico', 'icons/pmchart.ico');

    $self->trace_line(1, "Completed menu items\n");

    $self->install_exe_launcher(
		name	=> 'PCP Charts',
		bin	=> 'pmchart'
    );
    $self->trace_line(1, "Completed exe launcher\n");

    $self->install_script_launcher(
		name	=> 'PCP Shell Prompt',
		bin	=> 'pcpsh'
    );
    $self->trace_line(1, "Completed script launcher\n");

    $self->install_website(
	name		=> 'Performance Co-Pilot Web',
	url		=> 'http://oss.sgi.com/projects/pcp/',
	icon_file	=> catfile( $self->image_dir, 'icons', 'pcp.ico' ),
    );
    $self->trace_line(1, "Completed website launcher\n");

    return 1;
}

sub install_environment
{
    my $self = shift;

    $self->trace_line(1, "Installing environment\n");

    # Set up the environment variables for the binaries
    # Note: we do NOT add /bin here as it contains some
    # utilities that conflict with Windows utilities in
    # terms of name (sort, date, etc).

    $self->add_env( PATH	=> '[INSTALLDIR]local\\bin', 1 );

    # It would be good to get this to just the one variable
    # (i.e. PCP_DIR).  Require PCP changes to support that.

    $self->add_env( PCP_DIR	=> '[INSTALLDIR]' );
    $self->add_env( PCP_CONF	=> '[INSTALLDIR]etc\\pcp.conf' );
    $self->add_env( PCP_CONFIG	=> '[INSTALLDIR]local\\bin\\pmconfig.exe' );

    return 1;
}

sub install_run_scripts
{
    my $self = shift;

    $self->trace_line(1, "Installing run scripts\n");

    $self->custom_file('Post_Install_Script', 'postinst.bat', 'scripts/postinst.bat');
    $self->custom_file('Pre_Remove_Script', 'prerm.bat', 'scripts/prerm.bat');

    # These should be run automatically (however that is done,
    # scripts might not be the right approach).

#    $self->add_run(
#	filename	=> 'scripts\\postinst.bat'
#    );
#    $self->add_uninstallrun(
#	filename	=> 'scripts\\prerm.bat'
#    );

    return 1;
}

sub install_glider_modules_1
{
    my $self = shift;

    $self->install_modules( qw{
		enum
		Win32::Registry
		Win32::IPHelper
		Net::IP
		Net::DNS
    } );

    return 1;
}

sub install_glider_modules_2
{
    my $self = shift;

    $self->install_distribution_from_file(
	mod_name => 'PCP::PMDA',
	file	=> 'C:\\packages\\PCP-PMDA-1.07.tar.gz',
	buildpl_param => [ '--installdirs', 'vendor' ],
    );
    $self->install_distribution_from_file(
	mod_name	=> 'PCP::MMV',
	file	=> 'C:\\packages\\PCP-MMV-1.00.tar.gz',
	buildpl_param => [ '--installdirs', 'vendor' ],
    );

    return 1;
}

sub install_glider_toolchain
{
    my $self = shift;
    my $start = time;
    my $t;

    $t = time;
    $self->custom_binary('msys', '.');
    $self->custom_binary('coreutils', '.');
    $self->trace_line(1, "Completed msys in " . (time - $t) . " seconds\n");

    $t = time;
    $self->custom_binary('bison', '.');
    $self->custom_binary('flex', '.');
    $self->custom_binary('openssh', '.');
    $self->custom_binary('openssl', '.');
    $self->custom_binary('regex', '.');
    $self->custom_binary('zlib', '.');
    $self->custom_binary('minires', '.');
    $self->custom_binary('vim', '.');
    $self->custom_binary('gdb', 'local');
    $self->trace_line(1, "Completed toolchain in " . (time - $t) . " seconds\n");

    $t = time;
    $self->custom_binary('qt', 'local');
#    $self->custom_binary('coin', 'local');
    $self->trace_line(1, "Completed QT/Coin in " . (time - $t) . " seconds\n");

    $t = time;
    $self->custom_binary('pcp', '.');
    $self->custom_binary('pcp_gui', '.');
    $self->trace_line(1, "Completed PCP in " . (time - $t) . " seconds\n");

    return 1;
}

sub install_glider_extras
{
    my $self = shift;
    my $t = time;

    $self->install_menu_items();
    $self->install_environment();
    $self->install_run_scripts();
    $self->trace_line(1, "Completed extras in " . (time - $t) . " seconds\n");

    return 1;
}

1;
