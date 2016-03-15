#!/usr/bin/perl -w

use strict;
use Getopt::Long;
use File::Copy;

my $BASEDIR    = `pwd`; chomp($BASEDIR);
my $SRCDIR     = "src";
my $CONFIGDIR  = "config";
my $OVERLAYDIR = "overlays";
my $IMAGEDIR   = "image";
my $ISOLINUX   = "\$(find /usr/ -name isolinux.bin)";
my $LDLINUX    = "\$(find /usr/ -name ldlinux.c32)";
    
if (! -d $SRCDIR)     { mkdir $SRCDIR; }
if (! -d $IMAGEDIR)   { mkdir $IMAGEDIR; }

# List of packages to build and include in image
my @packages;

my %kernel;
$kernel{package_type}   = "tarball";
$kernel{version}	= "3.19.8";
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

my %libhugetlbfs;
$libhugetlbfs{package_type} = "git";
$libhugetlbfs{basename}	= "libhugetlbfs";
$libhugetlbfs{clone_cmd}[0] = "git clone https://github.com/libhugetlbfs/libhugetlbfs.git";
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

my %ofed;
$ofed{package_type}	= "tarball";
$ofed{version}		= "3.12-1";
$ofed{basename}		= "OFED-$ofed{version}-rc2";
$ofed{tarball}		= "$ofed{basename}.tgz";
$ofed{url}		= "http://downloads.openfabrics.org/downloads/OFED/ofed-$ofed{version}/$ofed{tarball}";
push(@packages, \%ofed);

my %ompi;
$ompi{package_type}	= "tarball";
$ompi{version}		= "1.10.2";
$ompi{basename}		= "openmpi-$ompi{version}";
$ompi{tarball}		= "$ompi{basename}.tar.bz2";
$ompi{url}		= "http://www.open-mpi.org/software/ompi/v1.10/downloads/$ompi{tarball}";
push(@packages, \%ompi);

my %pisces;
$pisces{package_type}	= "git";
$pisces{basename}	= "pisces";
$pisces{src_subdir}	= "pisces";
$pisces{clone_cmd}[0]	= "git clone http://essex.cs.pitt.edu/git/pisces.git";
$pisces{clone_cmd}[1]	= "git clone http://essex.cs.pitt.edu/git/petlib.git";
$pisces{clone_cmd}[2]	= "git clone http://essex.cs.pitt.edu/git/xpmem.git";
$pisces{clone_cmd}[3]	= "git clone https://github.com/hobbesosr/kitten";
$pisces{clone_cmd}[4]	= "git clone http://essex.cs.pitt.edu/git/palacios.git";
$pisces{clone_cmd}[5]	= "git clone http://essex.cs.pitt.edu/git/hobbes.git";
push(@packages, \%pisces);

my %curl;
$curl{package_type}	= "tarball";
$curl{version}		= "7.47.1";
$curl{basename}		= "curl-$curl{version}";
$curl{tarball}		= "$curl{basename}.tar.bz2";
$curl{url}		= "https://curl.haxx.se/download/$curl{tarball}";
push(@packages, \%curl);

my %hdf5;
$hdf5{package_type}	= "tarball";
$hdf5{version}		= "1.8.16";
$hdf5{basename}		= "hdf5-$hdf5{version}";
$hdf5{tarball}		= "$hdf5{basename}.tar.bz2";
$hdf5{url}		= "https://www.hdfgroup.org/ftp/HDF5/current/src/$hdf5{tarball}";
push(@packages, \%hdf5);

my %netcdf;
$netcdf{package_type}	= "tarball";
$netcdf{version}	= "4.4.0";
$netcdf{basename}	= "netcdf-$netcdf{version}";
$netcdf{tarball}	= "$netcdf{basename}.tar.gz";
$netcdf{url}		= "ftp://ftp.unidata.ucar.edu/pub/netcdf/$netcdf{tarball}";
push(@packages, \%netcdf);

my %dtk;
$dtk{package_type}	= "git";
$dtk{basename}		= "Trilinos";
$dtk{src_subdir}	= "dtk";
$dtk{clone_cmd}[0]	= "git clone https://github.com/trilinos/Trilinos.git";
$dtk{clone_cmd}[1]	= "git clone https://github.com/ORNL-CEES/DataTransferKit.git";
$dtk{clone_cmd}[2]	= "git clone https://github.com/ORNL-CEES/DTKData.git";
push(@packages, \%dtk);




