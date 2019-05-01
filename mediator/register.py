import socket 
import sys 
import random 

local_ip = socket.gethostbyname(socket.gethostname())
local_port = int(sys.argv[1])
print(local_ip,local_port)

s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

s.bind(('0.0.0.0', local_port))
s.connect(("157.230.29.114", 4123))
s.sendall(bytearray("register {} {}".format(local_ip,local_port), "ascii"))
data = s.recv(1024)
print(data.decode("ascii"))
with open("mediator.txt", "w") as f:
    f.write(data.decode("ascii"))
data = s.recv(1024)
print(data.decode("ascii"))
with open("mediator.txt", "w") as f:
    f.write(data.decode("ascii"))
s.close()