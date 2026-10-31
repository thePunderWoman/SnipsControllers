# Snips Controller V2 — BT840XE pin table

> The wiring source is `PCB/v2.x/stackup/board.kdl`, and the module is modeled in
> `PCB/v2.x/stackup/parts.kdl`. Pin data is from Fanstel's BT840X/BT840XE Product
> Specifications Ver 1.26, in `datasheets/`. The module is on the bottom side.

The Fanstel BT840XE (nRF52840-QIAA, SKY66112 PA, u.FL) runs the firmware and the Zigbee link.
It replaces the ESP32 and XBee used on V1.x.

## Module rules
- **Power:** VDD and VDDH are both tied to 3V3_RF, the 3.3 V rail after FB_RF1. The hall sensor and stick also use 3V3_RF, so their ratiometric outputs track the ADC's VDD reference. Sample them with the SAADC's VDD/4 reference. Fanstel Note 1 requires
  a regulated 3.3 V, 300 mA minimum, on VDD; VDDH alone cannot power the PA. DCCH stays open.
- **Reserved:** P0.06, P0.08, P0.17 and P0.19 control the PA and are not connected at the pads.
  P0.00 and P0.01 go to the module's 32.768 kHz crystal.
- **Pads:** castellated pins 1-16 are the only hand-solderable ones. All others are LGA.

## Pin assignments

| Function | nRF52840 | Module pad | Notes |
|---|---|---|---|
| I2C SDA | P0.26 | 1 | OLED, IMU. R_I2C_SDA1 4.7k fitted |
| I2C SCL | P0.27 | 2 | R_I2C_SCL1 4.7k fitted |
| Stick X | P0.02 (AIN0) | 5 | Analog |
| Stick Y | P0.03 (AIN1) | 6 | Analog |
| Analog trigger | P0.04 (AIN2) | A3 | DRV5055 through R_TRIG1 and C_TRIG1, about 1.6 kHz low-pass |
| VSYS sense | P0.05 (AIN3) | A4 | Divide-by-3 from VSYS |
| Digital trigger (bumper) | P1.04 | C6 | Direct, active low, internal pull-up |
| Macro 1 | P1.10 | Z5 | Direct, active low, internal pull-up |
| Macro 2 | P0.07 | A5 | Direct, active low, internal pull-up |
| Macro 3 | P1.09 | E3 | Direct, active low, internal pull-up |
| Macro 4 | P1.11 | Z0 | Direct, active low, internal pull-up |
| Macro 5 | P1.07 | E0 | Direct, active low, internal pull-up |
| Macro 6 | P0.20 | E1 | Direct, active low, internal pull-up |
| Thumbstick click | P1.03 | B6 | Direct, active low, internal pull-up |
| Trigger down | P1.06 | Z6 | Direct, active low, internal pull-up |
| Trigger up | P1.08 | A6 | Direct, active low, internal pull-up |
| Vol down | P0.12 | E6 | Direct, active low, internal pull-up |
| Vol up | P1.05 | D6 | Direct, active low, internal pull-up |
| IMU interrupt | P1.12 | Z1 | ICM-42670-P INT1 |
| Power hold (PWR_HOLD) | P1.13 | Z2 | Must go high early in boot |
| Charger STAT1 | P1.14 | Z3 | Open drain, pulled up |
| Charger STAT2 | P1.15 | Z4 | Open drain, pulled up |
| Power button sense | P0.11 | 11 | Direct, so power-off does not depend on I2C |
| Boot button | P1.00 | 12 | R_BOOT_PU1 pull-up |
| RGB LED data | P0.13 | 13 | SK6812, needs a fast pin |
| USB D+ / D- | D+ / D- | E4 / E5 | Native USB |
| USB VBUS | VBUS | F6 | |
| Reset | P0.18 | 14 | J_SWD1 pin 3, SW_RST1 |
| SWD | SWDIO / SWDCLK | 16 / 15 | J_SWD1 pins 2 and 4. First programming |

Antenna: one u.FL on the module.

## Buttons
Every button wires straight to a module GPIO and to GND, using the internal pull-ups, so simultaneous presses need no diodes. The pins are on the outer LGA ring, matched to the nearest button.

## Addresses
OLED 0x3C, ICM-42670-P 0x68. R_I2C_SDA1 and R_I2C_SCL1 stay on the board so the IMU works with no OLED attached.

## Free pins
The module still has plenty of LGA GPIOs free. P0.09 and P0.10 are NFC pins by default. AIN4-7 (P0.28-31) are free analog inputs.
