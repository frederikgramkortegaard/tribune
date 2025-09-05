#!/usr/bin/env python3
"""
Federated Learning Demo Script

This script demonstrates the Tribune federated learning system by:
1. Building the federated learning executables
2. Starting a federated learning server
3. Starting multiple federated learning clients
4. Letting them run through multiple training rounds
5. Showing the model weight updates over time

Usage:
    python3 run_demo.py [num_clients] [duration_seconds]
    
Default: 3 clients, 60 seconds
"""

import subprocess
import time
import signal
import sys
import os
import threading
from pathlib import Path

# Configuration
DEFAULT_NUM_CLIENTS = 3
DEFAULT_DURATION = 60
SERVER_PORT = 8080
CLIENT_BASE_PORT = 9001

# ANSI color codes
class Colors:
    BLUE = '\033[94m'      # Blue for server
    GREEN = '\033[92m'     # Green for clients
    YELLOW = '\033[93m'    # Yellow for system messages
    RED = '\033[91m'       # Red for errors
    ENDC = '\033[0m'       # End color

def signal_handler(sig, frame):
    """Handle Ctrl+C by cleaning up processes"""
    print("\nStopping demo...")
    cleanup_processes()
    sys.exit(0)

def cleanup_processes():
    """Kill any running federated learning processes"""
    try:
        subprocess.run(["pkill", "-f", "federated_server_app"], check=False)
        subprocess.run(["pkill", "-f", "federated_client_app"], check=False)
    except:
        pass

def build_executables():
    """Build the federated learning executables"""
    print("Building federated learning executables...")
    
    # Get paths relative to this script
    script_dir = Path(__file__).parent
    app_dir = script_dir.parent
    root_dir = app_dir.parent.parent
    build_dir = root_dir / "build"
    
    if not build_dir.exists():
        build_dir.mkdir()
    
    # Change to build directory and run cmake + make
    result = subprocess.run(
        ["cmake", "..", "&&", "make", "federated_server_app", "federated_client_app"],
        cwd=build_dir,
        shell=True,
        capture_output=True,
        text=True
    )
    
    if result.returncode != 0:
        print(f"Build failed: {result.stderr}")
        return False, None, None
        
    # Check that executables exist
    server_exe = build_dir / "federated_server_app"
    client_exe = build_dir / "federated_client_app"
    
    if not server_exe.exists() or not client_exe.exists():
        print("Build succeeded but executables not found")
        return False, None, None
    
    print("Build completed successfully")
    return True, server_exe, client_exe

def start_server(server_exe):
    """Start the federated learning server"""
    print(f"Starting federated learning server on port {SERVER_PORT}...")
    
    server_process = subprocess.Popen(
        [str(server_exe)],
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        bufsize=1,
        universal_newlines=True
    )
    
    # Wait a bit for server to start
    time.sleep(3)
    
    if server_process.poll() is not None:
        stdout, stderr = server_process.communicate()
        print(f"Server failed to start: {stdout}")
        return None
    
    print("Server started successfully")
    return server_process

def start_clients(client_exe, num_clients):
    """Start federated learning clients"""
    print(f"Starting {num_clients} federated learning clients...")
    
    client_processes = []
    
    for i in range(num_clients):
        port = CLIENT_BASE_PORT + i
        print(f"  Starting client {i+1} on port {port}...")
        
        client_process = subprocess.Popen(
            [str(client_exe), str(port)],
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            bufsize=1,
            universal_newlines=True
        )
        
        client_processes.append(client_process)
        time.sleep(1)  # Stagger client starts
    
    # Give clients time to connect
    time.sleep(3)
    
    # Check that all clients are running
    running_clients = 0
    for i, process in enumerate(client_processes):
        if process.poll() is None:
            running_clients += 1
        else:
            stdout, stderr = process.communicate()
            print(f"Client {i+1} failed to start: {stdout}")
    
    print(f"{running_clients}/{num_clients} clients started successfully")
    return client_processes

def monitor_client_output(client_process, client_id, stop_event):
    """Monitor a client's output in a separate thread"""
    while not stop_event.is_set() and client_process.poll() is None:
        try:
            line = client_process.stdout.readline()
            if line:
                print(f"{Colors.GREEN}[CLIENT-{client_id}] {line.strip()}{Colors.ENDC}")
        except:
            break

def monitor_training(server_process, client_processes, duration_seconds):
    """Monitor the training process and show progress"""
    print(f"{Colors.YELLOW}\nFederated learning training started!")
    print(f"Training will run for {duration_seconds} seconds...")
    print(f"Watch for model weight updates in server output\n{Colors.ENDC}")
    
    # Create stop event for client monitoring threads
    stop_event = threading.Event()
    
    # Start client monitoring threads
    client_threads = []
    for i, client_process in enumerate(client_processes):
        if client_process.poll() is None:  # Only monitor running clients
            thread = threading.Thread(
                target=monitor_client_output, 
                args=(client_process, i+1, stop_event)
            )
            thread.daemon = True
            thread.start()
            client_threads.append(thread)
    
    start_time = time.time()
    
    while time.time() - start_time < duration_seconds:
        # Check if server is still running
        if server_process.poll() is not None:
            print(f"{Colors.RED}Server process ended unexpectedly{Colors.ENDC}")
            break
            
        # Show server output in real-time
        try:
            line = server_process.stdout.readline()
            if line:
                print(f"{Colors.BLUE}[SERVER] {line.strip()}{Colors.ENDC}")
        except:
            pass
        
        time.sleep(0.1)
    
    # Stop client monitoring threads
    stop_event.set()
    
    print(f"{Colors.YELLOW}\nTraining completed after {duration_seconds} seconds{Colors.ENDC}")

def main():
    """Main demo function"""
    # Setup signal handler for clean exit
    signal.signal(signal.SIGINT, signal_handler)
    
    # Parse command line arguments
    num_clients = DEFAULT_NUM_CLIENTS
    duration = DEFAULT_DURATION
    
    if len(sys.argv) > 1:
        num_clients = int(sys.argv[1])
    if len(sys.argv) > 2:
        duration = int(sys.argv[2])
    
    print(f"{Colors.YELLOW}=" * 60)
    print("Tribune Federated Learning Demo")
    print("=" * 60)
    print(f"Configuration:")
    print(f"  Clients: {num_clients}")
    print(f"  Duration: {duration} seconds")
    print(f"  Server: localhost:{SERVER_PORT}")
    print(f"  Client ports: {CLIENT_BASE_PORT}-{CLIENT_BASE_PORT + num_clients - 1}")
    print(f"{Colors.ENDC}")
    
    # Clean up any existing processes
    cleanup_processes()
    
    try:
        # Build executables
        success, server_exe, client_exe = build_executables()
        if not success:
            print("Build failed")
            return 1
        
        # Start server
        server_process = start_server(server_exe)
        if not server_process:
            print("Failed to start server")
            return 1
        
        # Start clients
        client_processes = start_clients(client_exe, num_clients)
        if not client_processes:
            print("Failed to start clients")
            return 1
        
        # Monitor training
        monitor_training(server_process, client_processes, duration)
        
        print(f"{Colors.YELLOW}\n" + "=" * 60)
        print("Demo completed successfully!")
        print("Check the output above for federated learning progress")
        print("Model weights should have updated over multiple rounds")
        print("=" * 60 + f"{Colors.ENDC}")
        
    except Exception as e:
        print(f"Demo failed: {e}")
        return 1
        
    finally:
        # Clean up processes
        cleanup_processes()
        print("Cleaned up all processes")
    
    return 0

if __name__ == "__main__":
    sys.exit(main())