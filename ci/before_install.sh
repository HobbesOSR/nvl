SLC=syslinux-common_4.05+dfsg-6+deb8u1_all.deb
SL=syslinux_4.05+dfsg-6+deb8u1_amd64.deb
GI=genisoimage_1.1.11-2ubuntu2_amd64.deb

cd /tmp
wget http://us.archive.ubuntu.com/ubuntu/pool/main/c/cdrkit/$GI
wget http://us.archive.ubuntu.com/ubuntu/pool/main/s/syslinux/$SLC
wget http://us.archive.ubuntu.com/ubuntu/pool/main/s/syslinux/$SL
sudo dpkg -i $GI $SLC $SL

