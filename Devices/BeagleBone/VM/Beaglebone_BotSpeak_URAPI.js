var net = require('net');
var b = require('bonescript');
var fs = require('fs');

var pins = [ "USR0", "USR1", "USR2", "USR3", "P9_11","P9_12","P9_13","P9_15","P9_17",
			"P9_23","P9_27","P9_30","P9_41","P8_11","P8_12","P8_14","P8_15","P8_16","P8_17",
			"P8_18","P8_26"];
var AnalogInPins = ["P9_33","P9_35","P9_36","P9_37","P9_38","P9_39","P9_40"];  //Pin numbers for ADC's
var PWMPins = ["P9_14","P9_16","P9_21","P9_22","P9_42","P8_13", "P8_19"]; //PWM capable pins

var DIO_SIZE = pins.length;
var AI_SIZE = AnalogInPins.length;
var PWM_SIZE = PWMPins.length;

var SCRIPT = []; //initialize script space; javascript autosizes arrays
var TIMER = []; //initialize timer space
var VARS = {VER:'4', HI:'1', LO:'0', END: '0'}; //initialize variable space, first 4 are reserved for system info

var server = net.createServer(socketOpen); //start the server

function socketOpen(socket) {
    socket.on('end', tcpClose);
    function tcpClose() {
    }
    socket.on('data', tcpData);
    var my_input = "";
    function tcpData(data) {
        my_input = my_input + data;
	console.log('input received:\n' + my_input);
	var n = my_input.indexOf("\r\n");        
        if(n != -1) {
            var command = my_input.split("\r\n");
            var reply = RunBotSpeak(command[0],socket); // don't want \r\n and whatever is after it - assume only one \r\n
            if (reply !== '') socket.write(reply + "\r\n");
            if ((reply !== "close") && (command[0] !== '')) console.log("Got: " + command[0].replace(/\n/g,",") + " Replied: " + reply.replace(/\n/g,","));
            my_input = "";
        }
    }
}

console.log("starting");
server.listen(2012);
Startlights();

function RunBotSpeak (command,socket) {
    var BotCode = command.split('\n');
       //    console.log(BotCode);
    var TotalSize = BotCode.length;
    var reply = "";
    var scripting = -1, ptr = 0,i,j;
    
    function RunScript (debug){
		j = Retrieve(BotCode[i].slice(BotCode[i].indexOf(' ')));
		VARS["END"] = SCRIPT.length - 1;
		while (j < VARS["END"]) {
			var reply1 = ExecuteCommand(SCRIPT[j]);
			console.log('executed '+SCRIPT[j]+' -> ' + reply1);
			if (debug) socket.write(SCRIPT[j] + ' -> '+ reply1 + '\n');
			var goto = String(reply1).split(' ');
			j = (goto[0] == "GOTO") ? Number(goto[1]): j + 1;
		}
		if (debug) reply += "ran " +VARS["END"] + " lines of script\nDone";
		if (!debug) reply += "ran script";
	}

    for (i = 0;i < TotalSize; i++) {
        //        console.log(GetCommand(BotCode[i]));
        switch (GetCommand(BotCode[i])) {
            case "":console.log("blank"); break;
                
            case "SCRIPT":		//start recording the script to memory
                scripting = 0;
                ptr = 0;
                reply += "start script\n";
                break;
                
            case "ENDSCRIPT":		//finish recording the script to memory
                scripting = -1;
                ptr = 0;
                reply += "end script\n";
                // console.log(SCRIPT);
                break;
                
            case "RUN": // run the script without debugging
                socket.write('\r\n');
                RunScript(false);
				break;

            case "RUN&WAIT":
            case "DEBUG":		//Run script in debug mode: echo each command as it executes and also print the value returned by the command
                RunScript(true);
                break;
                
            case "Done": break;
            default: {
                if (scripting >= 0) {
                    SCRIPT[ptr] = BotCode[i];
                    reply += SCRIPT[ptr] + '\n';
                    ptr ++;
                }
                else reply += ExecuteCommand(BotCode[i]) + '\n';
			}
        }
    }
    return reply;
}

