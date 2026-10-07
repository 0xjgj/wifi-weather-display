#include <Arduino.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <time.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>

struct Weather {
  float temperature;
  float feelsLike;
  float precipitation;
  float wind;
  int code;
  bool daytime;
  String localTime;
};

struct TomorrowForecast {
  float high;
  float low;
  float wind;
  int rainChance;
  int code;
};

constexpr int TFT_SCK = 18;
constexpr int TFT_MOSI = 23;
constexpr int TFT_CS = 4;
constexpr int TFT_DC = 27;
constexpr int TFT_RST = 26;
constexpr unsigned long WEATHER_INTERVAL_MS = 15UL * 60UL * 1000UL;
constexpr unsigned long WEATHER_RETRY_MS = 15UL * 1000UL;

Preferences preferences;
String displayLocation;
String latitude;
String longitude;

Adafruit_ST7789 display(TFT_CS, TFT_DC, TFT_RST);
unsigned long lastWeatherRequest = 0;
String lastClockText;

String weatherUrl() {
  return "https://api.open-meteo.com/v1/forecast?latitude=" + latitude +
         "&longitude=" + longitude +
         "&current=temperature_2m,apparent_temperature,precipitation,weather_code,wind_speed_10m,is_day"
         "&daily=temperature_2m_max,temperature_2m_min,precipitation_probability_max,weather_code,wind_speed_10m_max"
         "&forecast_days=2&temperature_unit=fahrenheit&wind_speed_unit=mph&timezone=auto";
}

const uint16_t BLACK = ST77XX_BLACK;
const uint16_t WHITE = ST77XX_WHITE;
const uint16_t CYAN = ST77XX_CYAN;
const uint16_t ORANGE = 0xFD20;
const uint16_t GREEN = ST77XX_GREEN;
const uint16_t RED = ST77XX_RED;
const uint16_t GREY = 0x8410;

void drawCentered(const String &text, int y, uint8_t size, uint16_t color) {
  display.setTextSize(size);
  display.setTextColor(color);
  int16_t x1, y1;
  uint16_t w, h;
  display.getTextBounds(text, 0, y, &x1, &y1, &w, &h);
  display.setCursor((240 - w) / 2, y);
  display.print(text);
}

const char *conditionName(int code) {
  if (code == 0) return "CLEAR";
  if (code <= 3) return "CLOUDY";
  if (code <= 48) return "FOG";
  if (code <= 57) return "DRIZZLE";
  if (code <= 67) return "RAIN";
  if (code <= 77) return "SNOW";
  if (code <= 82) return "SHOWERS";
  if (code <= 86) return "SNOW";
  return "STORM";
}

bool rainLikely(const Weather &weather) {
  return weather.precipitation > 0.01f ||
         (weather.code >= 51 && weather.code <= 82) || weather.code >= 95;
}

bool rainLikelyTomorrow(const TomorrowForecast &forecast) {
  return forecast.rainChance >= 30 ||
         (forecast.code >= 51 && forecast.code <= 82) || forecast.code >= 95;
}

String formatDateLine(const String &isoTime) {
  if (isoTime.length() < 16) return "DATE UNAVAILABLE";
  const char *months[] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};
  const char *weekdays[] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};
  const int year = isoTime.substring(0, 4).toInt();
  const int month = isoTime.substring(5, 7).toInt();
  const int day = isoTime.substring(8, 10).toInt();
  if (year < 2000 || month < 1 || month > 12 || day < 1 || day > 31) return "DATE UNAVAILABLE";
  const int offsets[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
  int adjustedYear = year;
  if (month < 3) adjustedYear--;
  const int weekday = (adjustedYear + adjustedYear / 4 - adjustedYear / 100 + adjustedYear / 400 + offsets[month - 1] + day) % 7;
  return String(weekdays[weekday]) + ", " + months[month - 1] + " " + String(day) + ", " + String(year);
}

String formatTimeLine(const String &isoTime) {
  if (isoTime.length() < 16) return "TIME UNAVAILABLE";
  int hour = isoTime.substring(11, 13).toInt();
  const String minute = isoTime.substring(14, 16);
  const char *suffix = hour >= 12 ? "PM" : "AM";
  hour %= 12;
  if (hour == 0) hour = 12;
  return String(hour) + ":" + minute + suffix;
}

