#include "common.h"

static Window *window;

static TextLayer *temperature_layer;
static char *temperature;
static TextLayer *city_layer;
static BitmapLayer *icon_layer;
static GBitmap *icon_bitmap = NULL;

static CurrentWeather current_weather;
static WeatherForecast weather_forecast;

static uint32_t WEATHER_ICONS[] = {
  RESOURCE_ID_IMAGE_SUN,
  RESOURCE_ID_IMAGE_CLOUD,
  RESOURCE_ID_IMAGE_RAIN,
  RESOURCE_ID_IMAGE_SNOW
};

// "Do not send requests more then 1 time per 10 minutes from one device." (http://openweathermap.org/appid#work)
static void send_cmd() {
  DictionaryIterator *iter;
  app_message_outbox_begin(&iter);

  if (!iter) {
    return;
  }

  dict_write_uint8(iter, REQUEST_CURRENT_WEATHER_KEY, 1);
  app_message_outbox_send();
}

// http://openweathermap.org/weather-conditions
static int get_icon_from_weather_id(int weather_id) {
  if (weather_id < 600) {
    return 2;
  } else if (weather_id < 700) {
    return 3;
  } else if (weather_id > 800) {
    return 1;
  } else {
    return 0;
  }
}

static void received_message(DictionaryIterator *received, void *context) {
  Tuple *tuple = dict_find(received, RECONNECT_KEY);

  if (tuple) {
    send_cmd();
  } else {
    Tuple *tuple = dict_find(received, CURRENT_WEATHER_RESPONSE_KEY);

    if (tuple) {
      uint8_t count = tuple->value->data[0];

      if (count > 0) {
        memcpy(&current_weather, &tuple->value->data[1], sizeof(CurrentWeather));
      } else { // Location disabled
        text_layer_set_text(temperature_layer, "N/A");
        text_layer_set_text(city_layer, "Enable Location");
        return;
      }

      if (current_weather.icon == -1) {
        text_layer_set_text(city_layer, "Request Failed");
        return;
      }

      if (icon_bitmap) {
        gbitmap_destroy(icon_bitmap);
      }

      icon_bitmap = gbitmap_create_with_resource(WEATHER_ICONS[get_icon_from_weather_id(current_weather.icon)]);
      bitmap_layer_set_bitmap(icon_layer, icon_bitmap);

      if (temperature) {
        free(temperature);
      }

      int num = snprintf(NULL, 0, "%d\u00B0C", current_weather.temperature);
      temperature = malloc(num + 1);
      snprintf(temperature, num + 1, "%d\u00B0%c", current_weather.temperature, current_weather.metric_units ? 'C' : 'F');

      text_layer_set_text(temperature_layer, temperature);
      text_layer_set_text(city_layer, current_weather.city);

      struct tm *ptr_time = localtime(&current_weather.sunrise);
      char buffer[20];
      strftime(buffer, 20, "%x %X", ptr_time);

      APP_LOG(APP_LOG_LEVEL_DEBUG, "Sunrise: %s", buffer);

      ptr_time = localtime(&current_weather.sunset);
      strftime(buffer, 20, "%x %X", ptr_time);

      APP_LOG(APP_LOG_LEVEL_DEBUG, "Sunset: %s", buffer);
      APP_LOG(APP_LOG_LEVEL_DEBUG, "Humidity: %d%%", current_weather.humidity);
      APP_LOG(APP_LOG_LEVEL_DEBUG, "Pressure: %d mb", current_weather.pressure);
      APP_LOG(APP_LOG_LEVEL_DEBUG, "Wind: %s %d kph\n", current_weather.wind_direction, current_weather.wind_speed);
    } else {
      Tuple *tuple = dict_find(received, WEATHER_FORECAST_RESPONSE_KEY);

      if (tuple) {
        uint8_t count = tuple->value->data[0];

        if (count > 0) {
          memcpy(&weather_forecast, &tuple->value->data[1], sizeof(WeatherForecast));
        } else { // Location disabled
          text_layer_set_text(temperature_layer, "N/A");
          text_layer_set_text(city_layer, "Enable Location");
          return;
        }

        if (weather_forecast.icon[0] == -1) {
          text_layer_set_text(city_layer, "Request Failed");
          return;
        }

        text_layer_set_text(city_layer, weather_forecast.city);

        time_t rawtime = time(NULL);
        for (uint8_t idx = 0; idx < ARRAY_LENGTH(weather_forecast.icon); idx++) {
          int16_t icon = weather_forecast.icon[idx];

          if (icon == -1) {
            break;
          }

          struct tm *ptr_time = localtime(&rawtime);
          char buffer[5];
          strftime(buffer, 5, "%a", ptr_time);

          APP_LOG(APP_LOG_LEVEL_DEBUG, "%s", buffer);
          APP_LOG(APP_LOG_LEVEL_DEBUG, "Icon: %d", icon);

          int16_t temperature_min = weather_forecast.temperature_min[idx];
          int16_t temperature_max = weather_forecast.temperature_max[idx];
          char unit = weather_forecast.metric_units ? 'C' : 'F';

          APP_LOG(APP_LOG_LEVEL_DEBUG, "Temperature min: %d\u00B0%c", temperature_min, unit);
          APP_LOG(APP_LOG_LEVEL_DEBUG, "Temperature max: %d\u00B0%c\n", temperature_max, unit);

          rawtime += 86400;
        }
      }
    }
  }
}

static void window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);

  icon_layer = bitmap_layer_create(GRect(32, 10, 80, 80));
  layer_add_child(window_layer, bitmap_layer_get_layer(icon_layer));

  temperature_layer = text_layer_create(GRect(0, 95, 144, 68));
  text_layer_set_text_color(temperature_layer, GColorWhite);
  text_layer_set_background_color(temperature_layer, GColorClear);
  text_layer_set_font(temperature_layer, fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD));
  text_layer_set_text_alignment(temperature_layer, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(temperature_layer));

  city_layer = text_layer_create(GRect(0, 125, 144, 68));
  text_layer_set_text_color(city_layer, GColorWhite);
  text_layer_set_background_color(city_layer, GColorClear);
  text_layer_set_font(city_layer, fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD));
  text_layer_set_text_alignment(city_layer, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(city_layer));

  app_message_register_inbox_received(received_message);

  send_cmd();
}

static void window_unload(Window *window) {
  app_message_deregister_callbacks();

  if (icon_bitmap) {
    gbitmap_destroy(icon_bitmap);
  }

  text_layer_destroy(city_layer);
  text_layer_destroy(temperature_layer);
  bitmap_layer_destroy(icon_layer);
}

static void init() {
  window = window_create();
  window_set_background_color(window, GColorBlack);
  window_set_fullscreen(window, true);
  window_set_window_handlers(window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload
  });

  const int inbound_size = app_message_inbox_size_maximum();
  const int outbound_size = app_message_outbox_size_maximum();
  app_message_open(inbound_size, outbound_size);

  const bool animated = true;
  window_stack_push(window, animated);
}

static void deinit() {
  window_destroy(window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
