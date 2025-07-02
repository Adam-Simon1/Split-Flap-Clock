#include <Arduino.h>
#include <WiFi.h>
#include <time.h>
#include <sntp.h>
#include <AccelStepper.h>

// If you want 12-hour clock change is12Hour to true
bool is12Hour = false;

// Wifi (Enter your wifi credentials)
const char *ssid = "";
const char *password = "";

// NTP (Change the gmtOffset_sec and daylightOffset_sec to match your timezone)
const char *ntpServer1 = "pool.ntp.org";
const char *ntpServer2 = "time.nist.gov";
const long gmtOffset_sec = 3600;
const int daylightOffset_sec = 3600;

// Stepper
AccelStepper stepperHours = AccelStepper(AccelStepper::FULL4WIRE, 13, 14, 12, 27);
AccelStepper stepperMinutes = AccelStepper(AccelStepper::FULL4WIRE, 26, 33, 25, 32);
int stepsToMove = 2048 / 60;

// Hall effect sensors
const int hallEffectSensorHoursPin = 34;
const int hallEffectSensorMinutesPin = 35;

// Current time
int currentHour = 0;
int currentMinute = 0;

struct Time
{
  int hour;
  int minute;
};

// Homing in parallel
struct HomingState
{
  bool isHomed;
  int sensorValue;
};

HomingState homeStateHours = {false, 0};
HomingState homeStateMinutes = {false, 0};

Time localTime()
{
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo))
  {
    Serial.println("Failed to obtain time");
    return {0, 0};
  }

  char hour[3], minute[3];

  strftime(hour, 3, "%H", &timeinfo);
  strftime(minute, 3, "%M", &timeinfo);

  int iHour = atoi(hour);
  int iMinute = atoi(minute);

  if (is12Hour)
  {
    iHour = (iHour % 12);
    if (iHour == 0)
      iHour = 12;
  }

  return {iHour, iMinute};
}

void home(HomingState &state, int pin, AccelStepper &stepper, bool isHour)
{
  if (state.isHomed)
    return;

  state.sensorValue = analogRead(pin);

  // Reverse direction since the motors are mirrored
  stepper.setSpeed(isHour ? -300 : 300);
  stepper.setAcceleration(300);

  // If magnet in not on top of hall effect sensor
  if (state.sensorValue > 1000 && state.sensorValue < 2500)
  {
    stepper.runSpeed();
  }
  else
  {
    stepper.stop();
    state.isHomed = true;
  }
}

void moveTo(int nextNumber, int &currentNumber, AccelStepper stepper, bool isHours, int hallEffectSensorPin)
{
  int distance = (nextNumber - currentNumber + 60) % 60;
  // Examples:
  // (0 - 23 + 60) % 60 = 37
  // (23 - 22 + 60) % 60 = 1

  if (nextNumber == 0)
  {
    HomingState &homeState = isHours ? homeStateHours : homeStateMinutes;

    while (!homeState.isHomed)
    {
      home(homeState, hallEffectSensorPin, stepper, isHours);
    }

    homeState.isHomed = false;
  }
  else
  {
    distance = isHours ? -distance : distance;

    stepper.move(stepsToMove * distance);

    while (stepper.distanceToGo() != 0)
    {
      stepper.run();
    }
  }

  currentNumber = nextNumber;
}

void setup()
{
  Serial.begin(921600);

  WiFi.begin(ssid, password);
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer1, ntpServer2);

  stepperHours.setMaxSpeed(300);
  stepperHours.setAcceleration(300);
  stepperMinutes.setMaxSpeed(300);
  stepperMinutes.setAcceleration(300);

  while (!homeStateHours.isHomed || !homeStateMinutes.isHomed)
  {
    home(homeStateHours, hallEffectSensorHoursPin, stepperHours, true);
    home(homeStateMinutes, hallEffectSensorMinutesPin, stepperMinutes, false);
  }

  homeStateHours.isHomed = false;
  homeStateMinutes.isHomed = false;
}

void loop()
{
  Time time = localTime();
  int nextHour = time.hour;
  int nextMinute = time.minute;

  delay(2000);

  if (nextHour != currentHour)
  {
    moveTo(nextHour, currentHour, stepperHours, true, hallEffectSensorHoursPin);
  }

  if (nextMinute != currentMinute)
  {
    moveTo(nextMinute, currentMinute, stepperMinutes, false, hallEffectSensorMinutesPin);
  }
}
