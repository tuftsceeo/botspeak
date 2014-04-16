#define VERSION 9
#define DirectSize 30
#define ScriptSize 100
#define VarSize 30
#define PINSize 20

#define DIO_SIZE 20
#define AI_SIZE 6
#define PWM_SIZE 6
#define TMR_SIZE 10

//#define SDA 20
//#define SCL 21

int scripting = -1, Jump = 3;

String Reply = "";

unsigned char SCRIPT[ScriptSize],BotCode[DirectSize],PINS[PINSize];  // 255 = open, 0 = input, 1 = output, 2 = servo
unsigned char PWMPins[PWM_SIZE]={3,5,6,9,10,11};
unsigned char DIOPins[DIO_SIZE]={0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19};
unsigned char AnalogInPins[AI_SIZE]={0,1,2,3,4,5};
 
int VARS[VarSize];
long TIMER[TMR_SIZE];

void setup () {
  int i;
  Serial.begin(9600);
  for(i=0; i< PINSize; i++) PINS[i]=255;
  for(i=0; i< VarSize; i++) VARS[i]=0;
  for(i=0; i< TMR_SIZE; i++) TIMER[i]=0;  
  for(i=0; i< ScriptSize; i++) SCRIPT[i]=0;
  for(i=0; i< DirectSize; i++) BotCode[i]=0;
  
  VARS[0]=VERSION; VARS[1]=1; VARS[2]=0;  VARS[3]=0; // intial definitions - version, Hi, Lo, END
  Serial.println("starting ");
}

void loop () { 
  unsigned char length, i, Byte;
  float value;

  if (Serial.available()) {  //if there is something on the serial line - execute it
    length = Serial.read(); 
    
//    Serial.print(length);
//    Serial.print(" ");
    for (i=0; i < length; i++)  {
       while (Serial.available() < 1) { delay (1); }
       BotCode[i] = Serial.read();
    }
/*    for (i=0; i < length; i++)  {
       Serial.print (BotCode[i]);
       Serial.print(" ");
    }*/
   Serial.println(ExecuteCommand(0,0));
 
 /*   Reply = " ";
    Reply = " [" + length ;  

    for (i=0; i < length; i++)  {
       while (Serial.available() < 1) { delay (1); }
       Byte = Serial.read();
       Reply += ',' + Byte;
       if (Byte == 'E') {          // because if you are in script mode, you never run RunBotSpeak
          VARS[3] = scripting;
         scripting = -1;
       }
       if (scripting >= 0) {
         SCRIPT[scripting] = Byte;
         scripting ++; 
       }
       else BotCode[i] = Byte;
      }
    Reply += "]  ";
   if (scripting < 0) RunBotSpeak(length); else Serial.println(Reply); */
  } 
  delay (1);
}

void RunBotSpeak(int length) {
  int i;
  
  switch (BotCode[0]) {
    case 's': 
      scripting = 0;
 //     Serial.print(Reply);
      Serial.println("start script");
      break;
    case 'E':
 //     Serial.print(Reply);
 //     for (i = 0; i < VARS[3]; i++) {Serial.print(SCRIPT[i]); Serial.print(','); }
      Serial.println("end script");
      break;
      
    case 'r':
      Serial.println("Running");
      i = Retrieve(1,0);
      while (i < VARS[3]) {
        Reply = " ";
        ExecuteCommand(i,1);
        i = Jump;
      }
      break;
      
    case 'd':
     i = Retrieve(1,0);
      while (i < VARS[3]) {
        Reply = " ";
        Serial.print(ExecuteCommand(i,1));
        Serial.print(Reply + '\n');
        i = Jump;
      }
      Serial.println("Done");
      break;

    case 'R': 
      i = Retrieve(1,0);
      while (i < VARS[3]) {
        Reply = " ";
        ExecuteCommand(i,1);
        Serial.print('.');
        i = Jump;
      }
      Serial.println("\nDone");
      break;
      
    default: Serial.println(ExecuteCommand(0,0)); break; //Serial.println (Reply);
  }
}

