//Authors: Group 31 (Kevin Tran, Kevin Fregoso)
//Date: 0X.XX.2025
//Purpose: CPE 301.1001 Final Project
//Additional Notes: Modified to give players two attempts to guess the code  

#include <LiquidCrystal.h>
#include <Keypad.h>
#include <RTClib.h>
#include <Stepper.h>

#define RDA 0x80
#define TBE 0x20

RTC_DS1307 rtc;

// UART Pointers
volatile unsigned char *myUCSR0A = (unsigned char *)0x00C0;
volatile unsigned char *myUCSR0B = (unsigned char *)0x00C1;
volatile unsigned char *myUCSR0C = (unsigned char *)0x00C2;
volatile unsigned int  *myUBRR0  = (unsigned int *) 0x00C4;
volatile unsigned char *myUDR0   = (unsigned char *)0x00C6;

// GPIO Pointers
volatile unsigned char *portB = (unsigned char *) 0x25;
volatile unsigned char *portDDRB = (unsigned char *) 0x24;

volatile unsigned char *portL = (unsigned char *) 0x10B;
volatile unsigned char *portDDRL = (unsigned char *)0x10A;

volatile unsigned char *portD = (unsigned char *) 0x2B;
volatile unsigned char *portDDRD = (unsigned char *) 0x2A;

// Timer Pointers
volatile unsigned char *myTCCR1A = (unsigned char *) 0x80;
volatile unsigned char *myTCCR1B = (unsigned char *) 0x81;
volatile unsigned char *myTCCR1C = (unsigned char *) 0x82;
volatile unsigned char *myTIMSK1 = (unsigned char *) 0x6F;
volatile unsigned int  *myTCNT1  = (unsigned  int *) 0x84;
volatile unsigned char *myTIFR1 = (unsigned char *) 0x36;

//ADC Pointers
volatile unsigned char* my_ADMUX = (unsigned char*) 0x7C;
volatile unsigned char* my_ADCSRB = (unsigned char*) 0x7B;
volatile unsigned char* my_ADCSRA = (unsigned char*) 0x7A;
volatile unsigned int* my_ADC_DATA = (unsigned int*) 0x78;

const int interruptPin = 19;

volatile bool isPaused = false;

byte in_char;
//This array holds the tick values
//calculate all the tick value for the given frequencies and put that in the ticks array
unsigned int ticks[7]= {
  18181, //440
  17817, //449
  15296, //523
  13332,//587, //13628
  12139, //659,
  11461, //698,
  10204 //784
};

//global ticks counter
unsigned int currentTicks = 65535;
unsigned char timer_running = 0;

const int ROW_NUM = 4; //four rows
const int COLUMN_NUM = 4; //four columns
char keys[ROW_NUM][COLUMN_NUM] = {
  {'1','2','3', 'A'},
  {'4','5','6', 'B'},
  {'7','8','9', 'C'},
  {'*','0','#', 'D'}
};
byte pin_rows[ROW_NUM] = {48, 46, 44, 42};  //connect to the row pinouts of the keypad
byte pin_column[COLUMN_NUM] = {40, 38, 36, 34}; //connect to the column pinouts of the keypad
//if the numbers on the keypad are facing you, the order of the pins are: R1, R2, R3, R4, C1, C2, C3, C4 (Left to Right)

Keypad keypad = Keypad( makeKeymap(keys), pin_rows, pin_column, ROW_NUM, COLUMN_NUM );

// LCD pins <--> Arduino pins
const int RS = 11, EN = 13, D4 = 2, D5 = 3, D6 = 4, D7 = 5;
LiquidCrystal lcd(RS, EN, D4, D5, D6, D7);

bool gameStarted = false; // Flag to track if the game has started
  
String secretCode = ""; //empty string for code

int numAttempts = 2; //max number of attemps 
String attempt = ""; //empty string for the attempt

unsigned long startTime = 0;
unsigned long endTime = 0;
unsigned long previousPrintTime = 0;
unsigned long pauseStartTime = 0;
unsigned long totalPausedTime = 0;

// Defines the number of steps per rotation
const int stepsPerRevolution = 2038;
// Pins entered in sequence IN1-IN3-IN2-IN4 for proper step sequence
Stepper myStepper = Stepper(stepsPerRevolution, 23, 27, 25, 29);
//yellow, brown, purp, grey

