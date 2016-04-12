cp src/guests/simple_busybox/overlays/skel/home/user/.ssh/id_dsa ~/.ssh
chmod 0600 ~/.ssh/id_dsa
eval `ssh-agent`
ssh-add ~/.ssh/id_dsa

if sudo apt-get install -y genisoimage syslinux syslinux-common live-image-rescue; then
    exit 0
fi

