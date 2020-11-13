#include "Arduino.h"
//#define use_spi_disk
#define use_qspi_disk
//#define use_ram_disk

#include "MTP_LFS.h"
#include "usb1_mtp.h"

#if defined(use_spi_disk)
MTPStorage_SPI storage1;
#elif defined(use_qspi_disk)
MTPStorage_QSPI storage1;
#elif defined(use_ram_disk)
DMAMEM char my_buffer[400000];
MTPStorage_RAM storage1;
#else
#error "Need to define one of the storage classes to use"
#endif
MTPD1      mtpd1(&storage1);


void logg(uint32_t del, const char *txt)
{ static uint32_t to;
  if (millis() - to > del)
  {
    //Serial.println(txt);
#if USE_SDIO==1
    digitalWriteFast(2, !digitalReadFast(2));
#endif
    to = millis();
  }
}

void setup()
{
  while (!Serial && millis() < 2000);
  usb_mtp_configure();
#if defined(use_spi_disk)
  if (!Storage_init_spi(10, SPI)) {
    Serial.println("No storage");
    while (1);
  };
#elif defined(use_qspi_disk)
  if (!Storage_init_qspi()) {
    Serial.println("No storage");
    while (1);
  };
#elif defined(use_ram_disk)
  if (!Storage_init_ram(my_buffer, sizeof(my_buffer))) {
    Serial.println("No storage");
    while (1);
  };
#endif

  Serial.println("MTP test");

#if USE_SDIO==1
  pinMode(2, OUTPUT);
#endif

}

void loop()
{
  mtpd1.loop();

  logg(1000, "loop");
  //asm("wfi"); // may wait forever on T4.x
}
