#!/bin/bash
set -e

# TermiHUI Server installer
# Usage: ./install.sh [--uninstall]

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$PROJECT_ROOT/build"
INSTALL_PREFIX="/usr/local"
PLIST_NAME="studio.eugenezakharov.termihui-server"
PLIST_PATH="$HOME/Library/LaunchAgents/$PLIST_NAME.plist"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

log_info() { echo -e "${GREEN}[INFO]${NC} $1"; }
log_warn() { echo -e "${YELLOW}[WARN]${NC} $1"; }
log_error() { echo -e "${RED}[ERROR]${NC} $1"; }

stop_service() {
    if launchctl list | grep -q "$PLIST_NAME"; then
        log_info "Stopping existing service..."
        launchctl unload "$PLIST_PATH" 2>/dev/null || true
    fi
}

uninstall() {
    log_info "Uninstalling TermiHUI server..."
    stop_service
    
    if [ -f "$PLIST_PATH" ]; then
        rm -f "$PLIST_PATH"
        log_info "Removed $PLIST_PATH"
    fi
    
    if [ -f "$INSTALL_PREFIX/bin/termihui-server" ]; then
        sudo rm -f "$INSTALL_PREFIX/bin/termihui-server"
        log_info "Removed $INSTALL_PREFIX/bin/termihui-server"
    fi
    
    log_info "Uninstall complete"
    exit 0
}

# Handle --uninstall flag
if [ "$1" == "--uninstall" ]; then
    uninstall
fi

# Step 1: Clean and Build
log_info "Cleaning build directory..."
rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

log_info "Configuring..."
cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$INSTALL_PREFIX"

log_info "Building server..."
make termihui-server -j$(sysctl -n hw.ncpu)

# Step 2: Stop existing service
stop_service

# Step 3: Install binary
log_info "Installing binary to $INSTALL_PREFIX/bin..."
sudo cmake --install . --component server

# Step 4: Create launchd plist
log_info "Creating launchd service..."
mkdir -p "$HOME/Library/LaunchAgents"

cat > "$PLIST_PATH" << EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>Label</key>
    <string>$PLIST_NAME</string>
    <key>ProgramArguments</key>
    <array>
        <string>$INSTALL_PREFIX/bin/termihui-server</string>
        <string>-b</string>
        <string>0.0.0.0</string>
    </array>
    <key>RunAtLoad</key>
    <true/>
    <key>KeepAlive</key>
    <true/>
    <key>StandardOutPath</key>
    <string>/tmp/termihui-server.log</string>
    <key>StandardErrorPath</key>
    <string>/tmp/termihui-server.err</string>
    <key>EnvironmentVariables</key>
    <dict>
        <key>HOME</key>
        <string>$HOME</string>
        <key>PATH</key>
        <string>/usr/local/bin:/usr/bin:/bin:/usr/sbin:/sbin</string>
    </dict>
</dict>
</plist>
EOF

# Step 5: Load service
log_info "Starting service..."
launchctl load "$PLIST_PATH"

# Step 6: Verify
sleep 1
if launchctl list | grep -q "$PLIST_NAME"; then
    log_info "✅ TermiHUI server installed and running!"
    log_info "   Binary: $INSTALL_PREFIX/bin/termihui-server"
    log_info "   Logs:   /tmp/termihui-server.log"
    log_info "   Errors: /tmp/termihui-server.err"
    log_info ""
    log_info "Commands:"
    log_info "   View logs:    tail -f /tmp/termihui-server.log"
    log_info "   Restart:      launchctl kickstart -k gui/\$(id -u)/$PLIST_NAME"
    log_info "   Stop:         launchctl unload $PLIST_PATH"
    log_info "   Uninstall:    $SCRIPT_DIR/install.sh --uninstall"
else
    log_error "Service failed to start. Check /tmp/termihui-server.err"
    exit 1
fi
