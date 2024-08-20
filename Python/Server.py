# server.py created by S3RG / Sergalicious - 2024
import asyncio
import websockets
import threading
import requests
import orjson
import random
import time, sys
import struct
from PIL import Image, ImageDraw

import socket    
local_ip = socket.gethostbyname(socket.gethostname())

# size is limited to 1024*15 by default! see github readme for instructions to increase this limit!
# a higher max_frame_size will result in much faster data transfers
max_frame_size = 1024*15

sessions = {}
counter = 0

async def next():
    global counter
    counter += 1
    return counter

async def upload_image(ws, bins, width, height):
    for b in bins:
        await ws.send(b)

    imw1 = width
    imw2 = 0
    imw3 = 0

    if imw1 > 255:
        imw2 = imw1 - 255
        imw1 = 255
	
    if imw2 > 255:
        imw3 = imw2 - 255
        imw2 = 255

    bs = b'\x1F\x8B' + bytes([height, imw1, imw2, imw3])
    await ws.send(bs)

async def parse_image(ws, type, im):
    
    max_width = 536
    max_height = 240

    if type == "ESP":
        max_width = 320
        max_height = 170

    if im.height > im.width:
        im = im.transpose(Image.ROTATE_270)
	
    im.thumbnail((max_width, max_height), Image.Resampling.LANCZOS)
    
    if im.width < max_width and im.height < max_height:
        base = Image.new('RGB', (max_width, max_height), 0)

        base.paste(im, ((max_width - im.width) // 2, (max_height - im.height) // 2))

        im = base
    
    if im.width < max_width:
        base = Image.new('RGB', (max_width, im.height), 0)

        base.paste(im, ((max_width - im.width) // 2, 0))

        im = base

    if im.height < max_height:  
        base = Image.new('RGB', (im.width, max_height), 0)

        base.paste(im, (0, (max_height - im.height) // 2))

        im = base


    pixels = list(im.getdata())

    print(f"Num Pixels: {len(pixels)}")

    bin = bytearray()

    for pix in pixels:
        r = (pix[0] >> 3) & 0x1F
        g = (pix[1] >> 2) & 0x3F
        b = (pix[2] >> 3) & 0x1F
        bin.extend(struct.pack('H', (r << 11) + (g << 5) + b))

    def divide_chunks(l, n): 
        for i in range(0, len(l), n):  
            yield l[i:i + n] 

    bins = list(divide_chunks(bin, max_frame_size))

    total_size = len(bin)

    print("Image size:" + str(im.size), "Chunks: " + str(len(bins)), "Size: " + str(total_size), [len(b) for b in bins])

    await upload_image(ws, bins, im.width, im.height)

async def image_from_file(ws, type, filename):
    try:
        im = Image.open(f"./{filename}.png", "r")
        im = im.convert("RGB")
    except Exception as e:
        await ws.send("Image failed to load: " + str(e))
        return

    await parse_image(ws, type, im)

async def image_from_url(ws, type, the_url):
    response = requests.get(the_url, stream=True)
    im = Image.open(response.raw)   
    im = im.convert("RGB")

    await parse_image(ws, type, im)

async def create_session(ws, path):
    session_id = await next()
    sessions.update({session_id:{"ws":ws, "type":"unknown"}})
    await run(session_id, ws, path)

async def run(session_id, ws, path):
    while True:

        try:
            message = await ws.recv()
        except (websockets.exceptions.ConnectionClosed, websockets.exceptions.ConnectionClosedError):
            for session in sessions.copy():
                if sessions[session]["ws"].closed:
                    print(f"Client Disconnected: {sessions[session]['type']} {session} ")
                    del sessions[session]
            return

        session_type = sessions[session_id]["type"]

        if session_type == "unknown":
            
            if message in ["ESP", "ESPA", "MAIN"]:
                sessions[session_id]["type"] = message
                await ws.send(f"Connected as {message} {session_id} to 192.168.1.115")
                print(f"New Client Connected: {message} {session_id}")
                continue

            else:
                print("Unknown client conected: " + message)
                del sessions[session_id]
                await ws.close()
                return

        match session_type:
            case "ESP" | "ESPA":
                await ws.send(message)

            case "MAIN":
                args = message.split(" ")

                match args[0].lower():
                    case "image":
                        if len(args) != 2:
                            await ws.send("Usage: image <filename>")
                            continue

                        for session in sessions:
                            other_session_type = sessions[session]["type"]
                            if other_session_type in ["ESP", "ESPA"]:
                                await image_from_file(sessions[session]['ws'], other_session_type, args[1])
                    
                    case "url":
                        if len(args) != 2:
                            await ws.send("Usage: url <fileurl>")
                            continue
                        
                        for session in sessions:
                            other_session_type = sessions[session]["type"]
                            if other_session_type in ["ESP", "ESPA"]:
                                await image_from_url(sessions[session]['ws'], other_session_type, args[1])
                    
                    case "list":
                        if len(args) != 2:
                            await ws.send("Usage: list sessions")
                            continue

                        if args[1] == "sessions":
                            for session in sessions:
                                await ws.send(f"{str(session)}: {str(sessions[session])}")
                    
                    case _:
                        for session in sessions:
                            if session != session_id:
                                await sessions[session]["ws"].send(message)
                    

loop = asyncio.new_event_loop()
asyncio.set_event_loop(loop)

print(f"Starting Server on {local_ip}")

start_server = websockets.serve(create_session, host=local_ip, port=8765, ssl=None)
loop.run_until_complete(start_server)
loop.run_forever()
