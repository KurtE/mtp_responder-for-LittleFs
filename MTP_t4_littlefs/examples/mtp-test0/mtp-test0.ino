#include "Arduino.h"
#define use_spi_disk
#define use_qspi_disk
//#define use_ram_disk
DMAMEM char my_buffer[400000];

  #include "MTP.h"
  #include "usb1_mtp.h"

  MTPStorage_SPI storage;
  MTPStorage_QSPI storage1;
  //MTPStorage_RAM storage;
  MTPD_SPI       mtpd(&storage);
  MTPD_QSPI       mtpd1(&storage1);

void logg(uint32_t del, const char *txt)
{ static uint32_t to;
  if(millis()-to > del)
  {
    //Serial.println(txt); 
#if USE_SDIO==1
    digitalWriteFast(2,!digitalReadFast(2));
#endif
    to=millis();
  }
}

void setup()
{ 
  while(!Serial && millis()<2000); 
  usb_mtp_configure();
    if(!Storage_init_qspi()) {Serial.println("No storage"); while(1);};
    if(!Storage_init_spi(6, SPI)) {Serial.println("No storage"); while(1);};
    //if(!Storage_init_ram(my_buffer, sizeof(my_buffer))) {Serial.println("No storage"); while(1);};

  Serial.println("MTP test");

#if USE_SDIO==1
  pinMode(2,OUTPUT);
#endif

}

void loop()
{ 
  mtpd.loop();
  mtpd1.loop();

  logg(1000,"loop");
  //asm("wfi"); // may wait forever on T4.x
}
