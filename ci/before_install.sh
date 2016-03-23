DEB=genisoimage_1.1.11-2ubuntu2_amd64.deb
cd /tmp
wget http://us.archive.ubuntu.com/ubuntu/pool/main/c/cdrkit/$DEB
dpkg-deb --extract $DEB genisoimage
