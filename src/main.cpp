#include <M5Unified.h>
#include <TinyGPS++.h>
#include "time.h"

constexpr int GPS_RX = 33;
constexpr int GPS_TX = 32;
constexpr int GPS_BAUD = 9600;
constexpr int GPS_DATA_HISTORY_COUNT = 60;

constexpr int MODE_COUNT = 3;
constexpr int MODE_SPEED = 0;
constexpr int MODE_ALTITUDE = 1;
constexpr int MODE_CLOCK = 2;

HardwareSerial gpsSerial(2);
TinyGPSPlus gps;
M5Canvas canvas(&M5.Lcd);

TaskHandle_t readGpsTaskHandler;

int mode = MODE_SPEED;

double gpsSpeed = 0;
double gpsAltitude = 0;
TinyGPSDate gpsDate = {};
TinyGPSTime gpsTime = {};

double speedHistory[GPS_DATA_HISTORY_COUNT] = {0};
double altitudeHistory[GPS_DATA_HISTORY_COUNT] = {0};
int currentIndex = 0;

void getMaxAndAvg(double *arr, size_t length, double *outMax, double *outAvg);
void readGpsData(void *);
void showSpeed(bool isValid, double speed, double max, double avg);
void showAltitude(bool isValid, double altitude, double max, double avg);
void showClock(bool isValid, TinyGPSDate date, TinyGPSTime time);

void setup()
{
    // Initialize the M5StickC object
    M5.begin();
    M5.Lcd.setRotation(1);

    canvas.setPsram(true);
    canvas.createSprite(M5.Lcd.width(), M5.Lcd.height());

    // GPS serial port initialization
    gpsSerial.begin(GPS_BAUD, SERIAL_8N1, GPS_RX, GPS_TX);

    // 別のコアでGPSデータを読み取る
    xTaskCreatePinnedToCore(readGpsData, "readGpsData", 4096, nullptr, 24, &readGpsTaskHandler, 0);
}

void loop()
{
    M5.update();

    if (M5.BtnA.wasPressed())
    {
        mode = (mode + 1) % MODE_COUNT;
    }

    double max, avg;

    switch (mode)
    {
    case MODE_SPEED:
        getMaxAndAvg(speedHistory, GPS_DATA_HISTORY_COUNT, &max, &avg);
        showSpeed(gps.speed.isValid(), gpsSpeed, max, avg);
        break;
    case MODE_ALTITUDE:
        getMaxAndAvg(altitudeHistory, GPS_DATA_HISTORY_COUNT, &max, &avg);
        showAltitude(gps.altitude.isValid(), gpsAltitude, max, avg);
        break;
    case MODE_CLOCK:
        bool isValid = gps.date.isValid() && gps.time.isValid();
        showClock(isValid, gpsDate, gpsTime);
    }
}

void getMaxAndAvg(double *arr, size_t length, double *outMax, double *outAvg)
{
    double max = 0;
    double avg = 0;
    size_t validCount = 0;

    for (int i = 0; i < length; i++)
    {
        if (arr[i] == 0)
            continue;

        if (arr[i] > max)
            max = arr[i];

        avg += arr[i];
        validCount++;
    }

    if (validCount != 0)
        avg /= validCount;

    *outMax = max;
    *outAvg = avg;
}

void readGpsData(void *args)
{
    while (true)
    {
        while (gpsSerial.available() > 0)
        {
            if (gps.encode(gpsSerial.read()))
            {
                break;
            }
        }

        if (gps.altitude.isUpdated())
        {
            gpsAltitude = gps.altitude.meters();
            altitudeHistory[currentIndex] = gpsAltitude;
        }

        if (gps.speed.isUpdated())
        {
            gpsSpeed = gps.speed.kmph();
            speedHistory[currentIndex] = gpsSpeed;
        }

        if (gps.date.isUpdated() || gps.time.isUpdated())
        {
            gpsDate = gps.date;
            gpsTime = gps.time;
        }

        currentIndex = (currentIndex + 1) % GPS_DATA_HISTORY_COUNT;

        delay(1000);
    }
}

