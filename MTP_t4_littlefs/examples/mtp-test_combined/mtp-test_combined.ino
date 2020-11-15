#include "Arduino.h"
//#define USE_SPI
//#define USE_RAM
#define USE_QSPI

#ifdef USE_SPI
  #include "MTP_SPI.h"
#elif defined(USE_RAM)
  #include "MTP_RAM.h"
  char mybuffer0[40000];
  char mybuffer1[40000];
#elif defined(USE_QSPI)
  #include "MTP_QSPI.h"
#endif

#include "usb1_mtp.h"


#ifdef USE_SPI
  const char *sd_str[]={"PropShield", "Winbond1"}; // edit to reflect your configuration
  const int cs[] = {6, 10}; // edit to reflect your configuration
  const int nsd = sizeof(cs)/sizeof(int);

  LittleFS_SPIFlash sdx[nsd];
  MTPStorage_SPI storage1;
  MTPD_SPI       mtpd1(&storage1);

  void storage_configure(MTPStorage_SPI *storage1, const char **sd_str, const int *cs, LittleFS_SPIFlash *sdx, int num)
  {
      storage1->setStorageNumbers(sd_str,nsd);
      for(int ii=0; ii<nsd; ii++){
          pinMode(cs[ii],OUTPUT); digitalWriteFast(cs[ii],HIGH);
          if(!sdx[ii].begin(cs[ii], SPI)) {Serial.println("No storage"); while(1);}
      }
  }
#elif defined(USE_RAM)
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
#elif defined(USE_QSPI)
  const char *qspi_str[1]={"Winbond0"}; // edit to reflect your configuration
  const int qspiIdx[1] = {0}; // edit to reflect your configuration
  const int nsdQSPI = sizeof(qspiIdx)/sizeof(int);

  LittleFS_QSPIFlash sdr[nsdQSPI];
  MTPStorage_QSPI storage3;
  MTPD_QSPI       mtpd3(&storage3);

  void storage_configure(MTPStorage_QSPI *storage3, const char **qspi_str, const int *qspiIdx, LittleFS_QSPIFlash *sdr, int num)
  {
      storage3->setStorageNumbers(qspi_str, nsdQSPI);
  
      if(!sdr[0].begin()) {Serial.println("No storage"); while(1);}
  }
#endif

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
  #ifdef USE_RAM
    storage_configure(&storage2, ram_str,ramIdx, sdp, nsdRam);
  #elif defined(USE_SPI)
    storage_configure(&storage1, sd_str,cs, sdx, nsd);
  #elif defined(USE_QSPI)
      storage_configure(&storage3, qspi_str, qspiIdx, sdr, nsdQSPI);
  #endif
  Serial.println("Setup done");
  Serial.flush();
}

void loop()
{ 
  #ifdef USE_SPI
    mtpd1.loop();
  #elif defined(USE_RAM)
    mtpd2.loop();
  #elif defined(USE_QSPI)
    mtpd3.loop();
  #endif
  //logg(1000,"loop");
  //asm("wfi"); // may wait forever on T4.x
}
