#!/usr/bin/perl -w

# Test comment

use strict;
use Getopt::Long;
use File::Copy;

my $BASEDIR    = `pwd`; chomp($BASEDIR);
my $SRCDIR     = "src";
my $CONFIGDIR  = "config";
my $OVERLAYDIR = "overlays";
my $IMAGEDIR   = "image";

if (! -d $SRCDIR)     { mkdir $SRCDIR; }
if (! -d $IMAGEDIR)   { mkdir $IMAGEDIR; }

# List of packages to build and include in image
my @packages;

my %kernel;
$kernel{package_type}   = "tarball";
$kernel{version}	= "3.13.6";
$kernel{basename}	= "linux-$kernel{version}";
$kernel{tarball}	= "$kernel{basename}.tar.gz";
$kernel{url}		= "http://www.kernel.org/pub/linux/kernel/v3.x/$kernel{tarball}";
push(@packages, \%kernel);

my %busybox;
$busybox{package_type}  = "tarball";
$busybox{version}	= "1.22.1";
$busybox{basename}	= "busybox-$busybox{version}";
$busybox{tarball}	= "$busybox{basename}.tar.bz2";
$busybox{url}		= "http://www.busybox.net/downloads/$busybox{tarball}";
push(@packages, \%busybox);

my %dropbear;
$dropbear{package_type} = "tarball";
$dropbear{version}	= "2014.63";
$dropbear{basename}	= "dropbear-$dropbear{version}";
$dropbear{tarball}	= "$dropbear{basename}.tar.bz2";
$dropbear{url}		= "http://matt.ucc.asn.au/dropbear/releases/$dropbear{tarball}";
push(@packages, \%dropbear);

my %slurm;
$slurm{package_type}    = "tarball";
$slurm{version}		= "2.6.7";
$slurm{basename}	= "slurm-$slurm{version}";
$slurm{tarball}		= "$slurm{basename}.tar.bz2";
$slurm{url}		= "http://www.schedmd.com/download/archive/$slurm{tarball}";
push(@packages, \%slurm);

my %mpich;
$mpich{package_type}	= "tarball";
$mpich{version}		= "3.1";
$mpich{basename}	= "mpich-$mpich{version}";
$mpich{tarball}		= "$mpich{basename}.tar.gz";
$mpich{url}		= "http://www.mcs.anl.gov/research/projects/mpich2/downloads/tarballs/$mpich{version}/$mpich{tarball}";
push(@packages, \%mpich);

my %ompi;
$ompi{package_type}	= "tarball";
$ompi{version}		= "1.7.4";
$ompi{basename}		= "openmpi-$ompi{version}";
$ompi{tarball}		= "$ompi{basename}.tar.bz2";
$ompi{url}		= "http://www.open-mpi.org/software/ompi/v1.7/downloads//$ompi{tarball}";
push(@packages, \%ompi);

my %libhugetlbfs;
$libhugetlbfs{package_type} = "tarball";
$libhugetlbfs{version}	= "2.17";
$libhugetlbfs{basename}	= "libhugetlbfs-$libhugetlbfs{version}";
$libhugetlbfs{tarball}	= "$libhugetlbfs{basename}.tar.gz";
$libhugetlbfs{url}	= "http://sourceforge.net/projects/libhugetlbfs/files/libhugetlbfs/$libhugetlbfs{version}/$libhugetlbfs{tarball}/download";
push(@packages, \%libhugetlbfs);

my %numactl;
$numactl{package_type}  = "tarball";
$numactl{version}	= "2.0.9";
$numactl{basename}	= "numactl-$numactl{version}";
$numactl{tarball}	= "$numactl{basename}.tar.gz";
$numactl{url}		= "ftp://oss.sgi.com/www/projects/libnuma/download/$numactl{tarball}";
push(@packages, \%numactl);

my %hwloc;
$hwloc{package_type}    = "tarball";
$hwloc{version}		= "1.8";
$hwloc{basename}	= "hwloc-$hwloc{version}";
$hwloc{tarball}		= "$hwloc{basename}.tar.gz";
$hwloc{url}		= "http://www.open-mpi.org/software/hwloc/v$hwloc{version}/downloads/$hwloc{tarball}";
push(@packages, \%hwloc);

