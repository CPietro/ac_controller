#include <IRremoteESP8266.h>
#include <IRsend.h>
#include <ir_Fujitsu.h>
#include <Preferences.h>
#include <LiquidCrystal_I2C.h>
#include <NewEncoder.h>
#include <DHT.h>

/// IR Emitter
#define IR_PIN 23
/// Rotary encoder CLK
#define CLK 17
/// Rotary encoder DT
#define DT 16
/// Rotary encoder SW
#define SW 4
/// LD2410 Presence Detector output
#define LD2410_PIN 14
/// DHT11 Temp and RH sensor
#define DHT_PIN 13

#define DHTTYPE DHT11
/// Time between each AC Unit state switch due to presence detection, in minutes
#define PRESENCE_SWITCH_INTERVAL 1

LiquidCrystal_I2C lcd(0x27, 16, 2);  // Display I2C address 0x27
IRFujitsuAC ac(IR_PIN);

DHT dht(DHT_PIN, DHTTYPE);
int LastUpdate;

bool GotPresence;
int LastPresenceStateSwitch = -PRESENCE_SWITCH_INTERVAL * 60 * 1000; // Gets the presence detection to work in the first PRESENCE_SWITCH_INTERVAL minutes after board startup

/// Current menu lower bound
int16_t MenuMin = 0;
/// Current menu upper bound
int16_t MenuMax = 4;
NewEncoder encoder(DT, CLK, MenuMin, MenuMax, MenuMin, FULL_PULSE);
int16_t PrevEncoderValue, CurrentEncoderValue;
bool Pressed, Edited;
bool Rotated = true;
int TimePrevious, TimeNow;

/// @class FujiAC
/// @brief Represents an AC Unit.
/// Allows configuring and sending commands to an AC unit though an IR LED. It supports management of mode, fan speed, temperature, state (on/off) and presence detection. 
class FujiAC {
public:
  /// AC unit mode, in kFujitsuAcMode terms.
  uint8_t mode;
  /// AC unit fan speed, in kFujitsuAcFan terms.
  uint8_t fanSpeed;
  /// AC Unit state, in kFujitsuAcCmd terms.
  uint8_t state;
  /// Temperature setpoint in Celsius degrees.
  float_t temp;
  /// Human presence detection status, true if enabled, false if disabled.
  bool presence;

  FujiAC() {
    ac.begin();
    ac.setModel(ARRAH2E);
    ac.setSwing(kFujitsuAcSwingOff);

    loadFromMemory();
  }

  /// Saves the variables values to memory.
  void saveToMemory() {
    Preferences preferences;
    preferences.begin("FujiAC", false);
    preferences.putUInt("mode", mode);
    preferences.putUInt("fanSpeed", fanSpeed);
    preferences.putUInt("state", state);
    preferences.putFloat("temp", temp);
    preferences.putBool("presence", presence);
    preferences.end();
  }

  /// Loads the last saved variables value from memory if they exist, else they are set to default values.
  void loadFromMemory() {
    Preferences preferences;
    preferences.begin("FujiAC", true);
    mode = preferences.getUInt("mode", kFujitsuAcModeAuto);
    fanSpeed = preferences.getUInt("fanSpeed", kFujitsuAcFanAuto);
    state = preferences.getUInt("state", kFujitsuAcCmdStayOn);
    temp = preferences.getFloat("temp", 24.5f);
    presence = preferences.getBool("presence", false);
    preferences.end();
  }

  /// Sends a command to the AC unit with the currently set variables value and prints the AC unit state.
  void sendCommand() {
    saveToMemory();

    ac.setMode(mode);
    ac.setFanSpeed(fanSpeed);
    ac.setTemp(temp);
    ac.setCmd(state);
    ac.send();
    FujiAC::printState();
  }

  /// Turns on the AC unit
  void on() {
    Serial.println("ON request received");
    state = kFujitsuAcCmdTurnOn;
    this->sendCommand();
    state = kFujitsuAcCmdStayOn;
  }

  /// Turns off the AC unit
  void off() {
    Serial.println("OFF request received");
    state = kFujitsuAcCmdTurnOff;
    this->sendCommand();
  }

  /// Turns the AC unit on if off and the other way around by calling on() and off() methods.
  void switchState() {
    if (getState()) {
      off();
    } else {
      on();
    }
  }

  /// Turns human presence detection on if off and the other way around. Writes the new presence value to memory.
  void switchPresence() {
    presence = !presence;
    saveToMemory();
  }

