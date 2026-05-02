import socket
import binascii
import time

def bcd_to_bytes(s):
    """Converte uma string de dígitos para bytes BCD."""
    if len(s) % 2 != 0:
        s = '0' + s  # Garante número par de dígitos
    return bytes.fromhex(s)

def str_to_bytes(s, length):
    """Converte uma string ASCII para bytes, com padding se necessário."""
    return s.encode('ascii').ljust(length, b' ')

def create_iso_message(pan, stan):
    """
    Monta uma mensagem ISO 8583 binária para uma transação de autorização (0100).
    """
    # MTI: 0100 (BCD)
    mti = bcd_to_bytes("0100")

    # Bitmap: 7220000000C08000 (Campos 2, 3, 4, 7, 11, 41, 42, 49)
    bitmap = bytes.fromhex("7220000000C08000")

    # --- Campos de Dados ---
    # DE2 (PAN): LLVAR BCD
    pan_len_byte = len(pan).to_bytes(1, 'big')
    de2 = pan_len_byte + bcd_to_bytes(pan)

    # DE3 (Processing Code): FIXED 6 BCD
    de3 = bcd_to_bytes("000000")

    # DE4 (Amount): FIXED 12 BCD (R$ 10.00)
    de4 = bcd_to_bytes("000000001000")

    # DE7 (Transmission DateTime): FIXED 10 BCD
    de7 = bcd_to_bytes("0427150400") # MMDDhhmmss

    # DE11 (STAN): FIXED 6 BCD
    de11 = bcd_to_bytes(str(stan).zfill(6))

    # DE41 (Terminal ID): FIXED 8 ASCII
    de41 = str_to_bytes("TERM0001", 8)

    # DE42 (Merchant ID): FIXED 15 ASCII
    de42 = str_to_bytes("MERCH0000000001", 15)

    # DE49 (Currency Code): FIXED 3 ASCII
    de49 = str_to_bytes("986", 3) # BRL

    # Concatena todos os campos para formar o corpo da mensagem
    iso_body = mti + bitmap + de2 + de3 + de4 + de7 + de11 + de41 + de42 + de49
    
    # Adiciona o cabeçalho de 2 bytes com o tamanho
    header = len(iso_body).to_bytes(2, 'big')
    
    return header + iso_body

def send_and_receive(sock, message):
    """Envia a mensagem e imprime a resposta do gateway."""
    print(f"-> Enviando {len(message)} bytes: {message.hex().upper()}")
    sock.sendall(message)
    
    # Lê a resposta
    resp_header = sock.recv(2)
    if not resp_header:
        print("<- Conexão fechada pelo servidor.")
        return
        
    resp_len = int.from_bytes(resp_header, 'big')
    resp_body = sock.recv(resp_len)
    print(f"<- Resposta recebida ({resp_len} bytes): {resp_body.hex().upper()}")
    # Aqui, uma aplicação real faria o parse do DE39 para obter o código de resposta.

def main():
    host = '127.0.0.1'
    port = 8080
    
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
        sock.connect((host, port))
        print(f"Conectado ao Gateway em {host}:{port}\n")

        # --- Cenário 1: INCORRETO (Enviando PAN mascarado) ---
        print("--- CENÁRIO 1: FALHA (PAN Mascarado) ---")
        # Este PAN está mascarado e não corresponde a nenhum BIN válido no gateway.
        # O gateway irá rejeitar com erro "12 - Transação Inválida".
        invalid_pan = "454881******3456" 
        invalid_message = create_iso_message(invalid_pan, stan=1001)
        send_and_receive(sock, invalid_message)
        print("----------------------------------------\n")
        
        time.sleep(1)

        # --- Cenário 2: CORRETO (Enviando PAN completo) ---
        print("--- CENÁRIO 2: SUCESSO (PAN Completo) ---")
        # Este PAN é válido e o BIN "123456" está configurado para rotear para o Kafka.
        # O gateway irá aceitar e processar a transação.
        valid_pan = "1234567890123456"
        valid_message = create_iso_message(valid_pan, stan=1002)
        send_and_receive(sock, valid_message)
        print("---------------------------------------\n")

if __name__ == "__main__":
    main()