my %program_args = (
	build_kernel		=> 0,
	build_busybox		=> 0,
	build_dropbear		=> 0,
	build_libhugetlbfs	=> 0,
	build_numactl		=> 0,
	build_hwloc		=> 0,
	build_ofed		=> 0,
	build_ompi		=> 0,
	build_pisces		=> 0,
	build_curl		=> 0,
	build_hdf5		=> 0,
	build_netcdf		=> 0,
	build_dtk		=> 0,

	build_image		=> 0,
	build_isoimage		=> 0,
	build_nvl_guest		=> 0
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
	"build-libhugetlbfs"	=> sub { $program_args{'build_libhugetlbfs'} = 1; },
	"build-numactl"		=> sub { $program_args{'build_numactl'} = 1; },
	"build-hwloc"		=> sub { $program_args{'build_hwloc'} = 1; },
	"build-ofed"		=> sub { $program_args{'build_ofed'} = 1; },
	"build-ompi"		=> sub { $program_args{'build_ompi'} = 1; },
	"build-pisces"		=> sub { $program_args{'build_pisces'} = 1; },
	"build-curl"		=> sub { $program_args{'build_curl'} = 1; },
	"build-hdf5"		=> sub { $program_args{'build_hdf5'} = 1; },
	"build-netcdf"		=> sub { $program_args{'build_netcdf'} = 1; },
	"build-dtk"		=> sub { $program_args{'build_dtk'} = 1; },
	"build-image"		=> sub { $program_args{'build_image'} = 1; },
	"build-isoimage"        => sub { $program_args{'build_isoimage'} = 1; },
	"build-nvl-guest"	=> sub { $program_args{'build_nvl_guest'} = 1; },
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
			system ("wget --no-check-certificate --directory-prefix=$SRCDIR $pkg{url} -O $SRCDIR\/$pkg{tarball}") == 0 
                          or die "failed to wget $pkg{tarball}";
		}
	} elsif ($pkg{package_type} eq "git") {
		chdir "$SRCDIR" or die;
		if ($pkg{src_subdir}) {
			if (! -e "$pkg{src_subdir}") {
				mkdir $pkg{src_subdir} or die;
			}
			chdir $pkg{src_subdir} or die;
		}
		if (! -e "$pkg{basename}") {
			for (my $j=0; $j < @{$pkg{clone_cmd}}; $j++) {
				print "CNL: Running command '$pkg{clone_cmd}[$j]'\n";
				system ($pkg{clone_cmd}[$j]) == 0
                                  or die "failed to clone $pkg{clone_cmd}[$j]";
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
			system ("tar --directory $SRCDIR -zxvf $SRCDIR/$pkg{tarball} >/dev/null") == 0
                          or die "failed to unpack tar $pkg{tarball}";
		} elsif ($pkg{tarball} =~ m/tar\.bz2/) {
			system ("tar --directory $SRCDIR -jxvf $SRCDIR/$pkg{tarball} >/dev/null") == 0
                          or die "failed to unpack tar $pkg{tarball}";
		} elsif ($pkg{tarball} =~ m/tgz/) {
			system ("tar --directory $SRCDIR -zxvf $SRCDIR/$pkg{tarball} >/dev/null") == 0
                          or die "failed to unpack tar $pkg{tarball}";
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
		system ("make oldconfig") == 0 or die "failed to make oldconfig";
	}
	system ("make -j 4 bzImage modules") == 0 or die "failed to make bzImage or modules";
	system ("INSTALL_MOD_PATH=$BASEDIR/$SRCDIR/$kernel{basename}/_install/ make modules_install") == 0 or die "failed to make modules_install";
#	system ("sudo make modules_install") == 0 or die "failed to make modules_install";
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
		system ("make oldconfig") == 0 or die "failed to make oldconfig";
	}
	system ("make") == 0 or die "failed to make";
	system ("make install") == 0 or die "failed to install";
	chdir "$BASEDIR" or die;
}

# Build Dropbear
if ($program_args{build_dropbear}) {
	print "CNL: Building Dropbear $dropbear{basename}\n";
	chdir "$SRCDIR/$dropbear{basename}" or die;
	system ("./configure --prefix=/") == 0 or die "failed to configure";
	system ("make PROGRAMS=\"dropbear dbclient dropbearkey dropbearconvert scp\" MULTI=1") == 0 or die
          "failed to make";
	chdir "$BASEDIR" or die;
}

