#!/usr/bin/env python3
"""
C-CORD GUI Client v1.0 (Extras F15)
Requer: Python 3, Tkinter (sudo apt install python3-tk)
"""

import tkinter as tk
from tkinter import scrolledtext, messagebox
import socket
import threading
import time
import random

# ========== CONSTANTES CRIPTOGRÁFICAS (mesmas do C) ==========
DH_PRIMO = 23
DH_GERADOR = 5
RSA_N = 3233
RSA_E = 17

def exp_mod(base, exp, mod):
    res = 1
    base = base % mod
    while exp > 0:
        if exp % 2 == 1:
            res = (res * base) % mod
        exp //= 2
        base = (base * base) % mod
    return res

def hash_djb2(s):
    h = 5381
    for c in s:
        h = ((h << 5) + h) + ord(c)
    return h

def cifrar_cesar(texto, chave):
    res = []
    for c in texto:
        if 'a' <= c <= 'z':
            res.append(chr((ord(c) - ord('a') + chave) % 26 + ord('a')))
        elif 'A' <= c <= 'Z':
            res.append(chr((ord(c) - ord('A') + chave) % 26 + ord('A')))
        else:
            res.append(c)
    return ''.join(res)

def decifrar_cesar(texto, chave):
    return cifrar_cesar(texto, -chave)

