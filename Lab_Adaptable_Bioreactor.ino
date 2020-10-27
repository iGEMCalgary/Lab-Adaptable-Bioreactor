//# Lab-Adaptable-Bioreactor
//By Mackenzie Sampson
//iGEM 2020 Calgary

//This code is for an automated airlift bioreactor.

//This code was designed with an airlift bioreactor that is capable of sensing dissolved Oxygen, pH, and Temperature. 
//While also regulating temperature and dissolved oxygen with a heater and pump.

//Components list is as follows:

//Arduino Uno
//Geekstory Micro SD Card Module
//Alertseal 32Gb Micro SD Card
//Tongling 2-Channel Relay
//KeeYees LM2596 Step-Down Buck Converter
//12V 5A AC to DC Transformer
//Gikfun DS1820 Temperature Sensor
//Immersion Water Heater 1000 Watt
//Gravity: Analog pH Sensor Pro
//Gravity: Analog Dissolved O2 Sensor
//Ecoplus 728450 Air Pump 3000L/H 

//Please refer to DFRobots website for more information regarding the Gravity: 
//Analog pH Sensor Pro and the Gravity: Analog Dissolved O2 Sensor.
//
//
//Visit https://2020.igem.org/Team:Calgary/Bioreactor to learn more about this project
//
//
//Operation instructions
//
//
//pH meter calibraition - At about once per month, it is recommended that the pH meter be calibrated

    //Steps are as follows:
      //1. Input ENTER command in the serial monitor to enter the calibration mode.
      //
      //2. Input CAL commands in the serial monitor to start the calibration. The program will automatically 
      //   identify which of the two standard buffer solutions is present: either 4.0 and 7.0 In this step, the 
      //   standard buffer solution of 4.0 will be identified.
      //
      //3. After the calibration, input the EXIT command in the serial monitor to save the relevant parameters 
      //   and exit the calibration mode. Note: Only after inputing EXIT command in the serial monitor can the 
      //   relevant parameters be saved.

//Dissolved oxygen sensor calibration - for first time use or once a month, it must be calibrated and cap 
//refilled with NAOH at this time

    //single point callibration - easier
      //1. Prepare the probe
      //
      //2. Wet the probe in pure water and shake off excess water drops
      //
      //3. Expose the probe to the air and maintain proper air flow (do not use a fan to blow)
      //
      //4. After the output voltage is stable, record the voltage, which is the saturated dissolved oxygen voltage 
      //   at the current temperature

    //two-point callibration - more precise
      //1. Prepare two cups of purified water, put one cup in the refrigerator, and one cup to warm up 
      //   (Do not exceed 40°C, otherwise the probe may be damaged.)
      //
      //2. Use one of the following methods to make saturated oxygen water.
      //    A: Use a stirrer or an eggbeater to continuously stir for 10 minutes to saturate the dissolved oxygen.
      //    B: Use an air pump to continuously inflate the water for 10 minutes to saturate the dissolved oxygen.
      //
      //3. Stop stirring or pumping, and put the probe after the bubbles disappear.
      //
      //4. After placing the probe, keep stirring slowly while avoiding any bubbles.
      //
      //5. After the output voltage stable, record the temperature and voltage.
      //
      //6. Perform the same operation on another glass of water. Measure and record the temperature and voltage.


#include <Arduino.h>

#include <SPI.h> //SPI communication between the arduino and the SD card
#include <SD.h> //SD card library
#include <EEPROM.h> //reading and writing to storage library

#include <Wire.h> //I2C communication between the arduino and the Real Time Clock library
#include <Sodaq_DS3231.h> //Real Time Clock library

#include <DallasTemperature.h> //temperature sensor library 
#include <OneWire.h> //temperature sensor library by Jim Studt

#include <DFRobot_PH.h> //pH sensor library


//set up file for SD card
    File reactor_data;
    DateTime now;

//initialize time stamps
    int newHour = 0;
    int oldHour = 0;


///Arduino Pins
    #define PUMP 3 //Electrical outlet - attached to relay - Air pump
    #define HEATER 4 //Heating element attached to relay
    #define TEMPERATURE 6 //Temperature sensor
    #define SD_CARD 10 //CS
    #define OXYGEN A2 //Dissolved oxygen sensor -analog
    #define PH_PIN A0 // pH sensor - analog
    


//Temperature
    // Create a new instance of the oneWire class to communicate with any OneWire device:
    OneWire oneWire(TEMPERATURE);
    // Pass the oneWire reference to DallasTemperature library:
    DallasTemperature sensors(&oneWire);

    int temperature_c = 0;

//pH meter
    float voltage;
    float ph_value;
    DFRobot_PH pH;

