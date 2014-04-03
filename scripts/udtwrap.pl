#!/usr/bin/perl
use strict;
use Getopt::Std;

my $OPT = {};

my $server = "localhost:1081";
my $lib = "/usr/local/lib/libminisocks.so";

getopts("s:l:",$OPT);

$server = $OPT->{s} if(exists $OPT->{s});
$lib    = $OPT->{l} if(exists $OPT->{l});

$ENV{'MINISOCKS_SERVER'} = $server;
$ENV{'LD_PRELOAD'} .= ":$lib";

#print "==> ---\n";

#foreach (keys %ENV) {
#    print "==> $_\t $ENV{$_}\n";
#}

exec(@ARGV);

