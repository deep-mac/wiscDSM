import sys
import numpy as np
import random

startAddr = 0x40000000

def putHeaderGuard(fileName, fp, end):
    headerGuard = fileName[:-2].upper()
    if not end:
       fp.write('#ifndef ' + headerGuard + '\n')
       fp.write('#define ' + headerGuard + '\n\n')
    else:
       fp.write('#endif\n')

def putInclude(lib, fp):
    str_ = "#include<" + lib + ">"
    fp.write(str_)
    fp.write('\n')

def putFunc(totalVars, totalOps, ratio, stride, fp):
    ratio100 = int(ratio*100)
    funcName = "func_" + str(totalVars) + "_" + str(totalOps) + "_" + str(ratio100)
    if stride:
       funcName = funcName + "_stride"
    fp.write("int " + funcName + "() {\n")
    fp.write('\n')


def putRet(fp):
    fp.write("\treturn 0;\n}")
    fp.write('\n')


def putVars(totalVars, stride, fp):
    global startAddr
    for i in range (totalVars):
        str_ = "\tint *var" + str(i) + ";"
        fp.write(str_)
        fp.write('\n')
        str_ = "\tvar" + str(i) + " = (int*)"+hex(startAddr) + " + (int)" + str(i) + ";"
        fp.write(str_)
        fp.write('\n')
        if stride:
            startAddr = startAddr + 0x1000
    str_ = "\tint temp;"
    fp.write(str_)
    fp.write('\n')


def putRead(varNum, fp):
    str_ = "\ttemp = *var" + str(varNum) + ";"
    fp.write(str_)
    fp.write('\n')


def putWrite(varNum, num, fp):
    str_ = "\t*var" + str(varNum) + "= " + str(num) + ";"
    fp.write(str_)
    fp.write('\n')


totalVars = int(sys.argv[1])
totalOps = int(sys.argv[2])
ratio = float(sys.argv[3])
stride = False
try:
    if (sys.argv[4]):
       stride = True
except:
    stride = False

fileName = ""
ratio100 = int(ratio*100)
if stride:
    fileName = "test_" + str(totalVars) + "_" + str(totalOps) + "_" + str(ratio100) + "_stride.h"
else:
    fileName = "test_" + str(totalVars) + "_" + str(totalOps) + "_" + str(ratio100) + ".h"
fp = open(fileName, "w")
putHeaderGuard(fileName, fp, False)
putInclude("stdio.h",fp)
putInclude("stdlib.h", fp)
putFunc(totalVars, totalOps, ratio, stride, fp)
putVars(totalVars, stride, fp)
for i in range(totalOps):
    rand_ = np.random.uniform(0, 1)
    var = random.randint(0, totalVars-1)
    num = random.randint(0, 99)
    if(rand_ < ratio):
        putWrite(var, num, fp)
    else:
        putRead(var, fp)
putRet(fp)
putHeaderGuard(fileName, fp, True)