function ExecuteCommand(Code) {
    if (trim(Code.slice(0,2)) == '//') return " ";  // "//" means it's just a comment
    if (trim(Code) =='') return '';   // a space is a blank line
    var command = GetCommand(Code);   // everything before the space
    var args = trim(Code.slice(Code.indexOf(' '))).split(',');  // everything after the space, split by ','
    var dest = trim(args[0]); //trim the space off first operand, which is the destination
    var value = (args.length > 1) ? trim(args[1]):0; //if there is more than one operand, set the second one as the value; otherwise value = 0 because we only have one operand
    var waittime = 0;
    
    switch (command) {
        case "SET": return Assign(dest,Retrieve(value));		//set value
        case "GET": return Retrieve(dest);		//get value
        case "WAIT": waittime = 1000*Retrieve(dest); return delay(waittime);		//wait in milliseconds; BotSpeak sends wait in seconds
        case "WAITµs": microdelay(Retrieve(dest)); return Retrieve(dest);		//wait in microseconds
        case "ADD": return VARS[dest] += Retrieve(value); 
        case "SUB": return VARS[dest] -= Retrieve(value);
        case "MUL": return VARS[dest] *= Retrieve(value);
        case "DIV": return VARS[dest] /= Retrieve(value);
        case "AND": return VARS[dest] &= Retrieve(value);
        case "OR":  return VARS[dest] |= Retrieve(value);
        case "NOT": return VARS[dest] != Retrieve(value);
        case "BSL": return VARS[dest] <<= Retrieve(value);
        case "BSR": return VARS[dest] >>= Retrieve(value);
        case "MOD": return VARS[dest] %= Retrieve(value);
        case "GOTO":return Jump = command + ' ' + dest;		//used in loops for the most part
        case "LBL": return 0;		//not sure what this is supposed to do; website says it "Stores the current script line into the variable Jump (so you can use IF 1 GOTO Jump)"
        case "IF": {
        	var comparison;
            var conditional = trim(Code.slice(Code.indexOf('(')+1,Code.indexOf(')'))); // get conditional
            var Jump = trim(Code.slice(Code.indexOf('GOTO'))); // get 'GOTO #'
            var params = conditional.split(' ');
            switch (trim(params[1])) {
                case '>':   comparison = (Retrieve(trim(params[0])) > Retrieve(trim(params[2]))); break;
                case '<':   comparison = (Retrieve(trim(params[0])) < Retrieve(trim(params[2])));  break;
                case '=':  comparison = (Retrieve(trim(params[0])) == Retrieve(trim(params[2]))); break;
                case '==':  comparison = (Retrieve(trim(params[0])) == Retrieve(trim(params[2]))); break;
                case '!=':  comparison = (Retrieve(trim(params[0])) != Retrieve(trim(params[2]))); break;
                case '<=':  comparison = (Retrieve(trim(params[0])) <= Retrieve(trim(params[2]))); break;
                case '>=':  comparison = (Retrieve(trim(params[0])) >= Retrieve(trim(params[2]))); break;
                default: return "unsupported conditional";
            }
            if (Code.match('~') != null) comparison = !comparison;
            if (comparison) return Jump; else return 1;
        }    
        case "SYSTEM": return SystemCall(args); //used to enable  user-programmed commands
        case "": return 1; //blank line
        case "end": return "end";
        default: return (command + " not yet supported");
    }
}

function Assign(dest,value)  {		//function that writes values to the various registers - DIO, AO, PWM, etc
    var i;
    var param = dest.split('[');
    var param1 = GetArrayIndex(param);
    switch (param[0]) {
        case "DIO":			//first set pin as output, then write the value
            param1 = (param1 < DIO_SIZE)?param1:0;
            b.pinMode(pins[param1] , b.OUTPUT);
            b.digitalWrite(pins[param1], value);
            return value;
            
        case "AO":	return 0;	//BB has no analog out
        case "PWM":		//first set up PWM pin, then write value
            param1 = (param1 < PWM_SIZE)?param1:0;
            b.pinMode(PWMPins[param1] , b.ANALOG_OUTPUT);
            value = value / 100;  // PWM goes from 0 - 100 percent, but beaglescript pwm function takes 0-1
            value = (value < 1) ? value: 1;
            console.log("param " + param1 + " Pin " +  PWMPins[param1] + " value " + value);
            b.analogWrite(PWMPins[param1], value);
            return value;
            
        case "TMR":		
            TIMER[param1] = value + new Date().getTime();
            return value;
            
            // Read Only Variables
        case "VER": return VARS["VER"];
        case "AI":		//read analog pin
            param1 = (param1 < AI_SIZE)?param1:0;
            return b.analogRead(AnalogInPins[param1]);
        case "SERVO":
            param1 = (param1 < PWM_SIZE)?param1:0;
            b.pinMode(PWMPins[param1] , b.ANALOG_OUTPUT);
            value = (value > 1) ? value: 1;
            value = (value < 2) ? value: 2;
        	value = (value*.1) - 0.05;
        	b.analogWrite(PWMPins[param1], value, 60);
        	return value;
        default:
            if (param.length == 1) {
                var arrayName=dest.split('_SIZE');  // see if they are defining an array
                if (arrayName.length > 1) {
                    VARS[arrayName[0]] =[];
                    for (i=0;i<value;i++) VARS[arrayName[0]][i]=0;
                    VARS[arrayName[0]+'_COLS']=1;  // assume a 1D array initially
                }
                return VARS[dest] = value;
            }
            //            console.log("Assigning "+param[0]+"__"+param1+" of "+VARS[param[0]+'_SIZE']);
            param1 = (param1 < VARS[param[0]+'_SIZE'])?param1:VARS[param[0]+'_SIZE'] - 1;
            
            return VARS[param[0]][param1] = value;
    }
}

