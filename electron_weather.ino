// This #include statement was automatically added by the Particle IDE.
#include <SdFat.h>
#include <Adafruit_ADS1X15.h>
#include <stdint.h> // For a 16-bit data type to measure full dynamic range of ADS output
#include <Adafruit_DHT.h>

// ============================================================
// Time
// ---
// Set time zone and daylight savings information and initiate
// intervals to determine when to capture sensor readings and
// publish data.
// ============================================================
float offsetDST = Time.getDSTOffset(); // Get current DST offset; default is +1.0
// I read that Particle does not automatically detect DST, so
// it must be manually delclared; however, whenever publishing
// time, Unix UTC is expected, so I don't want to globally
// adjust the Time variable.. If I did, then the Unix timestamp
// may be altered (not 100% sure about this..?). My goal in
// declaring time zone is to know when to reset daily rain.
// Time.beginDST(); // Because DST is not automatically detected by Particle, it must be manually declared
// Time.endDST(); // Comment out beginDST() or endDST() depending on if DST is in effect
unsigned int isDST = 1; // Change to 0 if not observing daylight savings time
int gmt_timeZone = -6; // Change to appropriate time zone for your device
unsigned int sensorCapturePeriod = (5); // units are seconds
unsigned int publishPeriod = (60 * 30);  // units are seconds
unsigned int sensorCount = 1; // I believe this will give us 32 bits of dynamic range
unsigned int publishCount = 1;
time_t initialTime;
time_t endOfDay;
time_t scheduledSensor;
time_t scheduledPublish;

float rainToday_mm = 0;

String hhmmss(unsigned long int now)  //format value as "hh:mm:ss"
{
   String hour = String::format("%02i",Time.hour(now)); // Time.hourFormat12(now);
   String minute = String::format("%02i",Time.minute(now));
   String second = String::format("%02i",Time.second(now));
   return hour + ":" + minute + ":" + second;
};

String yyyymmdd(unsigned long int now)  //format value as "hh:mm:ss"
{
   String year = String(Time.year(now));
   String month = String::format("%02i",Time.month(now));
   String day = String::format("%02i",Time.day(now));
   return year + "-" + month + "-" + day;
};

// ============================================================
// SD Card
// ============================================================
// const uint8_t chipSelect = SS; // SD chip select pin.
// #define FILE_BASE_NAME "Wx" // Log file base name.  Must be six characters or less.
// SdFat sd; // File system object
// SdFile file; // Log file
// Write data header.
void writeHeader() {
    Serial.begin(9600);
    Serial.print("timestampUTC, ");
    Serial.print("yyyy-mm-dd, ");
    Serial.print("hh:mm:ss, ");
    Serial.print("tempC, ");
    Serial.print("humidityRH, ");
    Serial.print("dewPointC, ");
    Serial.print("light, ");
    Serial.print("soilMoisture, ");
    Serial.print("rain_mm, ");
    Serial.print("wind_mps, ");
    Serial.print("wind_gust, ");
    Serial.println("bat_percent");
    // file.print(F("timestampUTC, "));
    // file.print(F("yyyy-mm-dd, "));
    // file.print(F("hh:mm:ss, "));
    // file.print(F("tempC, "));
    // file.print(F("humidityRH, "));
    // file.print(F("light, "));
    // file.print(F("soilMoisture, "));
    // file.print(F("rain_mm, "));
    // file.print(F("wind_mps, "));
    // file.print(F("wind_gust, "));
    // file.print(F("bat_percent"));
    // file.println();
}

// ============================================================
// Publish
// ============================================================
// Initialize variables to publish to ThingSpeak
unsigned long thingspeakChannelNumber = <######>;
String api_key = "<key>"; // ThingSpeak Write API Key.
String tempc = "";  // for now fields are null
String humidity_ts = "";  // will be filled by publishToThingSpeak()
String lightrel = "";
String soilmoisture = "";
String dailyrainmm = "";
String windspeedmps = "";
String windgustmps = "";
String volts = "";
String status = "";

// Initialize variables to publish to Weather Underground
String wu_stationID = "<ABCDEFGH##>"; // From WUnderground account
String wu_password = "<pw>";
String dateutc = "";  // will be filled by publishToWUnderground()
String tempf = "";
String humidity_wu = "";
String rainin = "";
String dailyrainin = "";
String windspeedmph = "";
String windgustmph = "";

