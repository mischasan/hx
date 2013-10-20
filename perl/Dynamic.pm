package Hash::Dynamic;

use strict;
use XSLoader ();
use base 'Exporter';

our $VERSION = '1.000';

# The XS layer pushes a few constants onto @EXPORT_OK, so set up %EXPORT_TAGS
# after doing that.
our @EXPORT_OK = (qw());
XSLoader::load(__PACKAGE__, $VERSION);

sub new {
    my ($class, %opts) = @_;
    die "filename required" unless $opts{filename};
    my $self = {
	pagesize => 8192,
	writable => 1,
	repair => 0,
	%opts,
    };
    bless $self, $class;
    eval { $self->_open(@$self{qw(filename writable pagesize repair)}) };
    if ($@) { require Carp; Carp::confess $@ }
    return $self;
}

sub hold {
    my ($self,$key) = @_;
    return $self->_get($key,1);
}

sub get {
    my ($self,$key) = @_;
    return $self->_get($key,0);
}

sub filename {
    my $self = shift;
    return $self->{filename};
}

sub rewind {
    my $self = shift;
    $self->_close;
    $self->_open(@$self{qw(filename writable pagesize repair)});
}

sub as_hash {
    my $self = shift;

    $self->rewind;

    my %hash;
    while (my ($key,$val) = $self->nextpair) {
	$hash{$key} = $val;
    }
    return \%hash;
}

sub check {
    my ($class,%opts) = @_;
    $opts{writable} = 1 if $opts{repair} & 1;
    return 0 unless my $hxfile = Hash::Dynamic->new(%opts, repair => 1);
    return $hxfile->_check();
}

sub DESTROY {
    my $self = shift;
    $self->_close;
}

return 1;
