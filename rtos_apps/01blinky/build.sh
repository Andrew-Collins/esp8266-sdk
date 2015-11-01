# Make the code
make BOOT=new APP=1 SPI_SPEED=40 SPI_MODE=QIO SPI_SIZE_MAP=2

# Move the bin files into the right place
cd $BIN_PATH >/dev/null
mkdir old >/dev/null
mv *.bin old  >/dev/null
cd upgrade >/dev/null
cp user1.1024.new.2.bin ../user.bin  >/dev/null
cd ../ >/dev/null
cp "$SDK_PATH/bin/boot_v1.4(b1).bin" boot.bin >/dev/null

# Program the code to the ESP8266
sudo python2.7 esptool.py -p /dev/ttyUSB0 write_flash 0x00000 boot.bin 0x01000 user.bin