uint8_t compactGlyphRow(char c, uint8_t row) {
  const uint8_t *glyph = nullptr;
  static const uint8_t digits[][5] = {
    {7, 5, 5, 5, 7}, {2, 6, 2, 2, 7}, {7, 1, 7, 4, 7}, {7, 1, 7, 1, 7}, {5, 5, 7, 1, 1},
    {7, 4, 7, 1, 7}, {7, 4, 7, 5, 7}, {7, 1, 2, 2, 2}, {7, 5, 7, 5, 7}, {7, 5, 7, 1, 7}
  };
  static const uint8_t letters[][5] = {
    {2, 5, 7, 5, 5}, {6, 5, 6, 5, 6}, {3, 4, 4, 4, 3}, {6, 5, 5, 5, 6}, {7, 4, 6, 4, 7},
    {7, 4, 6, 4, 4}, {3, 4, 5, 5, 3}, {5, 5, 7, 5, 5}, {7, 2, 2, 2, 7}, {1, 1, 1, 5, 2},
    {5, 5, 6, 5, 5}, {4, 4, 4, 4, 7}, {5, 7, 7, 5, 5}, {5, 7, 7, 7, 5}, {2, 5, 5, 5, 2},
    {6, 5, 6, 4, 4}, {2, 5, 5, 7, 3}, {6, 5, 6, 5, 5}, {3, 4, 2, 1, 6}, {7, 2, 2, 2, 2},
    {5, 5, 5, 5, 7}, {5, 5, 5, 5, 2}, {5, 5, 7, 7, 5}, {5, 5, 2, 5, 5}, {5, 5, 2, 2, 2}, {7, 1, 2, 4, 7}
  };
  if (c >= '0' && c <= '9') glyph = digits[c - '0'];
  else if (c >= 'A' && c <= 'Z') glyph = letters[c - 'A'];
  else if (c == '-') { static const uint8_t dash[] = {0, 0, 7, 0, 0}; glyph = dash; }
  else if (c == ',') { static const uint8_t comma[] = {0, 0, 0, 2, 4}; glyph = comma; }
  else if (c == ':') { static const uint8_t colon[] = {0, 2, 0, 2, 0}; glyph = colon; }
  return glyph ? glyph[row] : 0;
}

void drawCompactHeader(const String &text) {
  const int characterWidth = 4;
  const int startX = (240 - text.length() * characterWidth) / 2;
  for (unsigned int i = 0; i < text.length(); i++) {
    const char c = toupper(text[i]);
    for (uint8_t row = 0; row < 5; row++) {
      const uint8_t bits = compactGlyphRow(c, row);
      for (uint8_t column = 0; column < 3; column++) {
        if (bits & (1 << (2 - column))) display.drawPixel(startX + i * characterWidth + column, 8 + row, CYAN);
      }
    }
  }
}

void drawHeader(const String &isoTime) {
  display.fillRect(0, 0, 240, 44, BLACK);
  drawCentered(displayLocation, 6, 2, CYAN);
  drawCentered(formatDateLine(isoTime) + " - " + formatTimeLine(isoTime), 25, 1, WHITE);
  display.drawFastHLine(8, 43, 224, GREY);
}

String networkLocalTime() {
  struct tm timeInfo;
  if (!getLocalTime(&timeInfo, 10)) return "";
  char timestamp[17];
  strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M", &timeInfo);
  return String(timestamp);
}

void refreshClockHeader() {
  const String now = networkLocalTime();
  if (now.length() == 16 && now != lastClockText) {
    drawHeader(now);
    lastClockText = now;
  }
}

String clothingAdvice(float feelsLike, float wind, bool wet, bool daytime, int code) {
  String advice;
  if (feelsLike < 42.0f) advice += "SCARF ";
  if (feelsLike < 58.0f || wind > 20.0f) advice += "JACKET ";
  if (wet) advice += "UMBRELLA ";
  if (daytime && !wet && code <= 3) advice += "SUNGLASSES ";
  if (feelsLike >= 76.0f && !wet) advice += "SHORTS ";
  if (advice.length() == 0) advice = "COMFORTABLE LAYERS";
  advice.trim();
  return advice;
}

void drawWeather(const Weather &weather, const TomorrowForecast &tomorrow) {
  const bool wetToday = rainLikely(weather);
  const bool wetTomorrow = rainLikelyTomorrow(tomorrow);
  const String todayAdvice = clothingAdvice(weather.feelsLike, weather.wind, wetToday, weather.daytime, weather.code);
  const String tomorrowAdvice = clothingAdvice((tomorrow.high + tomorrow.low) / 2.0f, tomorrow.wind, wetTomorrow, true, tomorrow.code);

  // Keep the heading compact so the two forecast panels receive equal space.
  display.fillScreen(BLACK);
  drawHeader(weather.localTime);
  lastClockText = weather.localTime;

  display.fillRect(4, 50, 232, 86, 0x0841);
  drawCentered("RIGHT NOW", 56, 1, ORANGE);
  drawCentered(String((int)roundf(weather.temperature)) + "F  " + conditionName(weather.code), 76, 2, WHITE);
  drawCentered("WEAR: " + todayAdvice, 112, 1, GREEN);

  display.fillRect(4, 145, 232, 86, 0x1020);
  drawCentered("TOMORROW", 151, 1, ORANGE);
  drawCentered("HI " + String((int)roundf(tomorrow.high)) + "F  LO " + String((int)roundf(tomorrow.low)) + "F", 171, 2, WHITE);
  drawCentered(String(conditionName(tomorrow.code)) + "  RAIN " + String(tomorrow.rainChance) + "%", 198, 1, CYAN);
  drawCentered("WEAR: " + tomorrowAdvice, 216, 1, GREEN);
}

