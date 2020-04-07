#include <SPI.h>
#include "tmc/ic/TMC5160/TMC5160_Register.h"
//https://blog.trinamic.com/2019/02/22/explore-and-tune-stallguard2-with-the-tmc5160-eval-arduino-mega-2560-in-step-and-direction-mode/
//https://www.trinamic.com/support/software/access-package/

/* The trinamic TMC5160 motor controller and driver operates through an 
 *  SPI interface.  Each datagram is sent to the device as an address byte
 *  followed by 4 data bytes.  This is 40 bits (8 bit address and 32 bit word).
 *  Each register is specified by a one byte (MSB) address: 0 for read, 1 for 
 *  write.  The MSB is transmitted first on the rising edge of SCK.
 *  
 * Arduino Pins   Eval Board Pins
 * 51 MOSI        32 SPI1_SDI
 * 50 MISO        33 SPI1_SDO
 * 52 SCK         31 SPI1_SCK
 * 25 CS          30 SPI1_CSN //Chip Select
 * 
 * 24 DIO         8 DIO0 (DRV_ENN)
 * 26 DIO         REFL_STEP pin header
 * 4 DIO          22 SPI_MODE
 * 5 DIO          20 SD_MODE
 * 20 DIO         DIAG0 pin header
 * GND            2 GND
 * +5V            5 +5V
 */

#define diag0_interrupt
// Pinbelegung
int chipCS = 25;
const byte STEPOUT = 26;
int enable = 24;
int SPImode = 4;
int SDmode = 5;
int DIAG0interruptpin = 20;

// Variablen
unsigned long SGvalue = 0;
unsigned long SGflag = 0;
boolean toggle = false;
void setup() {
  // put your setup code here, to run once:
  pinMode(SPImode,OUTPUT);
  pinMode(SDmode,OUTPUT);
  pinMode(chipCS,OUTPUT);
  pinMode(STEPOUT,OUTPUT);
  pinMode(enable, OUTPUT);
  digitalWrite(SPImode,HIGH);
  digitalWrite(SDmode,HIGH);
  digitalWrite(chipCS,HIGH);
  digitalWrite(enable,HIGH);

  //set up Timer1
  //TCCR1A = bit (COM1A0);                //toggle OC1A on Compare Match
  //TCCR1B = bit (WGM12) | bit (CS10);    //CTC, no prescaling
  //TCCR1B |= (1 << CS12);
  //OCR1A = 0;                            //output every cycle

  SPI.setBitOrder(MSBFIRST);
  SPI.setClockDivider(SPI_CLOCK_DIV8);
  SPI.setDataMode(SPI_MODE3);
  SPI.begin();

  Serial.begin(9600);

  sendData(0x80,0x00000080);      // GCONF -> Activate diag0_stall (Datasheet Page 31)
  sendData(0xED,0x00000000);      // SGT -> Needs to be adapted to get a StallGuard2 event
  sendData(0x94,0x00000050);      // TCOOLTHRS -> TSTEP based threshold = 80 (Datasheet Page 38)
  sendData(0x89,0x00010606);      // SHORTCONF
  sendData(0x8A,0x00080400);      // DRV_CONF
  sendData(0x90,0x00080303);      // IHOLD_IRUN
  sendData(0x91,0x0000000A);      // TPOWERDOWN
  sendData(0xAB,0x00000001);      // VSTOP
  sendData(0xBA,0x00000001);      // ENC_CONST
  sendData(0xEC,0x15410153);      // CHOPCONF
  sendData(0xF0,0xC40C001E);      // PWMCONF

  digitalWrite(enable,LOW);
}

void loop()
{
  SGvalue = readStallGuard2();
  SGflag = (SGvalue & 0x1000000) >> 24;
  SGvalue = (SGvalue & 0x3FF);
  Serial.print("StallGuard2 value: ");
  Serial.println(SGvalue, BIN);
  Serial.print("StallGuard2 event:");
  Serial.println(SGflag,BIN);

  /* Can be used to stop motor (essentially we stop the timer)
  if(SGvalue <= 0)
  {
    TCCR1B &= (0 << CS12);
    TCCR1B &= (0 << CS11);
    TCCR1B &= (0 << CS10);
  }*/
}

void sendData(unsigned long address, unsigned long datagram)
{
  //TMC5130 takes 40 bit data: 8 address and 32 data

  delay(100);
  unsigned long i_datagram = 0;

  digitalWrite(chipCS,LOW);
  delayMicroseconds(100);

  SPI.transfer(address);

  i_datagram |= SPI.transfer((datagram >> 24) & 0xff);
  i_datagram <<= 8;
  i_datagram |= SPI.transfer((datagram >> 16) & 0xff);
  i_datagram <<= 8;
  i_datagram |= SPI.transfer((datagram >> 8) & 0xff);
  i_datagram <<= 8;
  i_datagram |= SPI.transfer((datagram) & 0xff);
  digitalWrite(chipCS,HIGH);
  
  Serial.print("Received: ");
  Serial.println(i_datagram,HEX);
  Serial.print(" from register: ");
  Serial.println(address,HEX);
}

unsigned long readStallGuard2()
{
  delay(100);
  unsigned long i_datagram = 0;
  unsigned long datagram = 0xFFFFFFFF;

  digitalWrite(chipCS,LOW);
  delayMicroseconds(100);

  SPI.transfer(0x6F);

  i_datagram |= SPI.transfer((datagram >> 24) & 0xff);
  i_datagram <<= 8;
  i_datagram |= SPI.transfer((datagram >> 16) & 0xff);
  i_datagram <<= 8;
  i_datagram |= SPI.transfer((datagram >> 8) & 0xff);
  i_datagram <<= 8;
  i_datagram |= SPI.transfer((datagram) & 0xff);
  digitalWrite(chipCS,HIGH);

  return i_datagram;
}
