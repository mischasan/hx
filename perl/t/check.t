#!/usr/bin/perl
use strict;
use warnings;
use blib;
use Test::More tests => 1;
use Hash::Dynamic;

my $testfile = "test.hx";

my $hx;

# new
$hx = Hash::Dynamic->new(filename => $testfile);
$hx->put('monkey','donkey');
undef $hx;

my $ret = Hash::Dynamic->check(filename => $testfile, repair => 1);
ok($ret, "check($testfile) invoked");

END { unlink $testfile if -f $testfile };
