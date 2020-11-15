#include "Arduino.h"
#include "MTP_SPI.h"
#include "usb1_mtp.h"

  #define SD_MOSI 11
  #define SD_MISO 12
  #define SD_SCK  13

//  const char *sd_str[]={"sdio","sd1","sd2","sd3","sd4","sd5","sd6"}; // WMXZ example
//  const int cs[] = {BUILTIN_SDCARD,34,33,35,36,37,38}; // WMXZ example

  const char *sd_str[]={"PropShield", "Winbond1"}; // edit to reflect your configuration
  const int cs[] = {6, 10}; // edit to reflect your configuration
  const int nsd = sizeof(cs)/sizeof(int);

LittleFS_SPIFlash sdx[nsd];

MTPStorage_SPI storage1;
MTPD_SPI       mtpd1(&storage1);

void storage_configure(MTPStorage_SPI *storage1, const char **sd_str, const int *cs, LittleFS_SPIFlash *sdx, int num)
{
    #if defined SD_SCK
      SPI.setMOSI(SD_MOSI);
      SPI.setMISO(SD_MISO);
      SPI.setSCK(SD_SCK);
    #endif

    storage1->setStorageNumbers(sd_str,nsd);

    for(int ii=0; ii<nsd; ii++){
        pinMode(cs[ii],OUTPUT); digitalWriteFast(cs[ii],HIGH);
        if(!sdx[ii].begin(cs[ii], SPI)) {Serial.println("No storage"); while(1);}

        //uint32_t volCount  = sdx[ii].clusterCount();
        //uint32_t volFree  = sdx[ii].freeClusterCount();
        //uint32_t volClust = sdx[ii].sectorsPerCluster();
        //Serial.printf("Storage %d %d %d %d %d\n",ii,cs[ii],volCount,volFree,volClust);
      }

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
  storage_configure(&storage1, sd_str,cs, sdx, nsd);

  Serial.println("Setup done");
  Serial.flush();
}

void loop()
{ 
  mtpd1.loop();

  //logg(1000,"loop");
  //asm("wfi"); // may wait forever on T4.x
}