# Build libhugetlbfs
if ($program_args{build_libhugetlbfs}) {
	print "CNL: Building libhugetlbfs $libhugetlbfs{basename}\n";
	chdir "$SRCDIR/$libhugetlbfs{basename}" or die;
	system ("rm -rf ./_install") == 0 or die;
	system ("BUILDTYPE=NATIVEONLY make") == 0 or die "failed to make";
	system ("BUILDTYPE=NATIVEONLY make install DESTDIR=$BASEDIR/$SRCDIR/$libhugetlbfs{basename}/_install") == 0
          or die "failed to install";
	chdir "$BASEDIR" or die;
}

# Build numactl
if ($program_args{build_numactl}) {
	print "CNL: Building numactl $numactl{basename}\n";
	chdir "$SRCDIR/$numactl{basename}" or die;
	my $DESTDIR = "$BASEDIR/$SRCDIR/$numactl{basename}/_install/usr";
	$DESTDIR =~ s/\//\\\//g;
	system ("sed '/^prefix/s/\\/usr/$DESTDIR/' Makefile > Makefile.cnl") == 0 or die;
	system ("mv Makefile Makefile.orig") == 0 or die;
	system ("cp Makefile.cnl Makefile") == 0 or die;
	system ("make") == 0 or die "failed to make";
	system ("make install") == 0 or die "failed to install";
	system ("mv Makefile.orig Makefile") == 0 or die;
	system ("rm -rf ./_install/share") == 0 or die;  # don't need manpages
	chdir "$BASEDIR" or die;
}

# Build hwloc
if ($program_args{build_hwloc}) {
	print "CNL: Building hwloc $hwloc{basename}\n";
	chdir "$SRCDIR/$hwloc{basename}" or die;
	system ("rm -rf ./_install") == 0 or die;
	system ("./configure --prefix=/usr --enable-static --disable-shared") == 0 or die "failed to configure";
	system ("make") == 0 or die "failed to make";
	system ("make install DESTDIR=$BASEDIR/$SRCDIR/$hwloc{basename}/_install") == 0
	  or die "failed ot install";
	chdir "$BASEDIR" or die;
}

# Build OFED
if ($program_args{build_ofed}) {
	print "CNL: Building OFED $ofed{basename}\n";
	chdir "$SRCDIR/$ofed{basename}" or die;
	system ("sudo ./install.pl --basic --without-depcheck --kernel-sources $BASEDIR/$SRCDIR/$kernel{basename} --kernel $kernel{version}") == 0
          or die "failed to install";
	chdir "$BASEDIR" or die;
}

# Build OpenMPI
if ($program_args{build_ompi}) {
	print "CNL: Building OpenMPI $ompi{basename}\n";
	chdir "$SRCDIR/$ompi{basename}" or die;
	# This is a horrible hack. We're installing OpenMPI into /opt on the host.
	# This means we need to be root to do a make install and will possibly screw up the host.
	# We should really be using chroot or something better.
	#system ("LD_LIBRARY_PATH=$BASEDIR/$SRCDIR/slurm-install/lib ./configure --prefix=/opt/$ompi{basename} --disable-shared --enable-static --with-verbs=yes") == 0
	system ("LDFLAGS=-static ./configure --prefix=/opt/simple_busybox/$ompi{basename} --disable-shared --enable-static --disable-dlopen --without-memory-manager --disable-vt") == 0
          or die "failed to configure";
	system ("make -j 4 LDFLAGS=-all-static") == 0 or die "failed to make";
	system ("make install") == 0 or die "failed to install";
	chdir "$BASEDIR" or die;
}

