# 🌞 Smart Solar Panel Tracking and Energy Monitoring System

**Akdeniz University — CSE 328 Internet of Things — Term Project**  
**Student:** Eda Dana | **No:** 20210808072  
**GitHub:** https://github.com/dnaeda2507/IOT-Smart-Solar-PanelTracking-and-Energy-Monitoring-System

---

## 📋 Project Summary

This project solves the inefficiency problem of fixed-angle solar panels.
Conventional panels cannot follow the sun's movement throughout the day, causing significant energy loss. Two LDR sensors continuously detect the direction of sunlight and rotate a servo motor to keep the panel always facing the sun. An INA226 sensor measures the panel's real-time voltage, current, and power output. A BH1750 light intensity sensor measures ambient lux and compares expected power output with actual power output to calculate system efficiency. Under low-light conditions such as night or heavy cloud cover, the system automatically moves the panel to a safe park position and opens the relay to cut the circuit. A LED connected through the relay acts as the load, allowing real current measurement through the INA226. All data is published to Arduino Cloud and monitored live on a dashboard. Users can remotely switch between AUTO and MANUAL modes and adjust the panel angle via a dashboard slider.

---

## 🔧 Components List

| Component | Model | Role |
|---|---|---|
| Microcontroller | ESP32 DevKit V1 (30-pin) | Main controller, Wi-Fi, Cloud |
| Light direction sensor | LDR × 2 | Detects sunlight direction (left/right) |
| Light intensity sensor | BH1750 | Measures ambient lux value |
| Energy sensor | INA226 | Measures voltage, current, power |
| Display | OLED SSD1306 0.96" 128×64 | Live data display |
| Actuator 1 | SG90 Servo Motor | Rotates panel toward sunlight |
| Actuator 2 | 2-Channel Relay Module | Controls panel power circuit |
| Load | LED | Load for current measurement via relay |
| Power | Breadboard Power Module | 3.3V (sensors) + 5V (servo, relay) |

---

## 🔌 Wiring Table

### I2C Bus (Shared — SDA: GPIO21, SCL: GPIO22)

| Component | Pin | ESP32 GPIO | Power |
|---|---|---|---|
| OLED SSD1306 | SDA | GPIO 21 | 3.3V |
| OLED SSD1306 | SCL | GPIO 22 | 3.3V |
| BH1750 | SDA | GPIO 21 | 3.3V |
| BH1750 | SCL | GPIO 22 | 3.3V |
| INA226 | SDA | GPIO 21 | 3.3V |
| INA226 | SCL | GPIO 22 | 3.3V |

### Sensors

| Component | Pin | ESP32 GPIO | Notes |
|---|---|---|---|
| LDR Left | AO | GPIO 34 | Voltage divider with 10kΩ resistor |
| LDR Right | AO | GPIO 35 | Voltage divider with 10kΩ resistor |

### Actuators

| Component | Pin | ESP32 GPIO | Power |
|---|---|---|---|
| SG90 Servo | Signal | GPIO 18 | 5V (breadboard power module) |
| Relay Module | IN1 | GPIO 26 | 5V |
| Relay Module | IN2 | GPIO 27 | 5V |

### INA226 Power Measurement Circuit

```
Solar Panel (+) → INA226 IN+  (large hole)
INA226 IN-      → Relay COM
Relay NO        → LED (+) anode
LED (-)cathode  → Solar Panel (-) / GND
INA226 VBUS     → INA226 IN+  (bridged for voltage measurement)
```

> The INA226 module has a built-in R100 (0.1Ω) shunt resistor between IN+ and IN-.  
> The LED acts as the load to allow current to flow through the measurement circuit.  
> The relay switches this load on/off based on system mode.

### Power Distribution (Breadboard Power Module)

| Rail | Voltage | Connected Components |
|---|---|---|
| Left (+) rail | 3.3V | OLED, BH1750, INA226 VCC, LDR × 2 |
| Right (+) rail | 5V | Servo VCC, Relay VCC |
| Both (−) rails | GND | All components + ESP32 GND (rails bridged) |

---

## ☁️ Cloud Setup

**Platform:** Arduino Cloud (arduino.cc/cloud)  
**Device:** SolarESP32 — DOIT ESP32 DEVKIT V1

### Thing: `SolarTracker`

| Variable | Type | Direction | Description |
|---|---|---|---|
| `deviceMode` | String | Read/Write | AUTO / MANUAL / NIGHT |
| `lux` | float | Read Only | Light intensity (lx) |
| `voltage` | float | Read Only | Panel voltage (V) |
| `current` | float | Read Only | Panel current (A) |
| `power` | float | Read Only | Panel power (W) |
| `efficiency` | float | Read Only | System efficiency (%) |
| `panelAngle` | int | Read Only | Current servo angle (°) |
| `manuelAngle` | int | Read/Write | Manual angle from dashboard slider |
| `relayState` | bool | Read Only | Relay status |

