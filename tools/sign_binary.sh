f=$1
echo "signing $f"
cat "$f" | openssl dgst -binary -sha1 -hmac "3C44662f28a1c4d3b2f293f8e0d4be78" >> "$f"

