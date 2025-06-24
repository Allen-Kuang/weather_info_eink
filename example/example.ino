#include <SPI.h>
//EPD
#include "Display_EPD_W21_spi.h"
#include "Display_EPD_W21.h"
#include "Ap_29demo.h"  
#include "GUI_Paint.h"
#include "fonts.h"
#include "weather_icon.h"  // Include the weather icon
//Info
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

#if 1 
   unsigned char BlackImage[EPD_ARRAY];//Define canvas space  
#endif

// WiFi configuration
const char* ssid = "Maker_2.4G";
const char* password = "maker2025";

// OpenWeather API configuration
const char* apiKey = "7fa904ddcca965c7c641d02ac563da75";
const int cityId = 1795565; // Shenzhen city ID
const char* server = "api.openweathermap.org";

// Air pollution API configuration
const char* airPollutionApiKey = "7fa904ddcca965c7c641d02ac563da75"; // Same as weather API
const float lat = 22.5431; // Shenzhen latitude
const float lon = 114.0579; // Shenzhen longitude

// Data update interval (milliseconds)
const unsigned long updateInterval = 3600000; // 360 minutes
unsigned long lastUpdate = 0;

// Current weather data
struct WeatherData {
  float minTemperature;
  float maxTemperature;
  float feelsLike;
  int humidity;
  int pressure;
  int visibility;
  String description;
  float windSpeed;
};

// Air quality data
struct AirQualityData {
  int aqi;
  float pm2_5;
  float pm10;
  float so2;
  float no2;
  float co;
  float o3;
};

// Weather forecast data (daily)
struct ForecastData {
  String date;
  float minTemp;
  float maxTemp;
  float avgHumidity;
  String description;
};

WeatherData currentWeather;
AirQualityData airQuality;
ForecastData forecasts[5]; // 5-day forecast

void setup() {
  pinMode(4, INPUT);  //BUSY
  pinMode(38, OUTPUT); //RES 
  pinMode(10, OUTPUT); //DC   
  pinMode(44, OUTPUT); //CS     

  //SPI
  SPI.beginTransaction(SPISettings(10000000, MSBFIRST, SPI_MODE0)); 
  SPI.begin ();  

  //Initialize Serial
  Serial.begin(115200);

  // Connect to WiFi
  connectToWiFi();
  
  Serial.println("Weather Monitoring Station started");
}

void loop() {
  // Update data periodically
  if (millis() - lastUpdate >= updateInterval || lastUpdate == 0) {
    updateWeatherData();
    updateAirQualityData();
    updateForecastData();
    printAllData();
    displayData();
    lastUpdate = millis();
  }
  
  delay(1000);

}