//Dissolved oxygen sensor. Use this code if this is the first time using the sensor 
    int dissolved_oxygen = 0;
    
    #define VREF 5000    //VREF (mv)
    #define ADC_RES 1024 //ADC Resolution
    
    //Single-point calibration Mode=0
    //Two-point calibration Mode=1
    #define TWO_POINT_CALIBRATION 0
    
    //Single point calibration needs to be filled CAL1_V and CAL1_T
    #define CAL1_V (1600) //mv
    #define CAL1_T (25)   //℃
    
    //Two-point calibration needs to be filled CAL2_V and CAL2_T
    //CAL1 High temperature point, CAL2 Low temperature point
    #define CAL2_V (1300) //mv
    #define CAL2_T (15)   //℃
    
    const uint16_t DO_Table[41] = {
        14460, 14220, 13820, 13440, 13090, 12740, 12420, 12110, 11810, 11530,
        11260, 11010, 10770, 10530, 10300, 10080, 9860, 9660, 9460, 9270,
        9080, 8900, 8730, 8570, 8410, 8250, 8110, 7960, 7820, 7690,
        7560, 7430, 7300, 7180, 7070, 6950, 6840, 6730, 6630, 6530, 6410};
    
    uint8_t Temperaturet;
    uint16_t ADC_Raw;
    uint16_t ADC_Voltage;
    uint16_t DO;
    
    int16_t readDO(uint32_t voltage_mv, uint8_t temperature_c)
    {
    #if TWO_POINT_CALIBRATION == 0
      uint16_t V_saturation = (uint32_t)CAL1_V + (uint32_t)35 * temperature_c - (uint32_t)CAL1_T * 35;
      return (voltage_mv * DO_Table[temperature_c] / V_saturation);
    #else
      uint16_t V_saturation = (int16_t)((int8_t)temperature_c - CAL2_T) * ((uint16_t)CAL1_V - CAL2_V) / ((uint8_t)CAL1_T - CAL2_T) + CAL2_V;
      return (voltage_mv * DO_Table[temperature_c] / V_saturation);
    #endif
    }

void setup() {
  
  Wire.begin();
  rtc.begin();
  Serial.begin(9600); 
  sensors.begin(); //temperature sensor
  pH.begin();//pH sensor


//
//Set up pins
//
    pinMode(TEMPERATURE, INPUT_PULLUP);//The digital pin for the temperature sensor requires a resistor, this pulls one up from the board
  
    pinMode(PUMP, OUTPUT); //relay
    digitalWrite(PUMP, HIGH); //keeps pump on unless otherwise called
  
    pinMode(HEATER, OUTPUT); //relay
    digitalWrite(HEATER, LOW); //keeps heater off unless otherwise called


//
//Set up SD card
//
    Serial.print("Initializing SD card...");
    if (!SD.begin(SD_CARD)) {
    Serial.println("initialization failed!");
    while (1);
    }
    Serial.println("initialization done.");
    
    reactor_data = SD.open("Bioreactor_Data.txt", FILE_WRITE);
    
    now = rtc.now();
    oldHour = now.hour();

}

void loop() {

//
//Run temperature
//
    //Communicate with sensor to get reading
    sensors.requestTemperatures();
    //Fetch the temperature in degrees Celsius for device index
    float temperature_c = sensors.getTempCByIndex(0); //the index 0 refers to the first device
    
    Serial.print("Temperature: ");
    Serial.print(temperature_c);
    Serial.println(" \xC2\xB0"); //shows degree symbol

//
//Run pH meter
//
    voltage = analogRead(PH_PIN)/1024.0*5000;  // read the voltage
    
    ph_value = pH.readPH(voltage,temperature_c);  // convert voltage to pH with temperature compensation
    
    Serial.print("pH:");
    Serial.println(ph_value,2);
  
    pH.calibration(voltage,temperature_c);  // calibration process by serial CMD

//
//Run dissolved oxygen sensor
//
   
    ADC_Raw = analogRead(OXYGEN);
    ADC_Voltage = uint32_t(VREF) * ADC_Raw / ADC_RES;
    dissolved_oxygen = readDO(ADC_Voltage, temperature_c);
    
    Serial.print("ADC RAW:\t" + String(ADC_Raw) + "\t");
    Serial.print("ADC Voltage:\t" + String(ADC_Voltage) + "\t");
    Serial.println("DO:\t" + String(readDO(ADC_Voltage, temperature_c)) + "\t");

//
//Run data save
//
    now = rtc.now();
    newHour = now.hour();
    if (oldHour != newHour) {
    save_data();
    oldHour = newHour;
    }

////////////////////////////////////////////////////////////////////////////////////////
///////////////////                  Run automation                  ///////////////////
////////////////////////////////////////////////////////////////////////////////////////

    //Modify this code to suit your reactor conditions

    ////////TEMPERATURE SET//////// 
    if (temperature_c < 25){ //<------Change this to maintain a desired temperature or comment out if no temperature regulation is required
      digitalWrite(HEATER, HIGH); //turns it on
      delay(10000); //this is 10 seconds 
      digitalWrite(HEATER,LOW); 
    }
    else{
      digitalWrite(HEATER, LOW);
      
    }

    ///////DISSOLVED OXYGEN SET//////
//    if (dissolved_oxygen > 5000){ //<--- Change this to maintain a desired oxygenation level
//      digitalWrite(PUMP, LOW);

//    else {
//      digitalWrite(PUMP,HIGH);

////////////////////////////////////////////////////////////////////////////////////////
///////////////////                  Run automation                  ///////////////////
////////////////////////////////////////////////////////////////////////////////////////
  
    delay(1000);
}

//save all the data to the SD card
void save_data(){

   reactor_data = SD.open("Bioreactor_Data.txt", FILE_WRITE);
   now = rtc.now();
  
    reactor_data.print(now.hour());
    reactor_data.print(":");
    reactor_data.print(now.minute());
    reactor_data.print(",");
    reactor_data.print(dissolved_oxygen);
    reactor_data.print(",");
    reactor_data.print(ph_value);
    reactor_data.print(",");
    reactor_data.println(temperature_c);
    reactor_data.close();
  
}
