/*
What: LEDLightBoxAlnitak - PC controlled lightbox implmented using the 
  Alnitak (Flip-Flat/Flat-Man) command set found here:
  http://www.optecinc.com/astronomy/pdf/Alnitak%20Astrosystems%20GenericCommandsR3.pdf

Who: 
  Created By: Jared Wellman - jared@mainsequencesoftware.com

When: 
  Last modified:  2013/May/05
                  2021/Juin/29  (SCL)


Typical usage on the command prompt:
Send     : >S000\n      //request state
Recieve  : *S19000\n    //returned state

Send     : >B128\n      //set brightness 128
Recieve  : *B19128\n    //confirming brightness set to 128

Send     : >J000\n      //get brightness
Recieve  : *B19128\n    //brightness value of 128 (assuming as set from above)

Send     : >L000\n      //turn light on (uses set brightness value)
Recieve  : *L19000\n    //confirms light turned on

Send     : >D000\n      //turn light off (brightness value should not be changed)
Recieve  : *D19000\n    //confirms light turned off.
*/

#include <Servo.h>
#include <EEPROM.h>

#define SERVOMIN 5
#define SERVOMAX 250

#define PINSERVO 11 //5  // Choix: 3,5,6,9,10,11 (PWM)
#define PINFLAT   8 //3  // Choix: 3,5,6,9,10,11
#define PINBUTTON 2

Servo myservo;  // create servo object to control a servo

int pos = 0;
int brightness = 0;

enum devices
{
  FLAT_MAN_L = 10, // Flat-Man_XL
  FLAT_MAN_XL = 15, // Flat-Man_L
  FLAT_MAN = 19, // Flat-Man
  FLIP_DUST = 98,  // Flip-Mask/Remote Dust Cover
  FLIP_FLAT = 99 // Flip-Flat
};

enum motorStatuses
{
  STOPPED = 0,
  RUNNING
};

enum lightStatuses
{
  OFF = 0,
  ON
};

enum shutterStatuses
{
  UNKNOWN = 0, // ie not open or closed...could be moving
  OPEN = 1,
  CLOSED = 2
};


int deviceId = FLIP_DUST;
int motorStatus = STOPPED;
int lightStatus = OFF;
int coverStatus = CLOSED;

void setup()
{
  // Initialisation du bouton
  pinMode(PINBUTTON,INPUT_PULLUP);
  // initialize the serial communication:
  Serial.begin(38400);
  // initialize the ledPin as an output:
  pinMode(PINFLAT, OUTPUT);
  analogWrite(PINFLAT, 0);
  // Lecture de la position du couvercle
  EEPROM.get(0,coverStatus);
  if (coverStatus != OPEN && coverStatus != CLOSED) {
    coverStatus=CLOSED;
    EEPROM.put(0,coverStatus);
  }
  myservo.write(( coverStatus==CLOSED)?SERVOMIN:SERVOMAX);
  myservo.attach(PINSERVO); // серва подключена к 3 пину
  delay(500);
}

void loop() 
{
  handleSerial();
  if (!digitalRead(PINBUTTON)) {
    // Bouton appuyé, on change d'état le couvercle
    if (coverStatus==CLOSED) {  
      SetShutter(OPEN);
    }
    else {
      SetShutter(CLOSED);
    }
  }
}

