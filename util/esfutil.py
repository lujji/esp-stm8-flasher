import websocket, thread, time, argparse, os

BLOCK_SIZE = 512
ACK = '\xBB\xAA'
FILE = ''
ADDR = ''

CMD_UPLOAD_START = 0x0A
CMD_UPLOAD_DATA  = 0x0B
CMD_LIST_FILES   = 0x49
CMD_FLASH        = 0x46
CMD_DELETE       = 0x44

def fw_upload(file):
    print("Opening socket..")
    ws = websocket.create_connection(ADDR)
    print("Sending upload request")
    size = os.path.getsize(file)
    filename = os.path.basename(file)
    req = '%c%s;%s;' % (CMD_UPLOAD_START, filename, size)
    ws.send(req)
    ack = ws.recv()
    if ack != ACK:
        print('Server failed to ACK')
        exit()
    data = open(file, 'rb')
    total = 0
    with data as f:
        chunk = bytearray(f.read(BLOCK_SIZE))
        while chunk:
            ws.send(chr(CMD_UPLOAD_DATA) + chunk)
            #print ' '.join(hex(i) for i in chunk)
            ack = ws.recv()
            if ack != ACK:
                print('Server failed to ACK')
                exit()
            total += len(chunk)
            chunk = bytearray(f.read(BLOCK_SIZE))
    print('Sent %d bytes' % total)
    ws.close()
    print('Done')

def fw_flash(file):
    file = os.path.basename(file)
    print('Flashing firmware %s..' % file)
    ws = websocket.create_connection(ADDR)
    ws.send(chr(CMD_FLASH) + file)
    ws.settimeout(6.0)
    ack = ws.recv()
    if ack != ACK:
        print('Failed to flash firmware')
        exit()
    print('Done')

def fw_delete(file):
    file = os.path.basename(file)
    print('Deleting file %s..' % file)
    ws = websocket.create_connection(ADDR)
    ws.send(chr(CMD_DELETE) + file)
    ack = ws.recv()
    if ack != ACK:
        print('Failed to delete file')
        exit()
    print('Done')

def fw_program(file):
    fw_upload(file)
    fw_flash(file)
    fw_delete(file)

def fw_list():
    print('Requesting file list..')
    ws = websocket.create_connection(ADDR)
    ws.send(chr(CMD_LIST_FILES))
    while True:
        f = ws.recv()
        if not f or f == ACK: break
        f = f[:-1].split(':')
        print('* %s: %s bytes' % (f[0], f[1]))

if __name__ == "__main__":
    #websocket.enableTrace(True)
    parser = argparse.ArgumentParser(description='esp-stm8-flasher utility',
        epilog='example:\n  python esfutil.py -p firmware.bin 192.168.100.4\n\r',
        formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('-p', '--program', metavar='', help='program firmware (upload, flash and delete)')
    parser.add_argument('-u', '--upload-only', metavar='', help='upload a file (without flashing)')
    parser.add_argument('-f', '--flash-only', metavar='', help='flash remote file')
    parser.add_argument('-d', '--delete', metavar='', help='delete remote file')
    parser.add_argument('-l ', '--list', action='store_true', help='get file list')
    parser.add_argument('addr', type=str, help='destination IP address')
    args = parser.parse_args()
    ADDR = 'ws://%s/' % (args.addr)
    if args.program: fw_program(args.program)
    elif args.upload_only: fw_upload(args.upload_only)
    elif args.flash_only: fw_flash(args.flash_only)
    elif args.delete: fw_delete(args.delete)
    elif args.list: fw_list()
    else: print('Not enough arguments')
