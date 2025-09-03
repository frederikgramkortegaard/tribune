#!/bin/bash

# Test connect endpoint with valid data
curl -X POST http://localhost:8080/connect \
  -H "Content-Type: application/json" \
  -d '{
    "client_host": "192.168.1.100",
    "client_port": "9001",
    "client_id": "client-node-001",
    "x25519_pub": "abcd1234567890abcd1234567890abcd1234567890abcd1234567890abcd",
    "ed25519_pub": "1234abcd567890ef1234abcd567890ef1234abcd567890ef1234abcd5678"
  }'

echo