void startGame(){
  secretCode = "0722";
  attempt = ""; //reset the attempt

  //wlecome screen
  lcd.clear();
  lcd.print("Enter");
  lcd.setCursor(0,1);
  lcd.print("Access Code:");

  //starting the timer
  startTime = millis();
}

void Alarm(){
  if (timer_running == 0) {
    currentTicks = ticks[3];
    *myTCCR1B |= 0x01;  // Start timer
    timer_running = 1;
    delay(5000);         // Delay before stopping
    currentTicks = 65535;
    *myTCCR1B &= 0xF8;  // Stop timer
    timer_running = 0;
    *portB &= 0xBF;     // Ensure buzzer (PB6) is LOW
  }
}

// Timer setup function
void setup_timer_regs() {
  // setup the timer control registers
  *myTCCR1A = 0x00;
  *myTCCR1B = 0X00;
  *myTCCR1C = 0x00;
  
  // reset the TOV flag
  *myTIFR1 |= 0x01;
  
  // enable the TOV interrupt
  *myTIMSK1 |= 0x01;
  
  // I'm not sure if this line will work
  // Check page 161 of 
  // https://ww1.microchip.com/downloads/en/devicedoc/atmel-2549-8-bit-avr-microcontroller-atmega640-1280-1281-2560-2561_datasheet.pdf 
}

// TIMER OVERFLOW ISR
ISR(TIMER1_OVF_vect) {
  // Stop the Timer
  *myTCCR1B &= 0xF8;
  // Load the Count
  *myTCNT1 =  (unsigned int) (65535 -  (unsigned long) (currentTicks));
  // Start the Timer
  *myTCCR1B |= 0x01;
  // if it's not the STOP amount
  if(currentTicks != 65535) {
    // XOR to toggle PB6
    *portB ^= 0x40;
  }
}
/*
 * UART FUNCTIONS
 */
void U0Init(int U0baud) {
    unsigned long FCPU = 16000000;
    unsigned int tbaud;
    tbaud = (FCPU / 16 / U0baud - 1);
    // Same as (FCPU / (16 * U0baud)) - 1;
    *myUCSR0A = 0x20;
    *myUCSR0B = 0x18;
    *myUCSR0C = 0x06;
    *myUBRR0  = tbaud;
}

unsigned char kbhit() {
   return *myUCSR0A & RDA;
}

unsigned char getChar() {
   return *myUDR0;
}

void putChar(unsigned char U0pdata) {
  while((*myUCSR0A & TBE)==0);
    *myUDR0 = U0pdata;
}

void adc_init(){
  *my_ADMUX = 0x40;
  *my_ADCSRA = 0x87;
}

int adc_read(){
  *my_ADCSRA |= 0x40;  // Start ADC conversion
  while (!(*my_ADCSRA & 0x10)); // Wait for conversion to complete
  *my_ADCSRA |= 0x10;  // Clear ADC interrupt flag
  return *my_ADC_DATA;      // Return ADC value
}

void cw(){
	myStepper.setSpeed(10);
	myStepper.step(stepsPerRevolution);
	delay(1000);
}

void ccw(){
	myStepper.setSpeed(10);
	myStepper.step(-stepsPerRevolution);
	delay(1000);
}

void securityMessage(){
  String message2 = "Law enforcement will arrive momentarily.";
  for(int i = 0; i < message2.length(); i++){
    putChar(message2[i]);
  }
  putChar('\n');
}

void alarmActivationMessage(){
  String message3 = "Activating Alarm";
  for(int i = 0; i < message3.length(); i++){
    putChar(message3[i]);
  }
  putChar('\n');
}

void printTimeStamp(const String& event){
  DateTime now = rtc.now();
  String timeStamp = event + " on " + String(now.month()) + "." + String(now.day()) + "." + String(now.year()) + ": " + String(now.hour()) + ":" + String(now.minute()) + ":" + String(now.second());
  for(int i = 0; i < timeStamp.length(); i++){
    putChar(timeStamp[i]);
  }
  putChar('\n');
}

