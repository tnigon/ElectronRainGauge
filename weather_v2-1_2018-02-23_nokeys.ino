// This #include statement was automatically added by the Particle IDE.
// #include <SdFat.h>
#include <Adafruit_ADS1X15.h>
#include <stdint.h> // For a 16-bit data type to measure full dynamic range of ADS output
#include <Adafruit_DHT.h> // Temp/humidity sensor
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_TSL2591.h> // Lux sensor
#include <DS18B20.h> // Waterproof temperature sensor

// Updated: 23 February 2018
// Author: Tyler J. Nigon
// Version 2.1

// ============================================================
// Global user parameters
// ============================================================
// ****** Time ******
boolean daylight_savings = false; // DST begins in spring and ends in fall
int gmt_timeZone = -6; // Change to appropriate time zone for your device
unsigned int sensorCapturePeriod = (10); // units are seconds
unsigned int publishPeriod = (60 * 30);  // units are seconds

// ****** Sensors ******
boolean sense_dht = true; // temp/humidity sensor
boolean sense_photo = true; // photoresistor light sensor
boolean sense_tsl = false; // tsl lux sensor
boolean sense_wind = true; // anemometer sensor
boolean sense_dir = false; // wind direction sensor
boolean sense_moist = true; // soil moisture sensor
boolean sense_soiltemp = false; // soil temperature sensor

// ****** Publishing ******
// ** If publishing via webhook, be sure channel, key/pw, name and number of variables are correct ** 
boolean publish_particle = false;
boolean publish_thingspeak = false;
unsigned long thingspeakChannelNumber = 0123456;
String api_key = ""; // ThingSpeak Write API Key.
String webhook_ts_str = "webhook_thingspeak_device_name"; // Name of webhook - must match from "Integrations" page
boolean publish_wunderground = false;
String wu_stationID = ""; // From WUnderground account
String wu_password = "";
String webhook_wu_str_rain = "webhook_wunderground_rain";
String webhook_wu_str_norain = "webhook_wunderground_norain";
boolean publish_serial = true;
boolean publish_sd = false;
boolean units_imperial = true; // Temp, wind, and rain reported in imperial units (F, mph, & inches)

// ****** Testing ******
boolean testing = false; // Uses benchTime instead of initialTime

// ****** Sensors & pins ******
#define DHTPIN 5
#define DHTTYPE DHT11  // DHT11 (blue) or DHT22 (white)
DHT dht(DHTPIN, DHTTYPE);
int light_sensor_pin = A1;
const int RainPin = D2;
const int anemometerPin = A2;
DS18B20 ds18b20(D4, true); // Waterproof temperature sensor
const int led = D7;
int ledState = LOW; // ledState used to set the LED
Adafruit_ADS1115 ads1115;
Adafruit_TSL2591 tsl = Adafruit_TSL2591(2591); // The number identifies the sensor (multiple possible)

// ****** Other global variables ******
float offsetDST = Time.getDSTOffset(); // Get current DST offset; default is +1.0
unsigned int sensorCount = 1; // this provides 32 bits of dynamic range
unsigned int publishCount = 1;
time_t benchTime;
time_t initialTime;
time_t endOfDay;
time_t scheduledSensor;
time_t scheduledPublish;
String end_day;
float dilToday = 0; // Daily Integrated Light (mol / m2 / day); resets at midnight
float lux2par = (1/54);
float dil_conversion = lux2par * (1/1000000) * (sensorCapturePeriod); // lux to par and micromoles to moles and seconds to "sensorCapturePeriod" (e.g., 5 seconds)
// ============================================================

// ============================================================
// Functions
// ============================================================
void setup();
void loop();
String hhmmss();
String yyyymmdd();
void test_sensors();
void blinkLight();
void captureTempHumidity();
float getAndResetTempC();
float getAndResetHumidityRH();
float getAndResetDewPointC();
void captureLight();
float getAndResetLight();
void captureLux();
float getAndResetLux();
float getAndResetLux1();
float getAndResetParDIL();
void captureSoilMoisture();
float getAndResetSoilMoisture();
void capture_soil_temp();
float getAndResetSoilTempC();
void initializeRainGauge();
void handleRainEvent();
float getAndResetRain();
void captureWind();
float getAndResetAnemometer();
void initializeWindVane();
void captureWindVane();
float getAndResetWindVaneDegrees();
float lookupRadiansFromRaw();
void publishToSerial();
void publishToParticle();
void publishToParticle3();
void publishToThingSpeak();
void publishToWUndergroundRain();
void publishToWUndergroundNoRain();
void createThingSpeakjson();
void createWUndergroundjsonRain();
void createWUndergroundjsonNoRain();
double mapDouble();
// ============================================================