# Build Pisces
if ($program_args{build_pisces}) {
	print "CNL: Building Pisces\n";

	# Step 0: Add a convenience script to the base pisces dir that updates all pisces repos to the latest
	copy "$BASEDIR/$CONFIGDIR/pisces/update_pisces.sh", "$SRCDIR/$pisces{src_subdir}" or die;
	system ("chmod +x $SRCDIR/$pisces{src_subdir}/update_pisces.sh");

	# STEP 1: Configure and build Kitten... this will fail because
	# Palacios has not been built yet, but Palacios can't be built
	# until Kitten is configured in built. TODO: FIXME
	print "CNL: STEP 1: Building pisces/kitten stage 1\n";
	chdir "$SRCDIR/$pisces{src_subdir}/kitten" or die;
	if (-e ".config") {
		print "CNL: pisces/kitten aready configured, skipping copy of default .config\n";
	} else {
		print "CNL: pisces/kitten using default .config\n";
		copy "$BASEDIR/$CONFIGDIR/pisces/kitten_config", ".config" or die;
		system ("make oldconfig") == 0 or die "failed to make oldconfig";
	}
	system ("make"); # This will always fail
	chdir "$BASEDIR" or die;
	print "CNL: STEP 1: Done building pisces/kitten stage 1\n";

	# STEP 2: configure and build Palacios
	print "CNL: STEP 2: Building pisces/palacios\n";
	chdir "$SRCDIR/$pisces{src_subdir}/palacios" or die;
	if (-e ".config") {
		print "CNL: pisces/palacios aready configured, skipping copy of default .config\n";
	} else {
		print "CNL: pisces/palacios using default .config\n";
		copy "$BASEDIR/$CONFIGDIR/pisces/palacios_config", ".config" or die;
		system ("make oldconfig") == 0 or die "failed to make oldconfig";
	}
	system "make clean";
	system ("make") == 0 or die "failed to make";
	chdir "$BASEDIR" or die;
	print "CNL: STEP 2: Done building pisces/palacios\n";

	# STEP 3: Rebuild Kitten... this will now succeed since Palacios has been built.
	print "CNL: STEP 3: Building pisces/kitten stage 2\n";
	chdir "$SRCDIR/$pisces{src_subdir}/kitten" or die;
	system ("make") == 0 or die "failed to make";
	chdir "$BASEDIR" or die;
	print "CNL: STEP 3: Done building pisces/kitten stage 2\n";

	# STEP 4: Build petlib. Pisces depends on this.
	print "CNL: STEP 4: Building pisces/petlib\n";
	chdir "$SRCDIR/$pisces{src_subdir}/petlib" or die;
	system "make clean";
	system ("make") == 0 or die "failed to make";
	chdir "$BASEDIR" or die;
	print "CNL: STEP 4: Done building pisces/petlib\n";

	# STEP 5: Build XPMEM for host Linux. Pisces depends on this.
	print "CNL: STEP 5: Building pisces/xpmem\n";
	chdir "$SRCDIR/$pisces{src_subdir}/xpmem/mod" or die;
	system "PWD=$BASEDIR/$SRCDIR/$pisces{src_subdir}/xpmem/mod LINUX_KERN=$BASEDIR/$SRCDIR/$kernel{basename} make clean";
	system ("PWD=$BASEDIR/$SRCDIR/$pisces{src_subdir}/xpmem/mod LINUX_KERN=$BASEDIR/$SRCDIR/$kernel{basename} make") == 0
          or die "failed to make";
	chdir "$BASEDIR" or die;
	chdir "$SRCDIR/$pisces{src_subdir}/xpmem/lib" or die;
	system "PWD=$BASEDIR/$SRCDIR/$pisces{src_subdir}/xpmem/lib make clean";
	system ("PWD=$BASEDIR/$SRCDIR/$pisces{src_subdir}/xpmem/lib make") == 0 or die "failed to make";
	chdir "$BASEDIR" or die;
	print "CNL: STEP 5: Done building pisces/xpmem\n";

	# Step 6: Build Pisces for Kitten
	print "CNL: STEP 6: Building pisces/pisces\n";
	chdir "$SRCDIR/$pisces{src_subdir}/pisces" or die;
	system "PWD=$BASEDIR/$SRCDIR/$pisces{src_subdir}/pisces KERN_PATH=$BASEDIR/$SRCDIR/$kernel{basename} make clean XPMEM=y";
	system ("PWD=$BASEDIR/$SRCDIR/$pisces{src_subdir}/pisces KERN_PATH=$BASEDIR/$SRCDIR/$kernel{basename} make XPMEM=y") == 0
	  or die;
	chdir "$BASEDIR" or die;
	print "CNL: STEP 6: Done building pisces/pisces\n";

	# Step 7: Build WhiteDB
	print "CNL: STEP 7: Building pisces/hobbes/whitedb-0.7.3\n";
	chdir "$SRCDIR/$pisces{src_subdir}/hobbes/whitedb-0.7.3" or die;
	system ("autoreconf -fvi") == 0 or die "failed to autoreconf";
	system ("./configure --enable-locking=wpspin") == 0 or die "failed to configure";
	system ("make") == 0 or die "failed to make";
	chdir "$BASEDIR" or die;
	print "CNL: STEP 7: Done building pisces/hobbes/whitedb-0.7.3\n";

	# Step 8: Build libhobbes.a
	print "CNL: STEP 8: Building pisces/hobbes/libhobbes\n";
	chdir "$SRCDIR/$pisces{src_subdir}/hobbes/libhobbes" or die;
	system ("XPMEM_PATH=../../xpmem PALACIOS_PATH=../../palacios PISCES_PATH=../../pisces PETLIB_PATH=../../petlib WHITEDB_PATH=../whitedb-0.7.3 make clean") == 0 or die "failed to clean";
	system ("XPMEM_PATH=../../xpmem PALACIOS_PATH=../../palacios PISCES_PATH=../../pisces PETLIB_PATH=../../petlib WHITEDB_PATH=../whitedb-0.7.3 make") == 0 or die "failed to make";
	chdir "$BASEDIR" or die;
	print "CNL: STEP 8: Done building pisces/hobbes/libhobbes\n";

	# Step 9: Build libhobbes lnx_init
	print "CNL: STEP 9: Building pisces/hobbes/lnx_inittask/lnx_init\n";
	chdir "$SRCDIR/$pisces{src_subdir}/hobbes/lnx_inittask" or die;
	system ("XPMEM_PATH=../../xpmem PALACIOS_PATH=../../palacios PISCES_PATH=../../pisces PETLIB_PATH=../../petlib WHITEDB_PATH=../whitedb-0.7.3 make clean") == 0 or die "failed to clean";
	# this will allways die
	system ("XPMEM_PATH=../../xpmem PALACIOS_PATH=../../palacios PISCES_PATH=../../pisces PETLIB_PATH=../../petlib WHITEDB_PATH=../whitedb-0.7.3 make") == 0 or die;
	chdir "$BASEDIR" or die;
	print "CNL: STEP 9: Done building pisces/hobbes/lnx_inittask/lnx_init\n";

	# Step 10: Build libhobbes shell
	print "CNL: STEP 10: Building pisces/hobbes/shell\n";
	chdir "$SRCDIR/$pisces{src_subdir}/hobbes/shell" or die;
	system ("XPMEM_PATH=../../xpmem PALACIOS_PATH=../../palacios PISCES_PATH=../../pisces PETLIB_PATH=../../petlib WHITEDB_PATH=../whitedb-0.7.3 make clean") == 0 or die "failed to clean";
	system ("XPMEM_PATH=../../xpmem PALACIOS_PATH=../../palacios PISCES_PATH=../../pisces PETLIB_PATH=../../petlib WHITEDB_PATH=../whitedb-0.7.3 make") == 0 or die;
	chdir "$BASEDIR" or die;
	print "CNL: STEP 10: Done building pisces/hobbes/shell\n";

	# Step 11: Build Hobbes Kitten init_task
	print "CNL: STEP 11: Building pisces/hobbes/lwk_inittask\n";
	chdir "$SRCDIR/$pisces{src_subdir}/hobbes/lwk_inittask" or die;
	system "KITTEN_PATH=../../kitten XPMEM_PATH=../../xpmem PALACIOS_PATH=../../palacios PISCES_PATH=../../pisces PETLIB_PATH=../../petlib WHITEDB_PATH=../whitedb-0.7.3 make clean";
	system ("KITTEN_PATH=../../kitten XPMEM_PATH=../../xpmem PALACIOS_PATH=../../palacios PISCES_PATH=../../pisces PETLIB_PATH=../../petlib WHITEDB_PATH=../whitedb-0.7.3 make") == 0 or die "failed to make";
	chdir "$BASEDIR" or die;
	print "CNL: STEP 11: Done building pisces/hobbes/lwk_inittask\n";

	# Step 12: Build Hobbes PMI Hello Example App
	print "CNL: STEP 12: Building pisces/hobbes/examples/apps/pmi/test_pmi_hello\n";
	chdir "$SRCDIR/$pisces{src_subdir}/hobbes/examples/apps/pmi" or die;
	system ("make clean") == 0 or die "failed to clean";
	system ("make") == 0 or die "failed to make";
	chdir "$BASEDIR" or die;
	print "CNL: STEP 12: Done building pisces/hobbes/examples/apps/pmi/test_pmi_hello\n";
}


