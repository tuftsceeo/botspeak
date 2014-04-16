#include <Servo.h> 

Servo myservo;  // create servo object to control a servo 

#define VERSION 0.7
#define SIZE 30
#define ScriptSize 100
#define VarSize 30
#define PINSize 15
#define TimerSize 10
#define RunBtn 7

#define offset 5

int ScriptLength=0;
int RunState = 0;

unsigned char SCRIPT[ScriptSize],CMD[SIZE],PINS[PINSize];  // 255 = open, 0 = input, 1 = output, 2 = servo
int VAR[VarSize];
long TIMER[TimerSize];

void setup () {
  int i;
  Serial.begin(9600);
  for(i=0; i< PINSize; i++) PINS[i]=255;
  for(i=0; i< VarSize; i++) VAR[i]=0;
  for(i=0; i< TimerSize; i++) TIMER[i]=10;  
  for(i=0; i< ScriptSize; i++) SCRIPT[i]=0;
  for(i=0; i< SIZE; i++) CMD[i]=0;
  pinMode(RunBtn,INPUT); 
}

void loop () { 
  int length,morelength, i,Command,start = 0;
  float value;

  if (Serial.available()) {  //if there is something on the serial line - execute it
    length = Serial.read(); 
//    Serial.print("l = ");
//    Serial.print(length);
//    Serial.print(" C = ");
    start = 0;
    if (length !=80) Command = ReadCMD(length,start); else Command = 'P';   // if the length is 80 then it is reading the P of the first PING at startup
    switch (Command) {        // is it a script or   direct command?
      case 's':         // new script
          Serial.print("S:[");
          Serial.write(Command);
          Serial.print(length);
          Serial.println("]");
  
        morelength = 0;
        while (morelength != 1)
        {
          while (Serial.available() == 0) {delay (1);}  //wait for next line - or ENDSCRIPT
          morelength = Serial.read(); 
          start = length;
          Command = ReadCMD (morelength,start);
          length += morelength;
          Serial.print("M:[");
          Serial.write(Command);
          Serial.print(morelength);
          Serial.println("]");
      }
                 
        length -= 1;  // get rid of the 'E' for end of script    
  //      Serial.print("R:[");
  //      for (i=0; i < length; i++) {Serial.print(" "); Serial.print(SCRIPT[i]); }
  //      Serial.print("] \n");
  
       ScriptLength=length;
        switch(GetValue(SCRIPT[1],SCRIPT[2])) {
           case 0: Serial.println("R:[load script]");      value=0; Serial.println("Done"); break;
           case 1: Serial.println("R:[run script]");       break;   // should never get here - switches to just run in LV
           case 2: Serial.println("R:[load/run script]");  value = RunScript(length,0);  break;
           case 3: Serial.print  ("R:[Debug script]\n");   value = RunScript(length,1);  break;
           case 4: Serial.print  ("R:[load/run/wait]\n");  value = RunScript(length,0);  Serial.println("Done"); break;
        }
        break;
      case 'r':              // run existing script
        CMD[0]='s';
        Serial.println("R:[old script]");
        value = RunScript(ScriptLength,0);
        break;
      case 'd':              // run existing script in debug mode
        CMD[0]='s';
        Serial.print("R:[debug old script]\n");
        value = RunScript(ScriptLength,1);
        Serial.println("Done");
        break;
      case 'R':              // run existing script and wait until done
        CMD[0]='s';
        Serial.print("R:[old script w/ wait]\n");
        value = RunScript(ScriptLength,0);
        Serial.println("Done");
        break;
      case 'P':
        Serial.println(VERSION);
        break;
      default:              // anything else is direct mode
        Serial.print("R:[");
        for (i=0; i < length; i++) {Serial.print(" "); Serial.print(CMD[i]); }
        Serial.print("]");
        Serial.println(RunDirect(length));
        break;
    }    
  }
  if (CheckRunBtn())        // check the toggle button
     switch (RunState) {    // is a script running?
        case 0:
          CMD[0]='s';   
          RunScript(ScriptLength,0);
          break;
        case 1: break;
          // will pick up the abort from within RunScript()
        default: break;
     }
  delay (100);
}



