#!/bin/bash

# ===== Variables à modifier si besoin =====
WIFI_IFACE="wlp3s0"       # Interface Wi-Fi
ETH_IFACE="enp1s31f6"     # Interface Ethernet pour NAT
SSID="Chat-server"
WIFI_PASS="tpreseau"
SUBNET="192.168.0"
WIFI_IP="${SUBNET}.1"
DHCP_RANGE_START="${SUBNET}.10"
DHCP_RANGE_END="${SUBNET}.100"
DISABLE_OFFLOAD=1          # 1 = désactive offloading (optionnel)

# ===== Installer les paquets nécessaires =====
sudo apt update
sudo apt install -y hostapd dnsmasq iptables

# ===== Démasquer et activer hostapd (évite le problème "masked") =====
sudo systemctl unmask hostapd
sudo systemctl enable hostapd

# ===== Fonction pour réinitialiser l'interface Wi-Fi =====
reset_wifi_iface() {
    sudo systemctl stop hostapd
    sudo ip link set $WIFI_IFACE down
    sudo ip addr flush dev $WIFI_IFACE
    sudo ip link set $WIFI_IFACE up
    sudo ip addr add $WIFI_IP/24 dev $WIFI_IFACE
    sleep 1
    sudo systemctl start hostapd
}

# ===== Stop services avant configuration =====
sudo systemctl stop hostapd
sudo systemctl stop dnsmasq

# ===== Configurer l'IP statique pour l'interface Wi-Fi =====
sudo ip link set $WIFI_IFACE down
sudo ip addr flush dev $WIFI_IFACE
sudo ip link set $WIFI_IFACE up
sudo ip addr add $WIFI_IP/24 dev $WIFI_IFACE

# ===== Optionnel : désactiver offloading =====
if [ "$DISABLE_OFFLOAD" = "1" ]; then
    sudo ethtool -K $WIFI_IFACE gro off tso off gso off
fi

# ===== Configurer dnsmasq (DHCP) =====
sudo bash -c "cat > /etc/dnsmasq.conf" <<EOF
interface=$WIFI_IFACE
dhcp-range=$DHCP_RANGE_START,$DHCP_RANGE_END,255.255.255.0,24h
domain-needed
bogus-priv
EOF

# ===== Configurer hostapd (Wi-Fi AP 2.4GHz) =====
sudo bash -c "cat > /etc/hostapd/hostapd.conf" <<EOF
interface=$WIFI_IFACE
ssid=$SSID
hw_mode=g
channel=6
beacon_int=100
wmm_enabled=1
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

# ===== Activer IP forwarding et optimisation =====
sudo sysctl -w net.ipv4.ip_forward=1
sudo sysctl -w net.core.netdev_max_backlog=5000
sudo sysctl -w net.ipv4.tcp_congestion_control=bbr
sudo sed -i 's/#net.ipv4.ip_forward=1/net.ipv4.ip_forward=1/' /etc/sysctl.conf || true

# ===== Configurer NAT via iptables =====
sudo iptables -t nat -A POSTROUTING -o $ETH_IFACE -j MASQUERADE
sudo iptables -A FORWARD -i $WIFI_IFACE -o $ETH_IFACE -j ACCEPT
sudo iptables -A FORWARD -i $ETH_IFACE -o $WIFI_IFACE -m state --state RELATED,ESTABLISHED -j ACCEPT

# ===== Sauvegarder iptables =====
sudo sh -c "iptables-save > /etc/iptables.rules"
sudo bash -c 'cat > /etc/network/if-up.d/iptables' <<EOF
#!/bin/sh
iptables-restore < /etc/iptables.rules
EOF
sudo chmod +x /etc/network/if-up.d/iptables

# ===== Lancer dnsmasq et hostapd =====
sudo systemctl restart dnsmasq
reset_wifi_iface

# ===== Auto-restart hostapd sur crash =====
sudo mkdir -p /etc/systemd/system/hostapd.service.d
sudo bash -c 'cat > /etc/systemd/system/hostapd.service.d/override.conf' <<EOF
[Service]
Restart=always
RestartSec=2
EOF
sudo systemctl daemon-reload

echo "✅ Access Point prêt !"
echo "SSID: $SSID"
echo "Mot de passe: $WIFI_PASS"
echo "Interface Wi-Fi: $WIFI_IFACE"
echo "IP AP: $WIFI_IP"
echo "DHCP: ${DHCP_RANGE_START} → ${DHCP_RANGE_END}"
echo "Hostapd auto-restart activé. Pour reset manuel: reset_wifi_iface"
