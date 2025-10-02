#!/bin/bash

# Variables - change these as needed
WIFI_IFACE="wlp2s0"
ETH_IFACE="enp0s3"
SSID="Chat-server"
WIFI_PASS="0000"
SUBNET="192.168.0"
WIFI_IP="${SUBNET}.1"
DHCP_RANGE_START="${SUBNET}.10"
DHCP_RANGE_END="${SUBNET}.100"

# Install required packages
sudo apt update
sudo apt install -y hostapd dnsmasq iptables

# Stop services to configure
sudo systemctl stop hostapd
sudo systemctl stop dnsmasq

# Configure static IP for WiFi interface
sudo ip addr flush dev $WIFI_IFACE
sudo ip addr add $WIFI_IP/24 dev $WIFI_IFACE
sudo ip link set $WIFI_IFACE up

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
driver=nl80211
ssid=$SSID
hw_mode=g
channel=7
wmm_enabled=0
macaddr_acl=0
auth_algs=1
ignore_broadcast_ssid=0
wpa=2
wpa_passphrase=$WIFI_PASS
wpa_key_mgmt=WPA-PSK
wpa_pairwise=TKIP
rsn_pairwise=CCMP
EOF

sudo sed -i "s|#DAEMON_CONF=\"\"|DAEMON_CONF=\"/etc/hostapd/hostapd.conf\"|" /etc/default/hostapd

# Enable IP forwarding
sudo sysctl -w net.ipv4.ip_forward=1
sudo sed -i 's/#net.ipv4.ip_forward=1/net.ipv4.ip_forward=1/' /etc/sysctl.conf

# Set up NAT (iptables)
sudo iptables -t nat -A POSTROUTING -o $ETH_IFACE -j MASQUERADE
sudo iptables -A FORWARD -i $ETH_IFACE -o $WIFI_IFACE -m state --state RELATED,ESTABLISHED -j ACCEPT
sudo iptables -A FORWARD -i $WIFI_IFACE -o $ETH_IFACE -j ACCEPT

# Save iptables rules
sudo sh -c "iptables-save > /etc/iptables.rules"
sudo bash -c 'cat > /etc/network/if-up.d/iptables' <<EOF
#!/bin/sh
iptables-restore < /etc/iptables.rules
EOF
sudo chmod +x /etc/network/if-up.d/iptables

# Start services
sudo systemctl restart dnsmasq
sudo systemctl restart hostapd

echo "Access Point setup complete. SSID: $SSID, Password: $WIFI_PASS"