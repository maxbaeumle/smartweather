#ifndef common_h
#define common_h

#include <pebble.h>

#define RECONNECT_KEY 0
#define REQUEST_CURRENT_WEATHER_KEY 32
#define CURRENT_WEATHER_RESPONSE_KEY 33
#define REQUEST_WEATHER_FORECAST_KEY 34
#define WEATHER_FORECAST_RESPONSE_KEY 35

typedef struct {
  int16_t icon;
  int16_t temperature;
  char city[21];
  bool metric_units;

  time_t sunrise;
  time_t sunset;
  int8_t humidity;        // %
  int16_t pressure;       // hPa
  int16_t wind_speed;     // kph
  char wind_direction[4]; // N, NNE, NE, ...
} CurrentWeather;

typedef struct {
  int16_t icon[10];
  int16_t temperature_min[10];
  int16_t temperature_max[10];
  char city[21];
  bool metric_units;
} WeatherForecast;

#endif