void setup() {
    Serial.begin(9600); // Required if publishing to Serial
    // ============================================================
    // Time
    // ============================================================
    pinMode(led, OUTPUT);
    benchTime = Time.now(); // So all times are consistently calculated (it could presumably take several seconds to get through all code in which case times will be misaligned)
    if (Time.minute() >= 30) {
        initialTime = benchTime + ((59 - Time.minute(benchTime))*60) + (60 - Time.second(benchTime)); // ensures initial time will be at top of the hour
        if (Time.hour(initialTime) < abs(gmt_timeZone)) {
            endOfDay = initialTime - ((Time.hour(initialTime))*60*60) - (gmt_timeZone*60*60);
        }
        else {
            endOfDay = initialTime + ((24 - Time.hour(initialTime))*60*60) - (gmt_timeZone*60*60);
        }
    }
    if (Time.minute() < 30) {
        initialTime = benchTime + ((29 - Time.minute(benchTime))*60) + (60 - Time.second(benchTime)); // ensures initial time will be at bottom of the hour
        if (Time.hour(initialTime) < abs(gmt_timeZone)) {
            endOfDay = initialTime - ((Time.hour(initialTime))*60*60) - (gmt_timeZone*60*60) - (30*60); // benchTime.day is already tomorrow
        }
        else {
            endOfDay = initialTime + ((24 - Time.hour(initialTime))*60*60) - (gmt_timeZone*60*60) - (30*60); // benchTime.day is still today
        }
    }
    if (daylight_savings == true) {
        endOfDay -= int (offsetDST*60*60);  // Adding int makes the math correct; otherwise there's an extra 48 seconds..
    }

    // ============================================================
    // First publish will be at first top or bottom of the hour
    // (unless in testing mode)
    // ============================================================
    Particle.publish("Initial system time is: ", hhmmss(benchTime));
    delay(1000);
    scheduledSensor = benchTime + sensorCapturePeriod; // schedule initial sensor reading immediately
    if (testing == true) {
        scheduledPublish = benchTime + publishPeriod;
    }
    else {
        scheduledPublish = initialTime; // schedule first publish at first top (or bottom) of the hour
    }
    Particle.publish("Data will begin being collected at: ", hhmmss(scheduledSensor));
    delay(1000);
    Particle.publish("Data will first be published at: ", hhmmss(scheduledPublish));
    delay(4000);
    Particle.publish("GMT Time zone is: ", String(gmt_timeZone));
    delay(1000);
    if (daylight_savings == true) {
        Particle.publish("Daylight savings time: ", String("True"));
    }
    else {
        Particle.publish("Daylight savings time: ", String("False"));
    }
    delay(1000);
    end_day = yyyymmdd(endOfDay) + "T" + hhmmss(endOfDay);
    Particle.publish("Local end of day (UTC): ", end_day); // this is a check - should be +05:00:00 for CST during DST
    // ============================================================
    // Initialize Digital Sensors
    // ============================================================
    if (sense_dht == true) {
        dht.begin(); // Start DHT sensor
    }
    initializeRainGauge();
    if (sense_moist == true) {
        ads1115.begin();  // Initialize ads1115
        ads1115.setGain(GAIN_ONE); // 16x gain +/- 0.256V 1 bit = 0.125mV
    }
    if (sense_tsl == true) {
        tsl.begin();
        tsl.setGain(TSL2591_GAIN_LOW);    // LOW, MED, or HIGH for 1x, 25x, or 428x, respectively
        tsl.setTiming(TSL2591_INTEGRATIONTIME_100MS);  // can range from 100MS to 600MS
    }
    test_sensors();
    delay(10);
}

