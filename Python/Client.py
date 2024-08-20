import websocket
import traceback
import threading
import time

user_id = 'MAIN'

#Change this to the local ip displayed in server.py
local_ip = "192.168.1.115"

def connect():
	return websocket.create_connection(f"ws://{local_ip}:8765")

def loop():
	global ws

	ws = connect()
	connected = False

	while 1:
		if connected == False:
			ws.send(user_id)
			connected = True
			
		r = ws.recv()
		print(r)

def input_thread():
	while 1:
		text = input()

		if ws:
			ws.send(text)

t = threading.Thread(target = input_thread)
t.start()

while True:
	try:
		loop()
	except:
		ws = None
		traceback.print_exc()
		time.sleep(1)
