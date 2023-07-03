'''Probar algo mas grnade que el buffer del fd de salida'''

import socket # for socket
import sys
import time
import argparse
from difflib import SequenceMatcher

parser = argparse.ArgumentParser()

parser.add_argument('-f', action='store',
                    dest='filename',
                    help='File name to send')

parser.add_argument('-c', action='store',
                    dest='amount',
                    default= 1,
                    help='amount of repetitions')

parser.add_argument('-ip', action='store',
                    dest='ip',
                    default='10.13.40.149',
                    help='ip address')


parser.add_argument('--version', action='version',
                    version='%(prog)s 1.0')

results = parser.parse_args()
print('simple_value     = {!r}'.format(results.filename))
print('amount   = {!r}'.format(results.amount))
print('ip               = {!r}'.format(results.ip))


try:
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    print ("Socket successfully created")
except socket.error as err:
    print ("socket creation failed with error %s" %(err))
 
# default port for socket
port = 3490
# connecting to the server
#s.connect(("127.0.0.1", port))
#s.connect(("10.13.40.149", port))
if results.filename == None:
    s.connect((results.ip, port))
    message = '{123456789012312345678901231234567890123><2345678901231234567890123}'+str(time.time())
    try:
        # Send data
        print ('sending "%s"', message)
        s.send(message.encode())
        # Look for the response
        amount_received = 0
        amount_expected = len(message)
        data=""
        while amount_received < amount_expected:
            data += s.recv(50).decode("utf-8") 
            amount_received += len(data)
        print ( 'received "%s"', data)

    finally:
        print ('closing socket') 
        s.close()
        assert(data==message)

else:
    for x in range(int(results.amount)): 
        s.connect((results.ip, port))
        f = open(results.filename,'rb')
        input=""
        print ("Reading... ")
        l = f.read(1024)
        input = input +str(l)
        while (l):
            #print ("Sending...")
            s.send(l)
            l = f.read(1024)
            input = input +str(l)
        f.close()
        input = input.replace("\'b\'", "")
        input = input.replace("\'", "")
        #print (input)
        print ("Done Sending")
        datastring=""
        while True:
            #print ("Receiving...")
            data = s.recv(1024)
            if not data:
                break
            datastring += data.decode('utf-8')
        #assert("/usr/bin/lua" in datastring
        s.close()
        print("-sent-")
        print (input.split('\\n')[1])
        print("-received-")
        print (datastring.split('\\n')[0])
        print("-ratio -",  SequenceMatcher(a=input.split('\\n')[1],b=datastring.split('\\n')[0]).ratio())
        assert (SequenceMatcher(a=input.split('\\n')[1],b=datastring.split('\\n')[0]).ratio()>.9) 
        print ('closing socket') 
 
print ("the socket has successfully connected to epollCoro")
