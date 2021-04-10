#include <Arduino.h>
#include <U8g2lib.h>
#include <CheapStepper.h>

U8G2_SSD1306_128X64_NONAME_1_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);

#include <Wire.h> // must be included here so that Arduino library object file references work.
#include <RtcDS1307.h> //oled

RtcDS1307<TwoWire> Rtc(Wire);

CheapStepper stepper (8,9,10,11);  


 // let's also create a boolean variable to save the direction of our rotation
 // and a timer variable to keep track of move times
bool moveClockwise = true;
unsigned long moveStartTime = 0; // this will save the time (millis()) when we started each new move
const int button1Pin = 3; 
const int button2Pin = 4;               
const int button3Pin = 5; //mode
const int stopPin = 6;
const int slideDistance = 8600;

bool showScreen = true;
int screenBlankDelayCount = 0;
int screenBlankEffectDelay = 0;
int buttonStatus = 0;
int doorStatus = 0;
bool positionKnown = false;

long reading=0; //scale
int mode = 0;
int timerCount = 0;
bool button3Pressed = false;
bool button2Pressed = false;
bool button1Pressed = false;
int alarmHr = 0;
int alarmMin = 0;
int clockHr = 0;
int clockMin = 0;
int On = 0;
int Off = 1;

#define ModeDoNothing 0
#define ModeDisplayInit 1
#define ModeInitPos 2
#define ModeDisplayOpening 3
#define ModeRunForOpen 4
#define ModeDisplayClosing 5
#define ModeRunForClose 6
#define ModeInitPosAchieved 7
#define ModeTimeForFood 8
#define ModeEndOfTimeForFood 9
#define ModeError 10

#define DoorStatusUnkonwn 0
#define DoorStatusClosed 1
#define DoorStatusopen 2

#define ButtonStatusOpenClose 0
#define ButtonStatusSetTime 1
#define ButtonStatusSetAlarm 2



//used for oled display
void DrawToOled(int x, int y, const char *s)
{
  u8g2.firstPage();
  do {
    u8g2.drawStr(x,y,s);
    u8g2.drawStr(x,y+9,s);
    //u8g2.drawFrame(0,0,u8g2.getDisplayWidth(),u8g2.getDisplayHeight() );
  } while ( u8g2.nextPage() );
}

#define countof(a) (sizeof(a) / sizeof(a[0]))

void printDateTime(const RtcDateTime& dt)
{
    char datestring[20];
    snprintf_P(datestring, 
            countof(datestring),
            PSTR("%02u/%02u/%04u %02u:%02u:%02u"),
            dt.Month(),
            dt.Day(),
            dt.Year(),
            dt.Hour(),
            dt.Minute(),
            dt.Second() );
    Serial.print(datestring);
    DrawToOled(2, 30, datestring);
}

void printRandom()
{
  int mode = 0;
  char modeString[2];
  int x,y;
    x = random(12, 120);
    y = random(16, 58);
    snprintf_P(modeString, 
            countof(modeString),
            PSTR("%01u"),
            mode);             
              u8g2.firstPage();
  do {   
      u8g2.drawStr(x,y,modeString);
      } while ( u8g2.nextPage() );   
}

void printTimeAndAlarm(const RtcDateTime& dt, const RtcDateTime& alrm, String statusStr, long weight, int mode)
{
    char datestring[10];
    char alarmstring[7];
    char weightString[4];
    char modeString[2];
    int x,y;
    x = 12;
    y = 23;

    snprintf_P(datestring, 
            countof(datestring),
            PSTR("%02u:%02u:%02u"),
            dt.Hour(),
            dt.Minute(),
            dt.Second() );
    Serial.print(datestring);

    snprintf_P(alarmstring, 
            countof(alarmstring),
            PSTR("%02u:%02u"),
            alrm.Hour(),
            alrm.Minute());


  int lenStatusString = statusStr.length();
  char statusstring[lenStatusString+1];
    
  statusStr.toCharArray(statusstring, lenStatusString+1);

  //  snprintf_P(weightString, 
  //          countof(weightString),
  //          PSTR("%03u"),
  //          weight);

    snprintf_P(modeString, 
            countof(modeString),
            PSTR("%01u"),
            mode);            


  u8g2.firstPage();
  do {
    u8g2.setFont(u8g2_font_9x15B_mf);
    u8g2.drawStr(x, y, statusstring);
    u8g2.drawStr(x,y+20,datestring);
    u8g2.setFont(u8g2_font_10x20_mf);
    u8g2.drawStr(x,y+40,alarmstring);
    u8g2.setFont(u8g2_font_10x20_mf);
    //u8g2.drawStr(x+60,y+40,weightString);
    u8g2.setFont(u8g2_font_9x15_mf);
    //u8g2.drawStr(x+85,y,modeString);


    //u8g2.drawFrame(0,0,u8g2.getDisplayWidth(),u8g2.getDisplayHeight() );
  } while ( u8g2.nextPage() );            
}