// ============================================================
// Sensors
// ============================================================
#define DHTPIN 5
#define DHTTYPE DHT22  // Change to DHTTYPE DHT11 if using DHT11 sensor
DHT dht(DHTPIN, DHTTYPE);
int light_sensor_pin = A1;
const int RainPin = D2;
const int anemometerPin = A2;
const int led = D7;
int ledState = LOW; // ledState used to set the LED
unsigned long lastRainEvent = 0; // Use "unsigned long" for variables that hold time
const long interval = 100; // interval at which to blink (milliseconds)
Adafruit_ADS1115 ads1115;

void setup() {
    // ============================================================
    // Time
    // ============================================================
    pinMode(led, OUTPUT);
    // initialTime = Time.now();
    time_t benchTime = Time.now(); // So all times are consistently calculated (it could presumably take several seconds to get through all code in which case times will be misaligned)
    if (Time.minute() >= 30) {
        initialTime = benchTime + ((59 - Time.minute(benchTime))*60) + (60 - Time.second(benchTime)); // Ensures initial time will be at top of the hour
        if (isDST == 1) {
            endOfDay = initialTime + ((23 - Time.hour(benchTime))*60*60) - ((gmt_timeZone+offsetDST)*60*60) - 48; // To know when to reset daily rain
        }
        else {
            endOfDay = initialTime + ((23 - Time.hour(benchTime))*60*60) - (gmt_timeZone*60*60) - 48; // I have no idea where the extra 48 seconds is coming from, but seems to be coming from..
        }
    }
    if (Time.minute() < 30) {
        initialTime = benchTime + ((29 - Time.minute(benchTime))*60) + (60 - Time.second(benchTime)); // Ensures initial time will be at bottom of the hour
        if (isDST == 1) {
            endOfDay = initialTime + 30 + ((23 - Time.hour(benchTime))*60*60) - ((gmt_timeZone+offsetDST)*60*60) - 48; // To know when to reset daily rain
        }
        else {
            endOfDay = initialTime + 30 + ((23 - Time.hour(benchTime))*60*60) - (gmt_timeZone*60*60) - 48;
        }
    }
    scheduledSensor = initialTime + sensorCapturePeriod; // Schedule initial sensor reading and publish events
    scheduledPublish = initialTime + publishPeriod;
    Particle.publish("Initial system time is: ", hhmmss(benchTime));
    Particle.publish("Data will begin being collected at: ", hhmmss(initialTime));
    Particle.publish("GMT Time zone is: ", gmt_timeZone);
    if (isDST == 1) {
        Particle.publish("Daylight savings time: ", String("True"));
    }
    else {
        Particle.publish("Daylight savings time: ", String("False"));
    }
    String endOfDayDateTime = yyyymmdd(endOfDay) + "T" + hhmmss(endOfDay);
    Particle.publish("UTC corresponding to local end of day: ", endOfDayDateTime); // this is a check - should be +05:00:00 for CST during DST
    // ============================================================
    // Sensors
    // ============================================================
    dht.begin(); // Start DHT sensor
    initializeRainGauge();
    ads1115.begin();  // Initialize ads1115
    ads1115.setGain(GAIN_ONE); // 16x gain +/- 0.256V 1 bit = 0.125mV

    // ============================================================
    // SD Card
    // ============================================================
    // const uint8_t BASE_NAME_SIZE = sizeof(FILE_BASE_NAME) - 1;
    // char fileName[13] = FILE_BASE_NAME "00.csv";
    // Serial.begin(9600);
    // // Wait for USB Serial
    // while (!Serial) {
    //     SysCall::yield();
    // }
    // delay(1000);
    // ===================================================================
    //    Serial.println(F("Type any character to start")); // doesn't work!
//    while (!Serial.available()) {
//        SysCall::yield();
//    }
    // ===================================================================

    // while (sd.exists(fileName)) {
    //     if (fileName[BASE_NAME_SIZE + 1] != '9') {
    //         fileName[BASE_NAME_SIZE + 1]++;
    //     } else if (fileName[BASE_NAME_SIZE] != '9') {
    //         fileName[BASE_NAME_SIZE + 1] = '0';
    //         fileName[BASE_NAME_SIZE]++;
    //     } else {
    //       sd.errorPrint("Can't create file name");
    //     }
    // }

    // if (!file.open(fileName, O_CREAT | O_WRITE | O_EXCL)) {
    //     sd.errorPrint("file.open");
    // }

    // // Read any Serial data.
    // do {
    //     delay(10);
    // } while (Serial.available() && Serial.read() >= 0);
    // Serial.print(F("Logging to: "));
    // Serial.println(fileName);

    writeHeader(); // Write data header.
}

