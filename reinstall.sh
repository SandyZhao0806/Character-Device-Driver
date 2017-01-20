rmmod mychardriver
make clean
make
insmod mychardriver.ko
echo This is for testing purpose>/dev/mychardev
tail /var/log/syslog