void ScreenBlank()
{
  u8g2.clear();
}

void resetStepperPins()
{
    digitalWrite(8, LOW);
    digitalWrite(9, LOW);
    digitalWrite(10, LOW);
    digitalWrite(11, LOW);
}

void setup() {

  pinMode(button1Pin, INPUT_PULLUP);
  pinMode(button2Pin, INPUT_PULLUP);
  pinMode(button3Pin, INPUT_PULLUP);
  pinMode(stopPin, INPUT_PULLUP);

  pinMode(LED_BUILTIN, OUTPUT);

  Serial.begin(9600);
   Serial.print("start");

  //oled related
  u8g2.begin();


 
  //Clock related
  #pragma region ClockRelated  

  Serial.print("compiled: ");
  Serial.print(__DATE__);
  Serial.println(__TIME__);
  
  Rtc.Begin();

  RtcDateTime compiled = RtcDateTime(__DATE__, __TIME__);
  printDateTime(compiled);
  Serial.println();

  //clock related code from lib sample 
  if (!Rtc.IsDateTimeValid()) 
  {
      if (Rtc.LastError() != 0)
      {
          // we have a communications error
          // see https://www.arduino.cc/en/Reference/WireEndTransmission for 
          // what the number means
          Serial.print("RTC communications error = ");
          Serial.println(Rtc.LastError());
      }
      else
      {
          // Common Causes:
          //    1) first time you ran and the device wasn't running yet
          //    2) the battery on the device is low or even missing

          Serial.println("RTC lost confidence in the DateTime!");
          // following line sets the RTC to the date & time this sketch was compiled
          // it will also reset the valid flag internally unless the Rtc device is
          // having an issue

          Rtc.SetDateTime(compiled);
      }
  }

  if (!Rtc.GetIsRunning())
  {
      Serial.println("RTC was not actively running, starting now");
      Rtc.SetIsRunning(true);
  }

  RtcDateTime now = Rtc.GetDateTime();
  if (now < compiled) 
  {
      Serial.println("RTC is older than compile time!  (Updating DateTime)");
      Rtc.SetDateTime(compiled);
  }
  else if (now > compiled) 
  {
      Serial.println("RTC is newer than compile time. (this is expected)");
  }
  else if (now == compiled) 
  {
      Serial.println("RTC is the same as compile time! (not expected but all is fine)");
  }
 
  // never assume the Rtc was last configured by you, so
  // just clear them to your needed state
  Rtc.SetSquareWavePin(DS1307SquareWaveOut_Low); 

  #pragma endregion ClockRelated

  #pragma region StepperRelated

  //*********************************************************************
  //stepper ralated code from lib sample
  
  //stepper.setRpm(48);
  Serial.print("stepper RPM: "); Serial.print(stepper.getRpm());
  Serial.println();

  // and let's print the delay time (in microseconds) between each step
  // the delay is based on the RPM setting:
  // it's how long the stepper will wait before each step

  Serial.print("stepper delay (micros): "); Serial.print(stepper.getDelay());
  Serial.println(); Serial.println();

  // now let's set up our first move...
  // let's move a half rotation from the start point

  //stepper.newMoveTo(moveClockwise, 2048);
  /* this is the same as: 
    * stepper.newMoveToDegree(clockwise, 180);
    * because there are 4096 (default) steps in a full rotation
    */
  moveStartTime = millis(); // let's save the time at which we started this move

  #pragma endregion StepperRelated

}