void drawStatus(const char *line1, const char *line2 = "") {
  display.fillScreen(BLACK);
  drawCentered("WEAR TODAY", 78, 2, ORANGE);
  drawCentered(line1, 112, 1, WHITE);
  if (line2[0] != '\0') drawCentered(line2, 130, 1, GREY);
}

bool fetchWeather(Weather &weather, TomorrowForecast &tomorrow) {
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  if (!http.begin(client, weatherUrl())) {
    Serial.println("HTTP begin failed");
    return false;
  }
  const int httpCode = http.GET();
  if (httpCode != HTTP_CODE_OK) {
    http.end();
    return false;
  }

  // getString() collects the complete HTTP response before JSON parsing.
  const String response = http.getString();
  http.end();
  JsonDocument document;
  const DeserializationError error = deserializeJson(document, response);
  if (error) return false;

  JsonObject current = document["current"];
  if (current.isNull()) return false;
  weather.temperature = current["temperature_2m"] | 0.0f;
  weather.feelsLike = current["apparent_temperature"] | weather.temperature;
  weather.precipitation = current["precipitation"] | 0.0f;
  weather.wind = current["wind_speed_10m"] | 0.0f;
  weather.code = current["weather_code"] | 0;
  weather.daytime = (current["is_day"] | 1) == 1;
  weather.localTime = current["time"] | "";

  JsonObject daily = document["daily"];
  if (daily.isNull()) return false;
  tomorrow.high = daily["temperature_2m_max"][1] | weather.temperature;
  tomorrow.low = daily["temperature_2m_min"][1] | weather.temperature;
  tomorrow.rainChance = daily["precipitation_probability_max"][1] | 0;
  tomorrow.code = daily["weather_code"][1] | 0;
  tomorrow.wind = daily["wind_speed_10m_max"][1] | 0.0f;
  return true;
}

void setup() {
  Serial.begin(115200);
  SPI.begin(TFT_SCK, -1, TFT_MOSI, TFT_CS);
  display.init(240, 240, SPI_MODE0);
  display.setRotation(0);
  drawStatus("CONNECTING TO WIFI");

  preferences.begin("weather", false);
  displayLocation = preferences.getString("label", "MY WEATHER");
  latitude = preferences.getString("latitude", "");
  longitude = preferences.getString("longitude", "");

  WiFiManager wifiManager;
  WiFiManagerParameter locationParameter("label", "Display label (for example: MY WEATHER)", displayLocation.c_str(), 20);
  WiFiManagerParameter latitudeParameter("latitude", "Latitude", latitude.c_str(), 16);
  WiFiManagerParameter longitudeParameter("longitude", "Longitude", longitude.c_str(), 16);
  wifiManager.addParameter(&locationParameter);
  wifiManager.addParameter(&latitudeParameter);
  wifiManager.addParameter(&longitudeParameter);
  wifiManager.setConfigPortalTimeout(180);
  if (!wifiManager.autoConnect("Weather-Display-Setup", "weatherdisplay")) {
    drawStatus("WIFI NOT CONNECTED", "Restart to try again");
    delay(3000);
    ESP.restart();
  }

  displayLocation = String(locationParameter.getValue());
  latitude = String(latitudeParameter.getValue());
  longitude = String(longitudeParameter.getValue());
  displayLocation.trim();
  latitude.trim();
  longitude.trim();
  if (displayLocation.length() == 0) displayLocation = "MY WEATHER";
  preferences.putString("label", displayLocation);
  preferences.putString("latitude", latitude);
  preferences.putString("longitude", longitude);

  configTzTime("UTC0", "time.google.com", "pool.ntp.org");
  drawStatus("GETTING WEATHER");
  Weather weather;
  TomorrowForecast tomorrow;
  if (fetchWeather(weather, tomorrow)) {
    drawWeather(weather, tomorrow);
    lastWeatherRequest = millis();
  } else {
    drawStatus("WEATHER UNAVAILABLE", "Retrying in 15 seconds");
    lastWeatherRequest = millis() - WEATHER_INTERVAL_MS + WEATHER_RETRY_MS;
  }
}

void loop() {
  refreshClockHeader();
  if (millis() - lastWeatherRequest < WEATHER_INTERVAL_MS) return;
  lastWeatherRequest = millis();
  Weather weather;
  TomorrowForecast tomorrow;
  if (fetchWeather(weather, tomorrow)) {
    drawWeather(weather, tomorrow);
  } else {
    drawStatus("WEATHER UNAVAILABLE", "Retrying in 15 seconds");
    lastWeatherRequest = millis() - WEATHER_INTERVAL_MS + WEATHER_RETRY_MS;
  }
}