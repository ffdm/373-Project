#include <Wire.h> //allows communication over I2C devices
#include <LiquidCrystal_I2C.h> //allows interfacing with LCD screens

LiquidCrystal_I2C lcd (0x27, 2, 1, 0, 4, 5, 6, 7, 3, POSITIVE);

void setup() 
{
  pinMode(A4, OUTPUT);
  lcd.begin(16,2);
  //lcd.clear();
}

void loop() 
{
  
  // lcd.write('H');

  // analogWrite(A4, 67);
  // delay(10);
}