void loop() {
    blinkLight(); // To initialize currentMillis variable
    // ============================================================
    // Sensors
    // ============================================================
    if(Time.now() >= scheduledSensor) {
        if (sense_dht == true) {
            captureTempHumidity();
        }
        if (sense_photo == true) {
            captureLight();
        }
        if (sense_tsl == true) {
            captureLux();
        }
        if (sense_moist == true) {
            captureSoilMoisture();
        }
        if (sense_soiltemp == true) {
            capture_soil_temp();
        }
        if (sense_wind == true) {
            captureWind();
        }
        sensorCount++;
        scheduledSensor = benchTime + (sensorCount*sensorCapturePeriod);
    }

    // ============================================================
    // Publish
    // ============================================================
    if (Time.now() >= scheduledPublish) {
        float tempC;
        float humidityRH;
        float dewPointC;
        float temp_f;
        float dewPoint_f;
        if (sense_dht == true) {
            tempC = getAndResetTempC(); // Get the data to be published
            humidityRH = getAndResetHumidityRH();
            dewPointC = getAndResetDewPointC();
            if (units_imperial == true) {
                temp_f = (32 + (tempC * 1.8));
                dewPoint_f = (32 + (dewPointC * 1.8));
            }
        }

        float light;
        if (sense_photo == true) {
            light = getAndResetLight();
        }

        float lux;
        float lux_tot;
        float lux_n;
        float par;
        float dil_increment;
        if (sense_tsl == true) {
            lux = getAndResetLux();
            lux_tot = getAndResetLux1();
            lux_n = getAndResetLux2();
            getAndResetParDIL(&par, &dil_increment);
            dilToday += dil_increment;
        }

        float soilMoisture;
        if (sense_moist == true) {
            soilMoisture = getAndResetSoilMoisture();
        }

        float soil_temp_C;
        float soil_temp_F;
        if (sense_soiltemp == true) {
            soil_temp_C = getAndResetSoilTempC();
            if (units_imperial == true) {
                soil_temp_F = (32 + (soil_temp_C * 1.8));
            }
        }

        float wind_mps;
        float wind_gust;
        float wind_mph;
        float windGust_mph;
        if (sense_wind == true) {
            getAndResetAnemometer(&wind_mps, &wind_gust);
            if (units_imperial == true) {
                wind_mph = (wind_mps * 2.23694);
                windGust_mph = (wind_gust * 2.23694);
            }
        }
        
        float rain_mm;
        float rainToday_mm;
        float rain_in;
        float rainToday_in;
        rain_mm = getAndResetRain();
        rainToday_mm += rain_mm;
        if (units_imperial == true) {
            rain_in = (rain_mm * 0.0393701);
            rainToday_in = (rainToday_mm * 0.0393701);
        }
        
        float wind_direction;
        if (sense_dir == true) {
            wind_direction = getAndResetWindVaneDegrees();
        }

        FuelGauge fuel;
        float voltage = fuel.getVCell(); // Battery voltage
        float bat_percent = fuel.getSoC(); // State of Charge from 0-100%
        if (publish_serial == true) {
            if (units_imperial == false) {
                publishToSerial_metric(tempC,humidityRH,light,rainToday_mm,wind_mps,wind_gust,soil_temp_C);
            }
            else if (units_imperial == true) {
                publishToSerial_imperial(temp_f,humidityRH,light,rainToday_in,wind_mph,windGust_mph,soil_temp_F);
            }
        }
        if (publish_particle == true) {
            if (units_imperial == false) {
                publishToParticle(tempC,humidityRH,light,rainToday_mm,wind_mps,wind_gust,soil_temp_C);
            }
            else if (units_imperial == true) {
                publishToParticle(temp_f,humidityRH,light,rainToday_in,wind_mph,windGust_mph,soil_temp_F);
            }
        }    
        // publishToParticle3(lux, lux_tot, lux_n); // Comment out if you don't want to publish to particle.io
        if (publish_thingspeak == true) {
            if (units_imperial == false) {
                publishToThingSpeak(tempC,humidityRH,light,soilMoisture,rainToday_mm,wind_mps,wind_gust,soil_temp_C);
            }
            else if (units_imperial == true) {
                publishToThingSpeak(temp_f,humidityRH,light,soilMoisture,rainToday_in,wind_mph,windGust_mph,soil_temp_F);
            }
        }
        if (publish_wunderground == true) {
            if (rainToday_mm == 0.0) { // Do not publish rain data unless it has rained today
                if (units_imperial == false) {
                    publishToWUndergroundNoRain(tempC,humidityRH,wind_mps,wind_gust,soil_temp_C);
                }
                else if (units_imperial == true) {
                    publishToWUndergroundNoRain(temp_f,humidityRH,wind_mph,windGust_mph,soil_temp_F);
                }
            }
            else { // If it rained today, publish rainfall data for the rest of the day
                if (units_imperial == false) {
                    publishToWUndergroundRain(tempC,humidityRH,wind_mps,wind_gust,soil_temp_C,rain_mm,rainToday_mm);
                }
                else if (units_imperial == true) {
                    publishToWUndergroundRain(temp_f,humidityRH,wind_mph,windGust_mph,soil_temp_F,rain_in,rainToday_in);
                }
            }
        }
        if (publish_sd == true) {
            // logToSD(scheduledPublish,tempC,humidityRH,dewPointC,light,soilMoisture,rain_mm,wind_mps,wind_gust,soil_temp_C);
        }

        if (scheduledPublish + 10 > endOfDay) { // Reset accumulated rain and schedule next reset in 24 hours
            rainToday_mm = 0;
            dilToday = 0;
            endOfDay = endOfDay + (24*60*60);
        }

        if (testing == true) { // Declare next publish time
            scheduledPublish = benchTime + (publishCount*publishPeriod);
        }
        else {
            scheduledPublish = initialTime + (publishCount*publishPeriod);
        }
        publishCount++;
    }
    delay(10);
}

String hhmmss(unsigned long int now)  //format value as "hh:mm:ss"
{
   String hour = String::format("%02i",Time.hour(now)); // Time.hourFormat12(now);
   String minute = String::format("%02i",Time.minute(now));
   String second = String::format("%02i",Time.second(now));
   return hour + ":" + minute + ":" + second;
}

String yyyymmdd(unsigned long int now)  //format value as "hh:mm:ss"
{
   String year = String(Time.year(now));
   String month = String::format("%02i",Time.month(now));
   String day = String::format("%02i",Time.day(now));
   return year + "-" + month + "-" + day;
}

