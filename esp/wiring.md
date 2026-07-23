# SIM800L + ESP32 Wiring Guide

## Overview
This guide covers the complete wiring for SIM800L GSM module with ESP32 microcontroller, including power regulation, serial communication, and reset circuits.

## Power Supply Requirements

**SIM800L draws significant current during transmission (peaks 2A+):**
- Idle: ~50mA
- GSM TX: 1.5-2A (spikes)
- GPRS: 0.6-1.2A

**Recommendation:** Use a dedicated 5V power supply with minimum 2A continuous rating.

## Component List

| Component | Quantity | Notes |
|-----------|----------|-------|
| ESP32 DevKit v1 | 1 | Or ESP8266 |
| SIM800L Module | 1 | With PCB antenna or external antenna |
| Buck Converter 5V→3.3V | 1 | ≥2A capacity (AMS1117 or similar) |
| Capacitors 1000µF 16V | 2 | Input/output filtering |
| Capacitors 10µF 16V | 2 | Decoupling |
| Capacitor 100nF | 4 | Bypass/high-freq filtering |
| Resistor 10kΩ | 1 | Pull-up on RST line |
| Capacitor 10µF | 1 | RC filter on RST |
| SIM Card Holder | 1 | Micro-SIM or full SIM adapter |
| USB Cable | 1 | For ESP32 programming |
| Antenna | 1 | Optional GSM antenna (if not built-in) |

## Schematic Layout

```
┌─────────────────────────────────────────────────────────┐
│                    5V Power Supply                      │
│                     (2A+ rating)                        │
└──────────────────┬─────────────────────────────────────┘
                   │
                   ├─────[1000µF Cap]─────┐
                   │                       │
              ┌────┴───[AMS1117]───────┐   │
              │    Buck Converter      │   │
              │    5V → 3.3V (2A)      │   │
              └────┬──────────────────┬┘   │
                   │                  │    │
              [10µF Cap]          [10µF Cap]
                   │                  │
                   └──────┬───────────┘
                          │
              3.3V Rail (SIM800L + ESP32)
                          │
         ┌────────────────┼────────────────┐
         │                │                │
    ┌────┴────┐      ┌────┴─────┐    ┌───┴────┐
    │  ESP32  │      │ SIM800L  │    │SIM Card│
    │         │      │ Module   │    │Holder  │
    │ Pin 16  │──────→ TX       │    └────────┘
    │ (RX2)   │      │          │
    │         │      │ RX  ←────┤
    │ Pin 17  │──────→          │
    │ (TX2)   │      │          │
    │         │      │ RST ←────┤──[10kΩ Pull-up]
    │ Pin 5   │──────→          │
    │ (GPIO5) │      │          │
    │         │      │ GND  ────┤
    │ GND     │──────┘          │
    │         │      │ VCC  ────┤
    │ 3.3V    │──────┘          │
    └─────────┘      └──────────┘

Optional: External GSM Antenna on SIM800L ANT pin
```

## Pin Configuration (ESP32)

| ESP32 Pin | Function | SIM800L Pin | Notes |
|-----------|----------|-------------|-------|
| Pin 16 (GPIO16) | UART2 RX | TX | Serial receive from SIM800L |
| Pin 17 (GPIO17) | UART2 TX | RX | Serial transmit to SIM800L |
| Pin 5 (GPIO5) | Reset Control | RST | Hardware reset (active high) |
| GND | Ground | GND | Common ground (critical!) |
| 3.3V (via regulator) | Power | VCC | 3.3V regulated power |

## Detailed Connections

### 1. Power Regulation

```
5V PSU → Capacitor (1000µF, +pole) → AMS1117 Input
                    ↓
              AMS1117 Module
              │         │
        Input: 5V    Output: 3.3V
              │         │
       GND ──┴─────────┴── GND
```

**Notes:**
- Use thick wires (AWG 18 or thicker) for 5V → Regulator
- Place 1000µF capacitor close to AMS1117 input
- Place 10µF capacitors on both input and output
- Add 100nF ceramic capacitors for high-frequency noise

### 2. Serial Communication (UART2)

```
ESP32 Pin 16 (RX2) ──[10kΩ pull-up]──→ SIM800L TX
                              │
                              ├→ 3.3V (weak pull-up)
                              
ESP32 Pin 17 (TX2) ←─────────────────── SIM800L RX
```

**Notes:**
- SIM800L outputs 0-3.3V (compatible with ESP32 3.3V inputs)
- ESP32 TX (3.3V) is safe for SIM800L RX (which expects 2.8-3.3V)
- Pull-up on TX helps with level shifting if needed

### 3. Reset Control

```
ESP32 Pin 5 (GPIO5)
         │
         ├──[10kΩ resistor]──→ 3.3V (weak pull-up)
         │
    [RC Filter]
         │
      [10µF cap]
         │
        GND
         │
    SIM800L RST pin
```

