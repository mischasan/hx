#!/usr/bin/perl
use strict;
use warnings;

use Test::More qw(no_plan);
use Hash::Dynamic;

my $testfile = "fork.hx";

my $kid = fork;

if ($kid) {

    sleep 1;

    my $hx = Hash::Dynamic->new(filename => $testfile);
    is($hx->get('monkey'),'donkey', "child released the lock after setting monkey=donkey");

    wait;
}
else {
    my $hx = Hash::Dynamic->new(filename => $testfile);
    $hx->put('monkey','donkey');

    sleep 5;
    $hx->put('monkey','turkey');

    exit;
}
