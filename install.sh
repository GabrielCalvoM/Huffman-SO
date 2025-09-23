#!/bin/bash

# Installation Script for Debian 13
# [OS] Project 1 - Huffman Compression

#########################################################################
set -e # run away if something fails so Prof. Rodolfo won't zero us

RED='\033[0;31m'
GREEN='\033[0;32m'
BLUE='\033[0;34m'
NC='\033[0m'

echo "---------------------------------------------"
echo "[WELCOME] Installing Huffman Compression Tool"
echo "---------------------------------------------"

print_status() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

#########################################################################

# Check sudo (I do not understand Debian 13 su - , Xubuntu mvp )
check_sudo() {
    if ! command -v sudo &> /dev/null; then
        print_error "sudo is not installed or not available"
        print_status "Please install sudo or run this script as root"
        exit 1
    fi
    
    # Test sudo access (I hate Virtual Machines)
    if ! sudo -n true 2>/dev/null; then
        print_status "Testing sudo access..."
        sudo -v || {
            print_error "Unable to obtain sudo privileges"
            exit 1
        }
    fi
}

#########################################################################
# Update and install packages
check_sudo

print_status "Updating package database..."
sudo apt update

print_status "Installing dependencies..."
sudo apt install -y gcc make libutf8proc-dev

#########################################################################
# Build project
print_status "Building project..."
if [ -f "Makefile" ]; then
    make clean 2>/dev/null || true
    make
    print_success "Build complete! Run './huffman -h' to get started."
else
    print_error "Makefile not found in current directory"
    exit 1
fi