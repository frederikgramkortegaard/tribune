#!/usr/bin/env python3
"""
Federated Learning Demo Script

This script demonstrates the Tribune federated learning system by:
1. Building the federated learning executables
2. Starting a federated learning server
3. Starting multiple federated learning clients
4. Letting them run through multiple training rounds
5. Showing the model weight updates over time

This demo uses secure multi-party federated learning with additive secret sharing.
Each client computes gradients locally, splits them into random shards, and sends
different shards to different peers. The server aggregates results without seeing
individual gradients, preserving privacy.

Usage:
    python3 run_demo.py [num_clients] [duration_seconds]
    
Default: 4 clients, 60 seconds
Works with any number of clients (2-10 recommended)
"""

import subprocess
import time
import signal
import sys
import os
import threading
from pathlib import Path

# Configuration
DEFAULT_NUM_CLIENTS = 4
DEFAULT_DURATION = 60
SERVER_PORT = 8080
CLIENT_BASE_PORT = 9001

# Global storage for validation - organized by round
local_gradients_by_round = {}  # {round_id: [client_gradients]}
global_gradients_by_round = {}  # {round_id: global_gradient}
current_round = 1  # Start from round 1 (federated learning typically starts from round 1)

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
    """Build the federated learning executables if needed"""
    
    # Get paths relative to this script
    script_dir = Path(__file__).parent
    app_dir = script_dir.parent
    root_dir = app_dir.parent.parent
    build_dir = root_dir / "build"
    
    # Check if executables already exist
    server_exe = build_dir / "federated_server_app"
    client_exe = build_dir / "federated_client_app"
    
    if server_exe.exists() and client_exe.exists():
        print("Found existing executables")
        return True, server_exe, client_exe
    
    print("Building federated learning executables (this may take a moment)...")
    print("Note: Requires CMake and a C++ compiler")
    
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
        print(f"\nBuild failed. Please ensure you have CMake and a C++ compiler installed.")
        print(f"You can also build manually:")
        print(f"  cd {root_dir}")
        print(f"  mkdir -p build && cd build")
        print(f"  cmake .. && make federated_server_app federated_client_app")
        if result.stderr:
            print(f"\nError details: {result.stderr}")
        return False, None, None
        
    # Check that executables exist
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
    global local_gradients_by_round, current_round
    
    while not stop_event.is_set() and client_process.poll() is None:
        try:
            line = client_process.stdout.readline()
            if line:
                line_stripped = line.strip()
                print(f"{Colors.GREEN}[CLIENT-{client_id}] {line_stripped}{Colors.ENDC}")
                
                # Detect round changes from client perspective
                if "training-round-" in line_stripped:
                    import re
                    round_match = re.search(r'training-round-(\d+)', line_stripped)
                    if round_match:
                        detected_round = int(round_match.group(1))
                        global current_round
                        if detected_round != current_round:
                            current_round = detected_round
                            print(f"{Colors.YELLOW}CLIENT-{client_id} detected round {current_round}{Colors.ENDC}")
                
                # Capture local gradients for validation
                if "LOCAL_GRADIENT:" in line_stripped:
                    print(f"{Colors.YELLOW}Captured local gradient from CLIENT-{client_id} for round {current_round}{Colors.ENDC}")
                    if current_round not in local_gradients_by_round:
                        local_gradients_by_round[current_round] = []
                    
                    # Prevent duplicate gradients from same client in same round
                    client_key = f"CLIENT-{client_id}"
                    existing_gradients = [g for g in local_gradients_by_round[current_round] if client_key in g]
                    if len(existing_gradients) == 0:  # Only add if we don't have one from this client yet
                        local_gradients_by_round[current_round].append(f"CLIENT-{client_id}: {line_stripped}")
                    else:
                        print(f"{Colors.YELLOW}Skipping duplicate gradient from CLIENT-{client_id} for round {current_round}{Colors.ENDC}")
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
                line_stripped = line.strip()
                print(f"{Colors.BLUE}[SERVER] {line_stripped}{Colors.ENDC}")
                
                # Track training rounds from server
                if ("Round" in line_stripped and "Current weights:" in line_stripped) or "training-round-" in line_stripped:
                    global current_round
                    import re
                    round_match = re.search(r'Round (\d+)', line_stripped) or re.search(r'training-round-(\d+)', line_stripped)
                    if round_match:
                        detected_round = int(round_match.group(1))
                        if detected_round != current_round:
                            current_round = detected_round
                            print(f"{Colors.YELLOW}SERVER starting training round {current_round}{Colors.ENDC}")
                
                # Capture global gradients for validation
                if "GLOBAL_GRADIENT:" in line_stripped:
                    print(f"{Colors.YELLOW}Captured global gradient from server for round {current_round}{Colors.ENDC}")
                    global_gradients_by_round[current_round] = line_stripped
        except:
            pass
        
        time.sleep(0.1)
    
    # Stop client monitoring threads
    stop_event.set()
    
    print(f"{Colors.YELLOW}\nTraining completed after {duration_seconds} seconds{Colors.ENDC}")
    
    # Validate gradient aggregation
    validate_gradient_aggregation()

