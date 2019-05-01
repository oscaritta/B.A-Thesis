import sys 
import re 
import uuid 
import struct 
import socket
import time
import ctypes
from threading import Thread, current_thread 
from flask import Flask, request, jsonify

def num_to_bytearray(n):
	return bytearray(ctypes.c_uint32(n))
	
def bytearray_to_num(b):
	return struct.unpack('i',b)[0]
	
def PRINT(o):
	print("{}".format(o))
	pass 
	
def recvall(socket, size):
	bytearr = b""
	while len(bytearr) < size:
		bytearr += socket.recv(size-len(bytearr))
	return bytearr 

def listen_udp():
	s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
	s.bind(('0.0.0.0', MediaProxy.PORT))
	while True:
		data, client_addr = s.recvfrom(100);
		Thread(target = MediaProxy.handle_udp, args=(s, data, client_addr)).start()

def http_server():
	app = Flask(__name__)
	
	@app.route('/debug_messages', methods=['GET'])
	def debug_messages():
		return str(MediaProxy.chat_messages), 200

	@app.route('/send_chat_messages', methods=['POST'])
	def send_chat_messages():
		content = request.json
		if content is None:
			return jsonify({'error':'json object not found in your request'}), 400

		type = content.get('type')
		id = content.get('id')
		passw = content.get('passw')
		message = content.get('message')

		if type is None:
			return jsonify({'error':'type is None'}), 400
		if id is None:
			return jsonify({'error':'id is None'}), 400
		if passw is None:
			return jsonify({'error':'password is None'}), 400
		if message is None:
			return jsonify({'error':'message is None'}), 400

		if not MediaProxy.id_exits(id):
			return jsonify({'error':'id does not exit'}), 400
		if passw != MediaProxy.password_of(id):
			return jsonify({'error':'incorrect password'}), 400

		if type == 'server':
			if (id,passw) not in MediaProxy.chat_messages:
				MediaProxy.chat_messages[(id,passw)] = {'client':[], 'server':[]}
			MediaProxy.chat_messages[(id,passw)]['server'] += [message]
			print(MediaProxy.chat_messages[(id,passw)]['server'])
			print(MediaProxy.chat_messages)

		elif type == 'client':
			if (id,passw) not in MediaProxy.chat_messages:
				MediaProxy.chat_messages[(id,passw)] = {'client':[], 'server':[]}
			MediaProxy.chat_messages[(id,passw)]['client'] += [message]
			print(MediaProxy.chat_messages[(id,passw)]['client'])
			print(MediaProxy.chat_messages)
		else:
			return jsonify({'error':'unknwon type'}), 400

		return jsonify({'info':'message sent'}), 200

	@app.route('/get_chat_messages', methods=['POST'])
	def get_chat_messages():
		content = request.json
		if content is None:
			return jsonify({'error':'json object not found in your request'}), 400

		type = content.get('type')
		id = content.get('id')
		passw = content.get('passw')

		if type is None:
			return jsonify({'error':'type is None'}), 400
		if id is None:
			return jsonify({'error':'id is None'}), 400
		if passw is None:
			return jsonify({'error':'password is None'}), 400

		if not MediaProxy.id_exits(id):
			return jsonify({'error':'id does not exit'}), 400
		if passw != MediaProxy.password_of(id):
			return jsonify({'error':'incorrect password'}), 400

		if type == 'server':
			if (id,passw) not in MediaProxy.chat_messages:
				print("{} not in {}".format((id,passw), MediaProxy.chat_messages))
				MediaProxy.chat_messages[(id, passw)] = {'client':[],'server':[]}

			messages = {'messages':MediaProxy.chat_messages[(id, passw)]['client']}
			MediaProxy.chat_messages[(id, passw)]['client'] = []
			return jsonify(messages), 200
		elif type == 'client':
			if (id,passw) not in MediaProxy.chat_messages:
				print("{} not in {}".format((id,passw), MediaProxy.chat_messages))
				MediaProxy.chat_messages[(id, passw)] = {'client':[],'server':[]}
			
			messages = {'messages':MediaProxy.chat_messages[(id, passw)]['server']}
			MediaProxy.chat_messages[(id, passw)]['server'] = []
			return jsonify(messages), 200
		else:
			return jsonify({'error':'unknwon type'}), 400

		return jsonify({'messages':[]}), 200

	app.run('0.0.0.0', 80, threaded=True)

