# pixelitl peer

Hello-world ESP32 (WROOM, classic dual-core Xtensa) project. Stub for a future
ESP-NOW peer that talks to the main display (ESP32-S3) over the radio.

For now it just prints chip info, MAC addresses (the values you'll feed into
ESP-NOW peer config), and a 5 s heartbeat.

## Build & flash

```fish
source ~/esp/esp-idf/export.sh

cd peer
idf.py build flash monitor -p /dev/cu.usbserial-B002Y4LA
```

(Use whichever `/dev/cu.usbserial-*` or `/dev/cu.usbmodem*` your WROOM
enumerated as. `ls /dev/cu.*` will show.)

The first build sets the target to `esp32` (from `sdkconfig.defaults`). To
switch targets later, delete `sdkconfig` and `build/` and rebuild.

## Expected output

```
I (xxx) peer: ----- pixelitl peer (ESP32 WROOM) booting -----
I (xxx) peer: chip: esp32, 2 core(s), rev 3, features: WiFi BT BLE
I (xxx) peer: flash: 4 MB
I (xxx) peer: free heap: 295xxx bytes
I (xxx) peer: WIFI_STA MAC: AA:BB:CC:DD:EE:FF
I (xxx) peer: WIFI_AP  MAC: AA:BB:CC:DD:EE:FF
I (xxx) peer: BASE     MAC: AA:BB:CC:DD:EE:FF
I (xxx) peer: ready. heartbeat every 5s.
I (xxx) peer: heartbeat #0  uptime=5 s  free_heap=295xxx B
...
```

Write down the **WIFI_STA MAC** of each peer — that's the address ESP-NOW
will use when you wire the two boards together.

## Layout

```
peer/
├── CMakeLists.txt           # IDF top-level
├── main/
│   ├── CMakeLists.txt       # esp_timer + esp_hw_support
│   └── main.c               # hello world + MAC dump + heartbeat
├── sdkconfig.defaults       # target=esp32, FreeRTOS 1kHz tick, size-optimized
└── README.md
```

The main display project (`../`) is unchanged — different target (`esp32s3`),
different build dir, separate `idf.py` invocations.
