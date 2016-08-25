echo "Loading petos.ko"
/sbin/insmod /opt/hobbes/petos.ko

echo "Loading xpmem.ko ns=1"
/sbin/insmod /opt/hobbes/xpmem.ko ns=1

echo "Loading pisces.ko"
/sbin/insmod /opt/hobbes/pisces.ko

echo "Launching Hobbes Master Daemon"
export HOBBES_ENCLAVE_ID=0
export HOBBES_APP_ID=0
/opt/hobbes/lnx_init ${@:1} &
echo $! > leviathan.pid
unset HOBBES_ENCLAVE_ID
unset HOBBES_APP_ID
sleep 5

#######################################################################
# Start Kitten enclave and load HIO test application into it
#######################################################################
#cd /opt/hobbes
#
#echo "Creating Pisces Enclave"
#hobbes create_enclave ./kitten_enclave.xml
#sleep 5
#
#echo "Launching HIO io-daemon test"
#hobbes launch_app enclave-1 -with-hio=/opt/hobbes/stub /opt/hobbes/io-daemon /opt/hobbes/test_file.txt

echo "Done."
