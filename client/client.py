#!/usr/bin/env python3
"""
LSO Project - Forza 4 (Connect 4) Python Client

A multi-threaded TCP client for playing Connect 4.
Features:
- Interactive command-line interface
- Real-time server message handling
- Game state notifications
"""

import socket
import sys
import threading
import time
import os

# ============================================================================
# CONFIGURATION
# ============================================================================

DEFAULT_HOST = "server"
DEFAULT_PORT = 8080
BUFFER_SIZE = 4096

class Colors:
    RESET = "\033[0m"
    BOLD = "\033[1m"
    RED = "\033[91m"
    GREEN = "\033[92m"
    YELLOW = "\033[93m"
    BLUE = "\033[94m"
    MAGENTA = "\033[95m"
    CYAN = "\033[96m"
    WHITE = "\033[97m"

# ============================================================================
# TCP CLIENT CLASS
# ============================================================================

class Forza4Client:
    
    def __init__(self, host: str, port: int):
        self.host = host
        self.port = port
        self.socket = None
        self.connected = False
        self.running = True
        self.username = None
        
    def connect(self) -> bool:
        try:
            self.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.socket.connect((self.host, self.port))
            self.connected = True
            return True
        except socket.error as e:
            print(f"{Colors.RED}[ERRORE] Connessione fallita: {e}{Colors.RESET}")
            return False
    
    def disconnect(self):
        self.running = False
        self.connected = False
        if self.socket:
            try:
                self.socket.close()
            except:
                pass
    
    def receive_messages(self):
        while self.running and self.connected:
            try:
                data = self.socket.recv(BUFFER_SIZE)
                if not data:
                    print(f"\n{Colors.YELLOW}[INFO] Connessione chiusa dal server.{Colors.RESET}")
                    self.connected = False
                    self.running = False
                    break
                
                message = data.decode('utf-8')
                self.display_message(message)
                
            except socket.error as e:
                if self.running:
                    print(f"\n{Colors.RED}[ERRORE] Errore di ricezione: {e}{Colors.RESET}")
                    self.connected = False
                break
            except Exception as e:
                if self.running:
                    print(f"\n{Colors.RED}[ERRORE] {e}{Colors.RESET}")
                break
    
    def display_message(self, message: str):
        if "[ERRORE]" in message:
            message = message.replace("[ERRORE]", f"{Colors.RED}[ERRORE]{Colors.RESET}")
        if "[OK]" in message:
            message = message.replace("[OK]", f"{Colors.GREEN}[OK]{Colors.RESET}")
        if "[NOTIFICA]" in message:
            message = message.replace("[NOTIFICA]", f"{Colors.CYAN}[NOTIFICA]{Colors.RESET}")
        if "[RICHIESTA]" in message:
            message = message.replace("[RICHIESTA]", f"{Colors.YELLOW}[RICHIESTA]{Colors.RESET}")
        if "[TURNO]" in message:
            message = message.replace("[TURNO]", f"{Colors.GREEN}[TURNO]{Colors.RESET}")
        if "[INFO]" in message:
            message = message.replace("[INFO]", f"{Colors.BLUE}[INFO]{Colors.RESET}")
        if "[STATUS]" in message:
            message = message.replace("[STATUS]", f"{Colors.MAGENTA}[STATUS]{Colors.RESET}")
        
        message = message.replace(" X ", f" {Colors.RED}X{Colors.RESET} ")
        message = message.replace(" O ", f" {Colors.YELLOW}O{Colors.RESET} ")
        
        if "HAI VINTO" in message:
            message = message.replace("HAI VINTO!", f"{Colors.GREEN}HAI VINTO!{Colors.RESET}")
        if "HAI PERSO" in message:
            message = message.replace("HAI PERSO!", f"{Colors.RED}HAI PERSO!{Colors.RESET}")
        if "PAREGGIO" in message:
            message = message.replace("PAREGGIO!", f"{Colors.YELLOW}PAREGGIO!{Colors.RESET}")
        
        print(message, end='')
        sys.stdout.flush()
    
    def send_message(self, message: str) -> bool:
        if not self.connected:
            print(f"{Colors.RED}[ERRORE] Non connesso al server.{Colors.RESET}")
            return False
        try:
            self.socket.send((message + "\n").encode('utf-8'))
            return True
        except socket.error as e:
            print(f"{Colors.RED}[ERRORE] Invio fallito: {e}{Colors.RESET}")
            return False
    
    def show_local_help(self):
        help_text = f"""
{Colors.CYAN}╔═══════════════════════════════════════════════════════════════╗
║                    GUIDA RAPIDA CLIENT                         ║
╠═══════════════════════════════════════════════════════════════╣
║  Connessione:                                                  ║
║    - Sei connesso a: {self.host}:{self.port}                          
║    - Username: {self.username or 'Non impostato'}                              
║                                                                ║
║  Comandi locali:                                               ║
║    /help     - Mostra questa guida                             ║
║    /clear    - Pulisce lo schermo                              ║
║    /quit     - Disconnetti (equivale a 'quit')                 ║
║                                                                ║
║  Digita 'help' per i comandi del server                        ║
╚═══════════════════════════════════════════════════════════════╝{Colors.RESET}
"""
        print(help_text)
    
    def run_interactive(self):
        if not self.connect():
            return
        
        receiver_thread = threading.Thread(target=self.receive_messages, daemon=True)
        receiver_thread.start()
        
        print(f"\n{Colors.GREEN}[CLIENT] Connesso a {self.host}:{self.port}{Colors.RESET}")
        print(f"{Colors.CYAN}Digita /help per la guida locale o 'help' per i comandi di gioco.{Colors.RESET}\n")
        
        try:
            while self.connected and self.running:
                try:
                    user_input = input()
                    
                    if not self.connected:
                        break
                    
                    if user_input.startswith('/'):
                        cmd = user_input[1:].lower().strip()
                        
                        if cmd == 'help':
                            self.show_local_help()
                        elif cmd == 'clear':
                            os.system('cls' if os.name == 'nt' else 'clear')
                        elif cmd == 'quit' or cmd == 'exit':
                            self.send_message('quit')
                            time.sleep(0.3)
                            break
                        else:
                            print(f"{Colors.YELLOW}Comando locale sconosciuto. Usa /help{Colors.RESET}")
                        continue
                    
                    if user_input.lower() in ['quit', 'exit']:
                        self.send_message(user_input)
                        time.sleep(0.3)
                        break
                    
                    if user_input:
                        self.send_message(user_input)
                        
                except EOFError:
                    break
                except KeyboardInterrupt:
                    print(f"\n{Colors.YELLOW}[CLIENT] Interruzione...{Colors.RESET}")
                    self.send_message('quit')
                    time.sleep(0.3)
                    break
                    
        finally:
            self.disconnect()
            print(f"\n{Colors.CYAN}[CLIENT] Disconnesso.{Colors.RESET}")


