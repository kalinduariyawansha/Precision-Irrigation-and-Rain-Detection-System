/*
============================================================
SMART IRRIGATION SYSTEM
Final Professional Version

Hardware
---------
Arduino UNO
16x2 LCD
DHT11
Soil Moisture Sensor
Rain Sensor
Relay
Water Pump
LED
Buzzer

============================================================
*/

#include <LiquidCrystal.h>
#include <DHT.h>

#define DHTTYPE DHT11

//================ LCD =================

LiquidCrystal lcd(2,3,4,5,6,7);

//================ INPUTS ==============

#define SOIL_PIN     A0
#define DHT_PIN      9
#define RAIN_PIN     10

//================ OUTPUTS =============

#define RELAY_PIN    8
#define BUZZER_PIN   11
#define LED_PIN      12


DHT dht(DHT_PIN,DHTTYPE);

//==================================================
// CALIBRATION
//==================================================



const int DRY = 920;
const int WET = 310;

const int PUMP_ON_LEVEL = 30;
const int PUMP_OFF_LEVEL = 35; 

//==================================================
// SENSOR VALUES
//==================================================

int soilRaw=0;
int soilPercent=0;

float temperature=0;
float humidity=0;

bool rain=false;
bool sensorError=false;

bool highTempAlarmActive = false;
bool highHumidityAlarmActive = false;

//==================================================
// SYSTEM STATES
//==================================================

enum STATE
{
    IDLE,
    DRY_SOIL,
    RAIN_WAIT,
    RAIN_CONTINUE,
    IRRIGATION_COMPLETE,
    ERROR_STATE
};

STATE currentState=IDLE;

//==================================================
// PUMP VARIABLES
//==================================================

bool pumpRunning=false;

bool rainStartedWhilePumpRunning=false;

bool pumpStartBeepDone=false;

bool pumpStopBeepDone=false;

//==================================================
// LCD
//==================================================

String lcdLine1="";
String lcdLine2="";

String statusLine1 = "SYSTEM READY";
String statusLine2 = "MONITORING";

String oldLine1="";
String oldLine2="";

String irrigationLine1="";
String irrigationLine2="";

byte lcdPage = 0;
byte maxPage = 1;

//==================================================
// TIMERS
//==================================================

unsigned long previousSoil=0;
unsigned long previousDHT=0;
unsigned long previousLCD=0;
unsigned long previousBlink=0;
unsigned long previousBuzzer=0;

const unsigned long SOIL_INTERVAL=500;
const unsigned long DHT_INTERVAL=2000;
const unsigned long LCD_INTERVAL=2000;

//==================================================
// BLINK VARIABLES
//==================================================

bool ledBlinkState=false;
bool buzzerBlinkState=false;

//==================================================
// FUNCTION PROTOTYPES
//==================================================

void startupAnimation();

void readSensors();

void determineState();

void controlOutputs();



void relayON();

void relayOFF();

void pumpON();

void pumpOFF();

void showStatus(String,String);

void doubleBeep();

void singleBeep();

void highTemperatureAlarm();

void sensorErrorAlarm();

void setup()
{

Serial.begin(9600);

pinMode(RELAY_PIN,OUTPUT);
digitalWrite(RELAY_PIN, LOW);

pinMode(BUZZER_PIN,OUTPUT);

pinMode(LED_PIN,OUTPUT);

pinMode(RAIN_PIN,INPUT);

digitalWrite(RELAY_PIN,LOW);

digitalWrite(LED_PIN,LOW);

digitalWrite(BUZZER_PIN,LOW);

lcd.begin(16,2);

dht.begin();

startupAnimation();

}

void loop()
{
    readSensors();
    determineState();
    controlOutputs();
    updateLCD();
}

void startupAnimation()
{

lcd.clear();

lcd.setCursor(1,0);

lcd.print("SMART SYSTEM");

lcd.setCursor(2,1);

lcd.print("Starting");

for(int i=0;i<3;i++)
{

lcd.print(".");

delay(400);

}

lcd.clear();

lcd.setCursor(0,0);

lcd.print("Loading");

lcd.setCursor(0,1);

for(int i=0;i<16;i++)
{

lcd.write(byte(255));

delay(100);

}

lcd.clear();

lcd.setCursor(1,0);

lcd.print("SYSTEM READY");

lcd.setCursor(2,1);

lcd.print("WELCOME");

delay(1000);

lcd.clear();

}

