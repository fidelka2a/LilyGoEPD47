pokud se pokazi uploadovani a nepujde to zopakovat 
spustit  tento command

0. cd C:\Users\rendy\.espressif\python_env\idf5.3_py3.11_env\Scripts
1.python esptool.py --port COM4 --after=no_reset write_flash 0x0 C:\Users\rendy\Documents\PlatformIO\Projects\241011-074530-lilygo-t-display-s3\.pio\build\lilygo-t-display-s3\firmware.bin 
1a. alternative: python -m esptool --port COM4 --after=no_reset write_flash 0x0 C:\Users\rendy\Documents\PlatformIO\Projects\241011-074530-lilygo-t-display-s3\.pio\build\lilygo-t-display-s3\firmware.bin 
2. uploadovat znovu bootloader v ArduinoIDE OVERIT -> MOZNA NENI POTREBA 
3. uploadovat program

hardwarove to lze udelat 

co posilat do seruialu pro debug