### Dashboard: `Solar Panel Monitor`

| Widget | Type | Variable | Notes |
|---|---|---|---|
| Relay Status | Switch | `relayState` | Shows relay state |
| Setting Angle | Slider (0–180) | `manuelAngle` | Manual angle control |
| Panel Angle | Value | `panelAngle` | Current angle display |
| Device Mode | Value Selector | `deviceMode` | AUTO / MANUAL selection |
| Light Intensity | Gauge (0–100000) | `lux` | Live lux gauge |
| Efficiency | Percentage | `efficiency` | Live efficiency % |
| Lux Graph | Chart (spline) | `lux` | Historical lux trend |
| Voltage & Current | Advanced Chart | `voltage` + `current` | Both on same graph |
| Power Graph | Chart (spline) | `power` | Historical power trend |

---

## 🚀 How to Run

### 1. Required Libraries (Arduino IDE → Library Manager)

| Library | Author |
|---|---|
| Adafruit SSD1306 | Adafruit |
| Adafruit GFX Library | Adafruit |
| BH1750 | Christopher Laws |
| INA226_WE | Wolfgang Ewald |
| ESP32Servo | Kevin Harrington |
| ArduinoIoTCloud | Arduino |
| Arduino_ConnectionHandler | Arduino |

### 2. Board Settings (Arduino IDE)

- Board: **ESP32 Dev Module**
- Upload Speed: **115200**
- Flash Size: **4MB**
- Partition Scheme: **Default**

### 3. Arduino Cloud Configuration

1. Go to [arduino.cc/cloud](https://arduino.cc/cloud)
2. Create a **Device** → 3rd Party → ESP32 → DOIT ESP32 DEVKIT V1
3. Create a **Thing** named `SolarTracker` and link the device
4. Add all variables listed in the table above
5. Enter your **Wi-Fi SSID and password** in Network settings
6. Open the **Sketch** tab — `thingProperties.h` is auto-generated
7. Paste `SolarTracker.ino` into the sketch editor
8. Click **Upload**

---

## ⚙️ How It Works

### Data Upload

Every 500ms the ESP32 reads all sensors and publishes values to Arduino Cloud via MQTT over Wi-Fi. The BH1750 returns lux values over I2C. The INA226 measures bus voltage via the VBUS pin (bridged to IN+) and calculates current from the shunt voltage across the built-in 0.1Ω R100 resistor (`I = V_shunt / R_shunt`). Power is calculated as `P = V × I`. Efficiency compares actual power with expected output based on lux:

```
efficiency = (actual_power / ((lux / 1000) × 0.9W)) × 100
```

### AUTO Mode

Both LDR sensors are read via ADC (GPIO34, GPIO35). If the difference between left and right exceeds the threshold (150), the servo rotates 5° toward the brighter side. If both sensors are saturated (≥4090), the panel holds position. Relay 1 closes (LOW) — current flows through LED load and INA226 measures it.

### MANUAL Mode

Activated by selecting MANUAL on the dashboard. The servo moves to the angle set by the dashboard slider (`manuelAngle`, 0–180°). The system stays in MANUAL until the user selects AUTO.

### NIGHT Mode

When lux drops below 8.0 lx, the system automatically enters NIGHT mode. The servo returns to park position (90°) and Relay 1 opens (HIGH), cutting power to the LED load. This protects the system in dark conditions.

### Dashboard Control Logic

| Action | Result |
|---|---|
| Select MANUAL | ESP32 enters manual mode, slider controls servo |
| Adjust slider | Servo moves to selected angle in real time |
| Select AUTO | ESP32 resumes sun tracking via LDRs |
| Lux < 8 | System auto-enters NIGHT mode regardless of dashboard |

---

## 📸 Evidence

### Dashboard Screenshot
> *(Add your dashboard screenshot image here)*
> Example: `![Dashboard](images/dashboard.png)`

### System Photo
> *(Add a photo of your assembled hardware here)*
> Example: `![Hardware](images/hardware.jpg)`

### Demo Video
> *(Add your demo video link here)*
> Example: [Demo Video](https://youtube.com/your-link)

---

## 📁 Repository Structure

```
├── SolarTracker.ino        # Main Arduino sketch
├── thingProperties.h       # Auto-generated by Arduino Cloud
├── images/                 # Screenshots and photos
│   ├── dashboard.png
│   └── hardware.jpg
└── README.md               # Final Report
```
