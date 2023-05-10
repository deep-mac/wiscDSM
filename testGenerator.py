import sys
import numpy as np
import random

startAddr = 0x40000000

def putInclude(lib):
    str_ = "#include<" + lib + ">"
    print(str_)

def putFunc(totalVars, totalOps, ratio):
    funcName = "func_" + str(totalVars) + "_" + str(totalOps) + "_" + str(ratio)
    print("int " + funcName + "() {\n")

def putRet():
    print("\treturn 0;\n}")

def putVars(totalVars, stride):
    global startAddr
    for i in range (totalVars):
        str_ = "\tint *var" + str(i) + ";"
        print(str_)
        str_ = "\tvar" + str(i) + " = (int*)"+hex(startAddr) + " + (int)" + str(i) + ";"
        print(str_)
        if stride:
            startAddr = startAddr + 0x1000
    str_ = "\tint temp;"
    print(str_)

def putRead(varNum):
    str_ = "\ttemp = *var" + str(varNum) + ";"
    print(str_)

def putWrite(varNum, num):
    str_ = "\t*var" + str(varNum) + "= " + str(num) + ";"
    print(str_)

totalVars = int(sys.argv[1])
totalOps = int(sys.argv[2])
ratio = float(sys.argv[3])
stride = False
try:
    if (sys.argv[4]):
       stride = True
except:
    stride = False

putInclude("stdio.h")
putInclude("stdlib.h")
putFunc(totalVars, totalOps, ratio)
putVars(totalVars, stride)
for i in range(totalOps):
    rand_ = np.random.uniform(0, 1)
    var = random.randint(0, totalVars-1)
    num = random.randint(0, 99)
    if(rand_ < ratio):
        putWrite(var, num)
    else:
        putRead(var)
putRet()
