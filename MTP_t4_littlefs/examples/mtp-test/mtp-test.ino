#include "Arduino.h"

#include "MTP.h"
#include "usb1_mtp.h"

#if HAVE_LITTLEFS==1        // is defined in storage.h
  #define DO_LITTLEFS 1     // set to zero if not wanted // needs LittleFS installed as library
  #define RAM_DISK_STORAGE (2'000'000)
  DMAMEM char buf1[400000];
#else
  #define DO_LITTLEFS 0
#endif


/****  Start device specific change area  ****/

  // edit SPI to reflect your configuration (following is fot T4.1)
  #define SD_MOSI 11
  #define SD_MISO 12
  #define SD_SCK  13

  #define SPI_SPEED SD_SCK_MHZ(16)  // adjust to sd card 

#if DO_LITTLEFS==1
  const char *sd_str[]={"sdio","RAM0","RAM1","Propshield", "Winbond0", "QSPI0"};     // edit to reflect your configuration
  const int typeStore[]={0,1,1,2,2,3};
  const int cs[] = {BUILTIN_SDCARD, 256, 256, 6, 10,257};  // edit to reflect your configuration
#else
  const int typeStore[]={0,0};
  const char *sd_str[]={"sdio","sd6"}; // edit to reflect your configuration
  const int cs[] = {BUILTIN_SDCARD,38}; // edit to reflect your configuration
#endif
  const int nsd = sizeof(cs)/sizeof(int);

// classes need to be declared here (in storage.h there are declared external)
SDClass sdx[nsd];
#if HAVE_LITTLEFS==1
  LittleFS_RAM ramfs[nsd]; // needs to be declared if LittleFS is used in storage.h
  LittleFS_SPIFlash spiFlash[nsd];
  LittleFS_QSPIFlash qspiFlash;
#endif

void storage_configure(MTPStorage_SD *storage, const char **sd_str, const int *cs, const int *stype, SDClass *sdx, int num)
{
    #if defined SD_SCK
      SPI.setMOSI(SD_MOSI);
      SPI.setMISO(SD_MISO);
      SPI.setSCK(SD_SCK);
    #endif

    storage->setStorageNumbers(sd_str, cs, stype, nsd);

    for(int ii=0; ii<nsd; ii++)
    { if(cs[ii] == BUILTIN_SDCARD)
      {
        if(!sdx[ii].sdfs.begin(SdioConfig(FIFO_SDIO))){Serial.println("No storage"); while(1);};
      }
      else if(cs[ii]<BUILTIN_SDCARD)
      {
        pinMode(cs[ii],OUTPUT); digitalWriteFast(cs[ii],HIGH);
        if(!sdx[ii].sdfs.begin(SdSpiConfig(cs[ii], SHARED_SPI, SPI_SPEED))) {Serial.println("No storage"); while(1);}
      }
      #if DO_LITTLEFS==1
        else if(cs[ii]==256) // LittleFS_RAM
        { 
          if(!ramfs[1].begin(RAM_DISK_STORAGE)) { Serial.println("No storage"); while(1);}
          if(!ramfs[2].begin(buf1, sizeof(buf1))) { Serial.println("No storage"); while(1);}
        }
        else if(stype[ii] == 2) {
          if(!spiFlash[ii].begin(cs[ii], SPI)) { Serial.println("No storage"); while(1);}
        }
       else if(stype[ii] == 3) {
          if(!qspiFlash.begin()) { Serial.println("No storage"); while(1);}
        }
       else {
        Serial.println("Storage Type Not supported");
        while(1) {};
       }
      
      #endif
      if(cs[ii]<256)
      {
        uint32_t volCount = sdx[ii].sdfs.clusterCount();
        uint32_t volFree  = sdx[ii].sdfs.freeClusterCount();
        uint32_t volClust = sdx[ii].sdfs.sectorsPerCluster()*512;
        Serial.printf("Storage %d %d %s %d %d %d\n",ii,cs[ii],sd_str[ii],volCount,volFree,volClust);
      }
      #if DO_LITTLEFS==1
        else if(cs[ii]==256) // LittleFS_RAM
        {
          uint32_t volCount = ramfs[ii].totalSize();
          uint32_t volFree  = volCount - ramfs[ii].usedSize();
          uint32_t volClust = 1;
          Serial.printf("Storage %d %d %s %d %d %d\n",ii,cs[ii],sd_str[ii],volCount,volFree,volClust);
        }
        else if(stype[ii] == 2) // LittleFS_RAM
        {
          uint32_t volCount = spiFlash[ii].totalSize();
          uint32_t volFree  = volCount - spiFlash[ii].usedSize();
          uint32_t volClust = 1;
          Serial.printf("Storage %d %d %s %d %d %d\n",ii,cs[ii],sd_str[ii],volCount,volFree,volClust);
        }
        else if(stype[ii] == 3) // LittleFS_RAM
        {
          uint32_t volCount = qspiFlash.totalSize();
          uint32_t volFree  = volCount - qspiFlash.usedSize();
          uint32_t volClust = 1;
          Serial.printf("Storage %d %d %s %d %d %d\n",ii,cs[ii],sd_str[ii],volCount,volFree,volClust);
        }
      #endif
    }
}
/****  End of device specific change area  ****/

MTPStorage_SD storage;
MTPD       mtpd(&storage);

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
  storage_configure(&storage, sd_str, cs, typeStore, sdx, nsd);

  #if DO_LITTLEFS==3
  //todo:  
  // store some files into disks (but only once)
  for(int ii=0; ii<10;ii++)
  { char filename[80];
    sprintf(filename,"test_%d.txt",ii);
    if(!ramfs[0].exists(filename))
    {
      File file=ramfs[0].open(filename,FILE_WRITE_BEGIN);
      file.println("This is a test line");
      file.close();
    }
  }
  #endif

  Serial.println("Setup done");
  Serial.flush();
}

void loop()
{ 
  mtpd.loop();

  //logg(1000,"loop");
  //asm("wfi"); // may wait forever on T4.x
}