  /// getState.
  /// @return true if state is either kFujitsuAcCmdTurnOn or kFujitsuAcCmdStayOn which means the AC unit is on.
  /// @return false in any other state.
  bool getState() {
    if (state == kFujitsuAcCmdTurnOn || state == kFujitsuAcCmdStayOn)
      return true;
    else
      return false;
  }

  /// stateToString.
  /// @return "ON " String if getState is true, else "OFF".
  String stateToString() {
    if (getState()) {
      return "ON ";
    } else {
      return "OFF";
    }
  }

  /// presenceToString.
  /// @return "ON " String if human presence detection is currently enabled, else "OFF".
  String presenceToString() {
    if (presence) {
      return "ON ";
    } else {
      return "OFF";
    }
  }

  /// currentSetpointToString.
  /// @return The current setpoint temperature in String type.
  String currentSetpointToString() {
    return String(temp);
  }


  /// tryGetSetpoint.
  /// @return Setpoint to be displayed in main menu if the AC unit is on, else a blank string.
  String tryGetSetpoint() {
    if (state == kFujitsuAcCmdTurnOn || state == kFujitsuAcCmdStayOn) {
      return "SET: " + currentSetpointToString();
    } else {
      return "            ";
    }
  }

  /// modeToString.
  /// @param[in] inputMode Mode in kFujitsuAcMode terms.
  /// @return Corresponding AC mode display value if in range.
  static String modeToString(uint8_t inputMode) {
    switch (inputMode) {
      case 0x0:
        return "AUTO";
      case 0x1:
        return "COOL";
      case 0x2:
        return "DRY ";
      case 0x3:
        return "FAN ";
      case 0x4:
        return "HEAT";
    }
  }

  /// fanSpeedToString.
  /// @param[in] inputFan Fan level in kFujitsuAcFan terms.
  /// @return Corresponding fan level display value if in range.
  static String fanSpeedToString(uint8_t inputFan) {
    switch (inputFan) {
      case 0x0:
        return "AUTO  ";
      case 0x1:
        return "HIGH  ";
      case 0x2:
        return "MEDIUM";
      case 0x3:
        return "LOW   ";
      case 0x4:
        return "QUIET ";
    }
  }

  /// Converts the menu case term to its corresponding setpoint value.
  /// @param[in] value Menu case value to be converted.
  /// @return The converted Celsius setpoint value.
  static float_t encoderToSetpoint(int16_t value) {
    return static_cast<float_t>(value) / 2 + 16;
  }

  /// Converts the setpoint value in Celsius to a menu case.
  /// @param[in] value Setpoint value to be converted.
  /// @return The converted setpoint value in menu case terms.
  static int16_t setpointToEncoder(float_t value) {
    return static_cast<int16_t>((value - 16) * 2);
  }

  /// Prints the current IRFujitsuAC state and the hex IR code.
  static void printState() {
    Serial.println("Fujitsu A/C remote is in the following state:");
    Serial.printf("  %s\n", ac.toString().c_str());
    unsigned char* ir_code = ac.getRaw();
    Serial.print("IR Code: 0x");
    for (uint8_t i = 0; i < ac.getStateLength(); i++)
      Serial.printf("%02X", ir_code[i]);
    Serial.println();
  }
};

FujiAC acUnit;

/// Handles the encoder button press interrupt.
/// Has some kind of software debouncer built in.
void press() {
  TimeNow = millis();
  if (TimeNow - TimePrevious > 500) {
    Pressed = true;
  }
  TimePrevious = TimeNow;
}

/// Formats ambient data to be displayed on main menu (CurrentEncoderValue == 0).
/// @param[in] temp Temperature to be displayed, with 1 decimal.
/// @param[in] rh Relative humidity to be displayed, with 0 decimals.
/// @return String formatted as: "T: <temp> RH: <rh>%".
String ambientDataToString(float temp, float rh) {
  return "T: " + String(temp, 1) + " RH: " + String(rh, 0) + "%";
}

/// Handles temperature and humidity refreshing when on main menu (CurrentEncoderValue == 0), every 5 seconds.
/// Handles protective heating when the ambient temperature drops below 10 degrees Celsius.
/// Handles protective cooling when the ambient temperature rises above 30 degrees Celsius.
void ambientDataHandler() {
  TimeNow = millis();
  if (TimeNow - LastUpdate > 5000) {
    if (CurrentEncoderValue == 0) {
      lcd.setCursor(0, 1);
      lcd.print(ambientDataToString(dht.readTemperature(), dht.readHumidity()));
    }
    
    if (acUnit.state == kFujitsuAcCmdTurnOff) {
      float temp = dht.readTemperature();
      if (temp >= 30.0f) {
        acUnit.temp = 28.0f;
        acUnit.mode = kFujitsuAcModeCool;
        acUnit.fanSpeed = kFujitsuAcFanAuto;
        acUnit.on();
        Edited = true;
      } else if (temp <= 10.0f) {
        acUnit.temp = 16.0f;
        acUnit.mode = kFujitsuAcModeHeat;
        acUnit.fanSpeed = kFujitsuAcFanAuto;
        acUnit.on();
        Edited = true;
      }
    }
    LastUpdate = TimeNow;
  }
}

