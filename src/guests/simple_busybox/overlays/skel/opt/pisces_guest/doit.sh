/sbin/insmod /lib/modules/xpmem.ko ns=1
/sbin/insmod /lib/modules/pisces.ko
sleep 5

pisces_load ./vmlwk.bin ./pisces "console=pisces"
pisces_launch -m 1 /dev/pisces-enclave0
sleep 5

pisces_ctrl -m 10 /dev/pisces-enclave0
pisces_ctrl -c 1 /dev/pisces-enclave0
sleep 5

v3_create -b /opt/pisces_guest/nvl_guest.xml foo /dev/pisces-enclave0
sleep 5
v3_launch /dev/pisces-enclave0 0

# Open up console
# v3_cons_sc /dev/pisces-enclave0 0