void loop() {
    // ============================================================
    // Light
    // ============================================================
    blinkLight(); // To initialize currentMillis variable
    // ============================================================
    // Sensors
    // ---
    // Note: temp, humidity, and light are aggregated and averaged
    // over publish intervals; rain is meausured using an interrupt
    // ============================================================
    if(Time.now() >= scheduledSensor) {
        captureTempHumidity();
        captureLight();
        captureSoilMoisture();
        captureWind();
        sensorCount++;
        scheduledSensor = initialTime + (sensorCount*sensorCapturePeriod);
    }

    // ============================================================
    // Publish
    // ============================================================
    if (Time.now() >= scheduledPublish) {
        float tempC = getAndResetTempC(); // Get the data to be published
        float humidityRH = getAndResetHumidityRH();
        float dewPointC = getAndResetDewPointC();
        float light = getAndResetLight();
        float soilMoisture = getAndResetSoilMoisture();
        float rain_mm = getAndResetRain();
        rainToday_mm = rainToday_mm + rain_mm;
        float wind_mps;
        float wind_gust;
        getAndResetAnemometer(&wind_mps, &wind_gust);
//        float wind_direction = getAndResetWindVaneDegrees(); // Comment out and add variable to publish...() functions if you have a wind vane connected
        // ============================================================
        // Convert to imperial units and make variables available
        // ============================================================
        float temp_f = (32 + (tempC * 1.8));
        float dewPoint_f = (32 + (dewPointC * 1.8));
        float rain_in = (rain_mm * 0.0393701);
        float rainToday_in = (rainToday_mm * 0.0393701);
        float wind_mph = (wind_mps * 2.23694);
        float windGust_mph = (wind_gust * 2.23694);
        FuelGauge fuel;
        float voltage = fuel.getVCell(); // Battery voltage
        float bat_percent = fuel.getSoC(); // State of Charge from 0-100%
        // publishToParticle(tempC,humidityRH,light,soilMoisture,rain_mm,wind_mps,wind_gust); // Comment out if you don't want to publish to particle.io
        publishToThingSpeak(tempC,humidityRH,light,soilMoisture,rainToday_mm,wind_mps,wind_gust,voltage); // Comment out if you don't want to publish to ThinkSpeak
        // Comment out if you don't want to publish to Weather Underground
        if (rainToday_mm == 0.0) { // Do not publish rain data unless it has rained today
            publishToWUndergroundNoRain(temp_f,humidityRH,wind_mph,windGust_mph);
        }
        else { // If it rained today, publish rainfall data for the rest of the day
            publishToWUndergroundRain(temp_f,humidityRH,wind_mph,windGust_mph,rain_in,rainToday_in);
        }
        // logToSD(scheduledPublish,tempC,humidityRH,dewPointC,light,soilMoisture,rain_mm,wind_mps,wind_gust,bat_percent);

        if (scheduledPublish + 10 > endOfDay) { // Reset accumulated rain and schedule next reset in 24 hours
            rainToday_mm = 0;
            endOfDay = endOfDay + (24*60*60);
        }

        publishCount++;
        scheduledPublish = initialTime + (publishCount*publishPeriod); // Declare next publish time

    }
    delay(10);
}

// ============================================================
// SD Card
// ---
// Logs data to SD card
// ============================================================
void logToSD(time_t scheduledPublish, float tempC, float humidityRH, float dewPointC, float light, float soilMoisture, float rain_mm, float wind_mps, float wind_gust, float bat_percent) {
    Serial.print(String(scheduledPublish) + ", ");
    Serial.print(String(yyyymmdd(scheduledPublish)) + ", ");
    Serial.print(String(hhmmss(scheduledPublish)) + ", ");
    Serial.print(String(tempC) + ", ");
    Serial.print(String(humidityRH) + ", ");
    Serial.print(String(dewPointC) + ", ");
    Serial.print(String(light) + ", ");
    Serial.print(String(soilMoisture) + ", ");
    Serial.print(String(rain_mm) + ", ");
    Serial.print(String(wind_mps) + ", ");
    Serial.print(String(wind_gust) + ", ");
    Serial.println(String(bat_percent));
    // file.print(String(scheduledPublish) + ", ");
    // file.print(String(yyyymmdd(scheduledPublish)) + ", ");
    // file.print(String(hhmmss(scheduledPublish)) + ", ");
    // file.print(String(tempC) + ", ");
    // file.print(String(humidityRH) + ", ");
    // file.print(String(light) + ", ");
    // file.print(String(soilMoisture) + ", ");
    // file.print(String(rain_mm) + ", ");
    // file.print(String(wind_mps) + ", ");
    // file.print(String(wind_gust) + ", ");
    // file.println(String(bat_percent));
}

