#include "pins_arduino.h"

#define LED_HB    9
#define LED_ERR   8
#define LED_PMODE 7
#define CMD_RDSR  0x05
#define CMD_WRSR  0x01
#define CMD_WREN  0x06
#define CMD_EWSR  0x50
#define CMD_READ  0x03
#define CMD_PROGRAM  0x02
#define CMD_ERASE  0x60

void pulse(int pin, int times);

int error=0;
int pmode=0;
int window_len;
uint8_t buff[256]; // global block storage

void loop(void) {
  if (Serial.available()) {
    e_spi();
  }
}

uint8_t getch() {
  while(!Serial.available());
  return Serial.read();
}

uint8_t gethex() {
  String content = "";
  content.concat((char) getch());
  content.concat((char) getch());
  return hexToDec(content);
}

void fill(int n) {
  for (int x = 0; x < n; x++) {
    buff[x] = getch();
  }
}

void fillhex(int n) {
  for (int x = 0; x < n; x++) {
    buff[x] = gethex();
  }
}

#define PTIME 30
void pulse(int pin, int times) {
  do {
    digitalWrite(pin, HIGH);
    delay(PTIME);
    digitalWrite(pin, LOW);
    delay(PTIME);
  } 
  while (times--);
}

void setup() {
  Serial.begin(57600);
  pinMode(LED_PMODE, OUTPUT);
  pulse(LED_PMODE, 2);
  pinMode(LED_ERR, OUTPUT);
  pulse(LED_ERR, 2);
  pinMode(LED_HB, OUTPUT);
  pulse(LED_HB, 2);
}

unsigned int hexToDec(String hexString) {
  
  unsigned int decValue = 0;
  int nextInt;
  
  for (int i = 0; i < hexString.length(); i++) {
    
    nextInt = int(hexString.charAt(i));
    if (nextInt >= 48 && nextInt <= 57) nextInt = map(nextInt, 48, 57, 0, 9);
    if (nextInt >= 65 && nextInt <= 70) nextInt = map(nextInt, 65, 70, 10, 15);
    if (nextInt >= 97 && nextInt <= 102) nextInt = map(nextInt, 97, 102, 10, 15);
    nextInt = constrain(nextInt, 0, 15);
    
    decValue = (decValue * 16) + nextInt;
  }
  
  return decValue;
}


// Converting from Decimal to Hex:

// NOTE: This function can handle a positive decimal value from 0 - 255, and it will pad it
//       with 0's (on the left) if it is less than the desired string length.
//       For larger/longer values, change "byte" to "unsigned int" or "long" for the decValue parameter.


String decToHex(byte decValue, byte desiredStringLength) {
  
  String hexString = String(decValue, HEX);
  while (hexString.length() < desiredStringLength) hexString = "0" + hexString;
  
  return hexString;
}

void spi_init() {
  uint8_t x;
  //SPCR = 0x51; // f/16
  SPCR = 0x50; // f/128
  x=SPSR;
  x=SPDR;

  // following delays may not work on all targets...
  pinMode(SS, OUTPUT);
  digitalWrite(SS, HIGH);
  pinMode(SCK, OUTPUT);
  digitalWrite(SCK, LOW);
  //delay(5);
  digitalWrite(SS, LOW);
  //delay(5);
  pinMode(MISO, INPUT);
  pinMode(MOSI, OUTPUT);
}

void spi_end(void) {
  digitalWrite(SCK, LOW);
  digitalWrite(SS, HIGH);
  pinMode(MISO, INPUT);
  pinMode(MOSI, INPUT);
  pinMode(SCK, INPUT);
  pinMode(SS, INPUT);
}

void spi_wait() {
  do {
  } 
  while (!(SPSR & (1 << SPIF)));
}

uint8_t spi_send(uint8_t b) {
  uint8_t reply;
  SPDR=b;
  spi_wait();
  reply = SPDR;
  return reply;
}

void sync_reply() {
    Serial.print('\n');
}

void perform_spi(int len) {
  spi_init();
  while (len--) {
    Serial.print(decToHex(spi_send(gethex()), 2));
  }
  spi_end();
}

void wait_flash() {
  char tmp;
  while (true) {
    spi_init();
    spi_send(CMD_RDSR); //READ CMD
    tmp = spi_send(0x00);
    spi_end();
    if ((tmp & 0x01) == 0) {
      break;
    }
    //Serial.print(decToHex(tmp, 2));
    delay(2);   
  }
}

