//Tiny Speak is used on small micro processors and reduces all commands to a single byte.  
//At this point Tiny ONLY does integer math and does not support floating point.
//The format can be in a few different forms:
//Len Cmd V1 V2
//or
//Len Cmd D1 D2 V1 V2
//or
//Len Cmd Comp A1 A2 B1 B2 Jmp1 Jmp2
//
//where
//Len is the length of the packet (3,5, or 8 bytes)
//Cmd is the command byte
//V1 is the value high byte
//V2 is the value low byte
//D1 is the destination high byte
//D2 is the destination low byte
//Comp is the comparison byte (<,>, etc)
//A1,2,B1,2 are the two numbers being compared
//Jmp1,2 are the two bytes for the jump (if true)
//
//Numbers:
//are u16 ( hi byte first) or 8 bit source and 8 bit value, but numbers are limited to u12 so that they don't interfere with the source codes
//
//If MSB = 1 and it doesn't match a source code, then it is an array
//
//SET DIO[1],1
//SET Duration[8],100
//SET TONE[8],440,100
//SET TONE[8],[440,100]
//Len Cmd S1 S2 V1 V2 V3 V4

//sources:
//
//200 (C8) - TMR[#]
//201 (C9) - AI[#]
//202 (CA) - DIO[#]
//203 (CB) - PWM[#]
//204 (CC) - TOGGLE[#]
//205 (CD) - VAR[#]
//
//
//220 (DC) - TMR[var]
//221 (DD) - AI[var]
//222 (DE) - DIO[var]
//223 (DF) - PWM[var]
//224 (E0) - TOGGLE[var]
//225 (E1) - VAR[var]
//
//252 (FC) - 
//253 (FD) - var[var]
//254 (FE) - 
//255 (FF) - var#

#include <Servo.h> // servo library

#define VERSION 1.0
#define DirectSize 30 //BotCode buffer size
#define ScriptSize 1000 //Script storage size
#define VarSize 30 //allocated size for BotSpeak variables
#define PINSize 20 //total pins available

#define DIO_SIZE 20 //# of digital I/O pins available
#define AI_SIZE 6 //# of analog I/O pins available
#define PWM_SIZE 4 //6 //# of hardware PWM pins available
#define TMR_SIZE 10 //# of hardware timers available
#define SERVO_SIZE 2 //# of pins blocked from motor 

//#define SDA 20
//#define SCL 21

int scripting = -1, Jump = 3; //"scripting" is the current index of the script, Jump is the position in the script of the start of the next command

String Reply = "";


unsigned char SCRIPT[ScriptSize],BotCode[DirectSize],PINS[PINSize];  // PINS is the mode for each pin: 255 = open, 0 = input, 1 = output, 2 = servo
//unsigned char PWMPins[PWM_SIZE]={3,5,6,9,10,11}; //definition of PWM pins
unsigned char PWMPins[PWM_SIZE]={3,5,6,11};
unsigned char ServoPins[SERVO_SIZE]={9,10};
unsigned char DIOPins[DIO_SIZE]={0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19}; //definition of DIO pins
unsigned char AnalogInPins[AI_SIZE]={0,1,2,3,4,5}; //definition of Analog Input pins
 
int VARS[VarSize];
long TIMER[TMR_SIZE];

Servo SERVOS[SERVO_SIZE];

void setup () {
  int i;
  Serial.begin(9600);
  for(i=0; i< PINSize; i++) PINS[i]=255; //set all pins to open - this is used in the Set/ReadPorts functions
  for(i=0; i< VarSize; i++) VARS[i]=0; //clear all variables
  for(i=0; i< TMR_SIZE; i++) TIMER[i]=0;  //clear all timers
  for(i=0; i< ScriptSize; i++) SCRIPT[i]=0; //clear the script
  for(i=0; i< DirectSize; i++) BotCode[i]=0; //clear the BotCode buffer
  
  VARS[0]=VERSION; VARS[1]=1; VARS[2]=0;  VARS[3]=0; // intial definitions - version, Hi, Lo, END (last point in script)
}