function Retrieve(source)  {
    var param = source.split('[');
    var param1 = GetArrayIndex(param);
    var reply = '';
    
    switch (param[0]) {
        case "DIO":
            if (param1 < DIO_SIZE) b.pinMode(pins[param1],b.INPUT);
            reply = (param1 < DIO_SIZE)?b.digitalRead(pins[param1]):0; break;
        case "AI":  reply = (param1 < AI_SIZE)?b.analogRead(AnalogInPins[param1]):0; break;
        case "TMR": reply = new Date().getTime() - TIMER[param1]; break;
        default: {
            switch(param.length) {
                case 1: {
                    if (VARS[source] === undefined) reply = Number(source);
                    else reply = VARS[source];  // if not declared then probably a number
                    break;
                }
                default:  {
                    //                    console.log("Retrieving "+param[0]+"__"+param1+" of "+VARS[param[0]+'_SIZE']);
                    var ArrayPtr = (param1 < VARS[param[0]+'_SIZE'])? param1 : VARS[param[0]+'_SIZE']-1;
                    reply = VARS[param[0]][ArrayPtr];
                    break;
                }
                    
            }
        }
    }
    return reply;
}

function GetArrayIndex(param)  {
    if (param.length <= 1) return -1;
    var TwoD=param[1].split(':');
    //    console.log(param[1]+' : '+TwoD);  //2D does not work because the comma is used elsewhere
    if (TwoD.length <= 1) return Retrieve(trim(param[1].split(']')[0]));  // recursive to allow variables in the brackets]
    console.log(Retrieve(TwoD[0]*VARS[param[0]+'_COLS'])+Retrieve(trim(TwoD[1].split(']')[0])));
    return Retrieve(TwoD[0]*VARS[param[0]+'_COLS'])+Retrieve(trim(TwoD[1].split(']')[0]));
}

function trim(AnyString) { // get rid of spaces before and after
    return AnyString.replace(/^\s+|\s+$/g,"");
}
function delay (msec) {
    var start = new Date().getTime();
    for (var i = 0; i < 1e7; i++) {
        if ((new Date().getTime() - start) > msec) break;
    }
    return msec;
}
function microdelay (msec) {
    var start = new Date().getTime();
    for (var i = 0; i < 1e7; i++) {
        if ((new Date().getTime() - start) > msec/1000) break;
    }
}
function Startlights() {
    var i;
    for (i = 1; i < 4; i++) {
        b.pinMode(pins[i], 'out');
        b.digitalWrite(pins[i], 1);
        delay (100);
    }
    for (i = 3; i >= 0; i--) {
        b.digitalWrite(pins[i], 0);
        delay (100);
    }
}

function GetCommand(Code) {
    return (Code.indexOf(' ') >= 0) ? trim(Code.slice(0,Code.indexOf(' '))) : Code;
}

function SystemCall(args) { //this is just an example of something a user might program
    
}

process.on('uncaughtException', function(err) {
	console.log('Exception: ' + err);

	// Trigger autorun to restart us
	var stat = fs.statSync(__filename);
	fs.utimesSync(__filename, stat.atime, new Date());
	process.exit(1);
});
