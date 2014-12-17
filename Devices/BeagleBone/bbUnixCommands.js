var fs = require('fs'); // gets the file system module

// right now leds on different path, so these fucntions would not work with it
function pinMode (gpio,type){ // ex. pinMode('gpio30','out') - would make pin P9_11 an output
    var path = '/sys/class/gpio/'; // gpio path
    var num = Number(gpio.slice(4)); // gets gpio number ex. 'gpio30' --> '30'
       try { // check to see if pin already exists, if it does overwrite type
            fs.writeFileSync(path + gpio + '/direction', type);
       }
       catch (e) { // if pin doesn't exist, export and write type
            fs.writeFileSync(path + 'export', num);
            fs.writeFileSync(path + gpio + '/direction', type);
            console.log('adding new pin: ' + gpio);
        }
}

// read pin
function digitalRead(gpio){ //ex. digitialRead('gpio30') - would read pin P9_11 value
    var path = '/sys/class/gpio/' + gpio + '/value';
    var value = Number(fs.readFileSync(path, "utf8")); // returns the value in a string '0' or '1' and converts to number
    return value;
}


function digitalWrite(gpio, value){ //ex. digitialWrite('gpio30',0) - would set pin P9_11 value at 0
    var path = '/sys/class/gpio/' + gpio + '/value';
    fs.writeFileSync(path, value);
}

// read analog pin, right now reads between 0-1800 mv
function analogRead(AIN){ // ex. analogRead('AIN0')
    try{ // check to see if the helper.* exists
        var helperNum = getNumber(getOcpPath(),'helper.') // get it's version number, this will create an error if it doesn't exist
        var analogPath = getOcpPath() + '/' + 'helper.' + helperNum + '/' + AIN; // write the path of the analog pin
        var value = Number(fs.readFileSync(analogPath, "utf8")); // read the pin (same as reading digital)
        console.log(value)
        return value;
    }
    catch (e) { 
        fs.writeFileSync(getCapeMgr(), 'cape-bone-iio','ascii'); // if helper not there load in the cape-bone-iio
        console.log('added cape-bone-iio');
        analogRead(AIN); // send it back through the function to read pin
    }
}

// depends on how value is received, conversion may need to be applied for value
function analogWrite(PWM,value){ // ex. analogWrite('gpio7',.50) - will load in pin P9_42 at duty 50%
    am33xxLoad(); // need to load in am33xx to read pwms
    pwmLoad(); // load in the pwm , I might need to change this, not sure what happens if you try to load in the same pwm twice
    var pwmNum = getNumber(getOcpPath(), PWM + '.'); 
    var pwmPath = getOcpPath() + '/bs_pwm_test_' + PWM + '.' + pwmNum; // get pwm pth
    fs.writeFileSync(pwmPath + '/polarity', 0); // set polarity to 0
    var freqency = fs.readFileSync(pwmPath + '/period','utf8') // get frequency
    var value = value * frequency; 
    fs.writeFileSync(pwmPath + '/duty', value); // assign duty
    
    // load am33xx_pwm
    function am33xxLoad(){
        try{ // check to see if it already exists
            fs.readFileSync(getCapeMgr(),'ascii').match('am33xx')[0];
        }
        catch (e) { // otherwise load it in
            fs.writeFileSync(getCapeMgr(), 'am33xx_pwm','ascii');
            console.log('added am33xx_pwm');
        }
    }
    // load pwm
    function pwmLoad(){
        try{ // right the pwm to the cape manager
            fs.writeFileSync(getCapeMgr(), 'bspwm_' + PWM + '_' + getLetter(),'ascii');
            console.log(PWM + ' loaded')
        }
        catch (e) {}
    }
        function getLetter(){ // get the rest of the pwm path
            var findLetter = fs.readdirSync('/lib/firmware').toString().split('bspwm_' + PWM + '_')[1];
            var letter = findLetter.slice(0,1);
            return letter;
        }
}

// get bone_capemgr.* path
function getCapeMgr (){
    var capeMgrNum = getNumber('/sys/devices','capemgr.'); // find version number
    var boneCapeMgr = '/sys/devices/bone_capemgr.' + capeMgrNum + '/slots'; // create path
    return boneCapeMgr; // path
}

// get ocp.* path
function getOcpPath(){
    var ocpNum = getNumber('/sys/devices','ocp.'); // get version number
    var ocpPath =  '/sys/devices/ocp.' + ocpNum; // write path
    return ocpPath; 
}

// find version number
function getNumber(path,device){
    // read the directory that the device is in (returns array), convert to a string,
    // then split the string into 2 arrays at the device name. The 1st entry contains the number
    var findNum = fs.readdirSync(path).toString().split(device)[1];
    var num = 0; var i = 0;
    while(!isNaN(num)){ // convert to a number
        i++
        num = Number(findNum.slice(0,i));
    }
    num = Number(findNum.slice(0,i-1));
    return num;
}