void loop () { 
  unsigned char length, i, Byte;
  float value;

  if (Serial.available()) {  //if there is something on the serial line - execute it
    length = Serial.read(); 
    
    Reply = " ";
  //  Reply = " [" + length ;  

    for (i=0; i < length; i++)  { //read in the number of characters defined by "length") - note that we read everything in before executing any commands
       while (Serial.available() < 1) { delay (1); }
       Byte = Serial.read();
 //      Reply += ',' + Byte;
       if (Byte == 'E') {          // 'E' means end scripting, so we save our last position in the SCRIPT array and set index to -1, because if you are in script mode, you never run RunBotSpeak
          VARS[3] = scripting;
         scripting = -1;
       }
       if (scripting >= 0) {        //if scripting index is > 0, write the byte to the SCRIPT array because we are in script mode
         SCRIPT[scripting] = Byte;
         scripting ++; 
       }
       else BotCode[i] = Byte;
      }
//    Reply += "]  ";
    if (scripting < 0) RunBotSpeak(length); else Serial.println(Reply); //if scripting is -1, we are in BotSpeak mode; otherwise a script is running so we want to ignore serial input besides 'E' to end script mode
  }
  delay (1);
}

void RunBotSpeak(int length) {
  int i;
  
  switch (BotCode[0]) {
    case 's':     //start writing to SCRIPT by setting scripting to 0
      scripting = 0;
 //     Serial.print(Reply);
      Serial.println("start script");
      break;
      
    case 'E':    //break out of scripting; scripting index should already be set to -1 by now
 //     Serial.print(Reply);
 //     for (i = 0; i < VARS[3]; i++) {Serial.print(SCRIPT[i]); Serial.print(','); }
      Serial.println("end script");
      break;
      
    case 'r': 
      Serial.println("Running");
      i = Retrieve(1,0); //retrieve first byte code in script;
      while (i < VARS[3]) { // VARS[3] is the index of the final byte code in the script
        Reply = " ";
        ExecuteCommand(i,1); //execute the byte code
        i = Jump; //sets i to the initial script element? not exactly sure what Jump is
      }
      break;
      
    case 'd': //run bot code in debug mode, i.e. print out results to serial
     i = Retrieve(1,0); //retrieve first byte code in script
      while (i < VARS[3]) {
        Reply = " ";
        Serial.print(ExecuteCommand(i,1)); //print the result of the command
        Reply.concat('\n');
        Serial.print(Reply); //print out the Reply
        i = Jump; //set i to the Jump point
      }
      Serial.println("Done");
      break;

    case 'R': //run bot code
      i = Retrieve(1,0);  //retrieve first byte code in script
      while (i < VARS[3]) {
        Reply = " ";
        ExecuteCommand(i,1);
        Serial.print('.'); //print a '.' character to indicate command execution
        i = Jump; //set i to the Jump point
      }
      Serial.println("\nDone");
      break;
      
    default: Serial.println(ExecuteCommand(0,0)); break; //Serial.println (Reply);
  }
}

