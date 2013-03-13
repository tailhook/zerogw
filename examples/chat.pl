#!/usr/bin/perl -w

use strict;
use warnings;
use ZeroMQ qw/:all/;
use ZeroMQ::Constants qw(ZMQ_SUBSCRIBE ZMQ_SNDMORE);
use Data::Dumper;

my $cxt  = ZeroMQ::Context->new();
my $sub = $cxt->socket(ZMQ_SUB);
$sub->setsockopt(ZMQ_SUBSCRIBE, "");
$sub->connect("tcp://127.0.0.1:7002");


my $pub = $cxt->socket(ZMQ_PUB);
$pub->connect("tcp://127.0.0.1:7003");

while (1) {
	my $rcv = $sub->recv();

	my @parts = ();
	$parts[0] = $rcv->data;
	while ($sub->getsockopt(ZMQ_RCVMORE)) {
		push(@parts, $sub->recv()->data())
	}
	
	local $Data::Dumper::Indent = 0;
	local $Data::Dumper::Terse = 1;
	print "GOT:".Dumper(\@parts)."\n";

    if ($parts[1] eq 'connect') {
        $pub->send('subscribe', ZMQ_SNDMORE);
        $pub->send($parts[0], ZMQ_SNDMORE);
        $pub->send($parts[0]);
        
        $pub->send('subscribe', ZMQ_SNDMORE);
        $pub->send($parts[0], ZMQ_SNDMORE);
        $pub->send('room:default');
        
        $pub->send('subscribe', ZMQ_SNDMORE);
        $pub->send($parts[0], ZMQ_SNDMORE);
        $pub->send('room:default:joins');
        
        $pub->send('publish', ZMQ_SNDMORE);
        $pub->send('room:default:joins', ZMQ_SNDMORE);
        $pub->send('User joined room');
    } elsif ($parts[1] eq 'message' ) {
        if ($parts[2] eq 'no_spam') {
            $pub->send('unsubscribe', ZMQ_SNDMORE);
            $pub->send($parts[0], ZMQ_SNDMORE);
            $pub->send('room:default:joins');
        } else {
            $pub->send('publish', ZMQ_SNDMORE);
            $pub->send('room:default', ZMQ_SNDMORE);
            $pub->send($parts[2]);
		}
    } elsif ($parts[1] eq 'disconnect') {
        # will be unsubscribed automatically
    }

}


