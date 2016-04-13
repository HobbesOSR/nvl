#!/bin/sh

cssh() {
    ssh -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null $*
}

qemu-system-x86_64 -cdrom ./image.iso -m 2048 -smp 4 -serial stdio -monitor /dev/null -nographic -net nic -net user,hostfwd=tcp::2222-:22 &
PID=$!
sleep 40
ssh-keyscan -t rsa,dsa localhost -p 2222 2>&1 | sort -u - ~/.ssh/known_hosts > ~/.ssh/tmp_hosts
mv ~/.ssh/tmp_hosts ~/.ssh/known_hosts
cssh root@localhost -p 2222 /opt/tests/test_launch
cssh root@localhost -p 2222 /opt/hobbes/hobbes console enclave-1
#cssh root@localhost -p 2222 halt
# want a way to make qemu exit with the halt
#kill $PID
exit 0
