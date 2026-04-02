sudo apt update
sudo apt upgrade -y
sudo apt install -y build-essential git nasm binutils qemu-system-x86 genisoimage xorriso
sudo dpkg --add-architecture i386
sudo apt update
sudo apt install -y gcc-multilib g++-multilib
sudo snap install code --classic