void test_sensors() {
    if (sense_dht == true) {
        float temp_test;
        temp_test = dht.getTempCelcius();
        Particle.publish("Temp test (Celcius): ", String(temp_test));
        delay(500);
    }
    
    if (sense_photo == true) {
        unsigned int light_test;
        light_test = analogRead(light_sensor_pin);
        Particle.publish("Photoresistor test (raw int): ", String(light_test));
        delay(500);
    }
    
    if (sense_tsl == true) {
        sensor_t sensor;
        tsl.getSensor(&sensor);
        if (tsl.begin()) {
            float lux_test;
            sensors_event_t event;
            tsl.getEvent(&event);
            lux_test = event.light;
            Particle.publish("Lux test: ", String(lux_test));
        }
        else
          {
            Particle.publish("Lux test: ", String("No sensor found"));
          }
        delay(500);
    }

    if (sense_wind == true) {
        unsigned int wind_test;
        wind_test = analogRead(anemometerPin);
        Particle.publish("Wind test (raw int): ", String(wind_test));
    }
    
    if (sense_moist == true) {
        int16_t soil_moist_test;
        soil_moist_test = (ads1115.readADC_SingleEnded(3)/float(10)); // Capture raw ADC reading
        Particle.publish("Soil moisture test (raw int): ", String(soil_moist_test));
    }

    if (sense_soiltemp == true) {
        float soiltemp_test;
        if (ds18b20.getTemperature()) {
            soiltemp_test = ds18b20.getTemperature();
            Particle.publish("Waterproof temperature test (C): ", String(soiltemp_test));
        }
        // if (ds18b20.read()) {
        //     String soiltemp_id;
        //     uint8_t addr[8];
        //     ds18b20.addr(addr);
        //     soiltemp_id = (String(addr[0]) + String(addr[1]) + String(addr[2]) + String(addr[3]) + String(addr[4]) + String(addr[5]) + String(addr[6]) + String(addr[7]));
        //     Particle.publish("Waterproof temperature sensor ID: ", String(soiltemp_id));
        // }
        else {
            Particle.publish("Waterproof temperature test (C): ", String("No sensor found"));
        }
    }
}


// ============================================================
// SD Card
// ---
// Logs data to SD card
// ============================================================
// This #include statement was automatically added by the Particle IDE.
// #include <ParticleSoftSerial.h>
// ParticleSoftSerial SoftSer(C1, C0);
// void setup() {
//     SoftSer.begin(9600, SERIAL_8N1);
// }

// void loop() {
//     SoftSer.print("blah blah blah");
// }
// void logToSD(time_t scheduledPublish, float tempC, float humidityRH, float dewPointC, float light, float soilMoisture, float rain_mm, float wind_mps, float wind_gust, float bat_percent) {
//     Serial.print(String(scheduledPublish) + ", ");
//     Serial.print(String(yyyymmdd(scheduledPublish)) + ", ");
//     Serial.print(String(hhmmss(scheduledPublish)) + ", ");
//     Serial.print(String(tempC) + ", ");
//     Serial.print(String(humidityRH) + ", ");
//     Serial.print(String(dewPointC) + ", ");
//     Serial.print(String(light) + ", ");
//     Serial.print(String(soilMoisture) + ", ");
//     Serial.print(String(rain_mm) + ", ");
//     Serial.print(String(wind_mps) + ", ");
//     Serial.print(String(wind_gust) + ", ");
//     Serial.println(String(bat_percent));
// }

// ============================================================
// Miscellaneous
// ---
// blinkLight() is used by the handleRainEvent() function to
// illuminate the D7 LED every time the tipping bucket flips.
// This function is necessary since we can't afford a delay as
// that affects the entire loop for all the other parts of the
// firmware.
// ============================================================
const long interval = 100; // interval at which to blink (milliseconds)
unsigned long lastRainEvent = 0; // Use "unsigned long" for variables that hold time
void blinkLight() {
  unsigned long currentMillis = millis();
  
    if (currentMillis - lastRainEvent < interval) {
        ledState = HIGH;
        digitalWrite(led, ledState);
    }
    if (currentMillis - lastRainEvent >= interval) {
        // if the LED is on, turn it off
        if (ledState == HIGH) {
            ledState = LOW;
        }
        digitalWrite(led, ledState);
    }
}

// ============================================================
// Sensors
// ============================================================

// ============================================================
// Temperature, Humidity, Light, and Soil Matric Tension
// ---
// Temperature, humidity, light, and soil matric tension
// sensors are read in the main loop, keeping track of the
// summed value of all measurements and measurement count;
// results are then determined by dividing to get the average
// for that publish cycle.
// ============================================================
float tempC_read;
float humidityRH_read;
float dewPointC_read;
unsigned int tempCReadingCount = 0;
unsigned int humidityRHReadingCount = 0;
unsigned int dewPointCReadingCount = 0;
float tempCTotal = 0.0;
float humidityRHTotal = 0.0;
float dewPointCTotal = 0.0;
void captureTempHumidity() {
    tempC_read = dht.getTempCelcius();
    // Check that result is reasonable
    if(tempC_read > -60 && tempC_read < 70) {
        tempCTotal += tempC_read;
        tempCReadingCount++;
    }

    humidityRH_read = dht.getHumidity();
    if(humidityRH_read > 0 && humidityRH_read < 105) { // Supersaturation could cause humidity > 100%
        humidityRHTotal += humidityRH_read; // Add measurement to running summed value
        humidityRHReadingCount++; // Increment measurement count
    }

    dewPointC_read = dht.getDewPoint();
    if(dewPointC_read > 0 && dewPointC_read < 105) { // Supersaturation could cause humidity > 100%
        dewPointCTotal += dewPointC_read; // Add measurement to running summed value
        dewPointCReadingCount++; // Increment measurement count
    }
    return;
}
float getAndResetTempC() {
    if(tempCReadingCount == 0) {
        return -100;
    }
    float result = tempCTotal/float(tempCReadingCount);
    tempCTotal = 0.0;
    tempCReadingCount = 0;
    return result;
}
float getAndResetHumidityRH() {
    if(humidityRHReadingCount == 0) {
        return -1;
    }
    float result = float(humidityRHTotal)/float(humidityRHReadingCount);
    humidityRHTotal = 0.0;
    humidityRHReadingCount = 0;
    return result;
}
float getAndResetDewPointC() {
    if(dewPointCReadingCount == 0) {
        return -1;
    }
    float result = float(dewPointCTotal)/float(dewPointCReadingCount);
    dewPointCTotal = 0.0;
    dewPointCReadingCount = 0;
    return result;
}

