#include "Arduino.h"
#include "MTP_RAM.h"
#include "usb1_mtp.h"

char mybuffer0[40000];
char mybuffer1[40000];

//  const char *sd_str[]={"sdio","sd1","sd2","sd3","sd4","sd5","sd6"}; // WMXZ example
//  const int cs[] = {BUILTIN_SDCARD,34,33,35,36,37,38}; // WMXZ example

  const char *ram_str[]={"ram1", "ram2"}; // edit to reflect your configuration
  const int ramIdx[] = {0, 1}; // edit to reflect your configuration
  const int nsdRam = sizeof(ramIdx)/sizeof(int);

LittleFS_RAM sdp[nsdRam];

MTPStorage_RAM storage2;
MTPD_RAM       mtpd2(&storage2);

void storage_configure(MTPStorage_RAM *storage2, const char **ram_str, const int *ramIdx, LittleFS_RAM *sdp, int num)
{
    storage2->setStorageNumbers(ram_str, nsdRam);

        sdp[0].begin(mybuffer0, sizeof(mybuffer0));
        sdp[1].begin(mybuffer1, sizeof(mybuffer1));

}

void logg(uint32_t del, const char *txt)
{ static uint32_t to;
  if(millis()-to > del)
  {
    Serial.println(txt); 
    to=millis();
  }
}

void setup()
{ 
  while(!Serial && millis()<3000); 
  Serial.println("MTP_test");
  
  usb_mtp_configure();
  storage_configure(&storage2, ram_str,ramIdx, sdp, nsdRam);

  Serial.println("Setup done");
  Serial.flush();
}

void loop()
{ 
  mtpd2.loop();

  //logg(1000,"loop");
  //asm("wfi"); // may wait forever on T4.x
}
