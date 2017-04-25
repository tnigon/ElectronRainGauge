// This #include statement was automatically added by the Particle IDE.
#include <Adafruit_ADS1X15.h> // For soil moisture sensor
#include <stdint.h> // For a 16-bit data type to measure full dynamic range of ADS output
#include <Adafruit_DHT.h>

// ============================================================
// Time
// ---
// Initiate intervals to determine when to capture sensor
// readings and publish data.
// ============================================================
unsigned int sensorCapturePeriod = (2); // units are seconds
unsigned int publishPeriod = (15);  // units are seconds
unsigned int sensorCount = 1; // I believe this will give us 32 bits of dynamic range
unsigned int publishCount = 1;
time_t initialTime;
time_t scheduledSensor;
time_t scheduledPublish;

String hhmmss(unsigned long int now)  //format value as "hh:mm:ss"
{
   String hour = String(Time.hourFormat12(now));
   String minute = String::format("%02i",Time.minute(now));
   String second = String::format("%02i",Time.second(now));
   return hour + ":" + minute + ":" + second;
};

// ============================================================
// Publish
// ============================================================
unsigned long thingspeakChannelNumber = 256869;
//char thingSpeakWriteAPIKey[] = "0GNFLHX4317NSAH4";
String api_key = "IXQ6TIDEQ72QBH8A"; // ThingSpeak Write API Key.
String field1 = "";
String field2 = "";  // i.e. field2 is null
String field3 = "";
String field4 = "";
String field5 = "";
String field6 = "";
String field7 = "";
String field8 = "";
String status = "";

// ============================================================
// Sensors
// ============================================================
#define DHTPIN 5
#define DHTTYPE DHT11
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
    initialTime = Time.now();
//    if (Time.minute() >= 30) {
//        initialTime = Time.now() + ((59 - Time.minute())*60) + (60 - Time.second()); // Insures initial time will be at top of the hour
//    }
//    if (Time.minute() < 30) {
//        initialTime = Time.now() + ((29 - Time.minute())*60) + (60 - Time.second()); // Insures initial time will be at bottom of the hour
//    }
    // Add code to insure initial time is at top of hour
    scheduledSensor = initialTime + (sensorCapturePeriod); // Schedule initial sensor reading and publish events
    scheduledPublish = initialTime + publishPeriod;
    Particle.publish("Initial system time is: ", hhmmss(Time.now()));
    Particle.publish("Data will begin being collected at: ", hhmmss(initialTime));
    // ============================================================
    // Sensors
    // ============================================================
    dht.begin(); // Start DHT sensor
    initializeRainGauge();
    initializeAnemometer();
    ads1115.begin();  // Initialize ads1115
    ads1115.setGain(GAIN_ONE); // 16x gain +/- 0.256V 1 bit = 0.125mV

}

void loop() {
    // ============================================================
    // Light
    // ============================================================
    blinkLight();
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
        sensorCount++;
        scheduledSensor = initialTime + ((sensorCount*sensorCapturePeriod));
    }

    // ============================================================
    // Publish
    // ============================================================
    if (Time.now() >= scheduledPublish) {
        float tempC = getAndResetTempC(); // Get the data to be published
        float humidityRH = getAndResetHumidityRH();
        float light = getAndResetLight();
        float soilMoisture = getAndResetSoilMoisture();
        float rain_mm = getAndResetRain();
        float gust;
        float wind_mps = getAndResetAnemometer(&gust);
        FuelGauge fuel;
//        float voltage = fuel.getVCell(); // Battery voltage
        float bat_percent = fuel.getSoC(); // State of Charge from 0-100%

//        Particle.publish("System clock is: ", hhmmss(Time.now()));
//        publishToParticle(tempC,humidityRH,light,rain_mm,soilMoisture,wind_mps,gust);
        publishToThingSpeak(tempC,humidityRH,light,soilMoisture,rain_mm,wind_mps,gust,bat_percent);
        publishCount++;
        scheduledPublish = initialTime + (publishCount*publishPeriod);
    }
    delay(10);
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
        // save the last time you blinked the LED