/// Handles the AC Unit state switching if human presence detection is turned on at the menu level.
void presenceHandler() {
  TimeNow = millis();
  if (TimeNow - LastPresenceStateSwitch > PRESENCE_SWITCH_INTERVAL * 60 * 1000 && acUnit.presence) {
    GotPresence = (digitalRead(LD2410_PIN) == HIGH);
    if (GotPresence && acUnit.state == kFujitsuAcCmdTurnOff) { // Turns on the AC Unit since we got presence in the room and it's currently off.
      Serial.println("Presence detected: AC state on");
      acUnit.on();
      LastPresenceStateSwitch = TimeNow;
      Edited = true;
    } else if (!GotPresence && acUnit.state != kFujitsuAcCmdTurnOff) { // Turns off the AC Unit after we don't have presence anymore and it's currently on.
      Serial.println("Presence not detected: AC state off");
      acUnit.off();
      Edited = true;
    }
  }
}

/// Changes between menus by applying new boundaries to the encoder.
/// @param[in] min New menu's lower boundary, applied to MenuMin.
/// @param[in] max New menu's upper boundary, applied to MenuMax.
/// @param[in] current New menu's current position, applied to CurrentEncoderValue and PrevEcnoderValue.
/// Refreshes the menu output on the display.
void changeMenu(int16_t min, int16_t max, int16_t current) {
  NewEncoder::EncoderState currentEncoderState;
  encoder.getState(currentEncoderState);
  if (encoder.newSettings(min, max, current, currentEncoderState)) {
    MenuMin = min;
    MenuMax = max;
    CurrentEncoderValue = currentEncoderState.currentValue;
    PrevEncoderValue = CurrentEncoderValue;
    Edited = true;
  }
}

/// Handles displaying the menus on the 1602 LCD.
/// Called from loop() without delays.
/// Will update the menu when either the encoder is rotated, its button is pressed or the displayed data changed in the background (edited).
/// Visually (on the display) the menu may seem to have submenu, but it's really just one entity, of which you can access only the portion between current lower and upper boundaries (MenuMin and MenuMax).
void menuHandler() {
  if (Rotated || Pressed || Edited) {
    if (Edited)
      Edited = false;

    switch (CurrentEncoderValue) {
      // 0-Default menu:
      //    Line 0: AC state, AC setpoint
      //    Line 1: Ambient temp, RH
      // Onclick: switch on/off
      case 0:
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print(acUnit.stateToString().c_str());

        lcd.setCursor(6, 0);
        lcd.print(acUnit.tryGetSetpoint().c_str());

        lcd.setCursor(0, 1);
        lcd.print(ambientDataToString(dht.readTemperature(), dht.readHumidity()));

        if (Pressed) {
          acUnit.switchState();
          Edited = true;
        }
        break;
      // 1-Mode menu
      //    Line 0: Current mode
      // Onclick: mode submenu, works on Line 1
      case 1:
        lcd.clear();
        lcd.setCursor(2, 0);
        lcd.print("MODE:");
        lcd.setCursor(8, 0);
        lcd.print(FujiAC::modeToString(acUnit.mode).c_str());
        if (Pressed) {
          changeMenu(5, 9, 5 + acUnit.mode);
          Edited = true;
        }
        break;
      // 2-Setpoint menu:
      //    Line 0: Current setpoint
      // Onclick: setpoint submenu, works on Line 1
      case 2:
        lcd.clear();
        lcd.setCursor(2, 0);
        lcd.print("SET:");
        lcd.setCursor(7, 0);
        lcd.print(acUnit.currentSetpointToString().c_str());
        if (Pressed) {
          changeMenu(20, 48, 20 + FujiAC::setpointToEncoder(acUnit.temp));
          Edited = true;
        }
        break;
      // 3-Fan menu:
      //    Line 0: Current fan speed
      // Onclick: fan submenu, work on Line 1
      case 3:
        lcd.clear();
        lcd.setCursor(2, 0);
        lcd.print("FAN:");
        lcd.setCursor(7, 0);
        lcd.print(FujiAC::fanSpeedToString(acUnit.fanSpeed).c_str());
        if (Pressed) {
          changeMenu(10, 14, 10 + acUnit.fanSpeed);
          Edited = true;
        }
        break;
      case 4:
        lcd.clear();
        lcd.setCursor(4, 0);
        lcd.print("PRESENCE");
        lcd.setCursor(0, 1);
        lcd.print("DETECTION: ");
        lcd.setCursor(11, 1);
        lcd.print(acUnit.presenceToString());
        if (Pressed) {
          acUnit.switchPresence();
          Edited = true;
        }
        break;
      ////////////////
      // MODE SUBMENU
      // Mode Auto
      case 5:
      // Mode Cool
      case 6:
      // Mode Dry
      case 7:
      // Mode Fan
      case 8:
      // Mode Heat
      case 9:
        lcd.setCursor(4, 1);
        lcd.print(acUnit.modeToString(CurrentEncoderValue - MenuMin));
        if (Pressed) {
          acUnit.mode = CurrentEncoderValue - MenuMin;
          acUnit.sendCommand();
          changeMenu(0, 4, 1);
          Edited = true;
        }
        break;
      ////////////////
      // FAN SUBMENU
      // Fan Auto
      case 10:
      // Mode High
      case 11:
      // Mode Medium
      case 12:
      // Mode Low
      case 13:
      // Mode Quiet
      case 14:
        lcd.setCursor(4, 1);
        lcd.print(acUnit.fanSpeedToString(CurrentEncoderValue - MenuMin));
        if (Pressed) {
          acUnit.fanSpeed = CurrentEncoderValue - MenuMin;
          acUnit.sendCommand();
          changeMenu(0, 4, 3);
          Edited = true;
        }
        break;
      case 20:
      case 21:
      case 22:
      case 23:
      case 24:
      case 25:
      case 26:
      case 27:
      case 28:
      case 29:
      case 30:
      case 31:
      case 32:
      case 33:
      case 34:
      case 35:
      case 36:
      case 37:
      case 38:
      case 39:
      case 40:
      case 41:
      case 42:
      case 43:
      case 44:
      case 45:
      case 46:
      case 47:
      case 48:
        lcd.setCursor(4, 1);
        float_t newset = FujiAC::encoderToSetpoint(CurrentEncoderValue - MenuMin);
        lcd.print(newset);
        if (Pressed) {
          acUnit.temp = newset;
          acUnit.sendCommand();
          changeMenu(0, 4, 2);
          Edited = true;
        }
        break;
    }
  }

  if (Pressed)
    Pressed = false;
  if (Rotated)
    Rotated = false;
}

