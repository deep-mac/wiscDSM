import sys
import numpy as np
import random

startAddr = "0x40000000"

def putInclude(lib):
    str_ = "#include<" + lib + ">"
    print(str_)

def putMain():
    print("int main()\n{")

def putRet():
    print("\treturn 0;\n}")

def putVars(totalVars):
    for i in range (totalVars):
        str_ = "\tint *var" + str(i) + ";"
        print(str_)
        str_ = "\tvar = (int*)"+startAddr + " + (int)" + str(i) + ";"
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

putInclude("stdio")
putInclude("stdlib")
putMain()
putVars(totalVars)
for i in range(totalOps):
    rand_ = np.random.uniform(0, 1)
    var = random.randint(0, totalVars-1)
    num = random.randint(0, 99)
    if(rand_ < ratio):
        putWrite(var, num)
    else:
        putRead(var)
putRet()