# Build Curl
if ($program_args{build_curl}) {
	print "CNL: Building Curl $curl{basename}\n";
	chdir "$SRCDIR/$curl{basename}" or die;
	# This is a horrible hack. We're installing into /opt/simple_busybox/install on the host.
	# This means we need to be root to do a make install and will possibly screw up the host.
	# We should really be using chroot or something better.
	system ("LDFLAGS=-static ./configure --prefix=/opt/simple_busybox/install --disable-shared --enable-static") == 0
          or die "failed to configure";
	system ("make V=1 curl_LDFLAGS=-all-static") == 0 or die "failed to make";
	system ("make install") == 0 or die "failed to install";
	chdir "$BASEDIR" or die;
}


# Build HDF5
if ($program_args{build_hdf5}) {
	print "CNL: Building HDF5 $hdf5{basename}\n";
	chdir "$SRCDIR/$hdf5{basename}" or die;
	# This is a horrible hack. We're installing into /opt/simple_busybox/install on the host.
	# This means we need to be root to do a make install and will possibly screw up the host.
	# We should really be using chroot or something better.
	system ("LDFLAGS=-static ./configure --prefix=/opt/simple_busybox/install --enable-parallel --disable-shared --enable-static") == 0
          or die "failed to configure";
	system ("make V=1 LDFLAGS=-all-static") == 0 or die "failed to make";
	system ("make install") == 0 or die "failed to install";
	chdir "$BASEDIR" or die;
}


