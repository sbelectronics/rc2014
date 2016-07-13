# sudo bash
echo "i2c-bcm2708" >> /etc/modules
echo "i2c-dev" >> /etc/modules
reboot

sudo apt-get -y install python-smbus
sudo apt-get -y install i2c-tools

sudo emacs /etc/modprobe.d/raspi-blacklist.conf
# comment out spi-bcm2708 and i2c-bcm2708
sudo reboot

# this will show connected devices
sudo i2cdetect -y 1