'''Probar algo mas grnade que el buffer del fd de salida'''

import socket # for socket
import sys
import time
import argparse

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
    message = '{123456789012312345678901231234567890123><2345678901231234567890123}'
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

            time. sleep(0.4)
            amount_received += len(data)
        print ( 'received "%s"', data)

    finally:
        #assert(data=="message")
        print ('closing socket') 
        s.close()
else:
    for x in range(results.amount): 
        s.connect((results.ip, port))
        f = open(results.filename,'rb')
        print ("Reading... for the ",x, " time")
        l = f.read(1024)
        print (l)
        while (l):
            print ("Sending...")
            print (l)
            s.send(l)
            l = f.read(1024)
        f.close()
        print ("Done Sending")
        datastring=""
        while True:
            print ("Receiving...")
            data = s.recv(1024)
            if not data:
                break
            datastring += data.decode('utf-8')
        print(datastring)
        assert("/usr/bin/lua" in datastring)
        assert("7777" in datastring)

        print ('closing socket') 
        s.close()
 
print ("the socket has successfully connected to epollCoro")
