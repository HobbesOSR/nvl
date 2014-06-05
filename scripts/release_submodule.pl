#!/usr/bin/perl -w

$#ARGV==0 or die "usage: release_submodule.pl kitten|palacios\nThis command should be run from the NVL root directory only.\nOnly Kitten or Palacios developers should run it.\nIt forwards the *normal developer*'s view of the submodule\nto *your* current head.\nYou should commit and push your submodule changes first!\n\n";

$which=shift;

if ($which eq "palacios") {
  doit("palacios","src/nvl");
} elsif ($which eq "kitten") { 
  doit("kitten","src/nvl");
} else {
  print "Don't know how to handle $which\n";
}

sub doit {
  my $which = shift;
  my $dir = shift;

  print "This will advance the normal NVL developers view of $which\n";	  print "to your current devel head.\n\nAre you sure? (y/n) : ";
  $yn=<STDIN>; chomp($yn);
  if ($yn eq "y") { 
     print "Running a commit of the submodule and a push.\n";
     system "cd src/nvl && git add $which && git commit $which && git push";
     print "Done.\n";
  } else {
     print "Aborted.\n";
  }
}