class MediaProxy:
	'''
	MediaProxy class is responsible for running a proxy that forwards data between two terminals using logins.
	Things it does:
		-registers media senders with username and password, then stores the session in a database
		-stores the media receiver in a database
		-just plain forwards data between the sender and the receiver
	'''
	HOST, PORT = "0.0.0.0", 4123	
	registered = []
	connections = []
	udp_pairs = {}
	chat_messages = {}

	def id_exits(id):
		for i, server in enumerate(MediaProxy.registered):
			if id == server[0]:
				return True
		return False 

	def password_of(id):
		for i, server in enumerate(MediaProxy.registered):
			if id == server[0]:
				return server[1]
		return None 

	def handle_udp(s, data, addr):
		#PRINT("received {} from {}".format(data, addr))
		#PRINT("all connections: {}".format(MediaProxy.connections))
		PRINT("udp_pairs ---->{}".format(MediaProxy.udp_pairs))
		data = data.decode("ascii")
		print(data)

		if data.find("register_udp_server") == 0:
			if len(data.split(" ")) < 4:
				s.sendto(b"not enough arguments", addr)
				return

			local_udp_port, id, passw = data.split(" ")[1:]
			remote_udp_port = addr[1]

			for i, server in enumerate(MediaProxy.registered):
				if id == server[0]:
					if passw == server[1]:
						s.sendto(b"ok", addr)
						MediaProxy.udp_pairs[(id,passw)] = MediaProxy.udp_pairs.get((id,passw), {})
						MediaProxy.udp_pairs[(id,passw)]['server_udp'] = (local_udp_port,remote_udp_port)
						return
					else:
						s.sendto(b"incorrect password", addr)
						return

			s.sendto(b"id not found", addr)
			return

		if data.find("register_udp_client") == 0:
			if len(data.split(" ")) < 4:
				s.sendto(b"not enough arguments", addr)
				return

			local_udp_port, id, passw = data.split(" ")[1:]
			remote_udp_port = addr[1]

			for i, server in enumerate(MediaProxy.registered):
				if id == server[0]:
					if passw == server[1]:
						s.sendto(b"ok", addr)
						#s.sendto(bytearray(str(addr),'ascii'), addr)
						MediaProxy.udp_pairs[(id,passw)] = MediaProxy.udp_pairs.get((id,passw), {})
						MediaProxy.udp_pairs[(id,passw)]['client_udp'] = (local_udp_port,remote_udp_port)
						return 
					else:
						s.sendto(b"incorrect password", addr)
						return

			s.sendto(b"id not found", addr)
			return

		if data.find("get_client_udp_port") == 0:
			if len(data.split(" ")) < 3:
				s.sendto(b"not enough arguments", addr)
				return

			id, passw = data.split(" ")[1:]

			for i, server in enumerate(MediaProxy.registered):
				#print("checking {} ".format(server[0]))
				if id == server[0]:
					if passw == server[1]:
						if MediaProxy.udp_pairs.get((id,passw), None) == None:
							s.sendto(b"udp not ready", addr)
							return
						else:
							if MediaProxy.udp_pairs[(id,passw)].get('client_udp', None) == None:
								s.sendto(b"udp not ready", addr)
								return 

							else:
								local, remote = MediaProxy.udp_pairs[(id,passw)]['client_udp']
								s.sendto(bytearray("ports " + str(local)+" "+str(remote), "ascii"), addr)
								print(bytearray("ports " + str(local)+" "+str(remote), "ascii"))
								return 

					else:
						s.sendto(b"incorrect password", addr)
						return 

			s.sendto(b"id not found", addr)
			return

		if data.find("get_server_udp_port") == 0:
			if len(data.split(" ")) < 3:
				s.sendto(b"not enough arguments", addr)
				return

			id, passw = data.split(" ")[1:]

			for i, server in enumerate(MediaProxy.registered):
				#print("checking {} ".format(server[0]))
				if id == server[0]:
					#print("id's are matching")
					if passw == server[1]:
						#print("id's are REALLY matching")
						if MediaProxy.udp_pairs.get((id,passw), None) == None:
							#print("(1) udp_not_ready")
							s.sendto(b"udp not ready", addr)
							return
						else:
							if MediaProxy.udp_pairs[(id,passw)].get('server_udp', None) == None:
								#print("(2) udp_not_ready")
								s.sendto(b"udp not ready", addr)
								return
							else:
								local, remote = MediaProxy.udp_pairs[(id,passw)]['server_udp']
                                #print(bytearray(str(local)+";"+str(remote), "ascii"))
								s.sendto(bytearray("ports " + str(local)+" "+str(remote), "ascii"), addr)
								print(bytearray("ports " + str(local)+" "+str(remote), "ascii"))
								return
					else:
						s.sendto(b"incorrect password", addr)
						return 

			s.sendto(b"id not found", addr)
			return 

		s.sendto(bytearray("unknwon command","ascii"), addr)

	def handle(request):
		request, addr = request
		PRINT("socket connected")	
		#Managing the connections until the proxy is initialized
		while True:
			PRINT("------------------------------------------------------------")
			PRINT("socket is {}".format(request))
			PRINT("registered is {}".format(MediaProxy.registered))
			PRINT("connections are {}".format(MediaProxy.connections))

			#Proxy not initialized by now
			#size = bytearray_to_num(recvall(request,4))
			#PRINT("We need to read " + str(size) + " bytes from " + str(request))
			data = request.recv(128)
			if data == b'':
				return 
			#except Exception as e:
			#	print(e)
			#	return

			PRINT ("{} wrote: {}".format(addr, data))
			to_send = None
						
			if data == b"bye":
				break 

			elif data == b'iamready':
				for connection in MediaProxy.connections:
					if addr[0] in connection['addrs']:
						connection['ready'] = True
				to_send = b'ok'

			elif data == b'isready':
				found = False
				PRINT('inside isready with {}'.format(MediaProxy.connections))
				
				for connection in MediaProxy.connections:
					PRINT("{} in {} ? ".format(addr[0], connection['addrs']))
					if addr[0] in connection['addrs']:
						found = True
						if connection['ready']:
							to_send = b'yes'
						else:
							to_send = b'no'
				if not found:
					to_send = b'unconnected'

			elif data.split(b" ")[0] == b"register":
				data = data.decode("ascii")
				user = uuid.uuid4().hex[0:8]
				password = uuid.uuid4().hex[0:8]
				to_send = bytearray("credentials " + user+";"+password,"ascii")
				server_local_ip, server_local_port = None,None 
				
				try:
					server_local_ip, server_local_port = data.split(" ")[1:]
				except Exception as e:
					print(e)
					request.sendall(bytearray(str(e),"ascii"))

				found = False
				for usr in MediaProxy.registered:
					if usr[2] == request:
						to_send = b"already registered"
						found = True
				if not found:
					MediaProxy.registered += [(user,password,request,addr,(server_local_ip,int(server_local_port)))]
				
			elif b"connect" in data:
				command, client_local_ip, client_local_port, user, password = None,None,None,None,None
				try:
					command, client_local_ip, client_local_port, user, password = data.decode("ascii").split(" ")
				except Exception as e:
					print(e)
					request.sendall(bytearray(str(e),"ascii"))

				proxy_session_initialized = False
				to_send = b"sender not found"
				found = False 
				for usr in MediaProxy.registered:
					if usr[0] == user:
						found = True
						if usr[1] == password:
							PRINT("user + password match")
							to_send = b"connection established"
							
							msg = bytearray("request {};{};{};{}".format(client_local_ip, client_local_port, addr[0],addr[1]),"ascii")
							#usr[2].sendall(num_to_bytearray(len(msg)))
							usr[2].sendall(msg)
							
							msg = bytearray("address {};{};{};{}".format(usr[4][0], usr[4][1], usr[3][0], usr[3][1]),"ascii")
							#request.sendall(num_to_bytearray(len(msg)))
							request.sendall(msg)
							
							time.sleep(1)
							proxy_session_initialized = True

							MediaProxy.connections += [{'addrs':(addr[0], usr[3][0]), 'udpports' : None, 'ready':False}]
							
							request.close()
							usr[2].close()
						else:
							PRINT("incorrect password")
							to_send = b"incorrect password"
							break
							
				if proxy_session_initialized:
					continue
					
			else:
				to_send = data.upper()
			
			#request.sendall(num_to_bytearray(len(to_send)))
			request.sendall(to_send)
			
				
	def __init__(self):
		PRINT("Running MediaProxy with on port " + str(MediaProxy.PORT))
		self._server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
		self._server_socket.bind((MediaProxy.HOST, MediaProxy.PORT))
		self._server_socket.listen(1000)
		
	def run(self):
		Thread(target=listen_udp, args=()).start()
		Thread(target=http_server, args=()).start()
		while True:
			PRINT("Waiting for a socket")
			client = self._server_socket.accept()
			Thread(target = MediaProxy.handle, args=(client,)).start()
			
mp = MediaProxy()
mp.run()