# Build NetCDF
if ($program_args{build_netcdf}) {
	print "CNL: Building NetCDF $netcdf{basename}\n";
	chdir "$SRCDIR/$netcdf{basename}" or die;
	# This is a horrible hack. We're installing into /opt/simple_busybox/install on the host.
	# This means we need to be root to do a make install and will possibly screw up the host.
	# We should really be using chroot or something better.
	system ("LDFLAGS=-static ./configure --prefix=/opt/simple_busybox/install --disable-option-checking --with-hdf5=/opt/simple_busybox/install --disable-testsets CXX=mpicxx CC=mpicc F77=mpif77 FC=mpif90 CPPFLAGS=\"-I/opt/simple_busybox/install/include\" LIBS=\"-L/opt/simple_busybox/install/lib -lhdf5 -Wl,-rpath,/opt/simple_busybox/install/lib -ldl\" --cache-file=/dev/null --enable-static --disable-shared") == 0
          or die "failed to configure";
	system ("make V=1 LDFLAGS=-all-static") == 0 or die "failed to make";
	system ("make install") == 0 or die "failed to install";
	chdir "$BASEDIR" or die;
}


# Build DTK
if ($program_args{build_dtk}) {
	print "CNL: Building Data Transfer Kit\n";

	my $DTK_BASEDIR  = "$BASEDIR/$SRCDIR/$dtk{src_subdir}";
	my $DTK_BUILDDIR = "$BASEDIR/$SRCDIR/$dtk{src_subdir}/BUILD";

	# Steup symbolic links
	system ("ln -sf $DTK_BASEDIR/DataTransferKit $DTK_BASEDIR/Trilinos/DataTransferKit");
	system ("ln -sf $DTK_BASEDIR/DTKData $DTK_BASEDIR/Trilinos/DataTransferKit/DTKData");

	# Create BUILD directory
	system("mkdir -p $DTK_BUILDDIR");
	copy "$BASEDIR/$CONFIGDIR/dtk/configure_dtk.sh", "$DTK_BUILDDIR" or die;
	system ("chmod +x $DTK_BUILDDIR/configure_dtk.sh");

	# Setup Build directory (run cmake)
	chdir "$DTK_BUILDDIR" or die;
	system "PATH_TO_TRILINOS=$DTK_BASEDIR/Trilinos ./configure_dtk.sh";

	# Build Trilinos and DTK
	system ("make -j 4") == 0 or die "failed to make";

	chdir "$BASEDIR" or die;
}