int CheckRunBtn() {

  int state = 0;
  if (digitalRead(RunBtn) == 1) {
    delay (100);  //debounce
    if (digitalRead(RunBtn) == 1) state = 1;
  }
  return state;
}


unsigned char ReadCMD(int length, int start) {
     int i;
     char Byte;
     
     for (i=0; i < length; i++)  {
       while (Serial.available() < 1) {
         delay (1);
       }
       Byte = Serial.read();
       if (start == 0) if (i == 0)  
         {
           CMD[0] = Byte;    
//           Serial.write(CMD[0]);
//           Serial.print("\n");
         }
       if (CMD[0] == 's') SCRIPT[i+start] = Byte;       else  CMD[i] = Byte;
      }
     return CMD[0];
}

float RunDirect(int length) {
    int i, Source, Value;
    float value;
    unsigned char Command;

    Command = CMD[0];
    switch (length) {
          case 5:                               // maybe floats are 4 bytes long - in which case I know here if it is a float or not
            Source = GetValue(CMD[1],CMD[2]);
            Value  = GetValue(CMD[3],CMD[4]);
            break;
          case 4: 
            Source = CMD[1];
            Value = GetValue(CMD[2],CMD[3]);
            break;
          case 3: 
            Source = GetValue(CMD[1],CMD[2]);
            Value = 0;
            break;
          case 2: 
            Source = CMD[1];
            Value = 0;
            break;
          default: 
            Source = 0;
            Value = 0;
            break;
      }   
   return value = ExecuteLine(length,Command,Source,Value,0,0);
}

float RunScript(int length, char Debug) {
      int llength;
      int spot=0;
      float value;
      unsigned char Command;
      
      RunState = 1;
      while (spot <  (length - offset)) {
        if (CheckRunBtn()) break;   // if they hit the btn while a code is running, then stop it
        if (Serial.available()) {
            llength = Serial.read(); 
             Command = ReadCMD (llength,0);
             switch (Command) {
                case 's': 
                  Serial.println("Error - abort first");
                  break;
                case 'a':
                  Serial.println ("Aborted");
                  spot = length;
                  break;
                default:
                  Serial.println(RunDirect(llength));
             }
         }
        else spot = ExecuteScriptLine(spot, length, Debug);
      }
      value = length;
      RunState = 0;
      if (Debug) Serial.println("Done");
      return (value);
}
  
 int ExecuteScriptLine(int spot, int length, char Debug) {
      int pos, Linelength, Source, Value, Jump=0;
      unsigned char Command,Equal=0;
      float value;
      
      pos = spot + offset;
      Linelength = SCRIPT[pos];
      pos ++;
      Command = SCRIPT[pos];

      switch (Linelength) {
            case 8:   // If statement
              Equal  = SCRIPT[pos+1];
              Source = GetValue(SCRIPT[pos+2],SCRIPT[pos+3]);  // left
              Value  = GetValue(SCRIPT[pos+4],SCRIPT[pos+5]);  // Right
              Jump   = GetValue(SCRIPT[pos+6],SCRIPT[pos+7]);  // goto
              break;
            case 5: 
              Source = GetValue(SCRIPT[pos+1],SCRIPT[pos+2]);
              Value  = GetValue(SCRIPT[pos+3],SCRIPT[pos+4]);
              break;
            case 4: 
              Source = SCRIPT[pos+1];
              Value  = GetValue(SCRIPT[pos+2],SCRIPT[pos+3]);
              break;
            case 3:   // Wait statement
              Source = GetValue(SCRIPT[pos+1],SCRIPT[pos+2]);
              Value  = 0;
              break;
            case 2: 
              Source = SCRIPT[pos+1];
              Value  = 0;
              break;
            default: 
              Source = 0;
              Value  = 0;
              break;
       }   
       if (Debug) {
         Serial.print(spot);
         Serial.print(" (");
         Serial.print(Linelength);
         Serial.print("): ");
         Serial.write(Command);
         Serial.print(" ");
         Serial.print(Source);
         Serial.print(" ");
         if (Equal!=0) Serial.write(Equal);
         Serial.print(" ");
         Serial.print(Value);
         Serial.print(" ");
         if (Jump!=0) Serial.print(Jump);
         Serial.print(" \n");
       }
      value = ExecuteLine(Linelength,Command,Source,Value,Equal,Jump);
      switch (Command) {
           case 'G':
             spot = value;
             break;
           case 'i':
           case 'I':
             if (value < 0) spot +=  Linelength + 1; else spot = value;
             break;
           case 'a':
             spot = length;
             break;
           default: spot += Linelength + 1;  // add 1 for the length byte
      }
   if ((spot >= 0) && (spot < length)) return spot; else return length;
   
}

  
int GetValue(unsigned char HiByte, unsigned char LoByte) {
      int value;
      switch (HiByte) {
        case 255:                      // 255 - integer, 254 - SGL - 253 - indirect integer, 252 indirect SGL
          value = VAR[LoByte];        // 240 - T!#, 241 A!#, 242 D!#, 245 T!var, 246 A!var, 247 D!var
          break;
        case 242:
          value = ReadPort(LoByte);
          break;
        case 241:
          value = analogRead(LoByte);
          break;
        case 240:
          value =  millis() - TIMER[LoByte];
          break;
        case 247:
          value = ReadPort(VAR[LoByte]);
          break;
        case 246:
          value = analogRead(VAR[LoByte]);
          break;
        case 245:
          value =  millis() - TIMER[VAR[LoByte]];
          break;
        default:
          value = HiByte*256+LoByte;
          break;
      }
      return value;
}