int ExecuteCommand(int ptr,int s) { //ptr is the code index, s indicates script mode or BotCode mode
  unsigned char Command = (s) ? SCRIPT[ptr] : BotCode[ptr];
  unsigned char comparison = (s) ? SCRIPT[ptr+1] : BotCode[ptr+1];
  unsigned char Compare = 0;
  int A,B;

  Reply.concat(Command); Reply.concat("|");    
  A=Retrieve(ptr+1,s); //Retrieve first operand
  Jump = ptr + 5; //set Jump point to the start of the next command; each command is 1 byte followed by two operands for a total of 5 bytes long
  switch (Command) {
    case '=':                                                                              return Assign(ptr+1,Retrieve(ptr+3,s),s);
    case '+': Reply.concat(A); Reply.concat(':'); Reply.concat((B=Retrieve(ptr+3,s)));     return VARS[A] += B;
    case '-': Reply.concat(A); Reply.concat(':'); Reply.concat((B=Retrieve(ptr+3,s)));     return VARS[A] -= B;
    case '*': Reply.concat(A); Reply.concat(':'); Reply.concat((B=Retrieve(ptr+3,s)));     return VARS[A] *= B;
    case '/': Reply.concat(A); Reply.concat(':'); Reply.concat((B=Retrieve(ptr+3,s)));     return VARS[A] /= B;
    case '%': Reply.concat(A); Reply.concat(':'); Reply.concat((B=Retrieve(ptr+3,s)));     return VARS[A] = VARS[A] % B;
    case '&': Reply.concat(A); Reply.concat(':'); Reply.concat((B=Retrieve(ptr+3,s)));     return VARS[A] &= B;
    case '|': Reply.concat(A); Reply.concat(':'); Reply.concat((B=Retrieve(ptr+3,s)));     return VARS[A] |= B;
    case '~': Reply.concat(A); Reply.concat(':'); Reply.concat((B=Retrieve(ptr+3,s)));     return VARS[A] = !B;    
    case '<': Reply.concat(A); Reply.concat(':'); Reply.concat((B=Retrieve(ptr+3,s)));     return VARS[A] <<= B;
    case '>': Reply.concat(A); Reply.concat(':'); Reply.concat((B=Retrieve(ptr+3,s)));     return VARS[A] >>= B;
    case 'g': Jump = ptr + 3;                                                        return A; //get a value, in this case only one operand, so set Jump appropriately
    case 'W': Jump = ptr + 3;                                    delay(A);           return A; //wait in milliseconds
    case 'w': Jump = ptr + 3;                        delayMicroseconds(A);           return A; //wait in microseconds
    case 'G': Jump = A;                                                              return Jump; //GoTo
    case 'e': Jump = VARS[3];                                                        return Jump; //GoTo end of script
    case '!':                           return SystemCall(ptr,s);
    case 'I': //if statement
       A = Retrieve(ptr+2,s);  
       B = Retrieve(ptr+4,s);
       switch (comparison)  {
          case '<': Compare = A <  B; break;
          case '>': Compare = A >  B; break;
          case '!': Compare = A != B; break;
          case '=': Compare = A == B; break;
          case 'G': Compare = A >= B; break;
          case 'L': Compare = A <= B; break;
          default: Compare = 0;
        }
      Jump = (Compare) ? Retrieve(ptr+6,s) : ptr + 8;
      Reply.concat('('); Reply.concat(A); Reply.concat(' '); Reply.concat(comparison); Reply.concat(' '); Reply.concat(B); Reply.concat(')'); Reply.concat("->"); Reply.concat(Jump);
    case 'i': // ~if statement  
       A = Retrieve(ptr+2,s);  
       B = Retrieve(ptr+4,s);
       switch (comparison)  {
          case '<': Compare = A <  B; break;
          case '>': Compare = A >  B; break;
          case '!': Compare = A != B; break;
          case '=': Compare = A == B; break;
          case 'G': Compare = A >= B; break;
          case 'L': Compare = A <= B; break;
          default: Compare = 0;
        }
        Compare = !Compare;
        Jump = (Compare) ? Retrieve(ptr+6,s) : ptr + 8;
        Reply.concat('('); Reply.concat(A); Reply.concat(' '); Reply.concat(comparison); Reply.concat(' '); Reply.concat(B); Reply.concat(')'); Reply.concat("->"); Reply.concat(Jump);
      return Jump;
    default: Jump = ptr + 1;   return -1;
  }
}

