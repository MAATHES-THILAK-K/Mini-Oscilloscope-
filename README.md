# **Mini Oscilloscope – Arduino + SH1106 OLED**

A compact oscilloscope built using Arduino, an SH1106 OLED module, and a simple analog front-end.
Includes auto-range, trigger detection, waveform plotting, and frequency/duty-cycle measurement.

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

* Arduino (ATmega328P)
* SH1106 128×64 I²C OLED
* Input attenuator (1× / 10×)
* Buttons: Select / Up / Down / Hold

---

## 🧪 **How It Works**

* Samples analog input using ADC with variable prescaler
* Computes minimum, maximum, and average voltage
* Detects trigger events to stabilize waveform display
* Calculates frequency and duty cycle using edge-cross detection
* Renders waveform using the **Adafruit GFX** library

---

## 🖼 **Photos / Screenshots**

*Add your project images inside the `images/` folder.*

---

## 📁 **Code Structure**

```
MiniOscilloscope/
├── src/
│   └── MiniOscilloscope.ino
├── README.md
├── LICENSE
└── images/
```

---