float light_read;
unsigned int lightReadingCount = 0;
float lightTotal = 0.0;
void captureLight() {
    light_read = analogRead(light_sensor_pin);
    lightTotal += light_read;
    lightReadingCount++;
}
float getAndResetLight() {
    if(lightReadingCount == 0) {
        return -1;
    }
    float result = (1-(float(lightTotal)/(4096*float(lightReadingCount))))*100;  //This was originally 4096, but I noticed light levels never got below 50%
    lightTotal = 0.0;
    lightReadingCount = 0;
    return result;
}

float lux_read;
unsigned int luxReadingCount = 0;
float luxTotal = 0.0;
float dil_inst;
float dil_now;
void captureLux() {
  /* Get a new sensor event */ 
  sensors_event_t event;
  tsl.getEvent(&event);
  lux_read = event.light;
  luxTotal += lux_read;
  luxReadingCount++;
  dil_inst = (lux_read * dil_conversion);
  dil_now += dil_inst;
}
float getAndResetLux() {
    if(luxReadingCount == 0) {
        return -1;
    }
    float result = float(luxTotal)/float(luxReadingCount);
    luxTotal = 0.0;
    luxReadingCount = 0;
    return result;
}

float getAndResetLux1() {
    return luxTotal;
}
float getAndResetLux2() {
    return luxReadingCount;
}
float getAndResetParDIL(float *par, float *dil_increment) {
    if(luxReadingCount == 0) {
        return -1;
    }
    *par = (float(luxTotal)/float(luxReadingCount)*lux2par);
    *dil_increment = dil_now;
    luxTotal = 0.0;
    luxReadingCount = 0;
    dil_now = 0;
}

int16_t soilMoisture_read; // 16-bit variable for ADS output at channel #3
unsigned int soilMoistureReadingCount;
float soilMoistureTotal;
void captureSoilMoisture() {
    soilMoisture_read = (ads1115.readADC_SingleEnded(3)/float(10)); // Capture raw ADC reading
    soilMoistureTotal += soilMoisture_read;
    soilMoistureReadingCount++;
}
float getAndResetSoilMoisture() {
    if(soilMoistureReadingCount == 0) {
        return -1;
    }
    // To convert voltage signal (mV) from A3 input on ADS1115 to the matric tension (cbar): y = 0.0831x * -0.3188
    float result = ((0.0831*(float(soilMoistureTotal)/float(soilMoistureReadingCount)))-0.3188);
    soilMoistureTotal = 0.0;
    soilMoistureReadingCount = 0;
    return result;
}

float soil_temp_read;
unsigned int soil_temp_count = 0;
float soil_temp_total = 0.0;
void capture_soil_temp() {
    soil_temp_read = ds18b20.getTemperature();
    soil_temp_total += soil_temp_read;
    soil_temp_count++;
}
float getAndResetSoilTempC() {
    if(soil_temp_count == 0) {
        return -100;
    }
    float result = soil_temp_total/float(soil_temp_count);
    soil_temp_total = 0.0;
    soil_temp_count = 0;
    return result;
}
// ============================================================
// Rain
// ---
// Uses a reed switch a magnet to count the number of times the
// bucket tips. The handleRainEvent() function is only called
// when signal is going from LOW (default) to HIGH, so [when
// LED illuminates], we want to count.
// ============================================================

// struct rainData {
//     float rainNow_mm;
//     float rainPastHour_mm;
//     float rainPast6h_mm;
//     float rainPast24h_mm;
//     float rainToday_mm;
// };
float rainToday_mm = 0;
volatile unsigned int rainEventCount; // volatile becuase it is part of an interrupt
unsigned long timeRainEvent;
float RainScale_mm = 0.25; // Each pulse is 0.25 mm of rain
void initializeRainGauge() {
    pinMode(RainPin, INPUT); // INPUT_PULLUP provides a "digital resistor" between signal and ground
    rainEventCount = 0;
    lastRainEvent = millis();
    attachInterrupt(RainPin, handleRainEvent, RISING);
    return;
}
void handleRainEvent() {
    // Just in case there is a bounce in signal while tipping bucket is flipping,
    // wait a period of time before recording data again
    timeRainEvent = millis(); // grab current time
    if(timeRainEvent - lastRainEvent > 300) {
        blinkLight();
        rainEventCount++;
        lastRainEvent = timeRainEvent;
    }
}
float getAndResetRain() {
    float rainNow_mm = RainScale_mm * float(rainEventCount);
    rainEventCount = 0;
    return rainNow_mm;
}

