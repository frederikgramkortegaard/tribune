#!/bin/bash

# Generate self-signed certificates for Tribune TLS testing

# Create certs directory if it doesn't exist
mkdir -p certs

# Generate server private key
openssl genrsa -out certs/server-key.pem 2048

# Generate server certificate
openssl req -new -x509 -key certs/server-key.pem -out certs/server-cert.pem -days 365 \
    -subj "/C=US/ST=State/L=City/O=Tribune/CN=localhost"

# Generate client private key
openssl genrsa -out certs/client-key.pem 2048

# Generate client certificate
openssl req -new -x509 -key certs/client-key.pem -out certs/client-cert.pem -days 365 \
    -subj "/C=US/ST=State/L=City/O=Tribune Client/CN=localhost"

echo "Certificates generated in certs/ directory:"
echo "  - Server: certs/server-cert.pem, certs/server-key.pem"
echo "  - Client: certs/client-cert.pem, certs/client-key.pem"
echo ""
echo "To enable TLS, update your config files:"
echo ""
echo "server.json:"
echo '  "use_tls": true,'
echo '  "cert_file": "certs/server-cert.pem",'
echo '  "private_key_file": "certs/server-key.pem"'
echo ""
echo "client.json:"
echo '  "use_tls": true,'
echo '  "verify_server_cert": false'