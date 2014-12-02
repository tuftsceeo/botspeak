var net = require('net');
var os = require('os');
var sys = require('sys');
var fs = require('fs');

var pins = [ "DIO0", "DIO1", "DIO2", "DIO3", "DIO4","DIO5","DIO6","DIO7","DIO8","DIO9","DIO10","DIO11","DIO12","DIO13","DIO14","DIO15","DIO16"]; // digital IO pins
var gpio = ["gpio0", "gpio1", "gpio4", "gpio17", "gpio21", "gpio22", "gpio10", "gpio9", "gpio11", "gpio7", "gpio8", "gpio25", "gpio24", "gpio23", "gpio18", "gpio15", "gpio14"];

var DIO_SIZE = pins.length;

var SCRIPT = []; //initialize script space; javascript autosizes arrays
var TIMER = []; //initialize timer space
var VARS = {VER:'1', HI:'1', LO:'0', END: '0'}; //initialize variable space, first 4 are reserved for system info

var server = net.createServer(socketOpen); //start the server

function socketOpen(socket) {
    socket.on('end', tcpClose);
    function tcpClose() {
    }
    socket.on('data', tcpData);
    var address = socket.remoteAddress;
    var port = socket.remotePort;
    console.log("New connection from " + address + ":" + port );
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

var ifaces=os.networkInterfaces();
for (var dev in ifaces) {
	var alias=0;
	ifaces[dev].forEach(function(details){
		if (details.family=='IPv4') {
			console.log(details.address+":"+server.address().port);
			++alias;
		}
	});
}

function pinMode(DIOIndex, type) { // ex. pinMode('DIO0','out') - would make pin DIO0 an output
	
	var index = pins.indexOf(DIOIndex);
	if(index !== -1) {
		
		var path = '/sys/class/gpio/'; // gpio path
		var num = Number(gpio[index].slice(4)); // gets gpio number ex. 'gpio30' --> '30'
		try { // check to see if pin already exists, if it does overwrite type
			
			fs.writeFileSync(path + gpio[index] + '/direction', type);
		}
	    catch (e) { // if pin doesn't exist, export and write type
	    	
	    	fs.writeFileSync(path + 'export', num);
	    	fs.writeFileSync(path + gpio[index] + '/direction', type);
	    	console.log('adding new pin: ' + gpio[index]);
	    }
	}
}

// read pin
function digitalRead(DIOIndex){ //ex. digitialRead('gpio30') - would read pin P9_11 value
    
	var value = -1;
	var index = pins.indexOf(DIOIndex);
	if(index !== -1) {
		
		var path = '/sys/class/gpio/' + gpio[index] + '/value';
	    value = Number(fs.readFileSync(path, "utf8")); // returns the value in a string '0' or '1' and converts to number
		
	}
    return value;
}


function digitalWrite(DIOIndex, value){ //ex. digitialWrite('gpio30',0) - would set pin P9_11 value at 0
	
	var index = pins.indexOf(DIOIndex);
	if(index !== -1) {
		
		 var path = '/sys/class/gpio/' + gpio[index] + '/value';
		 fs.writeFileSync(path, value);
	}
}

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
            pinMode(pins[param1] , 'out');
            digitalWrite(pins[param1], value);
            return value;
                        
        case "TMR":		
            TIMER[param1] = value + new Date().getTime();
            return value;
            
            // Read Only Variables
        case "VER": return VARS["VER"];
       
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
	    if (param1 < DIO_SIZE) pinMode(pins[param1],'in');
            reply = (param1 < DIO_SIZE)?digitalRead(pins[param1]):0;
            break;
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
