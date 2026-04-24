#!/bin/bash
# Deployment Pipeline in Bash
# Build Docker image, push to registry, SSH to server, deploy, health check
# Lines: 67 | Complexity: Very High | Debugging: Nightmare

set -euo pipefail

# Config
ENV="${1:-staging}"
VERSION="${2:-$(git describe --tags --always 2>/dev/null || echo 'latest')}"
REGISTRY="docker.io/myapp"
SERVER="${DEPLOY_SERVER:-prod-01.example.com}"
SSH_KEY="${SSH_KEY:-~/.ssh/deploy_key}"
MAX_RETRIES=3
RETRY_DELAY=5

echo "=== Deploying $VERSION to $ENV ==="

# Build Docker image
echo "[1/5] Building Docker image..."
if ! docker build -t "$REGISTRY:$VERSION" -t "$REGISTRY:$ENV-latest" .; then
    echo "ERROR: Docker build failed" >&2
    exit 1
fi

# Push with retry
echo "[2/5] Pushing to registry..."
for ((i=1; i<=MAX_RETRIES; i++)); do
    if docker push "$REGISTRY:$VERSION" && docker push "$REGISTRY:$ENV-latest"; then
        echo "Push successful"
        break
    fi
    if [[ $i -eq MAX_RETRIES ]]; then
        echo "ERROR: Push failed after $MAX_RETRIES attempts" >&2
        exit 1
    fi
    echo "Push failed, retrying in ${RETRY_DELAY}s... ($i/$MAX_RETRIES)"
    sleep $RETRY_DELAY
done

# SSH to server and deploy
echo "[3/5] Deploying to $SERVER..."
SSH_OPTS="-o StrictHostKeyChecking=no -o ConnectTimeout=10 -i $SSH_KEY"

if ! ssh $SSH_OPTS "deploy@$SERVER" << EOF
    set -e
    echo "Pulling image..."
    docker pull "$REGISTRY:$VERSION"
    
    echo "Stopping old container..."
    docker stop myapp-$ENV 2>/dev/null || true
    docker rm myapp-$ENV 2>/dev/null || true
    
    echo "Starting new container..."
    docker run -d \
        --name myapp-$ENV \
        -p 8080:8080 \
        -e ENV=$ENV \
        -e VERSION=$VERSION \
        --restart unless-stopped \
        "$REGISTRY:$VERSION"
    
    echo "Pruning old images..."
    docker image prune -f
EOF
then
    echo "ERROR: Deployment failed on server" >&2
    exit 1
fi

# Health check
echo "[4/5] Running health checks..."
HEALTH_URL="http://$SERVER:8080/health"
for ((i=1; i<=MAX_RETRIES; i++)); do
    sleep 2
    if curl -sf "$HEALTH_URL" > /dev/null 2>&1; then
        echo "Health check passed"
        break
    fi
    if [[ $i -eq MAX_RETRIES ]]; then
        echo "WARNING: Health check failed, but deployment completed" >&2
    fi
    echo "Health check failed, retrying... ($i/$MAX_RETRIES)"
done

# Cleanup local images
echo "[5/5] Cleaning up..."
docker rmi "$REGISTRY:$VERSION" "$REGISTRY:$ENV-latest" 2>/dev/null || true

echo "=== Deployment complete ==="
echo "Version: $VERSION"
echo "Server: $SERVER"
echo "URL: http://$SERVER:8080"
