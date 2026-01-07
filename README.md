# **Mini Oscilloscope – Arduino + SH1106 OLED**

A compact oscilloscope built using Arduino, an SH1106 OLED module, and a simple analog front-end.
Includes auto-range, trigger detection, waveform plotting, and frequency/duty-cycle measurement.

> **Reference Project Used:**
> This build was inspired by the project on Instructables:
> *“Build Your Own Arduino Nano Based DIY Oscilloscope”*
> *https://www.instructables.com/Build-Your-Own-Arduino-Nano-Based-DIY-Oscilloscope/*

---

## 🔧 **Features**

* SH1106 128×64 OLED display
* Auto/Manual voltage ranges
* Time-base options from **200 ms/div** to **200 µs/div**
* Trigger detection (rising & falling edge)
* Frequency and duty-cycle calculation
* Average voltage measurement
* EEPROM settings storage
* Battery measurement mode

---

## 📦 **Hardware**

### **Core Components**

* Arduino Nano (ATmega328P)
* SH1106 128×64 I²C OLED display
* Tactile push buttons (4×: SELECT, UP, DOWN, HOLD)
* Voltage divider resistors
* Schottky diode
* 104 capacitor
* Breadboard or custom PCB
* Jumper wires
* 5V regulated power supply

### **Additional Interface Components**

* Input attenuator (1× / 10×)
* User control buttons.

---

## 🧪 **How It Works**

* Samples analog input using ADC with a variable prescaler
* Computes minimum, maximum, and average voltage
* Detects trigger events to stabilize waveform display
* Calculates frequency and duty cycle using edge-cross detection
* Renders waveform using the **Adafruit GFX** library

---

## 🖼 **Photos / Screenshots**

Add images inside the `Images/` folder once your hardware build is ready.

---

## 📁 **Code Structure**

```
MiniOscilloscope/
├── src/
│   └── miniOscilloscope.ino
├── README.md
├── LICENSE
└── Images/
```

---
