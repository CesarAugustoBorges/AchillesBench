if [[ $1 == "mount" && -n $2 && -n $3 ]]; then
	sudo mkdir -p /home/mnt/$2 &&
	sudo mount.sdfs $2 /home/mnt/$2/ &&
	sudo mkfs.ext4 $3 &&
	sudo mount $3 /opt/sdfs/volumes/$2
elif [[ $1 == "umount" && -n $2 ]]; then
	sudo umount -q /opt/sdfs/volumes/$2
	sudo umount -q /home/mnt/$2
	sudo rm -r /home/mnt/$2
elif [[ $1 == "create" && -n $2 && -n -$3 ]]; then
	sudo mkfs.sdfs /home/mnt/$2 --volume-name=$2 --volume-capacity=$3  --io-dedup-files=true --io-safe-sync=true --io-safe-close=true --io-write-threads=1 --chunk-store-encrypt=false --chunk-store-compress=false --compress-metadata=false
elif [[ $1 == "remove" && -n $2 ]]; then
	sudo rm -rf /opt/sdfs/volumes/$2
	sudo rm -rf /var/log/sdfs/$2*
	sudo rm -rf /etc/sdfs/$2*
else
	echo "\"openddedup mount <name> <dir>\"\nuse \"openddedup create <name> <space> <bdus>\"\nuse \"openddedup remove <name>\""
fi