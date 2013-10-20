#!/usr/bin/perl
use strict;
use warnings;

use blib;
use Test::More tests => 7;
use Hash::Dynamic;

my $testfile = "test.hx";

my $hx;

# new
{
    $hx = Hash::Dynamic->new(filename => $testfile);
    ok(-f $testfile, "file exists");
    is(-s $testfile, 16384, "hxfile starts out as two pages");
}

# put and get a value
{
    $hx->put('monkey','donkey');
    is($hx->get('monkey'), 'donkey', "put/get");
}

# get but don't put a value
{
    is($hx->get('turkey'), undef, "get an undefined value");
}

# delete
{
    $hx->delete('monkey');
    is($hx->get('monkey'), undef, "monkey was deleted");
}

use Data::Dump 'dump';

# next
{
    my %expected = ( one => 1, two => 2, three => 3 );
    $hx->put($_, $expected{$_}) for keys %expected;

    my %actual;
    while (my ($key,$val) = $hx->nextpair) {
	$actual{$key} = $val;
    }

    is_deeply(\%actual, \%expected, "Values were correct");

    is_deeply($hx->as_hash, \%expected, "as_hash returns the proper hash");
}

END { unlink $testfile if -f $testfile };