void read_flash() {
  char addr_h = getch();
  char addr_m = getch();
  char addr_l = getch();
  int len = getch();
  len = len+1;
  
  wait_flash();
  
  spi_init();
  spi_send(CMD_READ); //READ CMD
  spi_send(addr_h);
  spi_send(addr_m);
  spi_send(addr_l);
  while (len--) {
    Serial.print((char) spi_send(0x00));
  }
  spi_end();
}

void enable_write() {
  spi_init();
  spi_send(CMD_WREN); //READ CMD
  spi_end();
}

void write_flash() {
  char addr_h = getch();
  char addr_m = getch();
  char addr_l = getch();
  int len = getch();
  len = len+1;
  
  fill(len);
  
  for (int x = 0; x < len; x++) {
    wait_flash();
    enable_write();
    spi_init();
    spi_send(CMD_PROGRAM); //CMD
    spi_send(addr_h);
    spi_send(addr_m);
    spi_send(addr_l);
    spi_send(buff[x]);
    spi_end();
    addr_l++;
  }

  wait_flash();
}

void erase_flash() {
  wait_flash();
  enable_write();
  wait_flash();
  spi_init();
  spi_send(CMD_ERASE); //ERASE CMD
  spi_end();
  wait_flash();
}

void protect_flash() {
  wait_flash();
  enable_write();
  wait_flash();
  spi_init();
  spi_send(CMD_EWSR); //READ CMD
  spi_end();

  spi_init();
  spi_send(CMD_WRSR);
  spi_send(0x1c);
  spi_end();
  wait_flash();
}

void unprotect_flash() {
  wait_flash();
  enable_write();
  wait_flash();
  spi_init();
  spi_send(CMD_EWSR); //READ CMD
  spi_end();
  spi_init();
  spi_send(CMD_WRSR);
  spi_send(0x00);
  spi_end();
  wait_flash();
}

int e_spi() { 
  uint8_t data, low, high;
  uint8_t ch = getch();
  switch (ch) {
  case '0': // signon
    error = 0;
    break;
  case '1':
    Serial.print("EEPROM V1");
    break;
  case 'T':
    spi_init();
    spi_send(0xac);
    spi_send(0x53);
    spi_send(0x00);
    spi_send(0x00);
    spi_send(0x30);
    spi_send(0x00);
    spi_send(0x00);
    Serial.print(decToHex(spi_send(0x00), 2));
    spi_send(0x30);
    spi_send(0x00);
    spi_send(0x01);
    Serial.print(decToHex(spi_send(0x00), 2));
    spi_send(0x30);
    spi_send(0x00);
    spi_send(0x02);
    Serial.print(decToHex(spi_send(0x00), 2));
    spi_end();
    break;
  case 'r':
    spi_init();
    spi_send(0x9f);
    Serial.print(decToHex(spi_send(0x00), 2));
    Serial.print(decToHex(spi_send(0x00), 2));
    Serial.print(decToHex(spi_send(0x00), 2));
    spi_end();
    break;
  case 's':
    spi_init();
    spi_send(0x05);
    Serial.print(decToHex(spi_send(0x00), 2));
    Serial.print(decToHex(spi_send(0x00), 2));
    Serial.print(decToHex(spi_send(0x00), 2));
    spi_end();
    break;
  case 'm':
    spi_init();
    spi_send(CMD_READ);
    spi_send(0x00);
    spi_send(0x00);
    spi_send(0x00);
    Serial.print(decToHex(spi_send(0x00), 2));
    Serial.print(decToHex(spi_send(0x00), 2));
    Serial.print(decToHex(spi_send(0x00), 2));
    spi_end();
    break;
  case 'n':
    enable_write();
    spi_init();
    spi_send(CMD_PROGRAM);
    spi_send(0x00);
    spi_send(0x00);
    spi_send(0x00);
    spi_send(0x01);
    spi_end();
    break;
  case 'R': // reed
    read_flash();
    break;
  case 'W': // reed
    write_flash();
    break;
  case 'S': // reed/write
    window_len = gethex() << 8;
    window_len += gethex();
    perform_spi(window_len);
    break;
  case 'C':
    erase_flash();
    break;
  case 'P':
    protect_flash();
    break;
  case 'p':
    unprotect_flash();
    break;
  case 't':
    Serial.print(decToHex(gethex(), 2));
    break;
  default:
    return 0;
    break;
  }

  sync_reply();
}