int ExecuteCommand(int ptr,int s) {
  unsigned char Command = (s) ? SCRIPT[ptr] : BotCode[ptr];
  unsigned char comparison = (s) ? SCRIPT[ptr+1] : BotCode[ptr+1];
  unsigned char Compare = 0;
  int A,B;

  Reply = Reply + Command + "|";    
  A=Retrieve(ptr+1,s);
  Jump = ptr + 5;
  switch (Command) {
    case '=':                                                                        return Assign(ptr+1,Retrieve(ptr+3,s),s);
    case '+': Reply = Reply + A + ':' + (B=Retrieve(ptr+3,s));                       return VARS[A] += B;
    case '-': Reply = Reply + A + ':' + (B=Retrieve(ptr+3,s));                       return VARS[A] -= B;
    case '*': Reply = Reply + A + ':' + (B=Retrieve(ptr+3,s));                       return VARS[A] *= B;
    case '/': Reply = Reply + A + ':' + (B=Retrieve(ptr+3,s));                       return VARS[A] /= B;
    case '%': Reply = Reply + A + ':' + (B=Retrieve(ptr+3,s));                       return VARS[A] = VARS[A] % B;
    case '&': Reply = Reply + A + ':' + (B=Retrieve(ptr+3,s));                       return VARS[A] &= B;
    case '|': Reply = Reply + A + ':' + (B=Retrieve(ptr+3,s));                       return VARS[A] |= B;
    case '~': Reply = Reply + A + ':' + (B=Retrieve(ptr+3,s));                       return VARS[A] = !B;
    case '<': Reply = Reply + A + ':' + (B=Retrieve(ptr+3,s));                       return VARS[A] <<= B;
    case '>': Reply = Reply + A + ':' + (B=Retrieve(ptr+3,s));                       return VARS[A] >>= B;
    case 'g': Jump = ptr + 3;                                                        return A;
    case 'W': Jump = ptr + 3;                                    delay(A);           return A;
    case 'w': Jump = ptr + 3;                        delayMicroseconds(A);           return A;
    case 'G': Jump = A;                                                              return Jump;
    case 'e': Jump = VARS[3];                                                        return Jump;
    case '!':                           return SystemCall(ptr,s);
    case 'i':
    case 'I': 
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
     if (Command == 'i') Compare = (Compare == 0); // NOT Compare
      Jump = (Compare) ? Retrieve(ptr+6,s) : ptr + 8;
      Reply = Reply + '(' + A + ' ' + comparison +' ' + B + ')' + "->" + Jump;
      return Jump;
    default: Jump = ptr + 1;                    return -1;
  }
}

int Assign(int destIndex,int value, int s) {
      unsigned char LoByte = (s) ? SCRIPT[destIndex+1] : BotCode[destIndex+1];
      unsigned char HiByte = (s) ? SCRIPT[destIndex] : BotCode[destIndex];
      
      switch (HiByte) {
        case 255: LoByte = (LoByte < VarSize) ? LoByte:VarSize;     VARS[LoByte] = value;                     break;

        case 224: LoByte = (LoByte < DIO_SIZE) ? LoByte:DIO_SIZE;   TogglePort(DIOPins[VARS[LoByte]],value);  break;  
        case 223: LoByte = (LoByte < PWM_SIZE) ? LoByte:PWM_SIZE;   PWMPort(PWMPins[VARS[LoByte]],value);     break;  
        case 222: LoByte = (LoByte < DIO_SIZE) ? LoByte:DIO_SIZE;   SetPort(DIOPins[VARS[LoByte]],value);     break;  
        case 221:                                                                                             break;  // Analog in indirect
        case 220: LoByte = (LoByte < TMR_SIZE) ? LoByte:TMR_SIZE; TIMER[VARS[LoByte]] = millis() + value;     break;

        case 204: LoByte = (LoByte < DIO_SIZE) ? LoByte:DIO_SIZE;   TogglePort(DIOPins[LoByte],value);        break;  
        case 203: LoByte = (LoByte < PWM_SIZE) ? LoByte:PWM_SIZE;   PWMPort(PWMPins[LoByte],value);           break;  
        case 202: LoByte = (LoByte < DIO_SIZE) ? LoByte:DIO_SIZE;   SetPort(DIOPins[LoByte],value);           break;
        case 201:                                                                                             break;  // Analog in
        case 200: LoByte = (LoByte < TMR_SIZE) ? LoByte:TMR_SIZE; TIMER[LoByte] = millis() + value;           break;
        default:  
          if (HiByte/128) VARS[indexValue(HiByte,LoByte)] = value;    
          else VARS[HiByte * 256 + LoByte] = value;                                                        
          break;  
      }
      Reply = Reply + HiByte + ':' + LoByte; 
      return value;
}

