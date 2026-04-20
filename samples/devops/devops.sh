#!/bin/bash

# Simple deployment script in Bash

# Read arguments: source path and destination path
src="$1"
dest="$2"

echo "Starting deployment..."
echo "Source: $src"
echo "Destination: $dest"

# Copy files
cp -r "$src" "$dest"

# Set environment variable
export DEPLOY_ENV="production"
echo "Deployment environment set to: $DEPLOY_ENV"

# Restart service depending on platform
if [[ "$OSTYPE" == "msys" ]] || [[ "$OSTYPE" == "win32" ]]; then
  net stop MyService
  net start MyService
elif [[ "$OSTYPE" == "linux-gnu"* ]] || [[ "$OSTYPE" == "darwin"* ]]; then
  systemctl restart myservice
else
  echo "Unknown platform, manual restart required"
fi

echo "Deployment finished successfully."
exit 0