void handleInterrupt(){
  isPaused = !isPaused;
  if(isPaused){
    pauseStartTime = millis();
    lcd.clear();
    lcd.print("Paused");
    lcd.setCursor(0,1);
    lcd.print("Press to Resume");
  }
  else{
    totalPausedTime += (millis() - pauseStartTime);
    lcd.clear();
    lcd.print("Resuming");
    delay(2000);
    lcd.clear();
    lcd.print("Enter");
    lcd.setCursor(0,1);
    lcd.print("Access Code:");
  }
}

void setup() {
  Serial.begin(9600);
  lcd.begin(16, 2);
  rtc.begin();
  //only adjust the RTC module once to get the correct time 
  //rtc.adjust(DateTime(2025, 4, 29, 20, 17, 0));
  attachInterrupt(digitalPinToInterrupt(interruptPin), handleInterrupt, FALLING);
  // Display start screen message
  lcd.print("Press A");
  lcd.setCursor(0, 1);
  lcd.print("to start...");
  adc_init();
  // setup the Timer for Normal Mode, with the TOV interrupt enabled
  setup_timer_regs();
  // Start the UART
  U0Init(9600); 

  *portDDRB |= (0x1 << 6); // Configure PB6 as output
  *portB &= 0x3F;          // Set PB6 LOW

  *portDDRL |= (0x1 << 0); //configure pin 49 as output
  *portL &= ~(0x1 << 0); //set pin 49 to low

  *portDDRL |= (0x1 << 2);   //configure pin 47 as output
  *portL &= ~(0x1 << 2);  //set pin 47 to low

  *portDDRL |= (0x1 << 4);   //configure pin 45 as output
  *portL &= ~(0x1 << 4);  //set pin 45 to low

  *portDDRB |= (0x1 << 2); //configure pin 51 as output
  *portB &= ~(0x1 << 2); //set pin 51 to low
}

