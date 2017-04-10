/*
 * Firmware for sensing a tip-bucket rain gauge and recording time and rate of rainfall
 *
 * Tyler Nigon
 * 2017/03/15
 */

#include <TimeLib.h>

time_t t=now();

const int rainPin = 2;
const int ledPin = 13;

// Rain variable
bool rainHigh = false;
const float lowAmt = 5.0; // when rain is low, takes this ml to trip
const float hiAmt = 0.0;    // when rain is high, takes this ml to trip
float rainAccum = 0.0;     // Rain accumulator since start of sample

void setup(void) {
   // Rain get start state
   pinMode(rainPin, INPUT);
   pinMode(ledPin, OUTPUT);
   Serial.begin(9600);
   if (digitalRead(rainPin) == HIGH)
    {
        rainHigh = true;
    }
   else
   {
        rainHigh = false;
    }
}

// In setup, I determine if the RainPin is high or low. This just determines which bucket
// is up and the starting point to start counting tips of the bucket.
void loop() {
  // Rain calculator, looks for Rain continuously
  // Look for low to high
  if ((rainHigh == false) && (digitalRead(rainPin) == HIGH))
    {
       rainHigh = true;
       rainAccum += lowAmt;
       digitalClockDisplay();
       Serial.print(second(t));
       Serial.print(" - Cumulative Rainfall: ");
       Serial.print(rainAccum);
       Serial.println();
    }
  if ((rainHigh == true) && (digitalRead(rainPin) == LOW))
    {
       rainHigh = false;
       rainAccum += hiAmt;
    }
}

void digitalClockDisplay() {
 // digital clock display of the time
 Serial.print(hour());
 printDigits(minute());
 printDigits(second());
 Serial.print(" ");
 Serial.print(day());
 Serial.print(" ");
 Serial.print(month());
 Serial.print(" ");
 Serial.print(year());
 Serial.println();
}

void printDigits(int digits) {
 // utility function for digital clock display: prints preceding colon and leading 0
 Serial.print(":");
 if (digits < 10)
 Serial.print('0');
 Serial.print(digits);
}

//Returns the battery voltage as a float.
FuelGauge fuel;
Serial.println( fuel.getVCell() );
//Returns the State of Charge in percentage from 0-100% as a float.
Serial.println( fuel.getSoC() );
