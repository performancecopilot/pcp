package Perl::Dist::WiX::Asset::LaunchScript;

use 5.008001;
use Moose;
use MooseX::Types::Moose qw( Str );
use File::Spec::Functions qw( catfile );
use Perl::Dist::WiX::Exceptions;

our $VERSION = '1.100';
$VERSION = eval $VERSION; ## no critic (ProhibitStringyEval)

with 'Perl::Dist::WiX::Role::NonURLAsset';

has name => (
	is       => 'bare',
	isa      => Str,
	reader   => 'get_name',
	required => 1,
);

has bin => (
	is       => 'bare',
	isa      => Str,
	reader   => '_get_bin',
	required => 1,
);

sub install {
	my $self = shift;

	my $bin = $self->_get_bin();

	# Check the script exists
	my $to =
	  catfile( $self->_get_image_dir(), 'scripts', $bin . '.bat' );
	unless ( -f $to ) {
		PDWiX::File->throw(
			file    => $to,
			message => 'File does not exist'
		);
	}

	my $icons   = $self->_get_icons();
	my $icon_type = ref $icons;
	$icon_type ||= '(undefined type)';
	if ( 'Perl::Dist::WiX::IconArray' ne $icon_type ) {
		PDWiX->throw( "Icons array is of type $icon_type, "
			  . 'not a Perl::Dist::WiX::IconArray' );
	}

	my $icon_id =
	  $self->_get_icons()
	  ->add_icon( catfile( $self->_get_dist_dir(), 'icons', "$bin.ico" ),
		"$bin.bat" );

	# Add the icon.'(undefined type');
	$self->_add_icon(
		name     => $self->get_name(),
		filename => $to,
		fragment => 'StartMenuIcons',
		icon_id  => $icon_id
	);

	return 1;
} ## end sub install

no Moose;
__PACKAGE__->meta->make_immutable;

1;

__END__

=pod

=head1 NAME

Perl::Dist::WiX::Asset::LaunchScript - Start menu launcher asset for bat script

=head1 SYNOPSIS

  my $distribution = Perl::Dist::WiX::Asset::LaunchScript->new(
    ...
  );

=head1 DESCRIPTION

TODO

=head1 METHODS

TODO

This class is a L<Perl::Dist::WiX::Role::Asset> and shares its API.

=head2 new

The C<new> constructor takes a series of parameters, validates then
and returns a new B<Perl::Dist::WiX::Asset::LaunchScript> object.

It inherits all the params described in the L<Perl::Dist::WiX::Role::Asset> 
C<new> method documentation, and adds some additional params.

=over 4

=item name

The required C<name> param is the name of the package for the purposes
of identification.

This should match the name of the Perl distribution without any version
numbers. For example, "File-Spec" or "libwww-perl".

Alternatively, the C<name> param can be a CPAN path to the distribution
such as shown in the synopsis.

In this case, the url to fetch from will be derived from the name.

=back

The C<new> method returns a B<Perl::Dist::WiX::Asset::LaunchScript> object,
or throws an exception on error.

=head1 SUPPORT

Bugs should be reported via the CPAN bug tracker at

L<http://rt.cpan.org/NoAuth/ReportBug.html?Queue=Perl-Dist-WiX>

For other issues, contact the author.

=head1 AUTHOR

Nathan Scott E<lt>nathans@debian.orgE<gt>

=head1 SEE ALSO

L<Perl::Dist::WiX>, L<Perl::Dist::WiX::Role::Asset>

=head1 COPYRIGHT

Copyright 2009 Nathan Scott.

Copyright 2009 Curtis Jewell.

Copyright 2007 - 2009 Adam Kennedy.

This program is free software; you can redistribute
it and/or modify it under the same terms as Perl itself.

The full text of the license can be found in the
LICENSE file included with this module.

=cut
