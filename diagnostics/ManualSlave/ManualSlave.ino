// Escravo Modbus RTU "na unha", sem biblioteca, framing tolerante (5ms).
// Responde a QUALQUER request de leitura com 4 input registers fixos:
// 1111, 2222, 3333, 4444.  slave=1 func=4.
uint16_t crc16(const uint8_t* d, uint8_t n){
  uint16_t c=0xFFFF;
  for(uint8_t i=0;i<n;i++){ c^=d[i]; for(uint8_t b=0;b<8;b++) c=(c&1)?(c>>1)^0xA001:c>>1; }
  return c;
}
uint8_t buf[64]; uint8_t len=0; unsigned long last=0;
void setup(){ Serial.begin(9600); }
void loop(){
  while(Serial.available()){ if(len<64) buf[len++]=Serial.read(); last=millis(); }
  if(len>0 && millis()-last>5){          // fim de frame (pausa de 5ms)
    // monta resposta: 01 04 08 <4 regs> CRC
    uint8_t r[13];
    r[0]=0x01; r[1]=0x04; r[2]=0x08;
    uint16_t vals[4]={1111,2222,3333,4444};
    for(uint8_t i=0;i<4;i++){ r[3+i*2]=vals[i]>>8; r[4+i*2]=vals[i]&0xFF; }
    uint16_t c=crc16(r,11);
    r[11]=c&0xFF; r[12]=c>>8;
    Serial.write(r,13); Serial.flush();
    len=0;
  }
}