float ExecuteLine(int length, unsigned char Command,int Source, int Value, char Equal, int Jump) {
      float value;

      switch (Command) {
      case 'D': 
        if (length > 3)   value = SetPort(Source,Value);     else value = ReadPort(Source); 
        break;
      case 'S':
        if (length > 3)   value = SetServo(Source,Value);    else value = ReadPort(Source);  
        break;
      case 'A':     
        value = analogRead(Source);  
        break;
      case 'a':
        Serial.print(" [aborting...] ");
        value = -1;
        break;
      case 'O':    
        value = PWMPort(Source,Value);  
        break;
      case 'W':
        value = Source;
        delay (value);
        break;
      case 'G':
        value = Source;
        break;
      case 'g':
        value = VAR[Source];
        break;
      case 'T':
        value = TIMER[Source] = millis() + Value;
        break;
      case 'I':
      case 'i':
         value=-1;
         if (Command == 'i') { 
           value = Jump; 
           Jump = -1; 
         }
         switch (Equal) {
           case '<':  if (Source < Value) value = Jump; break;
           case '>':  if (Source > Value) value = Jump; break;
           case '!':  if (Source != Value) value = Jump; break;
           case '=':  if (Source == Value) value = Jump; break;
           case 'G':  if (Source >= Value) value = Jump; break;
           case 'L':  if (Source <= Value) value = Jump; break;
           default: break;
         }
        break;      
      case '=':
        value = VAR[Source] = Value;
        break;
      case '+':
        value = VAR[Source];
        VAR[Source]  = CoerceIntegers(value += Value);
        break;
      case '-':
        value = VAR[Source];
        VAR[Source]  = CoerceIntegers(value -= Value);
        break;
      case '*':
        value = VAR[Source];
        VAR[Source]  = CoerceIntegers(value *= Value);
        break;
      case '/':
        value = VAR[Source];
        VAR[Source]  = CoerceIntegers(value /= Value);
        break;
      default: break;
    }
  return (value);
}

int CoerceIntegers(float number)
{
  int value;
  value = (number > 65535)? 65535 : number;
  return value;
}

float SetPort(int port,int value)
  {
   if (PINS[port] != 1) {
     pinMode(port,OUTPUT); 
     PINS[port]=1;
   }
   digitalWrite(port,value);
   return (value);
  }
  
float ReadPort(int port)
  {
   if (PINS[port] == 255) {
     pinMode(port,INPUT); 
     PINS[port] = 0;
   }
   return ( digitalRead(port));
  }

float SetServo(int port,int value)
  {
   if (PINS[port] != 2) {
     myservo.attach(port); 
     PINS[port] = 2;
   }
   myservo.write(value);
   return (value);
  }
  
float PWMPort(int port,int value)
  {
   if (PINS[port] != 1) {
     pinMode(port,OUTPUT); 
     PINS[port]=1;
   }
   analogWrite(port,value);
   return (value);
  }

