# Wi-Fi Weather Display

I built this because I did not want to check my phone first thing in the morning before I was ready for the day. I wanted a small display that I could glance at to see the weather and get a simple idea of what to wear.

The display shows the weather right now, tomorrow’s forecast, the current date and time, and simple clothing suggestions such as whether to bring a jacket, umbrella, sunglasses, shorts, or a scarf.

During first-time setup, each person enters their own:

- Display label
- Wi-Fi details
- Latitude
- Longitude

Those settings are saved only on that person’s individual ESP32 display.

> Before sharing a photo or screenshot, make sure it does not show your personalized display label, city, coordinates, Wi-Fi name, password, or setup page.

## What you need

- ESP32 DevKit v1
- 1.54-inch square 240×240 ST7789 color display
- Female-to-female jumper wires
- Breadboard
- USB cable for the ESP32

## Wiring

**Unplug the ESP32 from USB power before adding, moving, or removing wires.** Read the printed labels on your own parts instead of guessing.

| Display pin | ESP32 pin | What it does |
|---|---:|---|
| VCC | 3V3 | Powers the display |
| GND | GND | Ground connection |
| SCK / SCL / CLK | GPIO18 | Screen clock signal |
| MOSI / SDA / DIN | GPIO23 | Sends picture and text data to the screen |
| CS | GPIO4 | Selects the screen |
| DC | GPIO27 | Tells the screen whether it is receiving data or a command |
| RST / RES | GPIO26 | Resets the screen when it starts |
| BL / LED | 3V3 | Powers the screen backlight |

> **Important:** Connect the display’s `VCC` and `BL` pins to the ESP32 pin labelled `3V3`, not `5V`. Swapped power wires can damage the display.

## First-time setup

1. With the ESP32 unplugged, connect the display using the wiring table above.
2. Check every wire against the printed label on both the ESP32 and display.
3. Plug the ESP32 into USB power.
4. The display creates a temporary Wi-Fi network named:

   ```text
   Weather-Display-Setup
   ```

5. Connect a phone, tablet, or computer to that temporary network.
6. Open this address in a web browser:

   ```text
   192.168.4.1
   ```

7. Enter:
   - your home Wi-Fi name and password
   - a display label, such as `MY WEATHER`
   - your own latitude
   - your own longitude

8. Save the form. The display connects to Wi-Fi and loads weather information.

## Finding your coordinates

Search online for:

```text
latitude and longitude for [your city]
```

Use your own coordinates during setup. Do not add your coordinates to this public repository.

## How it works

- Weather information comes from [Open-Meteo](https://open-meteo.com/), which does not require an API key for this project.
- The display requests new weather information every **15 minutes** after a successful update.
- If a weather request fails, it retries after **15 seconds**.
- The date and time update automatically over Wi-Fi.
- The upper panel shows conditions **right now**.
- The lower panel shows **tomorrow’s forecast**.
- Clothing suggestions are based on temperature, wind, rain, and daytime conditions.

## Project files

The main program file is:

```text
src/main.cpp
```

To add the program to GitHub:

1. Create a new repository.
2. Click **Add file** → **Create new file**.
3. Name the new file:

   ```text
   src/main.cpp
   ```

4. Copy the complete current code from Schematik and paste it into that file.
5. Click **Commit new file**.

## Safety and troubleshooting

- Always unplug USB power before changing any connection.
- Make one wiring change at a time.
- Check the printed labels on the physical parts, especially `VCC`, `GND`, `CS`, `DC`, and `RST`.
- If the screen stays dark, check that both `VCC` and `BL` go to `3V3`, and that `GND` goes to `GND`.
- If the screen lights up but does not show text, check the five signal wires: GPIO18, GPIO23, GPIO4, GPIO27, and GPIO26.
- Do not publish old code, screenshots, photos, or Git history that includes a personal location, Wi-Fi name, Wi-Fi password, or coordinates.