void loop(){
  if(isPaused){
    delay(100);
    return;
  }
  //logic for starting the game
  if(!gameStarted){
    char key = keypad.getKey();
    //only start the game if player presses A
    if(key == 'A'){
      //set gameStarted flag to true
      gameStarted = true;
      //call startGame function to start the game
      startGame();
    }
  }
  //logic for when gameStarted flag is set to true
  else{
    //read input
    char input = keypad.getKey();
    //check if input is valid
    if(input != NO_KEY){
      //check if star key (*) was pressed
      if(input == '*'){
        //turn off all LEDs
        *portB &= ~(0x1 << 2);
        *portL &= ~(0x1 << 4);
        *portL &= ~(0x1 << 2);
        *portL &= ~(0x1 << 0);
        //clear lcd scrren
        lcd.clear();
        //display program ended message
        lcd.print("Program Ended");
        lcd.setCursor(0, 1);
        lcd.print("By User");
        //wait before clearing screen
        delay(5000);
        lcd.clear();
        //set gameStarted flag to false
        gameStarted = false;
        //exit loop
        return;
      }
      //check if pound key (#) was pressed
      if(input == '#'){
        //clear lcd screen
        lcd.clear();
        //logic if the player got the code correct
        if(attempt == secretCode){
          //turn of all other LEDs
          *portL &= ~(0x1 << 4); 
          *portL &= ~(0x1 << 2);
          *portL &= ~(0x1 << 0);
          //turn on blue Led
          *portB |= (0x1 << 2);
          //get ending time
          endTime = millis();
          //calculate time spent and convert from miliseconds to seconds
          unsigned long totalTime = (endTime - startTime)/1000;
          //output time to serial monitor
          String timeMessage = "Time Taken to Break in: " + String(totalTime) + " Seconds.";
          for(int i = 0; i < timeMessage.length(); i++){
            putChar(timeMessage[i]);
          }
          putChar('\n');
          //output when the safe was accesed to serial monitor
          printTimeStamp("Correct passcode entered");
          putChar('\n');
          //display win screen on LCD for a bit before clearing
          lcd.print("Access Granted");
          delay(5000);
          lcd.clear();
          //turn off blue LED
          *portB &= ~(0x1 << 2);
          //set gameStarted flag to false to end game
          gameStarted = false;
          //exit
          return;
        }
        //logic if player got the code incorrect
        else{
          //decrease the number of attempts the player has by one
          numAttempts--;
          //logic if player still has attempts remaining
          if(numAttempts > 0){
            //display "Access Denied" screen on LCD and the number of attempts remaning
            lcd.print("Access Denied");
            lcd.setCursor(0, 1);
            lcd.print(numAttempts);
            lcd.print(" Attempts Left");
            //reset attempt
            attempt = "";
            //timestamp
            printTimeStamp("Incorrect passcode detected");
            //wait a bit before going back to "Enter Code" screen
            delay(2000);
            lcd.clear();
            lcd.print("Enter");
            lcd.setCursor(0,1);
            lcd.print("Access Code:");
          }
          //logic if player has no attempts remaning
          else{
            //output activating alarm message to serial monitor
            alarmActivationMessage();
            //clear LCD and display "Access Denied" and "No Attempts Remaning"
            lcd.clear();
            lcd.print("Access Denied");
            lcd.setCursor(0,1);
            lcd.print("No Attempts Left");
            //timestamp
            printTimeStamp("Incorrect passcode detected");
            alarmActivationMessage();
            //clear LCD after a bit
            delay(2000);
            lcd.clear();
            //activate alarm
            *portB |= (0x1 << 2);
            *portL |= (0x1 << 4);
            *portL |= (0x1 << 2);
            *portL |= (0x1 << 0);
            Alarm();
            //set gameStarted to false to end game
            gameStarted = false;
          }
          
        }
      }
      //logic for creating code that player inputs
      else if(attempt.length() < 4){
        //append the input to code
        attempt += input;
        //formatting for LCD
        lcd.setCursor(12+attempt.length() -1, 1);
        lcd.print(input);
      }
    }
    //logic for when player would take 15 seconds or longer (time-out loss)
    if((millis()-startTime-totalPausedTime) >= 15000){
      //output security message to Serial Monitor
      alarmActivationMessage();
      //clear LCD and print lose message
      lcd.clear();
      lcd.print("Out of Time");
      lcd.setCursor(0,1);
      lcd.print("You Got Caught");
      //timestamp
      printTimeStamp("Time-out detected");
      //clear LCD after a bit
      delay(2000);
      lcd.clear();
      //activate alarm
      *portB |= (0x1 << 2);
      *portL |= (0x1 << 4);
      *portL |= (0x1 << 2);
      *portL |= (0x1 << 0);
      Alarm();
      //set gameStarted to false to end game
    }
    //prevent serial monitor from getting flooded
    if(millis() - previousPrintTime >= 2000){
      int soundLevel = adc_read();
      int thresholdValue = 200;
      int defaultValue = 80;
      //logic for whwen the sound level is at its default value
      if(soundLevel <= defaultValue){
        //turn off yellow and red LEDs
        *portL &= ~(0x1 << 2);
        *portL &= ~(0x1 << 0);
        //turn on green LED
        *portL |= (0x1 << 4);
        //output sound level to serial monitor
        String sound = "Sound Level: " + String(soundLevel);
        for(int i = 0; i < sound.length(); i++){
          putChar(sound[i]);  
        }
        putChar('\n');
      }
      //logic for when the sound level is in between the default and threshold
      if(soundLevel > defaultValue && soundLevel <= thresholdValue){
        //turn off green and red LEDs
        *portL &= ~(0x1 << 4);
        *portL &= ~(0x1 << 0);
        //turn on yellow LED
        *portL |= (0x1 << 2);
        //output sound level to serial monitor
        String sound = "Sound Level: " + String(soundLevel);
        for(int i = 0; i < sound.length(); i++){
          putChar(sound[i]);  
        }
        putChar('\n');
      }
      //logic for when the sound level exceeds the threshold
      if(soundLevel > thresholdValue){
        //turn off green and yellow LEDs
        *portL &= ~(0x1 << 4);
        *portL &= ~(0x1 << 2);
        *portB &= ~(0x1 << 2);
        //turn on red LED
        *portL |= (1 << 0);
        //clear LCD
        lcd.clear();
        //output sound level to serial monitor
        String sound = "Sound Level: " + String(soundLevel);
        for(int i = 0; i < sound.length(); i++){
          putChar(sound[i]);  
        }
        putChar('\n');
        //output security message to serial monitor
        printTimeStamp("Sound sensor triggered");
        securityMessage();
        //logic to activate motors
        ccw();
        cw();
        //turn off red LED
        *portL &= ~(0x1 << 0);
        //set gameStarted flag to false to end game
        gameStarted = false;
      }
    }
  }
}