# ========== CLASSE DO CLIENTE ==========
class CCordClient:
    def __init__(self, root):
        self.root = root
        self.root.title("C-CORD GUI v4.0")
        self.root.geometry("700x600")

        # Estado da sessão
        self.sock = None
        self.user = ""
        self.admin = False
        self.canal = "#geral"
        self.chave_sessao = 3  # César (será substituída por DH)
        self.privado = 0
        self.conectado = False
        self.autenticado = False

        # Frame de conexão
        frm_conn = tk.Frame(root)
        frm_conn.pack(pady=5)
        tk.Label(frm_conn, text="IP:").pack(side=tk.LEFT)
        self.entry_ip = tk.Entry(frm_conn, width=15)
        self.entry_ip.insert(0, "127.0.0.1")
        self.entry_ip.pack(side=tk.LEFT, padx=5)
        tk.Label(frm_conn, text="Porto:").pack(side=tk.LEFT)
        self.entry_port = tk.Entry(frm_conn, width=6)
        self.entry_port.insert(0, "10000")
        self.entry_port.pack(side=tk.LEFT, padx=5)
        self.btn_connect = tk.Button(frm_conn, text="Ligar", command=self.connect)
        self.btn_connect.pack(side=tk.LEFT, padx=5)

        # Frame de login
        frm_login = tk.Frame(root)
        frm_login.pack(pady=5)
        tk.Label(frm_login, text="Utilizador:").pack(side=tk.LEFT)
        self.entry_user = tk.Entry(frm_login, width=15)
        self.entry_user.pack(side=tk.LEFT, padx=5)
        tk.Label(frm_login, text="Password:").pack(side=tk.LEFT)
        self.entry_pass = tk.Entry(frm_login, width=15, show="*")
        self.entry_pass.pack(side=tk.LEFT, padx=5)
        self.btn_login = tk.Button(frm_login, text="Login", command=self.login)
        self.btn_login.pack(side=tk.LEFT, padx=5)
        self.btn_logout = tk.Button(frm_login, text="Logout", command=self.logout, state=tk.DISABLED)
        self.btn_logout.pack(side=tk.LEFT, padx=5)

        # Frame de canais
        frm_canais = tk.Frame(root)
        frm_canais.pack(pady=5)
        tk.Label(frm_canais, text="Canal:").pack(side=tk.LEFT)
        self.canal_var = tk.StringVar(root)
        self.canal_var.set("#geral")
        self.option_canais = tk.OptionMenu(frm_canais, self.canal_var, "#geral", "#linux", "#ajuda")
        self.option_canais.pack(side=tk.LEFT, padx=5)
        self.btn_join = tk.Button(frm_canais, text="Entrar", command=self.join_canal)
        self.btn_join.pack(side=tk.LEFT, padx=5)

        # Área de mensagens
        self.txt_messages = scrolledtext.ScrolledText(root, height=20, state=tk.DISABLED)
        self.txt_messages.pack(fill=tk.BOTH, expand=True, padx=5, pady=5)

        # Frame de envio
        frm_send = tk.Frame(root)
        frm_send.pack(fill=tk.X, pady=5)
        self.entry_msg = tk.Entry(frm_send)
        self.entry_msg.pack(side=tk.LEFT, fill=tk.X, expand=True, padx=5)
        self.btn_send = tk.Button(frm_send, text="Enviar", command=self.send_msg)
        self.btn_send.pack(side=tk.RIGHT, padx=5)

        # Estado da GUI
        self.set_gui_state(False)
        self.running = True

        # Thread de receção (será iniciada após login)
        self.receiver_thread = None

    def set_gui_state(self, logged):
        """Ativa/desativa componentes conforme autenticação"""
        state = tk.NORMAL if logged else tk.DISABLED
        self.btn_login.config(state=tk.DISABLED if logged else tk.NORMAL)
        self.btn_logout.config(state=tk.NORMAL if logged else tk.DISABLED)
        self.option_canais.config(state=state)
        self.btn_join.config(state=state)
        self.entry_msg.config(state=state)
        self.btn_send.config(state=state)

    def log(self, msg, color="black"):
        self.txt_messages.config(state=tk.NORMAL)
        self.txt_messages.insert(tk.END, msg + "\n", color)
        self.txt_messages.see(tk.END)
        self.txt_messages.config(state=tk.DISABLED)

    def connect(self):
        ip = self.entry_ip.get()
        port = int(self.entry_port.get())
        try:
            self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.sock.connect((ip, port))
            self.conectado = True
            self.log("[*] Conectado ao servidor.", "green")
            self.btn_connect.config(state=tk.DISABLED)
        except Exception as e:
            messagebox.showerror("Erro", f"Não foi possível ligar: {e}")

    def login(self):
        if not self.conectado:
            messagebox.showerror("Erro", "Ligue primeiro ao servidor.")
            return
        user = self.entry_user.get()
        password = self.entry_pass.get()
        if not user or not password:
            messagebox.showerror("Erro", "Preencha utilizador e password.")
            return

        # Calcular hash + RSA
        h = hash_djb2(password) % RSA_N
        h_cifrado = exp_mod(h, RSA_E, RSA_N)
        cmd = f"AUTH {user} {h_cifrado}"
        try:
            self.sock.send(cmd.encode())
            resp = self.sock.recv(4096).decode()
            if "AUTH_SUCCESS" in resp:
                self.user = user
                self.admin = "ADMIN" in resp
                self.autenticado = True
                self.log(f"[+] Login bem-sucedido como {user} ({'ADMIN' if self.admin else 'USER'})", "green")
                self.set_gui_state(True)
                self.btn_login.config(state=tk.DISABLED)

                # Iniciar Diffie-Hellman
                self.iniciar_dh()

                # Iniciar thread de receção
                self.receiver_thread = threading.Thread(target=self.receive_messages, daemon=True)
                self.receiver_thread.start()

                # Entrar automaticamente no canal #geral
                self.join_canal()
            elif "AUTH_PENDING" in resp:
                self.log("[!] Conta pendente de aprovação.", "orange")
            elif "AUTH_INACTIVE" in resp:
                self.log("[!] Conta suspensa.", "red")
            else:
                self.log("[X] Falha no login.", "red")
        except Exception as e:
            self.log(f"[X] Erro: {e}", "red")

    def iniciar_dh(self):
        # Gerar privado
        self.privado = random.randint(2, 16)
        A = exp_mod(DH_GERADOR, self.privado, DH_PRIMO)
        cmd = f"DH_EXCHANGE {A}"
        try:
            self.sock.send(cmd.encode())
            resp = self.sock.recv(4096).decode()
            if "DH_RESPONSE" in resp:
                B = int(resp.split()[1])
                self.chave_sessao = exp_mod(B, self.privado, DH_PRIMO)
                self.log(f"[CRYPTO] Chave DH estabelecida: K={self.chave_sessao}", "blue")
        except Exception as e:
            self.log(f"[X] DH falhou: {e}", "red")

    def join_canal(self):
        if not self.autenticado:
            return
        canal = self.canal_var.get()
        cmd = f"JOIN {canal}"
        try:
            self.sock.send(cmd.encode())
            resp = self.sock.recv(4096).decode()
            if "JOIN_OK" in resp:
                self.canal = canal
                self.log(f"[+] Entrou no canal {canal}", "green")
            else:
                self.log(f"[X] Falha ao entrar no canal: {resp}", "red")
        except Exception as e:
            self.log(f"[X] Erro: {e}", "red")

    def send_msg(self):
        if not self.autenticado:
            messagebox.showerror("Erro", "Faça login primeiro.")
            return
        msg = self.entry_msg.get()
        if not msg:
            return
        # Cifrar com César (chave DH)
        msg_cifrada = cifrar_cesar(msg, self.chave_sessao)
        cmd = f"BROADCAST {msg_cifrada}"
        try:
            self.sock.send(cmd.encode())
            self.entry_msg.delete(0, tk.END)
            # Aguardar BCAST_SENT
            resp = self.sock.recv(4096).decode()
            if "BCAST_SENT" in resp:
                self.log(f"[{self.user}] {msg}", "black")
            else:
                self.log(f"[X] Erro ao enviar: {resp}", "red")
        except Exception as e:
            self.log(f"[X] Erro: {e}", "red")

    def receive_messages(self):
        while self.running and self.autenticado:
            try:
                data = self.sock.recv(4096).decode()
                if not data:
                    break
                # Se for mensagem do canal (começa com [hh:mm:ss][#canal] user: msg)
                # Decifrar a parte da mensagem
                if ":" in data:
                    partes = data.split(": ", 1)
                    if len(partes) == 2:
                        cabecalho = partes[0]
                        msg_cifrada = partes[1]
                        msg_decifrada = decifrar_cesar(msg_cifrada, self.chave_sessao)
                        data = f"{cabecalho}: {msg_decifrada}"
                self.log(data, "purple")
            except:
                break

    def logout(self):
        if self.sock:
            try:
                self.sock.send(b"LOGOUT")
            except:
                pass
        self.autenticado = False
        self.set_gui_state(False)
        self.btn_login.config(state=tk.NORMAL)
        self.log("[*] Logout efetuado.", "orange")

    def on_close(self):
        self.running = False
        if self.sock:
            self.sock.close()
        self.root.destroy()

if __name__ == "__main__":
    root = tk.Tk()
    app = CCordClient(root)
    root.protocol("WM_DELETE_WINDOW", app.on_close)
    root.mainloop()