**Notes:**
- Keep RST pulled high (default)
- Pulling LOW briefly resets the module
- RC time constant helps debounce

### 4. Ground Plane

```
Multiple Ground Connections:
- ESP32 GND → SIM800L GND
- PSU GND → Regulator GND → SIM800L GND → ESP32 GND
- Use thick wires or busbar for GND distribution
- Critical: Common ground between all components!
```

## Pin Configuration (ESP8266)

If using ESP8266 instead of ESP32:

| ESP8266 Pin | Function | SIM800L Pin | Notes |
|-------------|----------|-------------|-------|
| GPIO12 (D6) | UART1 RX | TX | Serial receive |
| GPIO13 (D7) | UART1 TX | RX | Serial transmit |
| GPIO14 (D5) | Reset | RST | Hardware reset |
| GND | Ground | GND | Common ground |
| 3.3V | Power | VCC | Regulated 3.3V |

**Note:** ESP8266 has only one UART available (UART0 is used for serial console).

## Assembly Instructions

### Step 1: Prepare Breadboard/PCB
1. Place ESP32 on one side of breadboard
2. Place SIM800L module on opposite side
3. Place AMS1117 regulator between power entry and modules

### Step 2: Power Circuit
1. Connect 5V PSU positive through 1000µF capacitor to AMS1117 input
2. Connect AMS1117 output (3.3V) to breadboard power rail
3. Add 10µF capacitor on AMS1117 output
4. Connect all GND together (PSU, regulator, ESP32, SIM800L)
5. Add 100nF capacitors near VCC pins of both modules

### Step 3: Serial Communication
1. Connect ESP32 Pin 16 → SIM800L TX (with 10kΩ pull-up to 3.3V)
2. Connect ESP32 Pin 17 → SIM800L RX
3. Connect GND between both modules

### Step 4: Reset Circuit
1. Connect 10kΩ resistor from SIM800L RST to 3.3V
2. Connect 10µF capacitor from RST to GND
3. Connect ESP32 Pin 5 to RST pin (with 10kΩ pull-up option)

### Step 5: SIM Card
1. Insert SIM card into SIM800L module (check orientation)
2. Ensure card is locked in place
3. Verify contacts are clean

### Step 6: Antenna
1. If external antenna: connect to SIM800L ANT pin
2. If built-in antenna: ensure nothing is blocking it

## Testing

### Power Check
```
Measure voltage between GND and 3.3V rail: should be 3.3V ±0.1V
```

### Serial Communication Test
```bash
# After flashing firmware, run:
platformio device monitor --baud 115200

# You should see:
# [SIM800L] Testing connection...
# [SIM800L] Connected!
# AT
# OK
```

### SIM Card Detection
```
Watch serial output for:
[SIM800L] Registered on network
Signal strength: XX
```

## Troubleshooting

| Issue | Cause | Solution |
|-------|-------|----------|
| SIM800L not responding | No power | Check voltage: 3.3V ± 0.3V at VCC |
| | Incorrect wiring | Verify TX/RX/GND connections |
| | Dead SIM800L | Test with external power, check for shorts |
| Intermittent crashes | Low power supply | Upgrade PSU to 2A+ capacity |
| | Capacitor values wrong | Check: 1000µF input, 10µF output |
| | Ground loop | Use single GND point, avoid multiple returns |
| Serial garbage | Baud rate mismatch | SIM800L uses 9600 baud (hardcoded) |
| | Voltage levels | Check for 3.3V logic levels |
| RST not working | Pull-up wrong | Verify 10kΩ to 3.3V |
| | GPIO5 not connected | Check ESP32 pin assignments |

## Production Considerations

### EMI/RFI Shielding
- Place GSM module away from high-frequency circuits
- Use shielded enclosure if operating near WiFi/2.4GHz devices
- Keep antenna away from metal surfaces

### Thermal Management
- SIM800L can draw up to 2A during transmission
- Ensure adequate ventilation
- Add heatsink if module is in enclosed box

### Reliability
- Use quality connectors (Dupont, XT60)
- Solder connections if permanent installation
- Add inline fuse near PSU (2A recommended)
- Consider capacitor bank for transient protection

### Cable Routing
- Keep UART wires short (<30cm) and twisted
- Route away from high-current paths
- Use ferrite cores if interference issues
- Keep antenna cable separate from power lines

## References

- [SIM800L Datasheet](http://www.simcomm2m.com/EnProduct/product_id/SIM800L.html)
- [ESP32 Pinout](https://en.wikipedia.org/wiki/File:Pinout_of_ESPRESSIF_ESP-32.svg)
- [AMS1117 Datasheet](https://www.alldatasheet.com/datasheet-pdf/pdf/1132113/ADVANCED-MONOLITHIC-AMS/AMS1117-3.3.html)
