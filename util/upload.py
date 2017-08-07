import websocket
import thread
import time
import os

BLOCK_SIZE = 512
FILE = './fs/firmware.bin'
ADDR = 'ws://192.168.100.4/'
#FILE = './fs/index.html~'
#FILE = './fs/mega.ch8'
ACK = (0xAA, 0xBB)

def on_error(ws, error):
    print(error)

def fw_update():
    print("Opening socket..")
    ws = websocket.create_connection(ADDR)

    print("Sending update request")
    size = os.path.getsize(FILE)
    filename = os.path.basename(FILE)
    strx = chr(0x0A) + filename + ';' + str(size) + ';'
    ws.send(strx)
    ack = ws.recv()
    if ack != bytearray(ACK):
        print('Server failed to ACK')
        exit

    data = open(FILE, 'rb')
    total = 0
    with data as f:
        chunk = bytearray(f.read(BLOCK_SIZE))
        while chunk:
            ws.send(chr(0x0B) + chunk)
            print ' '.join(hex(i) for i in chunk)
            ack = ws.recv()
            if ack != bytearray(ACK):
                print('Server failed to ACK')
                return
            total += len(chunk)
            print(total)
            chunk = bytearray(f.read(BLOCK_SIZE))

    ws.close()
    print("Done!")

if __name__ == "__main__":
    #websocket.enableTrace(True)
    fw_update()
