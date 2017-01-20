echo "Unloading petos, pisces, and xpmem modules"
/sbin/rmmod petos
/sbin/rmmod pisces
/sbin/rmmod xpmem

echo "Loading Hobbes PetOS Driver"
/sbin/insmod /tmp/hobbes_install/petos.ko
chmod 777 /dev/petos

echo "Loading Hobbes XEMEM Driver 'xpmem.ko ns=1 xpmem_follow_page=1'"
/sbin/insmod /tmp/hobbes_install/xpmem.ko ns=1 xpmem_follow_page=1

echo "Loading Hobbes Pisces Driver '/sbin/insmod pisces.ko'"
/sbin/insmod /tmp/hobbes_install/pisces.ko

echo "Launching Hobbes Master Daemon 'lnx_init --cpulist=0,31'"
export HOBBES_ENCLAVE_ID=0
export HOBBES_APP_ID=0
#/tmp/hobbes_install/lnx_init --cpulist=0,8,9,10,31 ${@:1} &
# Don't use cores from socket 0, also core 31 from socket 1 can't be offlined

#/tmp/hobbes_install/lnx_init --cpulist=0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,31 ${@:1} &
#/tmp/hobbes_install/lnx_init --cpulist=0,1,2,3,4,5,9,10,11,12,13,14,15,16,20,21,22,23,24,25,26,27,28,29,30,31 ${@:1} &

# This was used for the two node IMB-MPI1 tests
#/tmp/hobbes_isntall/lnx_init --cpulist=0,1,3,5,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31 ${@:1} &

# This was used for the one node IMB-MPI1 tests
#/tmp/hobbes_install/lnx_init --cpulist=0,1,3,5,7,8,9,10,11,12,13,14,15,16,17,19,21,23,24,25,26,27,28,29,30,31 ${@:1} &

# Volta config... offline second hyperthread on each physical core except for on cpus 37, 38, and 47.
# Cray is puts some kernel threads on cores 37, 38, and 47.  Need to figure out if it is possible to move them.
# To see which CPU each task is running on: "ps -A -F" and look at "PSR" column
/tmp/hobbes_install/lnx_init --cpulist=0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,37,38,47 ${@:1} &
# Desired Volta config... offline second hyperthread on each physical core.
#/tmp/hobbes_install/lnx_init --cpulist=0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23 ${@:1} &

echo $! > leviathan.pid

unset HOBBES_ENCLAVE_ID
unset HOBBES_APP_ID

# Unmount huge page filesystems
for i in `mount | grep huge | awk '{print $3}'`; do umount $i; done

# Restart ALPS so it picks up that CPUs and memory have been offlined.
# Otherwise job launches will fail.
echo "Sleeping 10 seconds before restarting Cray ALPS Daemon"
sleep 10
# Magic to disable Cray's libnodeservices prologue/epilogue cleanup.
# Without this Hobbes job launch does not work because the prologue
# tries to configure offlined CPUs, which fails.
touch /tmp/795130.disable
echo "Restarting Cray ALPS Daemon"
killall apinit

echo "Sleeping 10 seconds before starting a Hobbes Pisces enclave 1 of 2"
sleep 10
echo "Starting a Hobbes Pisces enclave 'hobbes create_enclave kitten_enclave.xml'"
cd /tmp/hobbes_install
./hobbes create_enclave cray_kitten_enclave.xml enclave-aprun-0

#echo "Sleeping 10 seconds before starting a Hobbes Pisces enclave 2 of 2"
#sleep 10
#echo "Starting a Hobbes Pisces enclave 'hobbes create_enclave kitten_enclave.xml'"
#cd /tmp/hobbes_install
#./hobbes create_enclave cray_kitten_enclave.xml enclave-aprun-1

echo "List of Hobbes enclaves running on this node:"
cd /tmp/hobbes_install
./hobbes list_enclaves

#echo "Enabling gemini debug"
#echo 1 > /sys/module/kgni_gem/parameters/kgni_subsys_debug

echo "Done."
