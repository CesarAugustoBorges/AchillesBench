wget http://opendedup.org/downloads/sdfs-latest.deb
sudo apt-get install fuse libfuse2 ssh openssh-server jsvc libxml2-utils
sudo dpkg -i sdfs-latest.deb
echo "* hard nofile 65535" >> /etc/security/limits.conf
echo "* soft nofile 65535" >> /etc/security/limits.conf
exit