// Connect to WiFi
void connectToWiFi() {
  Serial.print("Connecting to WiFi: ");
  Serial.println(ssid);
  
  WiFi.begin(ssid, password);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  
  Serial.println();
  Serial.println("WiFi connected successfully");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

// Update weather data
void updateWeatherData() {
  if (WiFi.status() != WL_CONNECTED) {
    connectToWiFi();
    return;
  }
  
  HTTPClient http;
  String url = "https://" + String(server) + "/data/2.5/weather?id=" + 
               String(cityId) + "&appid=" + String(apiKey) + "&units=metric";
  
  Serial.print("Request URL: ");
  Serial.println(url);
  
  http.begin(url.c_str());
  int httpResponseCode = http.GET();
  
  if (httpResponseCode > 0) {
    String payload = http.getString();
    Serial.println("HTTP response code: " + String(httpResponseCode));
    
    // Parse JSON data
    DynamicJsonDocument doc(1024);
    DeserializationError error = deserializeJson(doc, payload);
    
    if (!error) {
      // Extract basic weather data
      currentWeather.feelsLike = doc["main"]["feels_like"];
      currentWeather.humidity = doc["main"]["humidity"];
      currentWeather.pressure = doc["main"]["pressure"];
      currentWeather.visibility = doc["visibility"] | -1; // Default -1 means no data
      currentWeather.description = doc["weather"][0]["description"].as<String>();
      currentWeather.windSpeed = doc["wind"]["speed"];
      
      // Initialize min and max temperatures with extreme values
      currentWeather.minTemperature = 1000;
      currentWeather.maxTemperature = -1000;
      
      Serial.println("Basic weather data updated successfully");
    } else {
      Serial.println("JSON parsing error: " + String(error.c_str()));
    }
  } else {
    Serial.println("HTTP request error: " + String(httpResponseCode));
  }
  
  http.end();
}

// Update air quality data
void updateAirQualityData() {
  if (WiFi.status() != WL_CONNECTED) {
    connectToWiFi();
    return;
  }
  
  HTTPClient http;
  String url = "https://" + String(server) + "/data/2.5/air_pollution?lat=" +
               String(lat) + "&lon=" + String(lon) + "&appid=" + String(airPollutionApiKey);
  
  Serial.print("Request URL: ");
  Serial.println(url);
  
  http.begin(url.c_str());
  int httpResponseCode = http.GET();
  
  if (httpResponseCode > 0) {
    String payload = http.getString();
    Serial.println("HTTP response code: " + String(httpResponseCode));
    
    // Parse JSON data
    DynamicJsonDocument doc(1024);
    DeserializationError error = deserializeJson(doc, payload);
    
    if (!error) {
      // Extract air quality data
      airQuality.aqi = doc["list"][0]["main"]["aqi"];
      airQuality.pm2_5 = doc["list"][0]["components"]["pm2_5"];
      airQuality.pm10 = doc["list"][0]["components"]["pm10"];
      airQuality.so2 = doc["list"][0]["components"]["so2"];
      airQuality.no2 = doc["list"][0]["components"]["no2"];
      airQuality.co = doc["list"][0]["components"]["co"];
      airQuality.o3 = doc["list"][0]["components"]["o3"];
      
      Serial.println("Air quality data updated successfully");
    } else {
      Serial.println("JSON parsing error: " + String(error.c_str()));
    }
  } else {
    Serial.println("HTTP request error: " + String(httpResponseCode));
  }
  
  http.end();
}

// Update future 5-day weather forecast data
void updateForecastData() {
  if (WiFi.status() != WL_CONNECTED) {
    connectToWiFi();
    return;
  }
  
  HTTPClient http;
  String url = "https://" + String(server) + "/data/2.5/forecast?id=" + 
               String(cityId) + "&appid=" + String(apiKey) + "&units=metric";
  
  Serial.print("Request URL: ");
  Serial.println(url);
  
  http.begin(url.c_str());
  int httpResponseCode = http.GET();
  
  if (httpResponseCode > 0) {
    String payload = http.getString();
    Serial.println("HTTP response code: " + String(httpResponseCode));
    
    // Parse JSON data
    DynamicJsonDocument doc(16384); // Larger buffer for forecast data
    DeserializationError error = deserializeJson(doc, payload);
    
    if (!error) {
      // Clear forecast array
      for (int i = 0; i < 5; i++) {
        forecasts[i].date = "";
        forecasts[i].minTemp = 1000;  // Initialize to max value
        forecasts[i].maxTemp = -1000; // Initialize to min value
        forecasts[i].avgHumidity = 0;
        forecasts[i].description = "";
      }
      
      // Process forecast data
      int forecastCount = doc["cnt"];
      int currentDay = -1;
      String currentDate = "";
      String todayDate = "";
      
      // Get today's date from the first forecast entry
      if (forecastCount > 0) {
        todayDate = doc["list"][0]["dt_txt"].as<String>().substring(0, 10); // YYYY-MM-DD
      }
      
      for (int i = 0; i < forecastCount; i++) {
        String dt_txt = doc["list"][i]["dt_txt"].as<String>();
        String date = dt_txt.substring(0, 10); // Extract date part
        
        // If it's a new day
        if (date != currentDate) {
          currentDate = date;
          currentDay++;
          
          // Only process next 5 days
          if (currentDay >= 5) break;
          
          forecasts[currentDay].date = date;
        }
        
        // Update min/max temperature for the day
        float temp = doc["list"][i]["main"]["temp"];
        if (temp < forecasts[currentDay].minTemp) {
          forecasts[currentDay].minTemp = temp;
        }
        if (temp > forecasts[currentDay].maxTemp) {
          forecasts[currentDay].maxTemp = temp;
        }
        
        // Accumulate humidity for average calculation
        float humidity = doc["list"][i]["main"]["humidity"].as<float>();
        forecasts[currentDay].avgHumidity += humidity;
        
        // Record weather description (use the first forecast's description)
        if (forecasts[currentDay].description == "") {
          forecasts[currentDay].description = doc["list"][i]["weather"][0]["description"].as<String>();
        }
        
        // Update current day's min/max temperature in currentWeather
        if (date == todayDate) {
          if (temp < currentWeather.minTemperature) {
            currentWeather.minTemperature = temp;
          }
          if (temp > currentWeather.maxTemperature) {
            currentWeather.maxTemperature = temp;
          }
        }
      }
      
      // Calculate average humidity
      for (int i = 0; i < 5; i++) {
        if (forecasts[i].date != "") {
          // 8 forecasts per day (3-hour intervals)
          forecasts[i].avgHumidity /= 8.0;
        }
      }
      
      Serial.println("Weather forecast data updated successfully");
    } else {
      Serial.println("JSON parsing error: " + String(error.c_str()));
    }
  } else {
    Serial.println("HTTP request error: " + String(httpResponseCode));
  }
  
  http.end();
}

// Print all data
void printAllData() {
  Serial.println("\n=========================================");
  Serial.println("Shenzhen Real-time Weather Data");
  Serial.print("Update time: ");
  Serial.println(getCurrentDateTime());
  Serial.println("-----------------------------------------");
  
  // Print weather data with temperature range
  Serial.println("Current weather: " + currentWeather.description);
  Serial.print("Temperature range: ");
  Serial.print(currentWeather.minTemperature);
  Serial.print("°C - ");
  Serial.print(currentWeather.maxTemperature);
  Serial.println("°C");
  
  Serial.print("Feels like: ");
  Serial.print(currentWeather.feelsLike);
  Serial.println("°C");
  
  Serial.print("Humidity: ");
  Serial.print(currentWeather.humidity);
  Serial.println("%");
  
  Serial.print("Pressure: ");
  Serial.print(currentWeather.pressure);
  Serial.println("hPa");
  
  if (currentWeather.visibility != -1) {
    Serial.print("Visibility: ");
    Serial.print(currentWeather.visibility);
    Serial.println("m");
  } else {
    Serial.println("Visibility: Data not available");
  }
  
  Serial.print("Wind speed: ");
  Serial.print(currentWeather.windSpeed);
  Serial.println("m/s");
  
  // Print air quality data
  Serial.println("\nAir Quality Data:");
  Serial.print("Air Quality Index (AQI): ");
  Serial.println(airQuality.aqi);
  
  Serial.print("PM2.5: ");
  Serial.print(airQuality.pm2_5);
  Serial.println("μg/m³");
  
  Serial.print("PM10: ");
  Serial.print(airQuality.pm10);
  Serial.println("μg/m³");
  
  Serial.print("Sulfur Dioxide (SO2): ");
  Serial.print(airQuality.so2);
  Serial.println("μg/m³");
  
  Serial.print("Nitrogen Dioxide (NO2): ");
  Serial.print(airQuality.no2);
  Serial.println("μg/m³");
  
  Serial.print("Carbon Monoxide (CO): ");
  Serial.print(airQuality.co);
  Serial.println("μg/m³");
  
  Serial.print("Ozone (O3): ");
  Serial.print(airQuality.o3);
  Serial.println("μg/m³");
  
  // Air quality category
  Serial.print("Air Quality Category: ");
  Serial.println(getAirQualityCategory(airQuality.aqi));
  
  // Print 5-day weather forecast
  Serial.println("\n5-day Weather Forecast:");
  Serial.println("Date        Temperature Range      Humidity    Condition");
  Serial.println("---------------------------------------------------------");
  
  for (int i = 0; i < 5; i++) {
    if (forecasts[i].date != "") {
      Serial.print(forecasts[i].date);
      Serial.print("  ");
      
      // Format temperature range
      Serial.printf("%5.1f°C - %5.1f°C  ", forecasts[i].minTemp, forecasts[i].maxTemp);
      
      // Format humidity
      Serial.printf("%5.1f%%  ", forecasts[i].avgHumidity);
      
      // Weather description
      Serial.println(forecasts[i].description);
    }
  }
  
  Serial.println("=========================================\n");
}

// Get air quality category description
String getAirQualityCategory(int aqi) {
  switch(aqi) {
    case 1: return "Good";
    case 2: return "Fair";
    case 3: return "Moderate";
    case 4: return "Poor";
    case 5: return "Very Poor";
    default: return "Unknown";
  }
}

// Get current date and time string (simplified)
String getCurrentDateTime() {
  // ESP32C3 has no built-in RTC, return runtime instead
  unsigned long seconds = millis() / 1000;
  unsigned long minutes = seconds / 60;
  unsigned long hours = minutes / 60;
  
  return String(hours % 24) + ":" + 
         String(minutes % 60) + ":" + 
         String(seconds % 60);
}

// ------------The next three functions are for converting date formats--------
// Get the abbreviated day of the week (based on the date string in the format YYYY - MM - DD)
String getWeekday(String dateStr) {

  int year = dateStr.substring(0, 4).toInt();
  int month = dateStr.substring(5, 7).toInt();
  int day = dateStr.substring(8, 10).toInt();
  
  // Calculation（0=sunday，1=monday...6=saturday）
  if (month < 3) {
    month += 12;
    year--;
  }
  int c = year / 100;
  year %= 100;
  int w = (c/4 - 2*c + year + year/4 + 13*(month+1)/5 + day - 1) % 7;
  
  // Switch to weekday
  switch(w) {
    case 0: return "Sun";
    case 1: return "Mon";
    case 2: return "Tue";
    case 3: return "Wed";
    case 4: return "Thu";
    case 5: return "Fri";
    case 6: return "Sat";
    default: return "";
  }
}

// Get months abbr（1-12 → Jan-Dec）
String getMonthAbbreviation(int month) {
  switch(month) {
    case 1: return "Jan";
    case 2: return "Feb";
    case 3: return "Mar";
    case 4: return "Apr";
    case 5: return "May";
    case 6: return "Jun";
    case 7: return "Jul";
    case 8: return "Aug";
    case 9: return "Sep";
    case 10: return "Oct";
    case 11: return "Nov";
    case 12: return "Dec";
    default: return "";
  }
}

// Formate data：2025-06-24 → Tue, Jun 24
String formatDate(String dateStr) {
  int month = dateStr.substring(5, 7).toInt();
  int day = dateStr.substring(8, 10).toInt();
  
  return getWeekday(dateStr) + ", " + 
         getMonthAbbreviation(month) + " " + 
         String(day);
}
//----fomatting end------

//-----The following part, I'll get those weather info and map them into 7 common weather icon----------
// Define the mapping structure from weather types to icons
struct WeatherMapping {
    const char* types[16];  // Up to 16 weather types can be associated with each icon
    const uint8_t* icon;    // Corresponding icon data
    int typeCount;          // Actual number of associated types
};

// Create the weather type to icon mapping table - small size
const WeatherMapping weatherMappings[] = {
    { {"hail"},                 weather_hail_100x,                 1 },
    { {"hurricane"},            weather_hurricane_outline_100x,    1 },
    { {"rain", "drizzle", "thunderstorm"}, weather_pouring_100x, 3 },
    { {"snow", "sleet"},        weather_snowy_100x,                2 },
    { {"sky"},                  weather_sunny_100x,                1 },
    { {"windy", "breeze", "gale", "squalls", "tornado", "storm", 
      "sand", "dust", "ash", "whirls", "mist", "fog", "smoke", "haze", "calm"}, 
                                              weather_windy_100x,               15 },
    { {"clouds"},               weather_partly_cloudy_100x,        1 }
};
// Create the weather type to icon mapping table - large size
const WeatherMapping bigWeatherMappings[] = {
    { {"hail"},                 weather_hail_200x,                 1 },
    { {"hurricane"},            weather_hurricane_outline_200x,    1 },
    { {"rain", "drizzle", "thunderstorm"}, weather_pouring_200x, 3 },
    { {"snow", "sleet"},        weather_snowy_200x,                2 },
    { {"sky"},                  weather_sunny_200x,                1 },
    { {"windy", "breeze", "gale", "squalls", "tornado", "storm", 
      "sand", "dust", "ash", "whirls", "mist", "fog", "smoke", "haze", "calm"}, 
                                              weather_windy_200x,               15 },
    { {"clouds"},               weather_partly_cloudy_200x,        1 }
};

// Get the corresponding icon based on the weather type string
const uint8_t* getBigWeatherIcon(const char* weatherType) {
    // Traverse the mapping table to find a matching weather type
    for (int i = 0; i < sizeof(bigWeatherMappings) / sizeof(bigWeatherMappings[0]); i++) {
        const WeatherMapping& mapping = bigWeatherMappings[i];
        for (int j = 0; j < mapping.typeCount; j++) {
            if (strcmp(weatherType, mapping.types[j]) == 0) {
                return mapping.icon; // Matching type found, return corresponding icon
            }
        }
    }
    // Default to partially cloudy icon
    return weather_partly_cloudy_100x;
}
// Get the corresponding icon based on the weather type string
const uint8_t* getWeatherIcon(const char* weatherType) {
    // Traverse the mapping table to find a matching weather type
    for (int i = 0; i < sizeof(weatherMappings) / sizeof(weatherMappings[0]); i++) {
        const WeatherMapping& mapping = weatherMappings[i];
        for (int j = 0; j < mapping.typeCount; j++) {
            if (strcmp(weatherType, mapping.types[j]) == 0) {
                return mapping.icon; // Matching type found, return corresponding icon
            }
        }
    }
    // Default to partially cloudy icon
    return weather_partly_cloudy_200x;
}

// Extract the last weather word from a string (split by space). For example: light rain -> rain
String getLastWord(String description) {
  int lastSpace = description.lastIndexOf(' '); // Find the position of the last space
  if (lastSpace != -1) {
    return description.substring(lastSpace + 1); // Extract the part after the space
  } else {
    return description; // Return the original string if there is no space
  }
}
//----mapping end-----

// Show icons
void EPD_ShowPicture(uint16_t x, uint16_t y, uint16_t sizex, uint16_t sizey, const uint8_t BMP[], uint16_t Color)
{
    uint16_t j = 0, t;
    uint16_t i, temp, x0;
    uint16_t bytes_per_line = (sizex + 7) / 8;  // 更准确的每行字节数计算
    uint16_t TypefaceNum = sizey * bytes_per_line;
    x0 = x;
    
    Serial.print("Debug: sizex=");
    Serial.print(sizex);
    Serial.print(", sizey=");  
    Serial.print(sizey);
    Serial.print(", bytes_per_line=");
    Serial.print(bytes_per_line);
    Serial.print(", TypefaceNum=");
    Serial.println(TypefaceNum);
    
    for (i = 0; i < TypefaceNum; i++)
    {
        temp = BMP[j];
        for (t = 0; t < 8; t++)
        {
            // 只处理在图像宽度范围内的像素
            if ((x - x0) < sizex)
            {
                if (temp & 0x80)
                {
                    Paint_SetPixel(x, y, !Color);
                }
                else
                {
                    Paint_SetPixel(x, y, Color);
                }
            }
            x++;
            temp <<= 1;
        }
        if ((x - x0) >= bytes_per_line * 8)  // 修正换行条件
        {
            x = x0;
            y++;
        }
        j++;
        // delayMicroseconds(10); // delay in microseconds
    }
}

//display data on eink screen
void displayData(){
		EPD_Init(); //Full screen refresh initialization.
		Paint_NewImage(BlackImage, EPD_WIDTH, EPD_HEIGHT, 0, WHITE); //Set canvas parameters
		Paint_SelectImage(BlackImage); //Select current settings
		Paint_Clear(WHITE); //Clear canvas with white background
		
		// Define 2 different sizes for comparison
		uint16_t size1 = 100;  // 100x100 
		uint16_t size2 = 200;  // 200x200
		
		// Layout: 2 icons on top row, 3 icons on bottom row
		// Adjust overall position up by 50 pixels
		uint16_t center_x = EPD_WIDTH / 2;   // 400
		uint16_t center_y = EPD_HEIGHT / 2 - 50;  // 190 (moved up more)
		
		// Top row positions (2 icons) - increased spacing
		uint16_t info_x = 90; 
		uint16_t info_y = 270; 
		uint16_t top_y = 20; 
		uint16_t mid_y = 170; 
		uint16_t bot_y = 320; 
		
		// today weather icon
		EPD_ShowPicture(90, 30, size2, size2, getBigWeatherIcon(getLastWord(currentWeather.description).c_str()), WHITE);
		// weather info
		Paint_DrawString_EN(info_x, info_y, currentWeather.description.c_str(), &Font24, WHITE, BLACK); //17*24
		Paint_DrawString_EN(info_x, info_y+50, formatDate(forecasts[0].date).c_str(), &Font24, WHITE, BLACK); //17*24
		Paint_DrawString_EN(info_x, info_y+100, String(String((int)currentWeather.minTemperature)+"C - "+String((int)currentWeather.maxTemperature)+"C").c_str(), &Font24, WHITE, BLACK); //17*24

    //--------------future part------------

    // weather icon
		EPD_ShowPicture(center_x+10, top_y+10, size1, size1, getWeatherIcon(getLastWord(forecasts[0].description).c_str()), WHITE);
		EPD_ShowPicture(center_x+10, mid_y+10, size1, size1, getWeatherIcon(getLastWord(forecasts[1].description).c_str()), WHITE);
		EPD_ShowPicture(center_x+10, bot_y+10, size1, size1, getWeatherIcon(getLastWord(forecasts[2].description).c_str()), WHITE);

		// weather frame
    Paint_DrawRectangle(center_x, top_y, center_x + 350, top_y+120, BLACK, DRAW_FILL_EMPTY, DOT_PIXEL_3X3); 
    Paint_DrawRectangle(center_x, mid_y, center_x + 350, mid_y+120, BLACK, DRAW_FILL_EMPTY, DOT_PIXEL_3X3); 
    Paint_DrawRectangle(center_x, bot_y, center_x + 350, bot_y+120, BLACK, DRAW_FILL_EMPTY, DOT_PIXEL_3X3); 

		// weather info
		Paint_DrawString_EN(center_x+150, top_y+20, formatDate(forecasts[1].date).c_str(), &Font24, WHITE, BLACK); //17*24
		Paint_DrawString_EN(center_x+150, top_y+70, String(String((int)forecasts[0].minTemp)+"C - "+String((int)forecasts[0].maxTemp)+"C").c_str(), &Font24, WHITE, BLACK); //17*24
		Paint_DrawString_EN(center_x+150, mid_y+20, formatDate(forecasts[2].date).c_str(), &Font24, WHITE, BLACK); //17*24
		Paint_DrawString_EN(center_x+150, mid_y+70, String(String((int)forecasts[1].minTemp)+"C - "+String((int)forecasts[1].maxTemp)+"C").c_str(), &Font24, WHITE, BLACK); //17*24
		Paint_DrawString_EN(center_x+150, bot_y+20, formatDate(forecasts[3].date).c_str(), &Font24, WHITE, BLACK); //17*24
		Paint_DrawString_EN(center_x+150, bot_y+70, String(String((int)forecasts[2].minTemp)+"C - "+String((int)forecasts[2].maxTemp)+"C").c_str(), &Font24, WHITE, BLACK); //17*24

		EPD_Display(BlackImage); //Display the image with weather icons
		EPD_DeepSleep(); //Enter the sleep mode
		delay(2000); //Delay for 2s  
}