// ============================================================
// Miscellaneous
// ---
// blinkLight() is used by the handleRainEvent() function to
// illuminate the D7 LED every time the tipping bucket flips.
// This function is necessary since we can't afford a delay as
// that affects the entire loop for all the other parts of the
// firmware.
// ============================================================
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
float light_read;
float tempCTotal = 0.0;
unsigned int tempCReadingCount = 0;
float humidityRHTotal = 0.0;
unsigned int humidityRHReadingCount = 0;
float dewPointCTotal = 0.0;
unsigned int dewPointCReadingCount = 0;
float lightTotal = 0.0;
unsigned int lightReadingCount = 0;

int16_t soilMoisture_read; // 16-bit variable for ADS output at channel #3
float soilMoistureTotal;
unsigned int soilMoistureReadingCount;

void captureTempHumidity() {
    tempC_read = dht.getTempCelcius();
    // Check that result is reasonable
    if(tempC_read > -60 && tempC_read < 70)
    {
        tempCTotal += tempC_read;
        tempCReadingCount++;
    }

    humidityRH_read = dht.getHumidity();
    if(humidityRH_read > 0 && humidityRH_read < 105) // Supersaturation could cause humidity > 100%
    {
        humidityRHTotal += humidityRH_read; // Add measurement to running summed value
        humidityRHReadingCount++; // Increment measurement count
    }

    dewPointC_read = dht.getDewPoint();
    if(dewPointC_read > 0 && dewPointC_read < 105) // Supersaturation could cause humidity > 100%
    {
        dewPointCTotal += dewPointC_read; // Add measurement to running summed value
        dewPointCReadingCount++; // Increment measurement count
    }
    return;
}

void captureLight() {
    light_read = analogRead(light_sensor_pin);
    lightTotal += light_read;
    lightReadingCount++;
}

void captureSoilMoisture() {
    soilMoisture_read = (ads1115.readADC_SingleEnded(3)/float(10)); // Capture raw ADC reading
    soilMoistureTotal += soilMoisture_read;
    soilMoistureReadingCount++;
}

float getAndResetTempC()
{
    if(tempCReadingCount == 0) {
        return -100;
    }
    float result = tempCTotal/float(tempCReadingCount);
    tempCTotal = 0.0;
    tempCReadingCount = 0;
    return result;
}

float getAndResetHumidityRH()
{
    if(humidityRHReadingCount == 0) {
        return -1;
    }
    float result = float(humidityRHTotal)/float(humidityRHReadingCount);
    humidityRHTotal = 0.0;
    humidityRHReadingCount = 0;
    return result;
}

float getAndResetDewPointC()
{
    if(dewPointCReadingCount == 0) {
        return -1;
    }
    float result = float(dewPointCTotal)/float(dewPointCReadingCount);
    dewPointCTotal = 0.0;
    dewPointCReadingCount = 0;
    return result;
}