int Assign(int destIndex,int value, int s) {
      unsigned char LoByte = (s) ? SCRIPT[destIndex+1] : BotCode[destIndex+1];
      unsigned char HiByte = (s) ? SCRIPT[destIndex] : BotCode[destIndex];
      
      switch (HiByte) { //HiByte determines the type
        case 255: LoByte = (LoByte < VarSize) ? LoByte:VarSize;     VARS[LoByte] = value;                     break; //LoByte is a pointer to a variable

        case 224: LoByte = (LoByte < DIO_SIZE) ? LoByte:DIO_SIZE;   ServoPort(VARS[LoByte],value);            break; //Toggles port indicated by LoByte variable using the delay in "value"
        case 223: LoByte = (LoByte < PWM_SIZE) ? LoByte:PWM_SIZE;   PWMPort(PWMPins[VARS[LoByte]],value);     break;  //sets PWM port indicated by LoByte variable to "value"
        case 222: LoByte = (LoByte < DIO_SIZE) ? LoByte:DIO_SIZE;   SetPort(DIOPins[VARS[LoByte]],value);     break;  //Sets DIO port indicated by LoByte variable to "value"
        case 221:                                                                                             break;  // Analog in indirect
        case 220: LoByte = (LoByte < TMR_SIZE) ? LoByte:TMR_SIZE; TIMER[VARS[LoByte]] = millis() + value;     break; //assigns value of a timer based on current clock reading

        case 204: LoByte = (LoByte < DIO_SIZE) ? LoByte:DIO_SIZE;   ServoPort(LoByte,value);        break;  
        case 203: LoByte = (LoByte < PWM_SIZE) ? LoByte:PWM_SIZE;   PWMPort(PWMPins[LoByte],value);           break;  
        case 202: LoByte = (LoByte < DIO_SIZE) ? LoByte:DIO_SIZE;   SetPort(DIOPins[LoByte],value);           break;
        case 201:                                                                                             break;  // Analog in
        case 200: LoByte = (LoByte < TMR_SIZE) ? LoByte:TMR_SIZE; TIMER[LoByte] = millis() + value;           break;
        default:  
          if (HiByte/128) VARS[indexValue(HiByte,LoByte)] = value;    
          else VARS[HiByte * 256 + LoByte] = value;                                                        
          break;  
      }
      Reply.concat(HiByte); Reply.concat(':'); Reply.concat(LoByte); 
      return value;
}

int Retrieve(int index,int s) { //this function decodes the bytes that were sent over serial; index is the current index, either in SCRIPT or BOTCODE, and "s" is a selector that determines which mode we are in
      int value, Array_Size = 0;
      unsigned char LoByte = (s) ? SCRIPT[index+1] : BotCode[index+1]; //each value is two bytes long, so grab the LoByte here
      unsigned char HiByte = (s) ? SCRIPT[index] : BotCode[index]; //and grab the HiByte here
      
      switch (HiByte) { //HiByte defines a source based on its value if it matches any of these cases, otherwise it's just a number
        case 255: LoByte = (LoByte < VarSize) ? LoByte:VarSize;     value = VARS[LoByte];                     break; //return variable indicated by LoByte
        case 222: LoByte = (LoByte < DIO_SIZE) ? LoByte:DIO_SIZE;   value = ReadPort(DIOPins[VARS[LoByte]]);  break;  //return DIO pin indicated by the variable that LoByte points to
        case 221: LoByte = (LoByte < VarSize) ? LoByte:VarSize;     value = analogRead(VARS[LoByte]);         break;  //return analog value indicated by the variable that LoByte points to
        case 220: LoByte = (LoByte < TMR_SIZE) ? LoByte:TMR_SIZE; value =  millis() - TIMER[VARS[LoByte]];    break;  //return timer value indicated by the variable that LoByte points to
        case 202: LoByte = (LoByte < DIO_SIZE) ? LoByte:DIO_SIZE;   value = ReadPort(DIOPins[LoByte]);        break;  //return DIO pin indicated by LoByte
        case 201: LoByte = (LoByte < AI_SIZE) ? LoByte:AI_SIZE;     value = analogRead(LoByte);               break;  //return analog value indicated by LoByte
        case 200: LoByte = (LoByte < TMR_SIZE) ? LoByte:TMR_SIZE; value =  millis() - TIMER[LoByte];          break;  //return timer indicated by LoByte
        default:                                                                
          if (HiByte/128) value = VARS[indexValue(HiByte,LoByte)]; //if MSB is 1, then this is reading from an array, so return that array element
          else value = HiByte * 256 + LoByte;       //otherwise combine HiByte and LoByte
          break;          
      }
      return value;
}