void readSensors()
{

    //==========================
    // Soil Sensor (500 ms)
    //==========================

    if (millis() - previousSoil >= SOIL_INTERVAL)
    {
        previousSoil = millis();

        soilRaw = analogRead(SOIL_PIN);
        Serial.println(soilRaw);

        soilPercent = map(soilRaw, DRY, WET, 0, 100);

        soilPercent = constrain(soilPercent, 0, 100);

        rain = (digitalRead(RAIN_PIN) == HIGH);
    }

    //==========================
    // DHT11 (2 seconds)
    //==========================

    if (millis() - previousDHT >= DHT_INTERVAL)
    {
        previousDHT = millis();

        float h = dht.readHumidity();
        float t = dht.readTemperature();

        if (isnan(h) || isnan(t))
        {
            sensorError = true;
        }
        else
        {
            sensorError = false;

            humidity = h;
            temperature = t;

            highTempAlarmActive = (temperature > 35.0);
            highHumidityAlarmActive = (humidity > 80.0);
        }
    }

}

void determineState()
{

    //==============================
    // Highest Priority
    // Sensor Error
    //==============================

    if(sensorError)
    {
        currentState = ERROR_STATE;
        return;
    }

    //==============================
    // High Temperature
    //==============================

    

    //=========================================
    // Pump already running
    //=========================================

    if(pumpRunning)
    {

        // Rain begins while watering

        if(rain)
        {
            rainStartedWhilePumpRunning = true;
        }

        // Continue watering until 30%

        if(soilPercent < PUMP_OFF_LEVEL)
        {

            if(rainStartedWhilePumpRunning)
            {
                currentState = RAIN_CONTINUE;
            }
            else
            {
                currentState = DRY_SOIL;
            }

            return;

        }

        // Soil reached target

        currentState = IRRIGATION_COMPLETE;

        return;

    }

    //=========================================
    // Pump NOT running
    //=========================================

    if(soilPercent < PUMP_ON_LEVEL)
    {
        // If rain is ON but the soil is critically dry,
        // allow irrigation to start again.
        currentState = DRY_SOIL;
        return;
    }

    //=========================================
    // Soil Normal
    //=========================================

    currentState = IDLE;

}

void controlOutputs()
{

    switch(currentState)
    {

        //------------------------------------------------
        // CASE 1
        //------------------------------------------------

        case DRY_SOIL:

            pumpON();

            if(!pumpStartBeepDone)
            {

                doubleBeep();

                pumpStartBeepDone = true;

                pumpStopBeepDone = false;

            }

            showStatus("SOIL DRY","PUMP ON");

        break;

        //------------------------------------------------
        // CASE 2
        //------------------------------------------------

        case RAIN_CONTINUE:

            pumpON();

            showStatus("RAIN DETECTED","LOW SOIL-PUMP");

        break;

        //------------------------------------------------
        // CASE 3
        //------------------------------------------------

        case IRRIGATION_COMPLETE:

            pumpOFF();

            if(!pumpStopBeepDone)
            {

                singleBeep();

                pumpStopBeepDone = true;

                pumpStartBeepDone = false;

            }

            if(rain)
            {

                showStatus("RAIN DETECTED","PUMP OFF");

            }
            else
            {

                showStatus("SOIL NORMAL","PUMP OFF");

            }

        break;

        //------------------------------------------------
        // CASE 4
        //------------------------------------------------

        case RAIN_WAIT:

            pumpOFF();

            pumpStartBeepDone = false;

            pumpStopBeepDone = false;

            showStatus("RAIN DETECTED","WAITING");

        break;

        //------------------------------------------------
        // IDLE
        //------------------------------------------------

        case IDLE:

            pumpOFF();

            pumpStartBeepDone = false;

            pumpStopBeepDone = false;

            showStatus("SOIL NORMAL","PUMP OFF");

        break;

        
        case ERROR_STATE:

            relayOFF();

            pumpRunning = false;

        break;

        default:

        break;

    }

    //----------------------------------------------------
    // Sensor Error
    //----------------------------------------------------

    if(sensorError)
    {
        sensorErrorAlarm();
        showStatus("SENSOR ERROR","CHECK SYSTEM");
        return;
    }

    //----------------------------------------------------
    // Relay always follows pump state
    //----------------------------------------------------

    if(pumpRunning)
        relayON();
    else
        relayOFF();

    //----------------------------------------------------
    // LED / Temperature Alarm
    //----------------------------------------------------

    if(highTempAlarmActive)
    {
        highTemperatureAlarm();
    }
    else
    {
        buzzerBlinkState = false;
        ledBlinkState = false;

        noTone(BUZZER_PIN);

        digitalWrite(LED_PIN, pumpRunning ? HIGH : LOW);
    }
}

