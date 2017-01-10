SRCDIR=.
IMAGEDIR=./hobbes_install

mkdir -p $IMAGEDIR

# Linux kernel modules
cp -R $SRCDIR/petlib/petos/petos.ko $IMAGEDIR
cp -R $SRCDIR/xpmem/mod/xpmem.ko $IMAGEDIR
cp -R $SRCDIR/pisces/pisces.ko $IMAGEDIR

# Pisces Kitten co-kernel image and init_task
cp -R $SRCDIR/../kitten/vmlwk.bin $IMAGEDIR
cp -R $SRCDIR/hobbes/lwk_inittask/lwk_init $IMAGEDIR

# Leviathan master daemon for Linux
cp -R $SRCDIR/hobbes/lnx_inittask/lnx_init $IMAGEDIR

# The Hobbes shell
cp -R $SRCDIR/hobbes/shell/hobbes $IMAGEDIR

# HIO stub that executes forwarded system calls
cp -R $SRCDIR/hobbes/hio/generic-io-stub/stub $IMAGEDIR

# Misc useful utilities
cp -R $SRCDIR/petlib/hw_status $IMAGEDIR
cp -R $SRCDIR/pisces/linux_usr/pisces_cons $IMAGEDIR
cp -R $SRCDIR/pisces/linux_usr/v3_cons_sc $IMAGEDIR
cp -R $SRCDIR/pisces/linux_usr/v3_cons_nosc $IMAGEDIR
