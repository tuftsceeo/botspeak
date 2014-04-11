import cmd, sys, os, time
import operator as op
import RPi.GPIO as GPIO
from BrickPi import *
from parsimonious.grammar import Grammar
import threading

#BrickPi Mappings:
#PWM[0] -> Right Motor
#PWM[3] -> Left Motor
#DIO[0] -> Right bumper
#DIO[1] -> GPIO 12 right LED
#DIO[2] -> GPIO 13 left LED
#DIO[3] -> Left bumper

PRGM_COUNTER = 0     #Program counter used run scripts
HALT         = False #Used to Halt the BrickPi concurrent thread
SCRIPTS      = {}    #Dictionary of all the scripts
STDIN        = False #True if we're running the interpreter in standard input mode
SCPT_MODE    = False #True if a script is running
SCPT_INPUT   = False #True if we've detected a 'SCRIPT'
SCPT_LINE_CTR  = 0   #Line counter used when SCPT_INPUT is True
CURR_SCPT_NAME = ''  #The current script name we are inputting

SCRIPTS['PWMDIO_TEST'] = {
    0: 'SET ctr, 0',
    1: 'SET PWM[0], SET DIO[1], 255', 
    2: 'WAIT 1', 
    3: 'SET PWM[0], SET DIO[1], 0',
    4: 'WAIT 1',
    5: 'SET PWM[3], SET DIO[2], 255',
    6: 'WAIT 1',
    7: 'SET PWM[3], SET DIO[2], 0',
    8: 'ADD ctr, 1',
    9: 'if (ctr < 2) then GOTO 1'
    }

#checks if string value is a number
def is_number(s):
    try:
        float(s)
        return True
    except ValueError:
        return False

