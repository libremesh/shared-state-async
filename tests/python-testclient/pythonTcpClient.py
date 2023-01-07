'''Probar algo mas grnade que el buffer del fd de salida'''

import socket # for socket
import sys
import time

try:
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    print ("Socket successfully created")
except socket.error as err:
    print ("socket creation failed with error %s" %(err))
 
# default port for socket
port = 3490
# connecting to the server
s.connect(("127.0.0.1", port))
try:
    
    # Send data
    message = '{123456789012312345678901231234567890123><2345678901231234567890123}'
    print ('sending "%s"', message)
    s.send(message.encode())

    # Look for the response
    amount_received = 0
    amount_expected = len(message)
    
    while amount_received < amount_expected:
        data = s.recv(50)
        time. sleep(0.4)
        amount_received += len(data)
        print ( 'received "%s"', data)

finally:
    print ('closing socket') 
    s.close()

 
print ("the socket has successfully connected to epollCoro")
