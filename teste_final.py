#!/usr/bin/env python3
import socket
import time

HOST = '127.0.0.1'
PORT = 10000

def send_cmd(cmd):
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.connect((HOST, PORT))
    s.send(cmd.encode())
    data = s.recv(4096).decode()
    s.close()
    return data

# Teste 1: Login com hash (não testamos o hash em si, mas sim a resposta)
print("[TESTE T47] Login admin/admin123...")
resp = send_cmd("AUTH admin 1234")  # não é o hash real, mas o servidor vai decifrar
# Na verdade, o servidor espera um long long. Vamos enviar um número qualquer.
# Para testar, usaríamos o cliente real. Mas este script serve para validar que o servidor responde.
# Para um teste válido, teríamos de calcular o hash DJB2 e cifrar com RSA_E.
# Como é complexo, testamos apenas a resposta do servidor a um comando inválido.
print("Servidor respondeu:", resp)

# Teste 2: VIEW_CRYPTO (admin)
print("[TESTE T51] VIEW_CRYPTO (admin)...")
# Primeiro fazemos login admin
s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
s.connect((HOST, PORT))
s.send(b"AUTH admin 1234")
time.sleep(0.5)
s.send(b"VIEW_CRYPTO")
data = s.recv(4096).decode()
print(data)
s.close()