#parsimonious interpreter grammar rules + functions
class BotSpeak(object):

    def __init__(self, env={}):
        global SCRIPTS
        env['PWM']      = {}
        env['SCRIPTS']  = SCRIPTS
        env['VER']      = '1.0'
        self.env = env

    def parse(self, source):
        grammar = '\n'.join(v.__doc__ for k, v in vars(self.__class__).items()
                      if '__' not in k and hasattr(v, '__doc__') and v.__doc__)
        return Grammar(grammar)['program'].parse(source)

    def eval(self, source):
        global SCPT_INPUT
        
        node = self.parse(source) if isinstance(source, str) else source

        #Detect when 'SCRIPT' is used and store the subsequent lines
        if SCPT_INPUT:
            global SCRIPTS
            global SCPT_LINE_CTR
            global CURR_SCPT_NAME
            scpt_line = node.text.strip()
            if scpt_line != "ENDSCRIPT":
                SCRIPTS[CURR_SCPT_NAME][SCPT_LINE_CTR] = scpt_line
                SCPT_LINE_CTR += 1
                return scpt_line
            else:
                CURR_SCPT_NAME = ''
                SCPT_LINE_CTR = 0
                SCPT_INPUT    = False
                return "end script"
        else:
            method = getattr(self, node.expr_name, lambda node, children: children)
            if node.expr_name in ['ifthen', 'ifelse']:
                return method(node)
            return method(node, [self.eval(n) for n in node])

    def program(self, node, children):
        'program = expr*'
        return children

    def expr(self, node, children):
        'expr = _ (setvar / getvar / sys_call / script / run_script / goto / wait / ifthen / infix / arith / float / number / name) _'
        return children[1][0]

    def sys_call(self, node, children):
        'sys_call = "SYSTEM" _ name _ "," _ number _ "," _ number _'
        _, _, var, _, _, _, sysID, _, _, _, encID, _ = children
        def f(sysID, encID):
            return {
                1: (BrickPi.Encoder[encID] % 720) / 2
           }.get(sysID, -1)
        return f(sysID, encID)

    #To-do: handle recursively defined scripts
    #To-do: add label support
    def script(self, node, children):
        'script = "SCRIPT" _ ((" "+ name) / (" "+ number))?'
        _, _, script_name = children
        global SCRIPTS
        global STDIN
        global SCPT_INPUT
        global CURR_SCPT_NAME

        if len(script_name) > 0:
            script_name = script_name[0][0]
        else:
            script_name = 0

        SCRIPTS[script_name] = {}
        SCPT_INPUT = True
        CURR_SCPT_NAME  = script_name
        return "start script"

    #To-do: add label support
    def run_script(self, node, children):
        'run_script = ("RUN&WAIT" / "RUN") _ (name / number)?'
        _, _, script_name = children
        global SCRIPTS
        global PRGM_COUNTER
        global SCPT_MODE

        if len(script_name) > 0:
            script_name = script_name[0][0]
        else:
            script_name = 0

        # Try and get the script
        bsp_script  = SCRIPTS.get(script_name, -1)
        if bsp_script is not -1:
            
            SCPT_MODE = True
            script_len  = len(bsp_script)
            
            while PRGM_COUNTER < script_len:
                #print 'prgm_ctr: ' + str(PRGM_COUNTER)
                #print 'line: ' + bsp_script[PRGM_COUNTER]
                BotSpeak().eval(bsp_script[PRGM_COUNTER])
                PRGM_COUNTER += 1

            SCPT_MODE = False
            PRGM_COUNTER = 0
            return "Done"
        else:
            return -1

    #To-do: add label support
    def goto(self, node, children):
        'goto = "GOTO" _ number'
        _, _, instruction = children
        global PRGM_COUNTER
        PRGM_COUNTER = instruction - 1
        return instruction

    def wait(self, node, children):
        'wait = "WAIT" _ (float / number)'
        _, _, wait_time = children
        time.sleep(wait_time[0])
        return wait_time[0]

    def ifthen(self, node):
        'ifthen = "if" expr "then" expr'
        _, cond, _, cons = node
        return self.eval(cons) if self.eval(cond) else False

    def infix(self, node, children):
        'infix = "(" expr comp_op expr ")"'
        _, expr1, comp_op, expr2, _ = children

        identifier1 = str(expr1).strip('[\'\']')
        identifier2 = str(expr2).strip('[\'\']')

        #case for looking up variables
        if not is_number(identifier1):
            val1 = self.env.get(identifier1, -1)
        else:
            val1 = expr1

        if not is_number(identifier2):
            val2 = self.env.get(identifier2, -1)
        else:
            val2 = expr2
            
        return comp_op(val1, val2)

    def comp_op(self, node, children):
        'comp_op = "<" / ">" / "<=" / ">=" / "!=" / "=="'
        comp_ops = {'<': op.lt, '>': op.gt, '<=': op.le, '>=': op.ge, '!=': op.ne, '==': op.eq}
        return comp_ops[node.text]
    
    def arith(self, node, children):
        'arith = arith_ops _ (name / float / number) _ "," _ expr'
        operator, _, val1, _, _, _, val2 = children

        identifier1 = str(val1).strip('[\'\']')
        identifier2 = str(val2).strip('[\'\']')

        if not is_number(identifier2):
            val2 = self.env.get(identifier2, -1)

        if not is_number(identifier1):
            val1 = self.env.get(identifier1, -1)
            if val1 != -1 or val2 != -1:
                val1 = operator(val1, val2)
                self.env[identifier1] = val1
            else:
                val1 = -1
        else:
            val1 = operator(float(identifier1), float(identifier2))
        return val1

    def arith_ops(self, node, children):
        'arith_ops = "ADD" / "SUB" / "DIV" / "MUL"'
        operators = {'ADD': op.iadd, 'SUB': op.isub, 'DIV': op.idiv, 'MUL': op.imul}
        return operators[node.text]

    def operator(self, node, children):
        'operator = "<" / ">" / "<=" / ">=" / "!=" / "=="'
        operators = {'<': op.lt, '>': op.gt, '<=': op.le, '>=': op.ge, '!=': op.ne, '==': op.eq}
        return operators[node.text]

    def setvar(self, node, children):
        'setvar = "SET" _ (array_val / name) _ "," _ expr'
        _, _, lvalue, _, _, _, val = children

        identifier = str(val).strip('[\'\']')
        
        #if the right hand side is a variable, look it up
        if not is_number(identifier):
            val = self.env.get(identifier, -1)

        if isinstance(lvalue[0], list):
            array_name  = lvalue[0][0]
            array_index = int(lvalue[0][1])

            #PWM[0] -> motor 1, PWM[1] -> Motor 1
            if array_name == 'PWM' and (array_index < 4):
                BrickPi.MotorSpeed[array_index] = int(val)

            #DIO[1] -> pin 12 right LED
            #DIO[2] -> pin 13 left LED
            if array_name == 'DIO':
                if array_index == 1:
                    GPIO.output(12, val)
                if array_index == 2:
                    GPIO.output(13, val)                

            else:
                the_array = self.env.get(array_name, -1)
                if the_array == -1:
                    self.env[array_name] = {}
                    self.env[array_name][array_index] = val
                else:
                    self.env[array_name][array_index] = val                    
        else:
            self.env[lvalue[0]] = val

        return val

    def getvar(self, node, children):
        'getvar = "GET" _ (array_val / name)'
        _, _, lvalue = children

        if isinstance(lvalue[0], list):
            array_name  = lvalue[0][0]
            array_index = int(lvalue[0][1])

            #DIO[0] -> right bumper
            #DIO[3] -> left bumper            
            if array_name == 'DIO' and (array_index == 0 or array_index == 3):
                return BrickPi.Sensor[array_index]
                
            if array_name == 'PWM':
                return -1

            the_array = self.env.get(array_name, -1)
            if the_array != -1:
                try:
                    return the_array[array_index]
                except KeyError:
                    return -1
            else:
                return -1
        else:
            return self.env.get(lvalue[0], -1)
    
    def array_val(self, node, children):
        'array_val = name "[" number "]"'
        var, _, index, _ = children
        return [var, index]

    def name(self, node, children):
        'name = ~"[A-Za-z_-]+" _'
        return node.text.strip()

    def float(self, node, children):
        'float = "-"? (~"[0-9]*" "." ~"[0-9]*")'
        return float(node.text.strip())

    def number(self, node, children):
        'number = "-"? ~"[0-9]+"'
        return int(node.text.strip())

    def _(self, node, children):
        '_ = ~"\s*"'

