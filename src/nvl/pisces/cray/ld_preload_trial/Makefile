#
 # Makefile
 #

libs:   libwrap_ioctl.so inspect_open.so
all:    $(libs) test_ioctl test


test: test.c
	gcc -o test test.c
test_ioctl: test_ioctl.c
	gcc -DCRAY_CONFIG_GHAL_GEMINI -I/scratch1/smukher/HEADERS/gni.headers/gni-headers-2.1-1.0400.4156.6.1.gem-include -o test_ioctl test_ioctl.c

inspect_open.so:       inspect_open.c
	rm -f inspect_open.so*
	gcc -fPIC -shared -Wl,-soname,inspect_open.so -ldl -o inspect_open.so  inspect_open.c

libwrap_ioctl.so:       wrap_ioctl.c
	rm -f libwrap_ioctl.so*
	gcc -fPIC -shared -Wl,-soname,libwrap_ioctl.so -ldl -o libwrap_ioctl.so  wrap_ioctl.c

clean:
	rm -f libwrap_ioctl.so* test_ioctl test inspect_open.so
