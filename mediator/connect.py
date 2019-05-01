import socket 
import sys 

print(sys.argv)

local_ip = socket.gethostbyname(socket.gethostname())
local_port = int(sys.argv[1])
print(local_ip,local_port)

s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

s.bind(('0.0.0.0', local_port))
s.connect(("157.230.29.114", 4123))
s.sendall(bytearray("connect {} {} {} {}".format(local_ip, local_port, sys.argv[2], sys.argv[3]), "ascii"))
data = s.recv(1024)
print(data.decode("ascii"))
with open("connect_resp.txt", "w") as f:
    f.write(data.decode("ascii"))
s.close()
