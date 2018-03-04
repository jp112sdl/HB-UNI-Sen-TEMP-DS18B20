#!/bin/sh
rm HB-UNI-Sen-TEMP-DS18B20_CCURM-addon.tgz
find . -name ".*" -exec rm -rf {} \;
cd HB-UNI-Sen-TEMP-DS18B20_CCURM-addon-src
chmod +x update_script
tar -zcvf ../HB-UNI-Sen-TEMP-DS18B20_CCURM-addon.tgz *
cd ..
