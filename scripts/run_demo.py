#!/usr/bin/env python3
"""
Tribune MPC Demo Script
Spawns a server and multiple clients with real Ed25519 keypairs
"""

import subprocess
import time
import signal
import sys
import os
from concurrent.futures import ThreadPoolExecutor
import threading

# Configuration
SERVER_HOST = "localhost"
SERVER_PORT = 8080
NUM_CLIENTS = 10
BASE_CLIENT_PORT = 18001

# Color codes for terminal output
class Colors:
    SERVER = '\033[94m'    # Blue
    CLIENT = '\033[92m'    # Green
    ERROR = '\033[91m'     # Red
    WARNING = '\033[93m'   # Yellow
    ENDC = '\033[0m'       # End color
    BOLD = '\033[1m'       # Bold

def print_colored(message, color=Colors.ENDC, prefix=""):
    """Print colored message with optional prefix"""
    print(f"{color}{prefix}{message}{Colors.ENDC}")

def generate_ed25519_keypair():
    """
    Generate Ed25519 keypair using OpenSSL
    Returns (private_key_hex, public_key_hex)
    """
    try:
        # Generate private key
        private_key_process = subprocess.run([
            'openssl', 'genpkey', '-algorithm', 'Ed25519'
        ], capture_output=True, text=True, check=True)
        
        private_key_pem = private_key_process.stdout
        
        # Extract public key
        public_key_process = subprocess.run([
            'openssl', 'pkey', '-in', '/dev/stdin', '-pubout'
        ], input=private_key_pem, capture_output=True, text=True, check=True)
        
        public_key_pem = public_key_process.stdout
        
        # Convert to hex (simplified for demo - in real implementation would parse PEM properly)
        private_hex = f"ed25519_private_{hash(private_key_pem) & 0xffffffff:08x}"
        public_hex = f"ed25519_public_{hash(public_key_pem) & 0xffffffff:08x}"
        
        return private_hex, public_hex
        
    except (subprocess.CalledProcessError, FileNotFoundError):
        # Fallback to mock keys if OpenSSL not available
        import random
        seed = random.randint(0, 2**32-1)
        private_hex = f"ed25519_private_{seed:08x}"
        public_hex = f"ed25519_public_{seed:08x}"
        return private_hex, public_hex

def stream_output(process, client_id, color):
    """Stream process output with colored prefixes"""
    while True:
        output = process.stdout.readline()
        if output == '' and process.poll() is not None:
            break
        if output:
            prefix = f"[{client_id}] "
            print_colored(output.strip(), color, prefix)

def run_server():
    """Run the Tribune server"""
    print_colored("Starting Tribune Server...", Colors.SERVER + Colors.BOLD)
    
    try:
        server_process = subprocess.Popen(
            ['./build/server_app'],
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            universal_newlines=True,
            cwd=os.path.dirname(os.path.abspath(__file__)) + "/../"
        )
        
        # Stream server output in a separate thread
        output_thread = threading.Thread(
            target=stream_output, 
            args=(server_process, "SERVER", Colors.SERVER)
        )
        output_thread.daemon = True
        output_thread.start()
        
        return server_process, output_thread
                
    except FileNotFoundError:
        print_colored("Error: server_app not found. Please build the project first.", Colors.ERROR)
        return None, None
    except Exception as e:
        print_colored(f"Error running server: {e}", Colors.ERROR)
        return None, None

def run_client(client_id, port):
    """Run a Tribune client"""
    client_name = f"CLIENT-{client_id:02d}"
    
    print_colored(f"Starting {client_name} on port {port}", Colors.CLIENT + Colors.BOLD)
    print_colored(f"Will generate real Ed25519 keypair", Colors.CLIENT, f"[{client_name}] ")
    
    try:
        # Call client_app with just port - it will generate real Ed25519 keys
        cmd = [
            './build/client_app',
            str(port)           # listen port - no keys provided, will auto-generate
        ]
        
        client_process = subprocess.Popen(
            cmd,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            universal_newlines=True,
            cwd=os.path.dirname(os.path.abspath(__file__)) + "/../"
        )
        
        # Enable client output to debug connection issues
        output_thread = threading.Thread(
            target=stream_output, 
            args=(client_process, client_name, Colors.CLIENT)
        )
        output_thread.daemon = True
        output_thread.start()
        
        return client_process, output_thread
        
    except FileNotFoundError:
        print_colored("Error: client_app not found. Please build the project first.", Colors.ERROR)
        return None, None
    except Exception as e:
        print_colored(f"Error running client {client_id}: {e}", Colors.ERROR)
        return None, None

def main():
    """Main demo function"""
    print_colored("=" * 60, Colors.BOLD)
    print_colored("TRIBUNE MPC DEMONSTRATION", Colors.BOLD)
    print_colored("=" * 60, Colors.BOLD)
    print()
    
    processes = []
    
    try:
        # Start server
        print_colored("Starting server...", Colors.WARNING)
        server_process, server_thread = run_server()
        if server_process:
            processes.append(server_process)
            time.sleep(2)  # Give server time to start
        else:
            return 1
        
        # Start clients
        print_colored(f"Starting {NUM_CLIENTS} clients with real Ed25519 keys...", Colors.WARNING)
        print()
        
        for i in range(NUM_CLIENTS):
            client_port = BASE_CLIENT_PORT + i
            
            client_process, output_thread = run_client(i+1, client_port)
            if client_process:
                processes.append(client_process)
                time.sleep(0.5)  # Stagger client starts
        
        print()
        print_colored("All processes started! MPC demo running...", Colors.BOLD + Colors.WARNING)
        print_colored("Press Ctrl+C to stop all processes", Colors.WARNING)
        print_colored("=" * 60, Colors.BOLD)
        print()
        
        # Keep main thread alive
        while True:
            time.sleep(1)
            # Check if any process died
            for i, process in enumerate(processes):
                if process.poll() is not None:
                    print_colored(f"Process {i} terminated with code {process.returncode}", Colors.ERROR)
    
    except KeyboardInterrupt:
        print_colored("\nShutting down demo...", Colors.WARNING)
    
    finally:
        # Clean shutdown
        print_colored("Terminating all processes...", Colors.WARNING)
        for process in processes:
            try:
                process.terminate()
                process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                print_colored("Force killing process...", Colors.ERROR)
                process.kill()
        
        print_colored("Demo stopped.", Colors.BOLD)
        return 0

if __name__ == "__main__":
    sys.exit(main())