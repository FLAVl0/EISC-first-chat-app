#!/bin/bash

# Variables - change these as needed
WIFI_IFACE="wlp3s0" # Change to your WiFi interface (use `ip a` or `ifconfig` to find it)
ETH_IFACE="enp1s31f6" # Change to your Ethernet interface (use `ip a` or `ifconfig` to find it)
# Optional: set to 1 to disable offloading (can help with slow forwarding)
DISABLE_OFFLOAD=1
SSID="Chat-server"
WIFI_PASS="tpreseau"
SUBNET="192.168.0"
WIFI_IP="${SUBNET}.1"
DHCP_RANGE_START="${SUBNET}.10"
DHCP_RANGE_END="${SUBNET}.100"

# Install required packages
sudo apt update
sudo apt install -y hostapd dnsmasq iptables


# Function to reset WiFi interface and restart hostapd cleanly
reset_wifi_iface() {
	sudo systemctl stop hostapd
	sudo ip link set $WIFI_IFACE down
	sudo ip addr flush dev $WIFI_IFACE
	sudo ip link set $WIFI_IFACE up
	sleep 1
	sudo systemctl start hostapd
}

# Stop services to configure
sudo systemctl stop hostapd
sudo systemctl stop dnsmasq

# Configure static IP for WiFi interface
sudo ip addr flush dev $WIFI_IFACE
sudo ip addr add $WIFI_IP/24 dev $WIFI_IFACE
sudo ip link set $WIFI_IFACE up

# Optionally disable offloading (can help with slow forwarding)
if [ "$DISABLE_OFFLOAD" = "1" ]; then
	sudo ethtool -K $WIFI_IFACE gro off tso off gso off
fi

# Configure dnsmasq (DHCP/DNS)
sudo bash -c "cat > /etc/dnsmasq.conf" <<EOF
interface=$WIFI_IFACE
dhcp-range=$DHCP_RANGE_START,$DHCP_RANGE_END,255.255.255.0,24h
domain-needed
bogus-priv
EOF

# Configure hostapd (WiFi AP)
sudo bash -c "cat > /etc/hostapd/hostapd.conf" <<EOF
interface=$WIFI_IFACE
ssid=$SSID
hw_mode=a
channel=0
beacon_int=100
wmm_enabled=1
macaddr_acl=0
auth_algs=1
ignore_broadcast_ssid=0
wpa=2
#wpa_psk=f82a334e9c350b0c4597069190d5bde223f82c2ebf03652d8cfb180b318b09f2
wpa_passphrase=$WIFI_PASS
wpa_key_mgmt=WPA-PSK
wpa_pairwise=TKIP
rsn_pairwise=CCMP
EOF

sudo sed -i "s|#DAEMON_CONF=\"\"|DAEMON_CONF=\"/etc/hostapd/hostapd.conf\"|" /etc/default/hostapd

# Enable IP forwarding and tune sysctl for better throughput
sudo sysctl -w net.ipv4.ip_forward=1
sudo sysctl -w net.core.netdev_max_backlog=5000
sudo sysctl -w net.ipv4.tcp_congestion_control=bbr
sudo sed -i 's/#net.ipv4.ip_forward=1/net.ipv4.ip_forward=1/' /etc/sysctl.conf

# Set up NAT (iptables)
sudo iptables -t nat -A POSTROUTING -o $ETH_IFACE -j MASQUERADE

# Allow ICMP (ping) in both directions
sudo iptables -A FORWARD -p icmp -i $WIFI_IFACE -o $ETH_IFACE -j ACCEPT
sudo iptables -A FORWARD -p icmp -i $ETH_IFACE -o $WIFI_IFACE -j ACCEPT

# Allow all TCP port in both directions
sudo iptables -A FORWARD -p tcp -i $WIFI_IFACE -o $ETH_IFACE -j ACCEPT
sudo iptables -A FORWARD -p tcp -i $ETH_IFACE -o $WIFI_IFACE -j ACCEPT

# Allow established/related connections
sudo iptables -A FORWARD -i $ETH_IFACE -o $WIFI_IFACE -m state --state RELATED,ESTABLISHED -j ACCEPT
sudo iptables -A FORWARD -i $WIFI_IFACE -o $ETH_IFACE -j ACCEPT

# Set default FORWARD policy to ACCEPT (after all specific rules)
sudo iptables -P FORWARD ACCEPT

# Save iptables rules
sudo sh -c "iptables-save > /etc/iptables.rules"
sudo bash -c 'cat > /etc/network/if-up.d/iptables' <<EOF
#!/bin/sh
iptables-restore < /etc/iptables.rules
EOF
sudo chmod +x /etc/network/if-up.d/iptables

# Start services
sudo systemctl restart dnsmasq
reset_wifi_iface

# Add systemd override to auto-restart hostapd on failure
sudo mkdir -p /etc/systemd/system/hostapd.service.d
sudo bash -c 'cat > /etc/systemd/system/hostapd.service.d/override.conf' <<EOF
[Service]
Restart=always
RestartSec=2
EOF
sudo systemctl daemon-reload

echo "Access Point setup complete. SSID: $SSID, Password: $WIFI_PASS"
echo "If you experience WiFi disconnects, hostapd will now auto-restart. For manual reset, run: reset_wifi_iface"