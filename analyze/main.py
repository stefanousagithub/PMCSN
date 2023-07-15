import ctypes
from ctypes import *
import os
import matplotlib.pyplot as plt
import numpy as np

lib_name = '/demo.so'
res_lib = CDLL(os.getcwd() + lib_name)

class IOresults(Structure):
    _fields_ = [('respCenter1', c_double),
                ('respCenter2', c_double),
                ('respCenter3', c_double),
                ('respCenter4', c_double),
                ('respCenter5', c_double),
                ('respCenter6', c_double),
                ('utilCenter1', c_double),
                ('utilCenter2', c_double),
                ('utilCenter3', c_double),
                ('utilCenter4', c_double),
                ('utilCenter5', c_double),
                ('utilCenter6', c_double),
                ('respJobPr1', c_double),
                ('respJobPr2', c_double),
                ('respJobPr3', c_double),
                ('respJobPr4', c_double)]
def console():
    print("-------------------------------------------- ");
    print("----------------- Analizer ----------------- ");
    print("-------------------------------------------- ");
    print("");
    print("Choose an operation:")
    print("1 - Verification")
    print("2 - Validation")
    print("3 - Steady-state analysis")
    print("4 - Transient analysis")
    command = int(input())
    print()
    type = 0
    if command == 1:
        print("Please insert the server you want to analize")
        type = int(input("1 to 6: "))
        print()
        if not(type == 1 or type == 2 or type == 3 or type == 4 or type == 5 or type == 6):
            print("Wrong option")
            return -1, -1
    elif command == 2:
        print("Modify the system: Choose an option:")
        print("1 - Increase Lamda")
        print("2 - decrease service")
        print("3 - Delete feedbacks")
        print("4 - Increase number of servers:")
        type = int(input())
        print()

        if not(type == 1 or type == 2 or type == 3 or type == 4):
            print("Wrong option")
            return -1, -1
    elif command == 3:
        print("What chart do you need:")
        print("1 - Total response time jobs")
        print("2 - Response time centers")
        print("3 - IUtilization centers")
        type = int(input())
        print()

        if not(type == 1 or type == 2 or type == 3):
            print("Wrong option")
            return -1, -1

    elif command == 4:
        type = 0
    else:
        print("Wrong option")
        return -1, -1
    return command, type


command, type = -1, -1
while command == -1:
    command, type = console()

dev_n, test_n, prod_n = 1, 2, 2
label = [["Lambda = 4", "Lambda = 4.1"], ["mu = 5,4,8,6,2,12", "mu = 6,5,9,7,3,13"], ["feedback", "without feedback"], ["servers: 1,2,2", "servers:2,3,3"]]

res_lib.analyse.restype = c_void_p
f = res_lib.finiteHorizon
f.restype = ctypes.POINTER(ctypes.c_double * 16)
b = 512
k = 1024


if command == 1:
    IOresults.from_address(res_lib.verification(b, k, type-1))

if command == 2:
    # Exec simulations
    res = IOresults.from_address(res_lib.analyse(0))
    res_modif = IOresults.from_address(res_lib.analyse(type))

    i = 0
    #print rensponse time and utilization
    while i < 2:
        names = ['Development', 'Test', 'Production', 'Delay Prod', 'Rollback', 'Fast Dev']
        if i == 0:
            val1 = (res.respCenter1, res.respCenter2, res.respCenter3, res.respCenter4, res.respCenter5, res.respCenter6)
            val2 = (res_modif.respCenter1, res_modif.respCenter2, res_modif.respCenter3, res_modif.respCenter4, res_modif.respCenter5, res_modif.respCenter6)
        else:
            val1 = (res.utilCenter1 / dev_n, res.utilCenter2 / test_n, res.utilCenter3 / prod_n, res.utilCenter4, res.utilCenter5, res.utilCenter6)
            if type == 4:
                dev_n += 1
                test_n += 1
                prod_n += 1
            val2 = (res_modif.utilCenter1 / dev_n, res_modif.utilCenter2 / test_n, res_modif.utilCenter3 / prod_n, res_modif.utilCenter4, res_modif.utilCenter5, res_modif.utilCenter6)
        fig = plt.figure(figsize=(6, 5), dpi=150)
        left, bottom, width, height = 0.1, 0.3, 0.8, 0.6
        ax = fig.add_axes([left, bottom, width, height])

        width = 0.35
        ticks = np.arange(len(names))
        rects1 = ax.bar(ticks - width/2, val1, width, label=label[type-1][0])
        rects2 = ax.bar(ticks + width/2, val2, width, align="center",
               label=label[type-1][1])
        ax.bar_label(rects1, padding=3, rotation=90)
        ax.bar_label(rects2, padding=3, rotation=90)
        plt.xticks(rotation=10)
        if i == 0:
            ax.set_ylabel('E[Ts]')
            ax.set_title('Response time centers')
        else:
            ax.set_ylabel('Utiliz.')
            ax.set_title('Utilization centers')
        ax.set_xticks(ticks + width / 2)
        ax.set_xticklabels(names)

        ax.legend(loc='best')
        i += 1
    plt.show()
    res_lib.clearAnalyse(byref(res))
    res_lib.clearAnalyse(byref(res_modif))

    del res
    del res_modif

if command == 3:
    res = IOresults.from_address(res_lib.analyse(0))
    # print jobs for priotity rensponse time and centers response and utilization
    if type == 1:
        names = ('pr1', 'pr2', 'pr3', 'pr4')
        values = (res.respJobPr1, res.respJobPr2, res.respJobPr3, res.respJobPr4)
    if type == 2:
        names = ['Development', 'Test', 'Production', 'Delay Prod', 'Rollback', 'Fast Dev']
        values = (res.respCenter1, res.respCenter2, res.respCenter3, res.respCenter4, res.respCenter5, res.respCenter6)
    if type == 3:
        names = ['Development', 'Test', 'Production', 'Delay Prod', 'Rollback', 'Fast Dev']
        values = (res.utilCenter1 / 1, res.utilCenter2 / 2, res.utilCenter3 / 2, res.utilCenter4, res.utilCenter5, res.utilCenter6)

    fig, ax = plt.subplots()
    bars = ax.bar(x=np.arange(len(values)),  # The x coordinates of the bars.
           height=values,  # the height(s) of the vars
           color="green",
           align="center",
           tick_label=names)
    ax.bar_label(bars)

    if type == 1:
        ax.set_ylabel('E[Ts] in hours')
        ax.set_title('Total response time jobs')

    if type == 2:
        ax.set_ylabel('E[Ts]')
        ax.set_title('Response time centers')
    if type == 3:
        ax.set_ylabel('Utiliz.')
        ax.set_title('Utilization centers')
    plt.show()

    # free space
    res_lib.clearAnalyse(byref(res))
    del res

if command == 4:
    y1 = [i for i in f(123456789).contents]
    y2 = [i for i in f(987654321).contents]
    y3 = [i for i in f(12345).contents]

    x = [8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096, 8192, 16384, 32768, 65536, 131072, 262144]

    plt.plot(x, y1, label="seed = 123456789")
    plt.plot(x, y2, label="seed = 987654321")
    plt.plot(x, y3, label="seed = 12345")

    # naming the x axis
    plt.xlabel('Close the door')
    # naming the y axis
    plt.ylabel('E[Ts]')
    # giving a title to my graph
    plt.title('Transient analysis')
    plt.xscale('log',base=2) 
    # show a legend on the plot
    plt.legend()

    # function to show the plot
    plt.show()


