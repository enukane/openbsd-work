# ex:ts=8 sw=4:
# $OpenBSD: signify.pm,v 1.12 2014/03/05 22:42:36 espie Exp $
#
# Copyright (c) 2013-2014 Marc Espie <espie@openbsd.org>
#
# Permission to use, copy, modify, and distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
# ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
# ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
# OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

use strict;
use warnings;

package OpenBSD::signify;

use OpenBSD::PackageInfo;
use OpenBSD::Paths;
use File::Temp qw/mkstemp/;

my $header = "untrusted comment: signify -- signature\n";
my $cmd = OpenBSD::Paths->signify;
my $suffix = ".sig";

sub compute_signature
{
	my ($plist, $state, $key, $pub) = @_;

	my $contents = $plist->infodir.CONTENTS;
	my $sigfile = $contents.$suffix;

	open my $fh, ">", $contents;
	$plist->write_no_sig($fh);
	close $fh;
	$state->system($cmd, '-S', '-q', '-s', $key, '-m', $contents) == 0 or 
	    $state->fatal("problem generating signature");
	if (defined $pub) {
		$state->system($cmd, '-V', '-q', '-p', $pub, '-m', 
		    $contents) == 0 or 
		    $state->fatal("public key and private key don't match");
	}
	open(my $sighandle, '<', $sigfile)
		or $state->fatal("problem reading signature");
	my $header = <$sighandle>;
	my $sig = <$sighandle>;
	close($sighandle);
	unlink($sigfile);
	chomp $sig;
	return $sig;
}

sub check_signature
{
	my ($plist, $state) = @_;
	
	if (!$plist->has('signer')) {
		$state->errsay("Invalid signed plist: no \@signer");
		return 0;
	}
	my $signer = $plist->get('signer')->name;
	my $pubkey = OpenBSD::Paths->signifykey($signer);
	if (!-f $pubkey) {
		$state->errsay("Can't find key #1 for signer #1", $pubkey, 
		    $signer);
		return 0;
	}

	my $sig = $plist->get('digital-signature');
	my ($fh, $fname) = mkstemp("/tmp/pkgcontent.XXXXXXXXX");
	$plist->write_no_sig($fh);
	open(my $fh2, ">", $fname.$suffix);
	print $fh2 $header, $sig->{b64sig}, "\n";
	close $fh;
	close $fh2;
	my $rc = $state->system($cmd, '-V', '-q', '-p', $pubkey, '-m', $fname);
	unlink $fname;
	unlink $fname.$suffix;

	if ($rc != 0) {
	    	$state->log("Bad signature");
		return 0;
	}
	if (!grep 
	    {ref($_) eq 'Regexp' ? $signer =~ $_ : $_ eq $signer} 
	    @{$state->signer_list}) {
		$state->errsay("Package signed by untrusted party #1", $signer);
		return 0;
	}
	return 1;
}

1;
