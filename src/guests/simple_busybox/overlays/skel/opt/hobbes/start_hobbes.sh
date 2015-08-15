echo "Loading xpmem.ko ns=1"
/sbin/insmod /opt/hobbes/xpmem.ko ns=1

echo "Loading pisces.ko"
/sbin/insmod /opt/hobbes/pisces.ko

echo "Launching Hobbes Master Daemon"
export HOBBES_ENCLAVE_ID=0
export HOBBES_APP_ID=0
/opt/hobbes/lnx_init ${@:1} &
echo $! > leviathan.pid

echo "Done."