float getAndResetLight()
{
    if(lightReadingCount == 0) {
        return -1;
    }
    float result = (1-(float(lightTotal)/(4096*float(lightReadingCount))))*100;  //This was originally 4096, but I noticed light levels never got below 50%
    lightTotal = 0.0;
    lightReadingCount = 0;
    return result;
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
volatile unsigned int rainEventCount; // volatile becuase it is part of an interrupt
unsigned long timeRainEvent;
float RainScale_mm = 0.25; // Each pulse is 0.25 mm of rain

void initializeRainGauge() {
    pinMode(RainPin, INPUT); // INPUT_PULLUP provides a "digital resistor" between signal and ground
    rainEventCount = 0;
    lastRainEvent = millis();
//    attachInterrupt(digitalPinToInterrupt(RainPin), handleRainEvent, RISING);
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
// speed. Voltage output is read in the main loop, keeping
// track of the summed value of all measurements and
// measurement count; results are then determined by dividing
// to get the average for that publish cycle.
// ============================================================
int wind_read; // Stores the value direct from the analog pin
float windTotal = 0;
unsigned int windReadingCount = 0;
float sensorVoltage = 0; //Variable that stores the voltage (in Volts) from the anemometer being sent to the analog pin
float voltageNow = 0; // Temporary voltage reading for this instance
float gustVoltage = 0;
// NOTE: double check to see that the conversion is the same for you anemometer/microcontroller (e.g., mine read 487 while stagnant; 0.4/487 = 0.0008213552361)
float voltageConversionConstant = 0.0008213552361; // Constant that maps the value provided by the analog read function to actual voltage
int sensorDelay = 2000; //Delay between sensor readings, measured in milliseconds (ms)

// Technical variables (for the anemometer sold by Adafruit; can be modified for other anemometers):
float voltageMin = .4; // Mininum output voltage from anemometer (V); (this is where wind speed = 0)
float voltageMax = 2.0; // Maximum output voltage from anemometer (V)
float windSpeedMax = 32.4; // Wind speed (m/s) corresponding to maximum voltage

void captureWind(){
    wind_read = analogRead(anemometerPin); // Get an integer from the analog pin connected to the anemometer
    sensorVoltage = wind_read * voltageConversionConstant; // Convert sensor value to actual voltage
    if (sensorVoltage < voltageMin) {
        sensorVoltage = voltageMin; //Check if voltage is below minimum value. If so, set to minimum
    }
    windTotal += sensorVoltage;
    windReadingCount++;
    // Max wind speed calculation
    voltageNow = sensorVoltage;
    if (voltageNow >= gustVoltage) {
    gustVoltage = voltageNow;
    }
}

float getAndResetAnemometer(float *wind_mps, float *wind_gust)
{
    if(windReadingCount == 0) {
        return -1;
    }
    *wind_mps = (((windTotal/windReadingCount)-voltageMin)*windSpeedMax / (voltageMax-voltageMin)); //For voltages above minimum value, use the linear relationship to calculate wind speed.
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
    if(windVaneRadians > 0 && windVaneRadians < 6.14159)
    {
        windVaneCosTotal += cos(windVaneRadians);
        windVaneSinTotal += sin(windVaneRadians);
        windVaneReadingCount++;
    }
    return;
}

float getAndResetWindVaneDegrees()
{
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

float lookupRadiansFromRaw(unsigned int analogRaw)
{
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
void publishToParticle(float tempC, float humidityRH, float light, float soilMoisture, float rain_mm, float wind_mps, float wind_gust) {
    Particle.publish("Weather",
                        String::format("%0.2f°C, %0.2f%%, %0.2f%%, %0.1f cbar, %0.2f mm, %0.1f mps, %0.1f mps",
                            tempC,humidityRH,light,soilMoisture,rain_mm,wind_mps,wind_gust),60,PRIVATE);
}

void publishToThingSpeak(float tempC,float humidityRH,float light,float soilMoisture,float rainToday_mm,float wind_mps,float wind_gust,float voltage) {
    tempc = String(tempC,2);
    humidity_ts = String(humidityRH,2);
    lightrel = String(light,2);
    soilmoisture = String(soilMoisture,2);
    dailyrainmm = String(rainToday_mm,2);
    windspeedmps = String(wind_mps,2);
    windgustmps = String(wind_gust,2);
    volts = String(voltage,2);

    String TSjson;
    createThinkSpeakjson(TSjson);
    Particle.publish("webhook_thingspeak",TSjson,60,PRIVATE);
    // Note: In order to publish a private event, you must pass all four parameters.
}

void publishToWUndergroundRain(float temp,float humidityRH,float wind,float windGust,float rain,float rainToday) {
    // Be sure units align with the webhook!
    dateutc = String("now");
    tempf = String(temp,2);
    humidity_wu = String(humidityRH,2);
    windspeedmph = String(wind,2);
    windgustmph = String(windGust,2);
    float rainRateMultiplier = 60 / (publishPeriod/60); // WUnderground asks for hourly rate - this variable extrapolates so hourly rate can be reported based on the chosen publishPeriod
    rainin = String(rain*rainRateMultiplier,2);
    dailyrainin = String(rainToday,2);

    String WUrain_json;
    createWUndergroundjsonRain(WUrain_json);
    Particle.publish("webhook_wunderground_rain",WUrain_json,60,PRIVATE);
}

void publishToWUndergroundNoRain(float temp,float humidityRH,float wind,float windGust) {
    // Be sure units align with the webhook!
    dateutc = String("now");
    tempf = String(temp,2);
    humidity_wu = String(humidityRH,2);
    windspeedmph = String(wind,2);
    windgustmph = String(windGust,2);

    String WUnorain_json;
    createWUndergroundjsonNoRain(WUnorain_json);
    Particle.publish("webhook_wunderground_norain",WUnorain_json,60,PRIVATE);
}

void createThinkSpeakjson(String &dest) {
    // dest = "{ \"k\":\"" + api_key + "\", \"1\":\""+ field1 +"\", \"2\":\""+ field2 +"\",\"3\":\""+ field3 +"\",\"4\":\""+ field4 +"\",\"5\":\""+ field5 +"\",\"6\":\""+ field6 +"\",\"7\":\""+ field7 +"\",\"8\":\""+ field8 +"\",\"a\":\""+ lat +"\",\"o\":\""+ lon +"\",\"e\":\""+ el +"\", \"s\":\""+ status +"\"}";
    dest = "{";
    if(tempc.length()>0){
        dest = dest + "\"1\":\""+ tempc +"\",";
    }
    if(humidity_ts.length()>0){
        dest = dest + "\"2\":\""+ humidity_ts +"\",";
    }
    if(lightrel.length()>0){
        dest = dest + "\"3\":\""+ lightrel +"\",";
    }
    if(soilmoisture.length()>0){
        dest = dest + "\"4\":\""+ soilmoisture +"\",";
    }
    if(dailyrainmm.length()>0){
        dest = dest + "\"5\":\""+ dailyrainmm +"\",";
    }
    if(windspeedmps.length()>0){
        dest = dest + "\"6\":\""+ windspeedmps +"\",";
    }
    if(windgustmps.length()>0){
        dest = dest + "\"7\":\""+ windgustmps +"\",";
    }
    if(volts.length()>0){
        dest = dest + "\"8\":\""+ volts +"\",";
    }
    if(status.length()>0){
        dest = dest + "\"s\":\""+ status +"\",";
    }
    dest = dest + "\"k\":\"" + api_key + "\"}";
}

void createWUndergroundjsonRain(String &dest) {
    dest = "{";
    if(tempf.length()>0){
        dest = dest + "\"1\":\""+ tempf +"\",";
    }
    if(humidity_wu.length()>0){
        dest = dest + "\"2\":\""+ humidity_wu +"\",";
    }
    if(windspeedmph.length()>0){
        dest = dest + "\"3\":\""+ windspeedmph +"\",";
    }
    if(windgustmph.length()>0){
        dest = dest + "\"4\":\""+ windgustmph +"\",";
    }
    if(rainin.length()>0){
        dest = dest + "\"5\":\""+ rainin +"\",";
    }
    if(dailyrainin.length()>0){
        dest = dest + "\"6\":\""+ dailyrainin +"\",";
    }
    if(status.length()>0){
        dest = dest + "\"s\":\""+ status +"\",";
    }
    if(dateutc.length()>0){
        dest = dest + "\"t\":\""+ dateutc +"\",";
    }
    dest = dest + "\"id\":\"" + wu_stationID + "\",";
    dest = dest + "\"pw\":\"" + wu_password + "\"}";
}

void createWUndergroundjsonNoRain(String &dest) {
    dest = "{";
    if(tempf.length()>0){
        dest = dest + "\"1\":\""+ tempf +"\",";
    }
    if(humidity_wu.length()>0){
        dest = dest + "\"2\":\""+ humidity_wu +"\",";
    }
    if(windspeedmph.length()>0){
        dest = dest + "\"3\":\""+ windspeedmph +"\",";
    }
    if(windgustmph.length()>0){
        dest = dest + "\"4\":\""+ windgustmph +"\",";
    }
    if(status.length()>0){
        dest = dest + "\"s\":\""+ status +"\",";
    }
    if(dateutc.length()>0){
        dest = dest + "\"t\":\""+ dateutc +"\",";
    }
    dest = dest + "\"id\":\"" + wu_stationID + "\",";
    dest = dest + "\"pw\":\"" + wu_password + "\"}";
}
