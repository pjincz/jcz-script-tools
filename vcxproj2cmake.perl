#!/usr/bin/perl
$fname = $ARGV[0];
open(fin, $fname);
@cont = <fin>;
close(fin);
@cfgs = ();
foreach(@cont)
{
	if(/<Configuration>(.*)<\/Configuration>/)
	{
		push @cfgs, $1;
	}
}
$i = 0;
foreach(@cfgs)
{
	printf("$i: $cfgs[$i]\n");
	$i = $i + 1;
}
printf("Choose a config as convert config:");
$c = <STDIN>;
$cfg = $cfgs[$c];
@settings = ();
$collect = 0;
foreach(@cont)
{
	if(/<ItemDefinitionGroup.*$cfg\|Win32/)
	{
		$collect = 1;
	}
	elsif(/<\/ItemDefinitionGroup>/)
	{
		$collect = 0;
	}
	elsif($collect)
	{
		print $_;
#		if(/<AdditionalOptions>(.*)</)
#		{
#		}
	}
}