//        previousMillis = currentMillis;

        // if the LED is on, turn it off
        if (ledState == HIGH) {
            ledState = LOW;
        }

        digitalWrite(led, ledState);
    }
}

void publishToParticle(float tempC, float humidityRH, float light, float soilMoisture, float rain_mm, float wind_mps, float gust) {
    Particle.publish("Weather",
                        String::format("%0.2f°C, %0.2f%%, %0.2f%%, %0.1f cbar, %0.2f mm, %0.1f mps, %0.1f mps",
                            tempC,humidityRH,light,soilMoisture,rain_mm),60,PRIVATE);
}

void publishToThingSpeak(float tempC,float humidityRH,float light,float soilMoisture,float rain_mm,float wind_mps,float gust, float bat_percent) {
    field1 = String(tempC,2);
    field2 = String(humidityRH,2);
    field3 = String(light,2);
    field4 = String(soilMoisture,2);
    field5 = String(rain_mm,2);
    field6 = String(wind_mps,2);
    field7 = String(gust,2);
    field8 = String(bat_percent,2);

    String TSjson;
    createThinkSpeakjson(TSjson);
    Particle.publish("webhook_electron",TSjson,60,PRIVATE);
    // Note: In order to publish a private event, you must pass all four parameters.
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
float light_read;
float humidityRHTotal = 0.0;
unsigned int humidityRHReadingCount = 0;
float tempCTotal = 0.0;
unsigned int tempCReadingCount = 0;
float lightTotal = 0.0;
unsigned int lightReadingCount = 0;

int16_t soilMoisture_read; // 16-bit variable for ADS output at channel #3
float soilMoistureTotal;
unsigned int soilMoistureReadingCount;

void captureTempHumidity() {
    humidityRH_read = dht.getHumidity();
    // Check that result is reasonable
    if(humidityRH_read > 0 && humidityRH_read < 105) // Supersaturation could cause humidity > 100%
    {
        humidityRHTotal += humidityRH_read; // Add measurement to running summed value
        humidityRHReadingCount++; // Increment measurement count
    }

    tempC_read = dht.getTempCelcius();
    if(tempC_read > -60 && tempC_read < 70)
    {
        tempCTotal += tempC_read;
        tempCReadingCount++;
    }
    return;
}

void captureLight() {
    light_read = analogRead(light_sensor_pin);
    lightTotal += light_read;
    lightReadingCount++;
}

void captureSoilMoisture() {
//    adc3 = ((0.0831*(ads1115.readADC_SingleEnded(3)))-0.3188); // Convert voltage signal (mV) from A3 input on ADS1115 to the matric tension (cbar)
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

float getAndResetLight()
{
    if(lightReadingCount == 0) {
        return -1;
    }
    float result = (1-(float(lightTotal)/(4096*float(lightReadingCount))))*100;
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
volatile unsigned int rainEventCount; // volatile becuase it is part of an interrupt
//unsigned int lastRainEvent;
unsigned long timeRainEvent;
float RainScale_mm = 0.25; // Each pulse is 0.25 mm of rain

void initializeRainGauge() {
    pinMode(RainPin, INPUT); // INPUT_PULLUP provides a "digital resistor" between signal and ground
    rainEventCount = 0;
    lastRainEvent = millis();
    attachInterrupt(digitalPinToInterrupt(RainPin), handleRainEvent, RISING);
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
    float result = RainScale_mm * float(rainEventCount);
    rainEventCount = 0;
    return result;
}

// ============================================================
// Wind Speed (Anemometer)
// ---
// The Anemometer generates a frequency relative to the
// windspeed: 1Hz: 1.492MPH, 2Hz: 2.984MPH, etc. The average
// time elaspsed between pulses is measured, then average
// windspeed is recored.
// ============================================================

float Anemometer_m_per_sec = 0.6667; // Windspeed if we got a pulse every second (i.e. 1Hz)
volatile unsigned int anemoneterTotal = 0;
volatile unsigned int anemoneterCount = 0;
volatile unsigned int GustPeriod = UINT_MAX;
unsigned int lastAnemoneterEvent = 0;

void initializeAnemometer() {
    pinMode(anemometerPin, INPUT_PULLUP);
    anemoneterTotal = 0;
    anemoneterCount = 0;
    GustPeriod = UINT_MAX;  //  The shortest period (and therefore fastest gust) observed
    lastAnemoneterEvent = 0;
    attachInterrupt(anemometerPin, handleAnemometerEvent, FALLING);
    return;
}

void handleAnemometerEvent() {
    // Activated by the magnet in the anemometer (2 ticks per rotation), attached to input D3
    unsigned int timeAnemometerEvent = millis(); // grab current time

    if(lastAnemoneterEvent != 0) {
        unsigned int period = timeAnemometerEvent - lastAnemoneterEvent; // Calculate time since last event
        // ignore switch-bounce glitches less than 10mS after initial edge (which implies a max windspeed of 149mph)
        if(period < 10) {
            return;
        }
        if(period < GustPeriod) {
            GustPeriod = period; // Shortest period equates to fastest windspeed --> record it
        }
        anemoneterTotal += period;
        anemoneterCount++;
    }
    lastAnemoneterEvent = timeAnemometerEvent; // set up for next event
}

float getAndResetAnemometer(float * gust)
{
    if(anemoneterCount == 0)
    {
        *gust = 0.0;
        return 0;
    }
    // Nonintuitive math:  We've collected the sum of the observed periods between pulses, and the number of observations.
    // Now, we calculate the average period (sum / number of readings), take the inverse and muliple by 1000 to give frequency, and then mulitply by our scale to get appropriate units.
    // The math below is transformed to maximize accuracy by doing all muliplications BEFORE dividing.
    float result = Anemometer_m_per_sec * 1000.0 * float(anemoneterCount) / float(anemoneterTotal);
    anemoneterTotal = 0;
    anemoneterCount = 0;
    *gust = Anemometer_m_per_sec  * 1000.0 / float(GustPeriod);
    GustPeriod = UINT_MAX;
    return result;
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
// Builds the JSON to trigger the Webhook
// Example format:
// dest = "{ \"k\":\"" + api_key + "\", \"1\":\""+ field1 ...
// ============================================================
void createThinkSpeakjson(String &dest) {
  // dest = "{ \"k\":\"" + api_key + "\", \"1\":\""+ field1 +"\", \"2\":\""+ field2 +"\",\"3\":\""+ field3 +"\",\"4\":\""+ field4 +"\",\"5\":\""+ field5 +"\",\"6\":\""+ field6 +"\",\"7\":\""+ field7 +"\",\"8\":\""+ field8 +"\",\"a\":\""+ lat +"\",\"o\":\""+ lon +"\",\"e\":\""+ el +"\", \"s\":\""+ status +"\"}";
    dest = "{";

    if(field1.length()>0){
        dest = dest + "\"1\":\""+ field1 +"\",";
    }

    if(field2.length()>0){
        dest = dest + "\"2\":\""+ field2 +"\",";
    }

    if(field3.length()>0){
        dest = dest + "\"3\":\""+ field3 +"\",";
    }

    if(field4.length()>0){
        dest = dest + "\"4\":\""+ field4 +"\",";
    }

    if(field5.length()>0){
        dest = dest + "\"5\":\""+ field5 +"\",";
    }

    if(field6.length()>0){
        dest = dest + "\"6\":\""+ field6 +"\",";
    }

    if(field7.length()>0){
        dest = dest + "\"7\":\""+ field7 +"\",";
    }

    if(field8.length()>0){
        dest = dest + "\"8\":\""+ field8 +"\",";
    }

    if(status.length()>0){
        dest = dest + "\"s\":\""+ status +"\",";
    }

    dest = dest + "\"k\":\"" + api_key + "\"}";
}
