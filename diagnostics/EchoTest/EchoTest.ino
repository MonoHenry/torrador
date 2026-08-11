void setup(){ Serial.begin(19200); }
void loop(){ if(Serial.available()){ char c=Serial.read(); Serial.print("got:"); Serial.println(c); } }