void drawHeader(M5Canvas *canvas, const char *title, int color, int titleColor = TFT_WHITE)
{
    canvas->setColor(color);
    canvas->fillRect(0, 0, 200, 32);
    canvas->fillCircle(200, 15, 16);
    canvas->setTextColor(titleColor, color);
    canvas->drawString(title, 10, 4, &fonts::Font4);
}

void showSpeed(bool isValid, double speed, double max, double avg)
{
    canvas.fillScreen(TFT_WHITE);
    canvas.setCursor(0, 0);

    // 現在の速度を文字列に変換
    char speedStr[8];

    if (isValid)
    {
        dtostrf(speed, 4, 1, speedStr);
    }
    else
    {
        strcpy(speedStr, "---.-");
    }

    char maxAvgStr[64];
    sprintf(maxAvgStr, "Max %.1f km/h | Avg %.1f km/h", max, avg);

    auto width = canvas.width();
    auto height = canvas.height();

    // ヘッダーを描画
    drawHeader(&canvas, "SPEED METER", TFT_SKYBLUE);

    // 単位、最高速度、平均速度を描画
    canvas.setTextColor(TFT_DARKGRAY, TFT_WHITE);
    canvas.drawRightString("km/h", width - 20, height / 2 + 4, &fonts::Font4);
    canvas.drawString(maxAvgStr, 20, height - 20, &fonts::Font2);

    // 現在の速度を描画
    canvas.setTextColor(TFT_BLACK, TFT_WHITE);
    canvas.drawRightString(speedStr, width - 85, height / 2 - 16, &fonts::Font7);

    canvas.pushSprite(0, 0);
}

void showAltitude(bool isValid, double altitude, double max, double avg)
{
    canvas.fillScreen(TFT_WHITE);
    canvas.setCursor(0, 0);

    // 現在の高度を文字列に変換
    char altitudeStr[8];

    if (isValid)
    {
        dtostrf(altitude, 5, 1, altitudeStr);
    }
    else
    {
        strcpy(altitudeStr, "----.-");
    }

    char maxAvgStr[64];
    sprintf(maxAvgStr, "Max %.1f m | Avg %.1f m", max, avg);

    auto width = canvas.width();
    auto height = canvas.height();

    // ヘッダーを描画
    drawHeader(&canvas, "ALTITUDE", TFT_GREEN);

    // 単位、最高高度、平均高度を描画
    canvas.setTextColor(TFT_DARKGRAY, TFT_WHITE);
    canvas.drawRightString("m", width - 20, height / 2 + 4, &fonts::Font4);
    canvas.drawString(maxAvgStr, 20, height - 20, &fonts::Font2);

    // 現在の高度を描画
    canvas.setTextColor(TFT_BLACK, TFT_WHITE);
    canvas.drawRightString(altitudeStr, width - 50, height / 2 - 16, &fonts::Font7);

    canvas.pushSprite(0, 0);
}

void showClock(bool isValid, TinyGPSDate date, TinyGPSTime time)
{
    canvas.fillScreen(TFT_WHITE);
    canvas.setCursor(0, 0);

    char timeStr[8];
    char dateStr[32];

    if (isValid)
    {
        sprintf(timeStr, "%02d:%02d", time.hour() + 9, time.minute());
        sprintf(dateStr, "%02d/%02d, %04d", date.month(), date.day(), date.year());
    }
    else
    {
        strcpy(timeStr, "--:--");
        strcpy(dateStr, "--/--, ----");
    }

    auto width = canvas.width();
    auto height = canvas.height();

    // ヘッダーを描画
    drawHeader(&canvas, "GPS CLOCK", TFT_ORANGE);

    // 日付を描画
    canvas.setTextColor(TFT_DARKGRAY, TFT_WHITE);
    canvas.drawString(dateStr, 20, height - 20, &fonts::Font2);

    // 時刻を描画
    canvas.setTextColor(TFT_BLACK, TFT_WHITE);
    canvas.drawCentreString(timeStr, width / 2, height / 2 - 16, &fonts::Font7);

    canvas.pushSprite(0, 0);
}