##############################################################################
# Build Initramfs Image
##############################################################################
if ($program_args{build_image}) {
	#system ("rm -rf $IMAGEDIR/*";

	# Create some directories needed for stuff
	system ("mkdir -p $IMAGEDIR/etc");

	# Busybox
	system ("cp -R $SRCDIR/$busybox{basename}/_install/* $IMAGEDIR/");
	system ("ln -sf /bin/busybox $IMAGEDIR/bin/cut");
	system ("ln -sf /bin/busybox $IMAGEDIR/bin/env");
	system ("ln -sf /bin/gawk $IMAGEDIR/bin/awk");
	system ("cp $IMAGEDIR/bin/busybox $IMAGEDIR/bin/busybox_root");
	system ("ln -sf /bin/busybox_root $IMAGEDIR/bin/su");
	system ("ln -sf /bin/busybox_root $IMAGEDIR/bin/ping");
	system ("ln -sf /bin/busybox_root $IMAGEDIR/bin/ping6");
	system ("ln -sf /bin/busybox_root $IMAGEDIR/usr/bin/traceroute");
	system ("ln -sf /bin/busybox_root $IMAGEDIR/usr/bin/traceroute6");

	# Dropbear
	system ("cp $SRCDIR/$dropbear{basename}/dropbearmulti $IMAGEDIR/bin");
	system ("cp -R $CONFIGDIR/dropbear_files/etc/dropbear $IMAGEDIR/etc");
	chdir  "$BASEDIR/$IMAGEDIR/bin" or die;
	system ("ln -sf dropbearmulti dropbearconvert");
	system ("ln -sf dropbearmulti dropbearkey");
	# Use OpenSSH clients, rather than dropbear clients so that OpenSSH generated keys work.
	system ("ln -sf /usr/bin/scp scp");
	system ("ln -sf /usr/bin/ssh ssh");
	chdir  "$BASEDIR/$IMAGEDIR/sbin" or die;
	system ("ln -sf ../bin/dropbearmulti dropbear");
	chdir  "$BASEDIR/$IMAGEDIR/usr/bin" or die;
	system ("ln -sf ../../bin/dropbearmulti dbclient");
	chdir  "$BASEDIR" or die;

	# Use rsync to merge in skeleton overlay
	system("rsync -a $OVERLAYDIR/skel/\* $IMAGEDIR/") == 0
		or die "Failed to rsync skeleton directory to $IMAGEDIR";	

	# Instal linux kernel modules
	system("rsync -a $SRCDIR/$kernel{basename}/_install/\* $IMAGEDIR/") == 0
	#system("rsync -a /lib/modules/$kernel{version} $IMAGEDIR/lib/modules/") == 0
		or die "Failed to rsync linux modules to $IMAGEDIR";

	# Install numactl into image
	system("rsync -a $SRCDIR/$numactl{basename}/_install/\* $IMAGEDIR/") == 0
		or die "Failed to rsync numactl to $IMAGEDIR";

	# Install hwloc into image
	system("rsync -a $SRCDIR/$hwloc{basename}/_install/\* $IMAGEDIR/") == 0
		or die "Failed to rsync hwloc to $IMAGEDIR";

	# Install libhugetlbfs into image
	system("rsync -a $SRCDIR/$libhugetlbfs{basename}/_install/\* $IMAGEDIR/") == 0
		or die "Failed to rsync libhugetlbfs to $IMAGEDIR";

	# Install OpenMPI into image
	mkdir "$IMAGEDIR/opt/simple_busybox";
	system("cp -R /opt/simple_busybox/$ompi{basename} $IMAGEDIR/opt/simple_busybox") == 0
		or die "Failed to rsync OpenMPI to $IMAGEDIR";

	# Install Pisces / Hobbes / Leviathan into image
	system("cp -R $SRCDIR/pisces/xpmem/mod/xpmem.ko $IMAGEDIR/opt/hobbes") == 0
		or die "error 1";
	system("cp -R $SRCDIR/pisces/pisces/pisces.ko $IMAGEDIR/opt/hobbes") == 0
		or die "error 2";
	system("cp -R $SRCDIR/pisces/petlib/hw_status $IMAGEDIR/opt/hobbes") == 0
		or die "error 2";
	system("cp -R $SRCDIR/pisces/hobbes/lnx_inittask/lnx_init $IMAGEDIR/opt/hobbes") == 0
		or die "error 4";
	system("cp -R $SRCDIR/pisces/hobbes/shell/hobbes $IMAGEDIR/opt/hobbes") == 0
		or die "error 5";
	system("cp -R $SRCDIR/pisces/kitten/vmlwk.bin $IMAGEDIR/opt/hobbes") == 0
		or die "error 6";
	system("cp -R $SRCDIR/pisces/hobbes/lwk_inittask/lwk_init $IMAGEDIR/opt/hobbes") == 0
		or die "error 7";
	system("cp -R $SRCDIR/pisces/pisces/linux_usr/pisces_cons $IMAGEDIR/opt/hobbes") == 0
		or die "error 8";
	system("cp -R $SRCDIR/pisces/pisces/linux_usr/v3_cons_sc $IMAGEDIR/opt/hobbes") == 0
		or die "error 9";
	system("cp -R $SRCDIR/pisces/pisces/linux_usr/v3_cons_nosc $IMAGEDIR/opt/hobbes") == 0
		or die "error 10";
	system("cp -R $SRCDIR/pisces/hobbes/examples/apps/pmi/test_pmi_hello $IMAGEDIR/opt/hobbes") == 0
		or die "error 11";

	# Install Hobbes Enclave DTK demo files
	system("cp -R $SRCDIR/dtk/BUILD/DataTransferKit/packages/Adapters/STKMesh/example/DataTransferKitSTKMeshAdapters_STKInlineInterpolation.exe $IMAGEDIR/opt/hobbes_enclave_demo");
	system("cp -R $SRCDIR/dtk/BUILD/DataTransferKit/packages/Adapters/STKMesh/example/input.xml $IMAGEDIR/opt/hobbes_enclave_demo");
	system("cp -R $SRCDIR/dtk/BUILD/DataTransferKit/packages/Adapters/STKMesh/example/cube_mesh.exo $IMAGEDIR/opt/hobbes_enclave_demo");
	system("cp -R $SRCDIR/dtk/BUILD/DataTransferKit/packages/Adapters/STKMesh/example/pincell_mesh.exo $IMAGEDIR/opt/hobbes_enclave_demo");

	# Files copied from build host
	system ("cp /etc/localtime $IMAGEDIR/etc");
	system ("cp /lib64/libnss_files.so.* $IMAGEDIR/lib64");
	system ("cp /usr/bin/ldd $IMAGEDIR/usr/bin");
	system ("cp /usr/bin/strace $IMAGEDIR/usr/bin");
	system ("cp /usr/bin/ssh $IMAGEDIR/usr/bin");
	system ("cp /usr/bin/scp $IMAGEDIR/usr/bin");
	system ("cp -R /usr/share/terminfo $IMAGEDIR/usr/share");

	# Infiniband files copied from build host
	#system ("cp -R /etc/libibverbs.d $IMAGEDIR/etc");
	#system ("cp /usr/lib64/libcxgb4-rdmav2.so $IMAGEDIR/usr/lib64");
	#system ("cp /usr/lib64/libocrdma-rdmav2.so $IMAGEDIR/usr/lib64");
	#system ("cp /usr/lib64/libcxgb3-rdmav2.so $IMAGEDIR/usr/lib64");
	#system ("cp /usr/lib64/libnes-rdmav2.so $IMAGEDIR/usr/lib64");
	#system ("cp /usr/lib64/libmthca-rdmav2.so $IMAGEDIR/usr/lib64");
	#system ("cp /usr/lib64/libmlx4-rdmav2.so $IMAGEDIR/usr/lib64");
	#system ("cp /usr/lib64/libmlx5-rdmav2.so $IMAGEDIR/usr/lib64");
	#system ("cp /usr/bin/ibv_devices $IMAGEDIR/usr/bin");
	#system ("cp /usr/bin/ibv_devinfo $IMAGEDIR/usr/bin");
	#system ("cp /usr/bin/ibv_rc_pingpong $IMAGEDIR/usr/bin");

	# Find and copy all shared library dependencies
	copy_libs($IMAGEDIR);

	# Build the guest initramfs image
	# Fixup permissions, need to copy everything to a tmp directory
	system ("cp -R $IMAGEDIR $IMAGEDIR\_tmp");
	system ("sudo chown -R root.root $IMAGEDIR\_tmp");
	system ("sudo chmod +s $IMAGEDIR\_tmp/bin/busybox_root");
	system ("sudo chmod 777 $IMAGEDIR\_tmp/tmp");
	system ("sudo chmod +t $IMAGEDIR\_tmp/tmp");
	chdir  "$IMAGEDIR\_tmp" or die;
	system ("sudo find . | sudo cpio -H newc -o > $BASEDIR/initramfs.cpio");
	chdir  "$BASEDIR" or die;
	system ("cat initramfs.cpio | gzip > initramfs.gz");
	system ("rm initramfs.cpio");
	system ("sudo rm -rf $IMAGEDIR\_tmp");

	# As a convenience, copy Linux bzImage to top level
	system ("cp $SRCDIR/$kernel{basename}/arch/x86/boot/bzImage bzImage");
}


##############################################################################
# Build an ISO Image
##############################################################################
if ($program_args{build_isoimage}) {
	system ("mkdir -p isoimage");
	system ("cp $ISOLINUX isoimage");
	system ("cp $LDLINUX isoimage");
	system ("cp $SRCDIR/$kernel{basename}/arch/x86/boot/bzImage isoimage");
	system ("cp initramfs.gz isoimage/initrd.img");
	system ("echo 'default bzImage initrd=initrd.img console=ttyS0 console=tty0' > isoimage/isolinux.cfg");
#	system "echo 'default bzImage initrd=initrd.img' > isoimage/isolinux.cfg";
	system ("genisoimage -J -r -o image.iso -b isolinux.bin -c boot.cat -no-emul-boot -boot-load-size 4 -boot-info-table isoimage");
}


##############################################################################
# Build a Palacios Guest Image for the NVL (xml config file + isoimage)
##############################################################################
if ($program_args{build_nvl_guest}) {
        system ("../../nvl/palacios/utils/guest_creator/build_vm config/nvl_guest.xml -o image.img");
}