void updateLCD()
{

    if(sensorError)
    {
        lcdLine1 = "SENSOR ERROR";
        lcdLine2 = "CHECK SYSTEM";
    }

    else
    {

        if(millis()-previousLCD>=LCD_INTERVAL)
        {
            previousLCD=millis();

            maxPage = 1;

            if(highTempAlarmActive)
                maxPage = 3;

            if(highHumidityAlarmActive)
                maxPage = 5;

            lcdPage++;

            if(lcdPage > maxPage)
                lcdPage = 0;
        }

        switch(lcdPage)
        {

        case 0:

            lcdLine1="S:"+String(soilPercent)+"%";

            lcdLine1+=" T:"+String((int)temperature)+"C";

            lcdLine2="H:"+String((int)humidity)+"%";

            lcdLine2+=" R:";

            lcdLine2+=(rain?"Y":"N");

        break;

        case 1:

            lcdLine1=irrigationLine1;

            lcdLine2=irrigationLine2;

        break;

        case 2:

            lcdLine1="S:"+String(soilPercent)+"%";

            lcdLine1+=" T:"+String((int)temperature)+"C";

            lcdLine2="H:"+String((int)humidity)+"%";

            lcdLine2+=" R:";

            lcdLine2+=(rain?"Y":"N");

        break;

        case 3:

            if(highTempAlarmActive)
            {
                lcdLine1="HIGH TEMP";
                lcdLine2="CHECK FIELD";
            }
            else
            {
                lcdPage=4;
            }

        break;

        case 4:

            lcdLine1="S:"+String(soilPercent)+"%";

            lcdLine1+=" T:"+String((int)temperature)+"C";

            lcdLine2="H:"+String((int)humidity)+"%";

            lcdLine2+=" R:";

            lcdLine2+=(rain?"Y":"N");

        break;

        case 5:

            if(highHumidityAlarmActive)
            {
                lcdLine1="HIGH HUMIDITY";
                lcdLine2="MONITORING";
            }
            else
            {
                lcdPage=0;
            }

        break;

        }

    }

    if(lcdLine1!=oldLine1)
    {

        lcd.setCursor(0,0);
        lcd.print("                ");
        lcd.setCursor(0,0);
        lcd.print(lcdLine1);

        oldLine1=lcdLine1;

    }

    if(lcdLine2!=oldLine2)
    {

        lcd.setCursor(0,1);
        lcd.print("                ");
        lcd.setCursor(0,1);
        lcd.print(lcdLine2);

        oldLine2=lcdLine2;

    }

}

void relayON()
{
    digitalWrite(RELAY_PIN, HIGH);
}

void relayOFF()
{
    digitalWrite(RELAY_PIN, LOW);
}

void pumpON()
    {
        pumpRunning = true;
    }

void pumpOFF()
    {
        pumpRunning = false;
        rainStartedWhilePumpRunning = false;
    }

void showStatus(String line1, String line2)
{

    irrigationLine1 = line1;
    irrigationLine2 = line2;

}

void doubleBeep()
{

    tone(BUZZER_PIN,2000);

    delay(120);

    noTone(BUZZER_PIN);

    delay(100);

    tone(BUZZER_PIN,2000);

    delay(120);

    noTone(BUZZER_PIN);

}

void singleBeep()
{

    tone(BUZZER_PIN,2000);

    delay(200);

    noTone(BUZZER_PIN);

}

void highTemperatureAlarm()
{
    if(!highTempAlarmActive)
    {
        noTone(BUZZER_PIN);
        return;
    }

    if(millis() - previousBlink >= 300)
    {
        previousBlink = millis();

        ledBlinkState = !ledBlinkState;

        digitalWrite(LED_PIN, ledBlinkState);
    }

    if(millis() - previousBuzzer >= 300)
    {
        previousBuzzer = millis();

        buzzerBlinkState = !buzzerBlinkState;

        if(buzzerBlinkState)
            tone(BUZZER_PIN, 1800);
        else
            noTone(BUZZER_PIN);
    }

}

void sensorErrorAlarm()
{
    if(!highTempAlarmActive)
    {
        noTone(BUZZER_PIN);
        return;
    }
    
    if(millis() - previousBlink >= 100)
    {
        previousBlink = millis();

        ledBlinkState = !ledBlinkState;

        digitalWrite(LED_PIN, ledBlinkState);
    }

    if(millis() - previousBuzzer >= 100)
    {
        previousBuzzer = millis();

        buzzerBlinkState = !buzzerBlinkState;

        if(buzzerBlinkState)
            tone(BUZZER_PIN,2500);
        else
            noTone(BUZZER_PIN);
    }

}