// ============================================================
// Wind Speed (Anemometer)
// ---
// The Anemometer generates a voltage relative to the
// windspeed: 400mV = 0 m/s wind speed; 2000mV = 32.4 m/s wind
// speed. A 12-bit DN (0-4096) is read in the main loop,
// keeping track of the summed value of all measurements and
// measurement count; results are then determined by dividing
// to get the average for that publish cycle.
// To convert DN to voltage:
// voltage = (DN * 3.3 * conversionConstant) / 4095
// * where conversionConstant is a multiplier derived from the
//   ratio between measured voltage and Electron output voltage
// ============================================================
int wind_read; // Stores the value direct from the analog pin
float windTotal = 0;
unsigned int windReadingCount = 0;
float sensorVoltage = 0; //Variable that stores the voltage (in Volts) from the anemometer being sent to the analog pin
float voltageNow = 0; // Temporary voltage reading for this instance
float gustVoltage = 0;
// NOTE: double check to see that the conversion is the same for you anemometer/microcontroller
// (e.g., mine averaged 463.59 while stagnant; 0.4 / ((463.59 * 3.3)/4095) = 1.0706953048246)
// float voltageConversionConstant = 1.0706953048246; // Constant accounting for difference in measured and output voltage
// int sensorDelay = 2000; //Delay between sensor readings, measured in milliseconds (ms)

// Technical variables (for the anemometer sold by Adafruit; can be modified for other anemometers):
float voltageMin = .4; // Mininum output voltage from anemometer (V); (this is where wind speed = 0)
float voltageMax = 2.0; // Maximum output voltage from anemometer (V)
//float windSpeedMax = 70; // Wind speed (m/s) corresponding to maximum voltage
float windSpeedMax = 32.4; // Wind speed (m/s) corresponding to maximum voltage

void captureWind() {
    wind_read = analogRead(anemometerPin); // Get 12-bit integer from the analog pin connected to the anemometer
    sensorVoltage = (wind_read * 3.3) / 4095; // Convert sensor value to actual voltage
    // sensorVoltage = (wind_read * 3.3 * voltageConversionConstant) / 4095; // Convert sensor value to actual voltage
    if (sensorVoltage < voltageMin) { 
        sensorVoltage = voltageMin; // Check if voltage is below minimum value. If so, set to minimum
    }
    windTotal += sensorVoltage;
    windReadingCount++;
    // Max wind speed calculation
    voltageNow = sensorVoltage;
    if (voltageNow >= gustVoltage) {
    gustVoltage = voltageNow;
    }
}

float getAndResetAnemometer(float *wind_mps, float *wind_gust) {
    if(windReadingCount == 0) {
        return -1;
    }
    *wind_mps = (float)mapDouble((double)(windTotal/windReadingCount), (double)voltageMin, (double)voltageMax, 0, (double)windSpeedMax);
    //*wind_mps = (((windTotal/windReadingCount)-voltageMin)*windSpeedMax / (voltageMax-voltageMin)); //For voltages above minimum value, use the linear relationship to calculate wind speed.
    *wind_gust = ((gustVoltage-voltageMin)*windSpeedMax / (voltageMax-voltageMin));
    windTotal = 0.0;
    windReadingCount = 0;
    gustVoltage = 0;
}

// ============================================================
// Wind Vane
// ============================================================
void initializeWindVane() {
    return;
}

// For the wind vane, we need to average the unit vector components (the sine and cosine of the angle)
int WindVanePin = A0;
float windVaneCosTotal = 0.0;
float windVaneSinTotal = 0.0;
unsigned int windVaneReadingCount = 0;
void captureWindVane() {
    // Read the wind vane, and update the running average of the two components of the vector
    unsigned int windVaneRaw = analogRead(WindVanePin);
    float windVaneRadians = lookupRadiansFromRaw(windVaneRaw);
    if(windVaneRadians > 0 && windVaneRadians < 6.14159) {
        windVaneCosTotal += cos(windVaneRadians);
        windVaneSinTotal += sin(windVaneRadians);
        windVaneReadingCount++;
    }
    return;
}

float getAndResetWindVaneDegrees() {
    if(windVaneReadingCount == 0) {
        return 0;
    }
    float avgCos = windVaneCosTotal/float(windVaneReadingCount);
    float avgSin = windVaneSinTotal/float(windVaneReadingCount);
    float result = atan(avgSin/avgCos) * 180.0 / 3.14159;
    windVaneCosTotal = 0.0;
    windVaneSinTotal = 0.0;
    windVaneReadingCount = 0;
    // atan can only tell where the angle is within 180 degrees.  Need to look at cos to tell which half of circle we're in
    if(avgCos < 0) result += 180.0;
    // atan will return negative angles in the NW quadrant -- push those into positive space.
    if(result < 0) result += 360.0;
    return result;
}