#Thread that run in parallel with BotSpeak to update BrickPi values
def updateBrickPi():
    global HALT
    while not HALT:
        BrickPiUpdateValues()
        time.sleep(.05)

#Initializes BrickPi motors, LEDs, and sensors
def initBrickPi():
    BrickPiSetup()  # setup the serial port for communication

    BrickPi.MotorEnable[PORT_A] = 1 #Enable the Motor A
    BrickPi.MotorEnable[PORT_B] = 1 #Enable the Motor B
    BrickPi.MotorEnable[PORT_C] = 1 #Enable the Motor C
    BrickPi.MotorEnable[PORT_D] = 1 #Enable the Motor D

    BrickPi.MotorSpeed[PORT_A] = 0  #Set the speed of MotorA (-255 to 255)
    BrickPi.MotorSpeed[PORT_B] = 0  #Set the speed of MotorB (-255 to 255)
    BrickPi.MotorSpeed[PORT_C] = 0  #Set the speed of MotorC (-255 to 255)
    BrickPi.MotorSpeed[PORT_D] = 0  #Set the speed of MotorD (-255 to 255)

    BrickPi.SensorType[PORT_1] = TYPE_SENSOR_TOUCH   #Bumper right bumper
    BrickPi.SensorType[PORT_4] = TYPE_SENSOR_TOUCH   #Bumper left bumper
    
    BrickPiSetupSensors()   #Send the properties of sensors to BrickPi

    #Setup GPIO for LEDs
    GPIO.setwarnings(False)
    GPIO.setmode(GPIO.BOARD)
    GPIO.setup(12, GPIO.OUT) #right LED
    GPIO.setup(13, GPIO.OUT) #left LED

    updateStuff = threading.Thread( target=updateBrickPi );
    updateStuff.start()

#Called via bs_server.py to gracefully stop updateBrickPi() thread
def haltBSP():
    global HALT
    HALT = True

if __name__ == "__main__":
    #Initialize BrickPi
    # initBrickPi()
    STDIN = True
    while True:
        try:
            c = raw_input(str(CURR_SCPT_NAME) + "> ")
            print BotSpeak().eval(c)
        except EOFError:
            HALT = True
            print "\nBye."
            break
#        except:
#            HALT = True
#            exc_type, exc_obj, exc_tb = sys.exc_info()
#            fname = os.path.split(exc_tb.tb_frame.f_code.co_filename)[1]
#            print(exc_type, fname, exc_tb.tb_lineno)
