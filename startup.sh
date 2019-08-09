echo "mount nvme and AEP"
mount /dev/nvme0n1 /mnt/nvme0n1/
mount -o dax /dev/pmem0 /mnt/pmem8/ 

echo "set cpu power"
cpupower frequency-set -g performance 


screen -x always_running
python3 /home/yuyangh/test/always_running.py