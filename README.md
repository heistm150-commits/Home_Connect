
┌─────────────┐     ┌─────────────┐     ┌─────────────┐
│   Client    │     │   Network   │     │   Server    │
│  ESP32      │────>│   Router    │────>│  ESP32/PC   │
└─────────────┘     └─────────────┘     └─────────────┘
       │                     │                     │
       │ 1. mDNS Query       │                     │
       │ "homeconnect._tcp?" │────────────────────>│
       │                     │                     │
       │                     │                     │
       │ 2. mDNS Response    │                     │
       │<────────────────────│<────────────────────│
       │ IP:192.168.1.100    │                     │
       │ Port:81             │                     │
       │ Path:/ws            │                     │
       │                     │                     │
       │ 3. WebSocket Connect│                     │
       │──────────────────────────────────────────>│
       │                     │                     │
       │ 4. Connection OK    │                     │
       │<──────────────────────────────────────────│

Features:
✅ Consistent 6 lights + 4 shades format for all rooms
✅ Scene support with predefined configurations
✅ State persistence in flash memory
✅ Multi-client broadcasting
✅ Custom name storage
✅ IR AC control with temperature mapping
✅ mDNS auto-discovery
✅ WiFi auto-reconnection
✅ Periodic state saving