def parse_gradient(gradient_str):
    """Parse gradient string like '[0.1, -0.05, 0.02, 0.15]' into list of floats"""
    import re
    
    # Find the bracket part: LOCAL_GRADIENT: [0.322, 1.288, 1.932, 0]
    bracket_match = re.search(r'\[([^\]]+)\]', gradient_str)
    if not bracket_match:
        print(f"{Colors.RED}Could not find brackets in: {gradient_str}{Colors.ENDC}")
        return []
    
    bracket_content = bracket_match.group(1)
    # Extract numbers only from within the brackets
    numbers = re.findall(r'[-+]?\d*\.?\d+(?:[eE][-+]?\d+)?', bracket_content)
    return [float(x) for x in numbers]

def validate_gradient_aggregation():
    """Validate that global gradient equals sum of local gradients for each training round"""
    global local_gradients_by_round, global_gradients_by_round
    
    print(f"\n{Colors.YELLOW}=" * 60)
    print("FEDERATED LEARNING VALIDATION")
    print("=" * 60 + f"{Colors.ENDC}")
    
    if not local_gradients_by_round:
        print(f"{Colors.RED}No local gradients captured!{Colors.ENDC}")
        return
    
    if not global_gradients_by_round:
        print(f"{Colors.RED}No global gradients captured!{Colors.ENDC}")
        return
    
    print(f"Validating {len(global_gradients_by_round)} training rounds:")
    
    total_successes = 0
    total_rounds = 0
    
    # Validate each round separately
    for round_num in sorted(global_gradients_by_round.keys()):
        total_rounds += 1
        print(f"\n{Colors.BLUE}--- ROUND {round_num} ---{Colors.ENDC}")
        
        # Get local gradients for this round
        if round_num not in local_gradients_by_round:
            print(f"{Colors.RED}No local gradients found for round {round_num}{Colors.ENDC}")
            continue
            
        local_grads_this_round = local_gradients_by_round[round_num]
        print(f"Local gradients ({len(local_grads_this_round)} clients):")
        
        # Sum local gradients for this round
        local_sums = [0.0, 0.0, 0.0, 0.0]
        for local_grad in local_grads_this_round:
            print(f"  {local_grad}")
            try:
                grad_values = parse_gradient(local_grad)
                if len(grad_values) >= 4:
                    for i in range(4):
                        local_sums[i] += grad_values[i]
            except Exception as e:
                print(f"{Colors.RED}Error parsing gradient: {e}{Colors.ENDC}")
        
        print(f"Expected global gradient (sum): [{local_sums[0]:.6f}, {local_sums[1]:.6f}, {local_sums[2]:.6f}, {local_sums[3]:.6f}]")
        
        # Compare with server's global gradient for this round
        global_grad = global_gradients_by_round[round_num]
        print(f"Server global gradient: {global_grad}")
        
        try:
            global_values = parse_gradient(global_grad)
            if len(global_values) >= 4:
                print(f"Parsed global gradient: [{global_values[0]:.6f}, {global_values[1]:.6f}, {global_values[2]:.6f}, {global_values[3]:.6f}]")
                
                # Compare with expected
                tolerance = 1e-6  # Slightly more lenient for floating point
                matches = True
                for i in range(4):
                    if abs(global_values[i] - local_sums[i]) > tolerance:
                        matches = False
                        break
                
                if matches:
                    print(f"{Colors.GREEN}✓ SUCCESS: Round {round_num} global gradient matches sum of local gradients!{Colors.ENDC}")
                    total_successes += 1
                else:
                    print(f"{Colors.RED}✗ ERROR: Round {round_num} global gradient mismatch!{Colors.ENDC}")
                    for i in range(4):
                        diff = global_values[i] - local_sums[i]
                        print(f"  Parameter {i}: expected {local_sums[i]:.6f}, got {global_values[i]:.6f}, diff: {diff:.6f}")
        except Exception as e:
            print(f"{Colors.RED}Error parsing global gradient: {e}{Colors.ENDC}")
    
    # Summary
    print(f"\n{Colors.YELLOW}VALIDATION SUMMARY:{Colors.ENDC}")
    print(f"Successfully validated: {total_successes}/{total_rounds} rounds")
    if total_successes == total_rounds and total_rounds > 0:
        print(f"{Colors.GREEN}🎉 ALL ROUNDS PASSED! Federated learning with additive secret sharing is working correctly!{Colors.ENDC}")
    else:
        print(f"{Colors.RED}Some rounds failed validation. Check the federated learning implementation.{Colors.ENDC}")
    
    print(f"{Colors.YELLOW}=" * 60 + f"{Colors.ENDC}")

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