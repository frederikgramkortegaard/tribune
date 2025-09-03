#!/bin/bash

# Client that connects and then listens for events
CLIENT_ID="client-listener-001"
CLIENT_PORT=9001

echo "Starting client listener on port $CLIENT_PORT..."

# First, connect to the server
echo "Connecting to server..."
curl -X POST http://localhost:8080/connect \
  -H "Content-Type: application/json" \
  -d "{
    \"client_host\": \"localhost\",
    \"client_port\": \"$CLIENT_PORT\",
    \"client_id\": \"$CLIENT_ID\",
    \"x25519_pub\": \"abcd1234567890abcd1234567890abcd1234567890abcd1234567890abcd\",
    \"ed25519_pub\": \"1234abcd567890ef1234abcd567890ef1234abcd567890ef1234abcd5678\"
  }"

echo -e "\n\nConnected! Now listening for events on port $CLIENT_PORT..."
echo "The server will POST to http://localhost:$CLIENT_PORT/event"
echo "Press Ctrl+C to stop"
echo "----------------------------------------"

# Start a simple HTTP server using netcat to listen for POST requests
while true; do
  echo -e "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n\r\n{\"status\":\"received\"}" | \
  nc -l localhost $CLIENT_PORT | while read line; do
    # Print incoming requests
    echo "$line"
    # Look for the event data after empty line
    if [[ -z "$line" || "$line" == $'\r' ]]; then
      # Read the JSON body
      read json_body
      if [[ ! -z "$json_body" ]]; then
        echo ">>> Received Event: $json_body"
        echo "----------------------------------------"
      fi
    fi
  done
done