int indexValue(unsigned char HiByte,unsigned char LoByte)  { //returns the VARS index of the desired array element. HiByte tells us the array start, LoByte tells us the index within the array
  int Array_Size;

  Array_Size = VARS[(HiByte & 0x3F)];                 // variable indicated by lower 6 bits of HiByte is the start index of the array
  LoByte = (LoByte < VarSize) ? LoByte:VarSize;    //checks to makes sure LoByte is within the size of the variable space
  if ((HiByte & 64)/64) LoByte = VARS[LoByte];      // if the 7th bit of HiByte is 1, it means that the LoByte is a pointer to a variable; 128 = array, 64 = indirect
  //direct: array[LoByte]
  //indirect: array[VARS[LoByte]]
  LoByte = (LoByte < Array_Size) ? LoByte:Array_Size - 1; // if LoByte exceeds Array_Size, return the index of the last array element, which is Array_Size-1
  return (HiByte & 63) + 1 + LoByte;   //sends array_SIZE, so add one for array[0]
}

int ReadPort(int port)  {
  //if the pin is set to 'open', i.e. doesn't have a mode, set it to an input; otherwise just read the pin
  //we don't want to just set it to input because, for example, we might just want to check the status of a pin that has been set high with a digital write
   if (PINS[port] == 255) {
     pinMode(port,INPUT); 
     PINS[port] = 0;
   }
   return (digitalRead(port));
}

int SetPort(int port,int value) {
   if (PINS[port] != 1) { //if the pin is not set to output mode, set it
     pinMode(port,OUTPUT); 
     PINS[port]=1;
   }
   digitalWrite(port,value);
   return (value);
}

int PWMPort(int port,int value) {
   if (PINS[port] != 1) { //if the pin is not set to output mode, set it
     pinMode(port,OUTPUT); 
     PINS[port]=1;
   }
    // rescale to 1024 because of the resolution of the analog output
   analogWrite(port,map(value, 0, 100, 0, 255));
   return (value);
  }
  
int ServoPort(int port,int value) {
   SERVOS[port].attach(ServoPins[port]);
   value = (value < 2) ? value:2; value = (value > 1) ? value:1;
   value = (value - 1) * 180; 
   SERVOS[port].write(value);
   return (value);
  }
  
int Tone(int port,int freq, int duration)
  {
   if (PINS[port] != 1) { //if pin is not set to output, set it
     pinMode(port,OUTPUT); 
     PINS[port]=1;
   }
   tone(port,freq,duration);
   return (freq);
  }
  
  
int SystemCall(int ptr,int s)  { //this is a hook for implementing custom functionality such as including other arduino libraries or shields
  unsigned char CmdSize = (s) ? SCRIPT[ptr + 1] : BotCode[ptr + 1];
  
  //put your code here. This is just an example of what you might do.
  unsigned char Command = (s) ? SCRIPT[ptr + 2] : BotCode[ptr + 2];
  int query = 0;
  int freq = 0;
  int ampl = 0;
  int ddd = 0;
  int rrr = 0;
  switch (Command) { //uses one of the operands as a secondary command, allowing multiple types of system calls
    case 'q':
      query = Retrieve(ptr + 3, s);
    case 'f':
      freq = Retrieve(ptr + 3, s);
    default:
      freq = Retrieve(ptr + 4,s);
      ampl = Retrieve(ptr + 6,s);  
      ddd = Retrieve(ptr + 8,s);
      rrr=ddd*ampl;
  }
  
  
  Jump = ptr + CmdSize + 2;  //moves the index to the correct position in the script or buffer
  
  Assign(ptr + 2,rrr,s);                // what gets saved in remote mode
  Reply.concat(" "); Reply.concat(Jump);         // what gets returned in debug mode
  return ampl*freq;                           // what gets returned in direct mode
}