# ============================================================================
# UTILITY FUNCTIONS
# ============================================================================

def wait_for_server(host: str, port: int, max_retries: int = 15, delay: float = 2.0) -> bool:
    print(f"{Colors.YELLOW}[CLIENT] Attesa del server {host}:{port}...{Colors.RESET}")
    
    for attempt in range(max_retries):
        try:
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            sock.settimeout(5)
            sock.connect((host, port))
            sock.close()
            print(f"{Colors.GREEN}[CLIENT] Server disponibile!{Colors.RESET}")
            return True
        except (socket.error, socket.timeout):
            print(f"{Colors.YELLOW}[CLIENT] Tentativo {attempt + 1}/{max_retries} - Riprovo in {delay}s...{Colors.RESET}")
            time.sleep(delay)
    
    print(f"{Colors.RED}[CLIENT] Server non raggiungibile dopo {max_retries} tentativi.{Colors.RESET}")
    return False


def print_banner():
    banner = f"""
{Colors.CYAN}╔═══════════════════════════════════════════════════════════════╗
║                                                               ║
║     ███████╗ ██████╗ ██████╗ ███████╗ █████╗     ██╗  ██╗     ║
║     ██╔════╝██╔═══██╗██╔══██╗╚══███╔╝██╔══██╗    ██║  ██║     ║
║     █████╗  ██║   ██║██████╔╝  ███╔╝ ███████║    ███████║     ║
║     ██╔══╝  ██║   ██║██╔══██╗ ███╔╝  ██╔══██║    ╚════██║     ║
║     ██║     ╚██████╔╝██║  ██║███████╗██║  ██║         ██║     ║
║     ╚═╝      ╚═════╝ ╚═╝  ╚═╝╚══════╝╚═╝  ╚═╝         ╚═╝     ║
║                                                               ║
║                    CLIENT PYTHON v1.0                         ║
║                                                               ║
╚═══════════════════════════════════════════════════════════════╝{Colors.RESET}
"""
    print(banner)


# ============================================================================
# MAIN
# ============================================================================

def main():
    host = sys.argv[1] if len(sys.argv) > 1 else os.environ.get('SERVER_HOST', DEFAULT_HOST)
    port = int(sys.argv[2]) if len(sys.argv) > 2 else int(os.environ.get('SERVER_PORT', DEFAULT_PORT))
    
    print_banner()
    
    print(f"{Colors.BLUE}[CLIENT] Host: {host}, Porta: {port}{Colors.RESET}")
    
    if not wait_for_server(host, port):
        sys.exit(1)
    
    client = Forza4Client(host, port)
    client.run_interactive()


if __name__ == "__main__":
    main()
