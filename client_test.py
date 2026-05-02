import socket
import binascii
import time
import random

def bcd_to_bytes(s):
    return bytes.fromhex(s)

def str_to_hex(s):
    return s.encode('ascii').hex()

def create_msg(mti, pan, stan):
    # Bitmap is 8 bytes. bits: 2,3,4,7, 11, 41, 42, 49
    bitmap = "7220000000C08000"
    
    payload = ""
    payload += mti
    payload += bitmap
    
    # F2: LLVAR (len 16) + PAN
    payload += str(len(pan)).zfill(2) + pan
    # F3: 6 BCD digits (Processing code)
    payload += "000000"
    # F4: 12 BCD digits (Amount) - 10.00
    payload += "000000001000"
    # F7: 10 BCD digits (DateTime)
    payload += "1122143000"
    # F11: 6 BCD digits (STAN)
    payload += str(stan).zfill(6)
    # F41: 8 ASCII chars -> hex
    payload += str_to_hex("TERM0001")
    # F42: 15 ASCII chars -> hex
    payload += str_to_hex("MERCH0000000001")
    # F49: 3 ASCII chars -> hex
    payload += str_to_hex("840")
    
    msg_bytes = bcd_to_bytes(payload)
    length = len(msg_bytes)
    header = length.to_bytes(2, byteorder='big')
    
    return header + msg_bytes

def send_msg(s, msg):
    s.sendall(msg)
    resp_header = s.recv(2)
    if len(resp_header) == 2:
        resp_len = int.from_bytes(resp_header, byteorder='big')
        resp_body = s.recv(resp_len)
        print(f"Resp Hex: {resp_body.hex()}")
    else:
        print("Invalid response header")

def main():
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.connect(('127.0.0.1', 8889)) # Alterado para 8889
    
    print("Sending standard auth 0100...")
    msg1 = create_msg("0100", "1234567890123456", 123456)
    send_msg(s, msg1)
    
    print("\nSending duplicate msg (should be ignored/cached)...")
    send_msg(s, msg1)

    print("\nSending reversal 0400...")
    msg2 = create_msg("0400", "1234567890123456", 123456)
    send_msg(s, msg2)
    
    print("\nSending multiple rapid requests to test load & metrics...")
    for i in range(10):
        stan = random.randint(100000, 999999)
        msg3 = create_msg("0100", "1234567890123456", stan)
        send_msg(s, msg3)
        time.sleep(0.05)
        
    s.close()

if __name__ == "__main__":
    main()