void loop() {

  //Clock *********************************************
  if (!Rtc.IsDateTimeValid()) 
  {
      if (Rtc.LastError() != 0)
      {
          // we have a communications error
          // see https://www.arduino.cc/en/Reference/WireEndTransmission for 
          // what the number means
          Serial.print("RTC communications error = ");
          Serial.println(Rtc.LastError());
      }
      else
      {
          // Common Causes:
          //    1) the battery on the device is low or even missing and the power line was disconnected
          Serial.println("RTC lost confidence in the DateTime!");
      }
  }

  RtcDateTime now = Rtc.GetDateTime();

  Serial.println();
  clockMin = now.Minute();
  clockHr = now.Hour();

// Buttons and states
#pragma region buttonsStates
  int buttonState1 = digitalRead(button1Pin);
  int buttonState2 = digitalRead(button2Pin);
  int buttonState3 = digitalRead(button3Pin);
  int stopPinState = digitalRead(stopPin);


  //Serial.print("B1-");Serial.println(buttonState1);
  //Serial.print("B2-");Serial.println(buttonState2);
  //Serial.print("B3-");Serial.println(buttonState3);
  //Serial.print("S1-");Serial.println(stopPinState);
  

  if(mode==ModeDoNothing || mode == ModeError)
  {
    if((clockMin==alarmMin) && (clockHr==alarmHr) && (now.Second() == 0))
    {
      mode = ModeTimeForFood;
      Serial.println("ALARM");
    }
  }

  if(buttonState3==Off)
  {
    button3Pressed = false;
  }

  if (buttonState3 == On) {
    //mode = 1; // motor runs
    if(!button3Pressed)
    {
      //Sets  mode of the system:
      screenBlankDelayCount = 0;
      switch(buttonStatus) {
        case ButtonStatusSetTime: 
        {
          buttonStatus = ButtonStatusSetAlarm;
          //RtcDateTime SetTime = RtcDateTime(2019,1,1,clockHr,clockMin,0);
          //Rtc.SetDateTime(SetTime);
          break;
        }
        case ButtonStatusSetAlarm:
          buttonStatus = ButtonStatusOpenClose;
          //burada alarm değeri değişmiş ise eproma kayıt etmesi lazım. elektrik gidince yeniden okur
          break;
        case ButtonStatusOpenClose:
          buttonStatus = ButtonStatusSetTime;
          break;
      }                                               
      button3Pressed = true;
    }
  }

  if(buttonState2 == Off)
  { 
    button2Pressed = false;
  }

  if (buttonState2 == On) {
    screenBlankDelayCount = 0;
    if(!button2Pressed)
    {
      if(buttonStatus==ButtonStatusSetAlarm)
      {
        alarmMin = alarmMin + 10;
        if(alarmMin==60) {alarmMin = 0;}
      }
      if(buttonStatus == ButtonStatusSetTime)
      {
        clockMin = now.Minute();
        clockMin = clockMin + 1;
        if(clockMin == 60) {clockMin = 0;}
        RtcDateTime setTime = RtcDateTime(2019, 1, 21, now.Hour(), clockMin, 0);
        Rtc.SetDateTime(setTime);
      }
    }
    button2Pressed = true;
  }

  if(buttonState1 == Off)
  { 
    button1Pressed = false;
  }

  if (buttonState1 == On) {
    timerCount = 0;
    screenBlankDelayCount = 0;
    if(!button1Pressed)
    {
      if(buttonStatus==ButtonStatusSetTime)
      {
        clockHr = now.Hour();
        clockHr = clockHr + 1;
        if(clockHr == 24) { clockHr = 0;}
        RtcDateTime setTime = RtcDateTime(2019,1,21,clockHr,now.Minute(),0);
        Rtc.SetDateTime(setTime);        
      }

      if(buttonStatus==ButtonStatusSetAlarm)
      {
        alarmHr = alarmHr + 1;
        if(alarmHr == 24) { alarmHr = 0;}
      }

      if(buttonStatus==ButtonStatusOpenClose)
      {
        if(mode==ModeDoNothing || mode == ModeError)
        {
          if(positionKnown)
          {
            if(doorStatus == DoorStatusClosed) 
            {
              mode = ModeDisplayOpening;
            }
            else //DoorOpen
            {
              mode = ModeDisplayClosing;
            }
          }
          else
          {
            mode = ModeDisplayInit;      
          }
        }
      }
    }
    button1Pressed = true;
  }

  //check if door reached init pos. 
  //This is needed for first time zero point detection
  if (stopPinState == On) {
    if(mode==ModeInitPos)
    {
      mode = ModeInitPosAchieved;
    }
  }

#pragma endregion buttonStates

// Modes 
#pragma region Modes

  if(mode==ModeInitPosAchieved)
  {
    positionKnown = true; //emergency stop
    doorStatus = DoorStatusClosed;
    mode = ModeDisplayOpening; //door is opening
    timerCount = 0;
  }

  if(mode==ModeTimeForFood) //yemek vakti
  {
    mode = ModeDisplayOpening;
    delay(2000);  //tekrar alarm tetiklenmesin diye 2 saniye sonra açar  
    buttonStatus = ButtonStatusOpenClose;    
  }

  if(mode == ModeInitPos)
  {
    for (int s=0; s<8000; s++){
      stepper.step(moveClockwise);
      int stopPinState = digitalRead(stopPin);
      if(stopPinState == On)
      {
        mode=ModeInitPosAchieved;
        break;
      }
    }
    timerCount = timerCount +1;
  }

  if(timerCount > 50)
  {
    mode = ModeError;
  }

  if(mode == ModeDisplayInit)
  {
    mode = ModeInitPos;
  }

  if(mode == ModeRunForOpen)
  {
    Serial.print("ModeRunForOpen");
    stepper.move(!moveClockwise, slideDistance); //** çalıştı
    resetStepperPins();
    mode = ModeDoNothing;
    doorStatus = DoorStatusopen;
  }

  if(mode == ModeDisplayOpening)
  {
    if(doorStatus == DoorStatusClosed)
    {
      mode = ModeRunForOpen;
      doorStatus = DoorStatusUnkonwn;
    }
  }

  if(mode == ModeRunForClose)
  {
    Serial.print("ModeRunForClose");
    stepper.move(moveClockwise, slideDistance);  //** çalıştı
    resetStepperPins();
    mode = ModeDoNothing;
    doorStatus = DoorStatusClosed;
  }

  if(mode == ModeDisplayClosing)
  {
    mode = ModeRunForClose;
    doorStatus = DoorStatusUnkonwn;
  }

#pragma endregion Modes

//display statüs ve other information

      String info = "---";
      String status = "";

      switch(buttonStatus)
      {
        case ButtonStatusOpenClose: status = "Open/Close";break;
        case ButtonStatusSetAlarm: status = "Set Alarm";break;
        case ButtonStatusSetTime:status  ="Set Time";break;
      }
      
      switch(mode) {
        case ModeDoNothing: info = "...";break;
        case ModeDisplayInit: info = "Initializing...";break;
        case ModeInitPos: info = "Initializing...";break;        
        case ModeDisplayOpening: info = "Opening...";break;        
        case ModeRunForOpen: info = "Open";break;        
        case ModeDisplayClosing: info = "Closing...";break;                
        case ModeRunForClose: info = "Closed";break;                        
        case ModeInitPosAchieved: info = "Ready";break; 
        case ModeTimeForFood: info = "Time for food";break;                                                              
        case ModeEndOfTimeForFood: info = "Time for food";break;
        case ModeError: info = "Error";break;
      }
      Serial.print(info);

    if(screenBlankDelayCount < 50)
    {
        screenBlankDelayCount= screenBlankDelayCount + 1;
        if(screenBlankDelayCount>100)
        {
          screenBlankDelayCount=100;
        }
        Serial.print(screenBlankDelayCount);
            //Serial.print("show");
        showScreen = true;
    }
    else
    {
        showScreen = false;
    }
    
    if(showScreen)
    {
      printTimeAndAlarm(now, RtcDateTime(2000,1, 1, alarmHr, alarmMin, 0), status, reading, mode);
    }
    else
    {
      screenBlankEffectDelay +=1;
      if(screenBlankEffectDelay == 30)
      {
        printRandom();
        screenBlankEffectDelay = 0;
      }
      Serial.print("Screen blank");
      //ScreenBlank();
      
    }
    
}







