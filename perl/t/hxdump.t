#!/usr/bin/perl
use strict;
use warnings;

use Test::More tests => 3;
use Hash::Dynamic;

my $hxfile1 = "one.hx";
my $hxfile2 = "two.hx";

my $hx1 = Hash::Dynamic->new(filename => $hxfile1);
my $hx2 = Hash::Dynamic->new(filename => $hxfile2);

$hx1->put('monkey','donkey');
$hx1->put('donkey','turkey');

$hx2->put('cat','mouse');

is(qx{$^X -Mblib bin/hxdump $hxfile1}, <<__EOF__, "hexdump $hxfile1");
--- 
one.hx:
    monkey: donkey
    donkey: turkey
__EOF__

is(qx{$^X -Mblib bin/hxdump $hxfile2}, <<__EOF__, "hexdump $hxfile2");
--- 
two.hx:
    cat: mouse
__EOF__

is(qx{$^X -Mblib bin/hxdump $hxfile1 $hxfile2}, <<__EOF__, "hexdump $hxfile1 $hxfile2");
--- 
one.hx:
    monkey: donkey
    donkey: turkey
two.hx:
    cat: mouse
__EOF__

END { unlink($hx1, $hx2) if -f $hx1 or -f $hx2 };