float lookupRadiansFromRaw(unsigned int analogRaw) {
    // The mechanism for reading the weathervane isn't arbitrary, but effectively, we just need to look up which of the 16 positions we're in.
    if(analogRaw >= 2200 && analogRaw < 2400) return (3.14);//South
    if(analogRaw >= 2100 && analogRaw < 2200) return (3.53);//SSW
    if(analogRaw >= 3200 && analogRaw < 3299) return (3.93);//SW
    if(analogRaw >= 3100 && analogRaw < 3200) return (4.32);//WSW
    if(analogRaw >= 3890 && analogRaw < 3999) return (4.71);//West
    if(analogRaw >= 3700 && analogRaw < 3780) return (5.11);//WNW
    if(analogRaw >= 3780 && analogRaw < 3890) return (5.50);//NW
    if(analogRaw >= 3400 && analogRaw < 3500) return (5.89);//NNW
    if(analogRaw >= 3570 && analogRaw < 3700) return (0.00);//North
    if(analogRaw >= 2600 && analogRaw < 2700) return (0.39);//NNE
    if(analogRaw >= 2750 && analogRaw < 2850) return (0.79);//NE
    if(analogRaw >= 1510 && analogRaw < 1580) return (1.18);//ENE
    if(analogRaw >= 1580 && analogRaw < 1650) return (1.57);//East
    if(analogRaw >= 1470 && analogRaw < 1510) return (1.96);//ESE
    if(analogRaw >= 1900 && analogRaw < 2000) return (2.36);//SE
    if(analogRaw >= 1700 && analogRaw < 1750) return (2.74);//SSE
    if(analogRaw > 4000) return(-1); // Open circuit?  Probably means the sensor is not connected
    Particle.publish("error", String::format("Got %d from Windvane.",analogRaw), 60 , PRIVATE);
    return -1;
}

// ============================================================
// Publish
// ---
// Functions to publish data to Electron (modify to your needs)
// Also builds the JSON to trigger the Webhook
// ============================================================
void publishToSerial_metric(float tempC,float humidityRH,float light,float rain_mm,float wind_mps,float wind_gust,float soil_temp_C) {
    Serial.print(F("Temperature: ")); Serial.print(tempC); Serial.println(F(" degrees C"));
    Serial.print(F("Relative Humidity: ")); Serial.print(humidityRH); Serial.println(F(" %"));
    Serial.print(F("Lux: ")); Serial.print(light); Serial.println(F(" %"));
    Serial.print(F("Daily Rain: ")); Serial.print(rain_mm); Serial.println(F(" mm"));
    Serial.print(F("Wind Speed: ")); Serial.print(wind_mps); Serial.println(F(" m s-1"));
    Serial.print(F("Wind Gust: ")); Serial.print(wind_gust); Serial.println(F(" m s-1"));
    Serial.print(F("Soil Temperature: ")); Serial.print(soil_temp_C); Serial.println(F(" degrees C"));
    Serial.println();
}

void publishToSerial_imperial(float temp_f,float humidityRH,float light,float rain_in,float wind_mph,float wind_gust_mph, float soil_temp_F) {
    Serial.print(F("Temperature: ")); Serial.print(temp_f); Serial.println(F(" degrees F"));
    Serial.print(F("Relative Humidity: ")); Serial.print(humidityRH); Serial.println(F(" %"));
    Serial.print(F("Lux: ")); Serial.print(light); Serial.println(F(" %"));
    Serial.print(F("Daily Rain: ")); Serial.print(rain_in); Serial.println(F(" in"));
    Serial.print(F("Wind Speed: ")); Serial.print(wind_mph); Serial.println(F(" mph"));
    Serial.print(F("Wind Gust: ")); Serial.print(wind_gust_mph); Serial.println(F(" mph"));
    Serial.print(F("Soil Temperature: ")); Serial.print(soil_temp_F); Serial.println(F(" degrees F"));
    Serial.println();
}
// void publishToParticle(float tempC, float humidityRH, float lux, float rain_mm, float wind_mps, float wind_gust) {
//     Particle.publish("Weather",
//                         String::format("%0.2f°C, %0.2f%%, %0.2f%%, %0.2f mm, %0.1f mps, %0.1f mps", 
//                             tempC,humidityRH,lux,rain_mm,wind_mps,wind_gust),60,PRIVATE);
// }

void publishToParticle(float var1, float var2, float var3, float var4, float var5, float var6, float var7) {
    Particle.publish("Weather",
                        String::format("1: %0.2f, 2: %0.2f, 3: %0.2f, 4: %0.2f, 5: %0.2f, 6: %0.2f, 7: %0.2f", 
                            var1,var2,var3,var4,var5,var6,var7),60,PRIVATE);
}

void publishToParticle3(float var1, float var2, float var3) {
    Particle.publish("Weather",
                        String::format("%0.2f, %0.2f, %0.2f", 
                            var1,var2,var3),60,PRIVATE);
}

String field1;  // for now fields are null but will be
String field2;  // filled by publishToThingSpeak()
String field3;
String field4;
String field5;
String field6;
String field7;
String field8;
String dateutc;
String status;
void publishToThingSpeak(float var1,float var2,float var3,float var4,float var5,float var6,float var7,float var8) { // Add or remove variables as appropriate for your ThingSpeak channel
// void publishToThingSpeak(float var1,float var2,float var3,float var4,float var5,float var6,float var7) {
    field1 = String(var1,2); // Comment out if not using all variables
    field2 = String(var2,2);
    field3 = String(var3,2);
    field4 = String(var4,2);
    field5 = String(var5,2);
    field6 = String(var6,2);
    field7 = String(var7,2);
    field8 = String(var8,2);
    String TSjson;
    createThingSpeakjson(TSjson);
    Particle.publish(webhook_ts_str,TSjson,60,PRIVATE);
    // Note: In order to publish a private event, you must pass all four parameters.
}