void setup() {
  // LCD init
  lcd.init();
  lcd.clear();
  lcd.backlight();

  // DHT init
  pinMode(DHT_PIN, INPUT);
  dht.begin();

  // LD2410C presence sensor init
  pinMode(LD2410_PIN, INPUT);

  // AC unit init
  ac.begin();
  acUnit = FujiAC();
  
  // Serial init
  Serial.begin(115200);

  // Encoder init
  NewEncoder::EncoderState state;
  delay(200);
  if (!encoder.begin()) {
    Serial.println("Encoder Failed to Start. Check pin assignments and available interrupts. Aborting.");
    while (1) {
      yield();
    }
  } else {
    encoder.getState(state);
    PrevEncoderValue = state.currentValue;
  }
  pinMode(SW, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(SW), press, FALLING);
}

void loop() {
  NewEncoder::EncoderState currentEncoderState;

  if (encoder.getState(currentEncoderState)) {
    CurrentEncoderValue = currentEncoderState.currentValue;
    if (CurrentEncoderValue != PrevEncoderValue) { // If this condition is met, the encoder rotated between the lower and the upper bound without surpassing them
      PrevEncoderValue = CurrentEncoderValue;
      Rotated = true;
    } else
      switch (currentEncoderState.currentClick) {
        // Handles rotation to the right when the menu's upper bound is reached by jumping to the lower bound
        case NewEncoder::UpClick:
          if (encoder.newSettings(MenuMin, MenuMax, MenuMin, currentEncoderState)) {
            CurrentEncoderValue = currentEncoderState.currentValue;
            PrevEncoderValue = CurrentEncoderValue;
            Rotated = true;
          }
          break;
        // Handles rotation to the left when the menu's lower bound is reached by jumping to the upper bound
        case NewEncoder::DownClick:
          if (encoder.newSettings(MenuMin, MenuMax, MenuMax, currentEncoderState)) {
            CurrentEncoderValue = currentEncoderState.currentValue;
            PrevEncoderValue = CurrentEncoderValue;
            Rotated = true;
          }
          break;

        default:
          break;
      }
  }

  menuHandler();
  ambientDataHandler();
  presenceHandler();
}