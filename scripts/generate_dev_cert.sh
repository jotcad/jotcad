#!/bin/bash
set -e

# Create .ssl directory if it doesn't exist
mkdir -p .ssl

echo "[HTTPS] Generating self-signed certificate for local development..."

# Create OpenSSL configuration for SAN (Subject Alternative Name)
# This ensures browsers don't complain about "Common Name mismatch" when using IPs.
cat > .ssl/openssl.conf <<EOF
[req]
distinguished_name = req_distinguished_name
x509_extensions = v3_req
prompt = no

[req_distinguished_name]
CN = localhost

[v3_req]
keyUsage = critical, digitalSignature, keyEncipherment
extendedKeyUsage = serverAuth
subjectAltName = @alt_names

[alt_names]
DNS.1 = localhost
DNS.2 = *.local
IP.1 = 127.0.0.1
IP.2 = 10.0.0.1
IP.3 = 192.168.1.1
# Tailscale / Common LAN ranges
IP.4 = 100.64.0.1
IP.5 = 100.127.255.254
EOF

openssl req -x509 -nodes -days 365 -newkey rsa:2048 \
  -keyout .ssl/localhost-key.pem \
  -out .ssl/localhost-cert.pem \
  -config .ssl/openssl.conf \
  -extensions v3_req

# Cleanup config
rm .ssl/openssl.conf

echo "[HTTPS] Success! Certificates created in .ssl/"
echo "        - .ssl/localhost-key.pem"
echo "        - .ssl/localhost-cert.pem"