int Retrieve(int index,int s) {
      int value, Array_Size = 0;
      unsigned char LoByte = (s) ? SCRIPT[index+1] : BotCode[index+1];
      unsigned char HiByte = (s) ? SCRIPT[index] : BotCode[index];
      
      switch (HiByte) {
        case 255: LoByte = (LoByte < VarSize) ? LoByte:VarSize;     value = VARS[LoByte];                     break;

        case 222: LoByte = (LoByte < DIO_SIZE) ? LoByte:DIO_SIZE;   value = ReadPort(DIOPins[VARS[LoByte]]);  break;
        case 221: LoByte = (LoByte < VarSize) ? LoByte:VarSize;     value = analogRead(VARS[LoByte]);         break;
        case 220: LoByte = (LoByte < TMR_SIZE) ? LoByte:TMR_SIZE; value =  millis() - TIMER[VARS[LoByte]];    break;

        case 202: LoByte = (LoByte < DIO_SIZE) ? LoByte:DIO_SIZE;   value = ReadPort(DIOPins[LoByte]);        break;
        case 201: LoByte = (LoByte < AI_SIZE) ? LoByte:AI_SIZE;     value = analogRead(LoByte);               break;
        case 200: LoByte = (LoByte < TMR_SIZE) ? LoByte:TMR_SIZE; value =  millis() - TIMER[LoByte];          break;
        default:                                                                
          if (HiByte/128) value = VARS[indexValue(HiByte,LoByte)]; 
          else value = HiByte * 256 + LoByte;       
          break;          
      }
      return value;
}


int indexValue(unsigned char HiByte,unsigned char LoByte)  {
  int Array_Size;

  Array_Size = VARS[(HiByte & 63)];                 // sends array_SIZE - so add one for array_0
  LoByte = (LoByte < VarSize) ? LoByte:VarSize;
  if ((HiByte & 64)/64) LoByte = VARS[LoByte];      // 128 = array, 64 = indirect (array[test])
  LoByte = (LoByte < Array_Size) ? LoByte:Array_Size - 1;
  return (HiByte & 63) + 1 + LoByte;
}

int ReadPort(int port)  {
   if (PINS[port] == 255) {
     pinMode(port,INPUT); 
     PINS[port] = 0;
   }
   return (digitalRead(port));
}

int SetPort(int port,int value) {
   if (PINS[port] != 1) {
     pinMode(port,OUTPUT); 
     PINS[port]=1;
   }
   digitalWrite(port,value);
   return (value);
}

int PWMPort(int port,int value)
  {
   if (PINS[port] != 1) {
     pinMode(port,OUTPUT); 
     PINS[port]=1;
   }
    // rescale to 1024
   analogWrite(port,map(value, 0, 100, 0, 255));
   return (value);
  }
  
int TogglePort(int port,int value)
  {
   if (PINS[port] != 1) {
     pinMode(port,OUTPUT); 
     PINS[port]=1;
   }
   digitalWrite(port,HIGH);
   delayMicroseconds(value);
   digitalWrite(port,LOW);
   return (value);
  }
  
  
int SystemCall(int ptr,int s)  {
  unsigned char CmdSize = (s) ? SCRIPT[ptr + 1] : BotCode[ptr + 1];
  
  int freq = Retrieve(ptr + 4,s);
  int ampl = Retrieve(ptr + 6,s);  
  int ddd = Retrieve(ptr + 8,s);
  int rrr=ddd*ampl;
  
  Jump = ptr + CmdSize + 2;
  
  Assign(ptr + 2,rrr,s);                // what gets saved in remote mode
  Reply = Reply + " " + Jump;         // what gets returned in debug mode
  return ampl*freq;                           // what gets returned in direct mode
}