void publishToWUndergroundRain(float var1,float var2,float var3,float var4,float var5,float var6,float var7) {
    // Be sure units align with the webhook!
    dateutc = String("now");
    field1 = String(var1,2);
    field2 = String(var2,2);
    field3 = String(var3,2);
    field4 = String(var4,2);
    field5 = String(var5,2);
    float rainRateMultiplier = 60 / (publishPeriod/60); // WUnderground asks for hourly rate - this variable extrapolates so hourly rate can be reported based on the chosen publishPeriod 
    field6 = String(var6*rainRateMultiplier,2);
    field7 = String(var7,2);
    String WUrain_json;
    createWUndergroundjsonRain(WUrain_json);
    Particle.publish(webhook_wu_str_rain,WUrain_json,60,PRIVATE);
}

void publishToWUndergroundNoRain(float var1,float var2,float var3,float var4,float var5) {
    // Be sure units align with the webhook!
    dateutc = String("now");
    field1 = String(var1,2);
    field2 = String(var2,2);
    field3 = String(var3,2);
    field4 = String(var4,2);
    field5 = String(var5,2);
    String WUnorain_json;
    createWUndergroundjsonNoRain(WUnorain_json);
    Particle.publish(webhook_wu_str_norain,WUnorain_json,60,PRIVATE);
}

void createThingSpeakjson(String &dest) {
    // dest = "{ \"k\":\"" + api_key + "\", \"1\":\""+ field1 +"\", \"2\":\""+ field2 +"\",\"3\":\""+ field3 +"\",\"4\":\""+ field4 +"\",\"5\":\""+ field5 +"\",\"6\":\""+ field6 +"\",\"7\":\""+ field7 +"\",\"8\":\""+ field8 +"\",\"a\":\""+ lat +"\",\"o\":\""+ lon +"\",\"e\":\""+ el +"\", \"s\":\""+ status +"\"}";
    dest = "{";
    if(field1.length()>0){
        dest += "\"1\":\""+ field1 +"\",";
    }
    if(field2.length()>0){
        dest += "\"2\":\""+ field2 +"\",";
    }
    if(field3.length()>0){
        dest += "\"3\":\""+ field3 +"\",";
    }
    if(field4.length()>0){
        dest += "\"4\":\""+ field4 +"\",";
    }
    if(field5.length()>0){
        dest += "\"5\":\""+ field5 +"\",";
    }
    if(field6.length()>0){
        dest += "\"6\":\""+ field6 +"\",";
    }
    if(field7.length()>0){
        dest += "\"7\":\""+ field7 +"\",";
    }
    if(field8.length()>0){
        dest += "\"8\":\""+ field8 +"\",";
    }
    if(status.length()>0){
        dest += "\"s\":\""+ status +"\",";
    }
    dest += "\"k\":\"" + api_key + "\"}";
}

void createWUndergroundjsonRain(String &dest) {
    dest = "{";
    if(field1.length()>0){
        dest += "\"1\":\""+ field1 +"\",";
    }
    if(field2.length()>0){
        dest += "\"2\":\""+ field2 +"\",";
    }
    if(field3.length()>0){
        dest += "\"3\":\""+ field3 +"\",";
    }
    if(field4.length()>0){
        dest += "\"4\":\""+ field4 +"\",";
    }
    if(field5.length()>0){
        dest += "\"5\":\""+ field5 +"\",";
    }
    if(field6.length()>0){
        dest += "\"6\":\""+ field6 +"\",";
    }
    if(field7.length()>0){
        dest += "\"7\":\""+ field7 +"\",";
    }
    if(status.length()>0){
        dest += "\"s\":\""+ status +"\",";
    }
    if(dateutc.length()>0){
        dest += "\"t\":\""+ dateutc +"\",";
    }
    dest += "\"id\":\"" + wu_stationID + "\",";
    dest += "\"pw\":\"" + wu_password + "\"}";
}

void createWUndergroundjsonNoRain(String &dest) {
    dest = "{";
    if(field1.length()>0){
        dest += "\"1\":\""+ field1 +"\",";
    }
    if(field2.length()>0){
        dest += "\"2\":\""+ field2 +"\",";
    }
    if(field3.length()>0){
        dest += "\"3\":\""+ field3 +"\",";
    }
    if(field4.length()>0){
        dest += "\"4\":\""+ field4 +"\",";
    }
    if(field5.length()>0){
        dest += "\"5\":\""+ field5 +"\",";
    }
    if(status.length()>0){
        dest += "\"s\":\""+ status +"\",";
    }
    if(dateutc.length()>0){
        dest += "\"t\":\""+ dateutc +"\",";
    }
    dest += "\"id\":\"" + wu_stationID + "\",";
    dest += "\"pw\":\"" + wu_password + "\"}";
}

double mapDouble(double x, double in_min, double in_max, double out_min, double out_max) {
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}