my %kvm;
$kvm{package_type}      = "tarball";
$kvm{version}		= "1.7.0";
$kvm{basename}		= "qemu-$kvm{version}";
$kvm{tarball}		= "$kvm{basename}.tar.bz2";
$kvm{url}		= "http://wiki.qemu-project.org/download/$kvm{tarball}";
push(@packages, \%kvm);

my %libvirt;
$libvirt{package_type}  = "tarball";
$libvirt{version}	= "1.2.2";
$libvirt{basename}	= "libvirt-$libvirt{version}";
$libvirt{tarball}	= "$libvirt{basename}.tar.gz";
$libvirt{url}		= "http://libvirt.org/sources/$libvirt{tarball}";
push(@packages, \%libvirt);

my %palacios;
$palacios{package_type} = "git";
$palacios{basename}	= "palacios";
$palacios{clone_cmd}	= "git clone ssh://ktpedre\@newskysaw.cs.northwestern.edu//home/palacios/palacios.releases $palacios{basename}";
$palacios{branch_cmd}	= "git checkout --track -b Release-1.3 origin/Release-1.3";
push(@packages, \%palacios);


my %program_args = (
	build_kernel		=> 0,
	build_busybox		=> 0,
	build_dropbear		=> 0,
	build_slurm		=> 0,
	build_mpich		=> 0,
	build_ompi		=> 0,
	build_libhugetlbfs	=> 0,
	build_numactl		=> 0,
	build_hwloc		=> 0,
	build_kvm		=> 0,
	build_libvirt		=> 0,
	build_palacios		=> 0,
	build_image		=> 0
);

if ($#ARGV == -1) {
	usage();
	exit(1);
}

GetOptions(
	"help"			=> \&usage,
	"build-kernel"		=> sub { $program_args{'build_kernel'} = 1; },
	"build-busybox"		=> sub { $program_args{'build_busybox'} = 1; },
	"build-dropbear"	=> sub { $program_args{'build_dropbear'} = 1; },
	"build-slurm"		=> sub { $program_args{'build_slurm'} = 1; },
	"build-mpich"		=> sub { $program_args{'build_mpich'} = 1; },
	"build-ompi"		=> sub { $program_args{'build_ompi'} = 1; },
	"build-libhugetlbfs"	=> sub { $program_args{'build_libhugetlbfs'} = 1; },
	"build-numactl"		=> sub { $program_args{'build_numactl'} = 1; },
	"build-hwloc"		=> sub { $program_args{'build_hwloc'} = 1; },
	"build-kvm"		=> sub { $program_args{'build_kvm'} = 1; },
	"build-libvirt"		=> sub { $program_args{'build_libvirt'} = 1; },
	"build-palacios"	=> sub { $program_args{'build_palacios'} = 1; },
	"build-image"		=> sub { $program_args{'build_image'} = 1; },
	"<>"			=> sub { usage(); exit(1); }
);

sub usage {
	print <<"EOT";
Usage: build.pl [OPTIONS...]
EOT
}

