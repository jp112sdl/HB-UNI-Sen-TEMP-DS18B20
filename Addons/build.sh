#!/bin/sh
rm HB-UNI-Sen-TEMP-DS18B20-addon.tgz
find . -name ".DS_Store" -exec rm -rf {} \;
cd HB-UNI-Sen-TEMP-DS18B20-addon-src
chmod +x update_script
chmod +x addon/install*
chmod +x addon/update-check.cgi
chmod +x rc.d/*
tar -zcvf ../HB-UNI-Sen-TEMP-DS18B20-addon.tgz *
cd ..
