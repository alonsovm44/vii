FROM mcr.microsoft.com/devcontainers/cpp:latest

# Install any additional tools needed for Vii development
RUN apt-get update && export DEBIAN_FRONTEND=noninteractive \
    && apt-get -y install --no-install-recommends make git

# Set the working directory inside the container
WORKDIR /workspaces/vii