# Scans given directory and copies over library dependancies 
sub copy_libs {
    my $directory = shift;
    my %library_list;

    foreach my $file (`find $directory -type f -exec file {} \\;`) {
        if ($file =~ /(\S+):/) {
            foreach my $ldd_file (`ldd $1 2> /dev/null \n`) {
                if ($ldd_file =~ /(\/\S+\/)(\S+\.so\S*) \(0x/) {
                    my $lib = "$1$2";
                    $library_list{$lib} = $1;
                    while (my $newfile = readlink($lib)) {
                        $lib =~ m/(.*)\/(.*)/;
                        my $dir = "$1";
                        if ($newfile =~ /^\//) {
                            $lib = $newfile;
                        }
                        else {
                            $lib = "$dir/$newfile";
                        }
                        $lib =~ m/(.*)\/(.*)/;
                        $library_list{"$1\/$2"} = $1;    # store in the library
                    }
                }
            }
        }
    }

    foreach my $file (sort keys %library_list) {
        # Do not copy libraries that will be nfs mounted
        next if $file =~ /^\/cluster_tools/;

        # Do not copy over libraries that already exist in the image
        next if -e "$directory/$file";

        # Create the target directory in the image, if necessary
        if (!-e "$directory/$library_list{$file}") {
            my $tmp = "$directory/$library_list{$file}";
            if ($tmp =~ s#(.*/)(.*)#$1#) {
                system("mkdir -p $tmp") == 0 or die "failed to create $tmp";
            }
        }

        # Copy library to the image
        system("rsync -a $file $directory/$library_list{$file}/") == 0
          or die "failed to copy list{$file}";
    }
}

# Download any missing package tarballs and repositories
for (my $i=0; $i < @packages; $i++) {
	my %pkg = %{$packages[$i]};
	if ($pkg{package_type} eq "tarball") {
		if (! -e "$SRCDIR/$pkg{tarball}") {
			print "CNL: Downloading $pkg{tarball}\n";
			system ("wget --directory-prefix=$SRCDIR $pkg{url} -O $SRCDIR\/$pkg{tarball}");
		}
	} elsif ($pkg{package_type} eq "git") {
		chdir "$SRCDIR" or die;
		if (! -e "$pkg{basename}") {
			system ($pkg{clone_cmd});
			if ($pkg{branch_cmd}) {
				print "got here";
				chdir "$pkg{basename}" or die;
				system ($pkg{branch_cmd});
				chdir "..";
			}
		}
		chdir "$BASEDIR" or die;
	} else {
		die "Unknown package_type=$pkg{package_type}";
	}
}

# Unpack each downloaded tarball
for (my $i=0; $i < @packages; $i++) {
	my %pkg = %{$packages[$i]};
	next if $pkg{package_type} ne "tarball";

	if (! -d "$SRCDIR/$pkg{basename}") {
		print "CNL: Unpacking $pkg{tarball}\n";
		if ($pkg{tarball} =~ m/tar\.gz/) {
			system ("tar --directory $SRCDIR -zxvf $SRCDIR/$pkg{tarball}");
		} elsif ($pkg{tarball} =~ m/tar\.bz2/) {
			system ("tar --directory $SRCDIR -jxvf $SRCDIR/$pkg{tarball}");
		} else {
			die "Unknown tarball type: $pkg{basename}";
		}
	}
}

# Build Linux Kernel
if ($program_args{build_kernel}) {
	print "CNL: Building Linux Kernel $kernel{basename}\n";
	chdir "$SRCDIR/$kernel{basename}" or die;
	if (-e ".config") {
		print "CNL: Aready configured, skipping copy of default .config\n";
	} else {
		print "CNL: Using default .config\n";
		copy "$BASEDIR/$CONFIGDIR/linux_config", ".config" or die;
		system "make oldconfig";
	}
	system "make -j 4 bzImage";
	chdir "$BASEDIR" or die;
}

# Build Busybox
if ($program_args{build_busybox}) {
	print "CNL: Building Busybox $busybox{basename}\n";
	chdir "$SRCDIR/$busybox{basename}" or die;
	if (-e ".config") {
		print "CNL: Aready configured, skipping copy of default .config\n";
	} else {
		print "CNL: Using default .config\n";
		copy "$BASEDIR/$CONFIGDIR/busybox_config", ".config" or die;
		system "make oldconfig";
	}
	system "make";
	system "make install";
	chdir "$BASEDIR" or die;
}

# Build Dropbear
if ($program_args{build_dropbear}) {
	print "CNL: Building Dropbear $dropbear{basename}\n";
	chdir "$SRCDIR/$dropbear{basename}" or die;
	system "./configure --prefix=/";
	system "make PROGRAMS=\"dropbear dbclient dropbearkey dropbearconvert scp\" MULTI=1";
	chdir "$BASEDIR" or die;
}

# Build SLURM
if ($program_args{build_slurm}) {
	print "CNL: Building SLURM $slurm{basename}\n";
	chdir "$SRCDIR/$slurm{basename}" or die;
	system "./configure --enable-debug --with-ssl --prefix=$BASEDIR/$SRCDIR/slurm-install";
	system "make";
	system "make install";
	system "cp -R $BASEDIR/$CONFIGDIR/slurm_config/etc $BASEDIR/$SRCDIR/slurm-install/";
	chdir "$BASEDIR" or die;
}

# Build MPICH2
if ($program_args{build_mpich}) {
	print "CNL: Building MPICH2 $mpich{basename}\n";
	chdir "$SRCDIR/$mpich{basename}" or die;
	system "LD_LIBRARY_PATH=$BASEDIR/$SRCDIR/slurm-install/lib ./configure --enable-fast --with-pmi=slurm --with-pm=no --with-slurm=$BASEDIR/$SRCDIR/slurm-install/ --prefix=$BASEDIR/$SRCDIR/$mpich{basename}-install";
	system "make";
	system "make install";
	chdir "$BASEDIR" or die;
}

# Build OpenMPI
if ($program_args{build_ompi}) {
	print "CNL: Building OpenMPI $ompi{basename}\n";
	chdir "$SRCDIR/$ompi{basename}" or die;
	system "LD_LIBRARY_PATH=$BASEDIR/$SRCDIR/slurm-install/lib ./configure --with-slurm=$BASEDIR/$SRCDIR/slurm-install/ --with-pmi=$BASEDIR/$SRCDIR/slurm-install/ --prefix=$BASEDIR/$SRCDIR/$ompi{basename}-install --with-xpmem=/cluster_tools/lw_linux_standalone/src/xpmem-install";
	system "make";
	system "make install";
	chdir "$BASEDIR" or die;
}

# Build libhugetlbfs
if ($program_args{build_libhugetlbfs}) {
	print "CNL: Building libhugetlbfs $libhugetlbfs{basename}\n";
	chdir "$SRCDIR/$libhugetlbfs{basename}" or die;
	system "rm -rf ./_install";
	system "BUILDTYPE=NATIVEONLY make";
	system "BUILDTYPE=NATIVEONLY make install DESTDIR=$BASEDIR/$SRCDIR/$libhugetlbfs{basename}/_install";
	chdir "$BASEDIR" or die;
}

# Build numactl
if ($program_args{build_numactl}) {
	print "CNL: Building numactl $numactl{basename}\n";
	chdir "$SRCDIR/$numactl{basename}" or die;
	my $DESTDIR = "$BASEDIR/$SRCDIR/$numactl{basename}/_install/usr";
	$DESTDIR =~ s/\//\\\//g;
	system "sed '/^prefix/s/\\/usr/$DESTDIR/' Makefile > Makefile.cnl";
	system "mv Makefile Makefile.orig";
	system "cp Makefile.cnl Makefile";
	system "make";
	system "make install";
	system "mv Makefile.orig Makefile";
	system "rm -rf ./_install/share";  # don't need manpages
	chdir "$BASEDIR" or die;
}

# Build hwloc
if ($program_args{build_hwloc}) {
	print "CNL: Building hwloc $hwloc{basename}\n";
	chdir "$SRCDIR/$hwloc{basename}" or die;
	system "rm -rf ./_install";
	system "./configure --prefix=/usr";
	system "make";
	system "make install DESTDIR=$BASEDIR/$SRCDIR/$hwloc{basename}/_install";
	chdir "$BASEDIR" or die;
}

# Build KVM
if ($program_args{build_kvm}) {
	print "CNL: Building kvm $kvm{basename}\n";
	chdir "$SRCDIR/$kvm{basename}" or die;
	system "rm -rf ./_install";
	system "./configure --prefix=/usr";
	system "make";
	system "make install DESTDIR=$BASEDIR/$SRCDIR/$kvm{basename}/_install";
	system "cp -R $BASEDIR/$CONFIGDIR/kvm_config/etc $BASEDIR/$SRCDIR/$kvm{basename}/_install/";
	chdir "$BASEDIR" or die;
}

# Build libvirt
if ($program_args{build_libvirt}) {
	print "CNL: Building libvirt $libvirt{basename}\n";
	chdir "$SRCDIR/$libvirt{basename}" or die;
	system "rm -rf ./_install";
	system "./configure --prefix=/usr --disable-static --without-selinux --without-python --without-lxc --without-openvz --without-uml";
	system "make";
	system "make install DESTDIR=$BASEDIR/$SRCDIR/$libvirt{basename}/_install";
	system "rm -rf ./_install/share"; # don't need manpages
	system "rm ./_install/etc/libvirt/qemu/networks/autostart/default.xml"; # don't start virbr0 interface automatically
	chdir "$BASEDIR" or die;
}

# Build Palacios
if ($program_args{build_palacios}) {
	print "CNL: Building $palacios{basename}\n";
	chdir "$SRCDIR/$palacios{basename}" or die;
	if (-e ".config") {
		print "CNL: Aready configured, skipping copy of default .config\n";
	} else {
		print "CNL: Using default .config\n";
		copy "$BASEDIR/$CONFIGDIR/palacios_config", ".config" or die;
		my $kernel_path = "\"$BASEDIR/$SRCDIR/$kernel{basename}\"";
		# perl foo to set the correct linux kernel path in the palacios config file
		system "/usr/bin/perl -p -i -e \"s/#LINUX_KERNEL_PATH#/\Q$kernel_path\E/g\" .config";
		system "make oldconfig";
	}

	# Build palacios library and linux modules
	system "make clean";
	system "make";

	# Build palacios linux user-level utilities
	chdir "linux_usr" or die;
	system "make clean";
	system "make";

	chdir "$BASEDIR" or die;
}

##############################################################################
# Build Initramfs Image
##############################################################################
if ($program_args{build_image}) {
	system "rm -rf $IMAGEDIR/*";

	# Create some directories needed for stuff
	system "mkdir -p $IMAGEDIR/etc";

	# Busybox
	system "cp -R $SRCDIR/$busybox{basename}/_install/* $IMAGEDIR/";
	system "ln -sf /bin/busybox $IMAGEDIR/bin/cut";
	system "ln -sf /bin/busybox $IMAGEDIR/bin/env";
	system "ln -sf /bin/gawk $IMAGEDIR/bin/awk";
	system "cp $IMAGEDIR/bin/busybox $IMAGEDIR/bin/busybox_root";
	system "ln -sf /bin/busybox_root $IMAGEDIR/bin/su";
	system "ln -sf /bin/busybox_root $IMAGEDIR/bin/ping";
	system "ln -sf /bin/busybox_root $IMAGEDIR/bin/ping6";
	system "ln -sf /bin/busybox_root $IMAGEDIR/usr/bin/traceroute";
	system "ln -sf /bin/busybox_root $IMAGEDIR/usr/bin/traceroute6";

	# Dropbear
	system "cp $SRCDIR/$dropbear{basename}/dropbearmulti $IMAGEDIR/bin";
	system "cp -R $CONFIGDIR/dropbear_files/etc/dropbear $IMAGEDIR/etc";
	chdir  "$BASEDIR/$IMAGEDIR/bin" or die;
	system "ln -s dropbearmulti dropbearconvert";
	system "ln -s dropbearmulti dropbearkey";
	system "ln -s dropbearmulti scp";
	system "ln -s dropbearmulti ssh";
	chdir  "$BASEDIR/$IMAGEDIR/sbin" or die;
	system "ln -s ../bin/dropbearmulti dropbear";
	chdir  "$BASEDIR/$IMAGEDIR/usr/bin" or die;
	system "ln -s ../../bin/dropbearmulti dbclient";
	chdir  "$BASEDIR" or die;

	# Use rsync to merge in skeleton overlay
	system("rsync -a $OVERLAYDIR/skel/\* $IMAGEDIR/") == 0
		or die "Failed to rsync skeleton directory to $IMAGEDIR";	

	# Use rsync to merge in gato overlay
	system("rsync -a $OVERLAYDIR/gato/\* $IMAGEDIR/") == 0
		or die "Failed to rsync gato directory to $IMAGEDIR";	

	# Install numactl into image
	system("rsync -a $SRCDIR/$numactl{basename}/_install/\* $IMAGEDIR/") == 0
		or die "Failed to rsync numactl to $IMAGEDIR";

	# Install hwloc into image
	system("rsync -a $SRCDIR/$hwloc{basename}/_install/\* $IMAGEDIR/") == 0
		or die "Failed to rsync hwloc to $IMAGEDIR";

	# Install libhugetlbfs into image
	system("rsync -a $SRCDIR/$libhugetlbfs{basename}/_install/\* $IMAGEDIR/") == 0
		or die "Failed to rsync libhugetlbfs to $IMAGEDIR";

	# Files copied from build host
	system "cp /etc/localtime $IMAGEDIR/etc";
	system "cp /lib/libnss_files.so.* $IMAGEDIR/lib";
	system "cp /lib64/libnss_files.so.* $IMAGEDIR/lib64";

	# Find and copy all shared library dependencies
	copy_libs($IMAGEDIR);

	# Build the guest initramfs image, initramfs_guest.gz
	# Fixup permissions, need to copy everything to a tmp directory
	system "cp -R $IMAGEDIR $IMAGEDIR\_tmp";
	system "sudo chown -R root.root $IMAGEDIR\_tmp";
	system "sudo chmod +s $IMAGEDIR\_tmp/bin/busybox_root";
	system "sudo chmod 777 $IMAGEDIR\_tmp/tmp";
	system "sudo chmod +t $IMAGEDIR\_tmp/tmp";
	chdir  "$IMAGEDIR\_tmp" or die;
	system "sudo find . | sudo cpio -H newc -o > $BASEDIR/initramfs_guest.cpio";
	chdir  "$BASEDIR" or die;
	system "cat initramfs_guest.cpio | gzip > initramfs_guest.gz";
	system "rm initramfs_guest.cpio";
	system "sudo rm -rf $IMAGEDIR\_tmp";

	# Install KVM and related files to image
	system("rsync -a $SRCDIR/$kvm{basename}/_install/\* $IMAGEDIR/") == 0
		or die "Failed to rsync KVM to $IMAGEDIR";

	# Install libvirt and related files to image
	system("rsync -a $SRCDIR/$libvirt{basename}/_install/\* $IMAGEDIR/") == 0
		or die "Failed to rsync libvirt to $IMAGEDIR";

#	# Install palacios kernel module and related utilities to image
#	system "cp $SRCDIR/$palacios{basename}/v3vee.ko $IMAGEDIR/";
#	foreach my $file (<$BASEDIR/$SRCDIR/$palacios{basename}/linux_usr/*>) {
#		if (-X $file) {
#			system "cp $file $IMAGEDIR/usr/bin";
#		}
#	}

	# Copy mirror of host boot image to image,
	# can be used to start a vm that is a mirror of the host
	system "mkdir -p $IMAGEDIR/opt/vms/host_mirror";
	system "cp $SRCDIR/$kernel{basename}/arch/x86/boot/bzImage $IMAGEDIR/opt/vms/host_mirror";
	system "cp initramfs_guest.gz $IMAGEDIR/opt/vms/host_mirror/initramfs.gz";
	system "cp $CONFIGDIR/host_mirror/* $IMAGEDIR/opt/vms/host_mirror/";

	# Find and copy all shared library dependencies
	copy_libs($IMAGEDIR);

	# Build the guest initramfs image
	# Fixup permissions, need to copy everything to a tmp directory
	system "cp -R $IMAGEDIR $IMAGEDIR\_tmp";
	system "sudo chown -R root.root $IMAGEDIR\_tmp";
	system "sudo chmod +s $IMAGEDIR\_tmp/bin/busybox_root";
	system "sudo chmod 777 $IMAGEDIR\_tmp/tmp";
	system "sudo chmod +t $IMAGEDIR\_tmp/tmp";
	chdir  "$IMAGEDIR\_tmp" or die;
	system "sudo find . | sudo cpio -H newc -o > $BASEDIR/initramfs.cpio";
	chdir  "$BASEDIR" or die;
	system "cat initramfs.cpio | gzip > initramfs.gz";
	system "rm initramfs.cpio";
	system "sudo rm -rf $IMAGEDIR\_tmp";
}
