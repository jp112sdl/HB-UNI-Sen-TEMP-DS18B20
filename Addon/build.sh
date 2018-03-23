#!/bin/sh
DEVICE="HB-UNI-Sen-TEMP-DS18B20" 
rm $DEVICE-addon.tgz
find . -name ".DS_Store" -exec rm -rf {} \;
cd $DEVICE-addon-src
chmod +x update_script
chmod +x addon/install*
chmod +x addon/uninstall*
chmod +x addon/update-check.cgi
chmod +x rc.d/*
tar -zcvf ../$DEVICE-addon.tgz *
cd ..
