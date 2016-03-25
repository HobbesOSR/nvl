SLC=syslinux-common_4.05+dfsg-6+deb8u1_all.deb
SL=syslinux_4.05+dfsg-6+deb8u1_amd64.deb
GI=genisoimage_1.1.11-2ubuntu2_amd64.deb
LI=live-image-rescue_4.0~a5-1_all.deb
LB=live-build_3.0~a57-1ubuntu11.3_all.deb
DS=debootstrap_1.0.59ubuntu0.4_all.deb

cd /tmp
wget http://us.archive.ubuntu.com/ubuntu/pool/main/c/cdrkit/$GI
wget http://us.archive.ubuntu.com/ubuntu/pool/main/s/syslinux/$SLC
wget http://us.archive.ubuntu.com/ubuntu/pool/main/s/syslinux/$SL
wget http://us.archive.ubuntu.com/ubuntu/pool/universe/l/live-images/$LI
wget http://us.archive.ubuntu.com/ubuntu/pool/main/l/live-build/$LB
wget http://us.archive.ubuntu.com/ubuntu/pool/main/d/debootstrap/$DS

sudo dpkg -i $GI $SLC $SL $LI $LB $DS