void handleSerial()
{
  if( Serial.available() >= 5 )  // all incoming communications are fixed length at 6 bytes including the \n
  { 
    char* cmd;
    char* data;
    char temp[10];
    
    int len = 0;

    char str[20];
    memset(str, 0, 20);
    
  // I don't personally like using the \n as a command character for reading.  
  // but that's how the command set is.
  Serial.readBytesUntil('\n', str, 20);
  cmd = str + 1;
  data = str + 2;
  // useful for debugging to make sure your commands came through and are parsed correctly.
    if( false )
    {
      sprintf( temp, "cmd = >%c%s;", cmd, data);
      Serial.println(temp);
    } 

    switch( *cmd )
    {
    /*
    Ping device
      Request: >P000\n
      Return : *Pii000\n
        id = deviceId
    */
      case 'P':
      sprintf(temp, "*P%dOOO", deviceId);
      Serial.println(temp);
      break;

      /*
    Open shutter
      Request: >O000\n
      Return : *Oii000\n
        id = deviceId

      This command is only supported on the Flip-Flat!
    */
      case 'O':
      sprintf(temp, "*O%d000", deviceId);
      Serial.println("*O000");
      SetShutter(OPEN);
      break;

      /*
    Close shutter
      Request: >C000\n
      Return : *Cii000\n
        id = deviceId

      This command is only supported on the Flip-Flat!
    */
      case 'C':
      sprintf(temp, "*C%d000", deviceId);
      Serial.println("*C000");
      SetShutter(CLOSED);
      break;

    /*
    Turn light on
      Request: >L000\n
      Return : *Lii000\n
        id = deviceId
    */
      case 'L':
      sprintf(temp, "*L000", deviceId);
      Serial.println(temp);
      lightStatus = ON;
      analogWrite(PINFLAT, brightness);
      break;

    /*
    Turn light off
      Request: >D000\n
      Return : *Dii000\n
        id = deviceId
    */
      case 'D':
      sprintf(temp, "*D000", deviceId);
      Serial.println(temp);
      lightStatus = OFF;
      analogWrite(PINFLAT, 0);
      break;

    /*
    Set brightness
      Request: >Bxxx\n
        xxx = brightness value from 000-255
      Return : *Biiyyy\n
        id = deviceId
        yyy = value that brightness was set from 000-255
    */
      case 'B':
      brightness = atoi(data);    
      if( lightStatus == ON ) 
        analogWrite(PINFLAT, brightness);   
      sprintf( temp, "*B%03d", brightness );
      Serial.println(temp);
        break;

    /*
    Get brightness
      Request: >J000\n
      Return : *Jiiyyy\n
        id = deviceId
        yyy = current brightness value from 000-255
    */
      case 'J':
        sprintf( temp, "*J%03d", brightness);
        Serial.println(temp);
        break;
      
    /*
    Get device status:
      Request: >S000\n
      Return : *SidMLC\n
        id = deviceId
        M  = motor status( 0 stopped, 1 running)
        L  = light status( 0 off, 1 on)
        C  = Cover Status( 0 moving, 1 open, 2 closed)
    */
      case 'S': 
        sprintf( temp, "*S%d%d%d", motorStatus, lightStatus, coverStatus);
        Serial.println(temp);
        break;

    /*
    Get firmware version
      Request: >V000\n
      Return : *Vii001\n
        id = deviceId
    */
      case 'V': // get firmware version
      //sprintf(temp, "*V%d001", deviceId);
      Serial.println("*V001");
      break;

      /**
       * UNKNOW command
       */
        default:
        sprintf( temp, "cmd = >%c%s;", cmd, data);
        Serial.print("Unknow command: ");
        Serial.println(temp);
        break;

    }    

  while( Serial.available() > 0 )
    Serial.read();

  }
}

void SetShutter(int val)
{
  if( val == OPEN && coverStatus != OPEN )
  {
    coverStatus = OPEN;
    for (pos = SERVOMIN; pos <= SERVOMAX; pos++) { 
      myservo.write(pos); 
      delay(12); 
    }
    EEPROM.put(0,coverStatus);
  }
  else if( val == CLOSED && coverStatus != CLOSED )
  {
    coverStatus = CLOSED;
    for (pos = SERVOMAX; pos >= SERVOMIN; pos--) { 
      myservo.write(pos); 
      delay(12); 
    }
    EEPROM.put(0,coverStatus);
  }
  else
  {
    coverStatus = val;
  }
}
