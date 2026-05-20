#include <Arduino.h>
#include <Servo.h>
#include <Stepper.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <TM1637Display.h>

const byte FLOOR_COUNT = 4;
const long STEPS_PER_FLOOR = 4096L;
const int MOTOR_STEPS_PER_REV = 2048;

const byte externalButtonPins[FLOOR_COUNT] = {2, 3, 4, 5};
const byte internalButtonPins[FLOOR_COUNT] = {6, 7, 8, 9};

const byte STEPPER_IN1 = 10;
const byte STEPPER_IN2 = 11;
const byte STEPPER_IN3 = 12;
const byte STEPPER_IN4 = 13;

const byte SERVO_PIN = A1;
const byte TM1637_CLK = A2;
const byte TM1637_DIO = A3;

const int DOOR_CLOSED_ANGLE = 0;
const int DOOR_OPEN_ANGLE = 90;
const unsigned long DOOR_MOVE_MS = 800;
const unsigned long DOOR_WAIT_MS = 1500;
const unsigned long DEBOUNCE_MS = 40;

// Asansor durum makinesi: hareket ve kapi islemleri ayri durumlarda yonetilir.
enum ElevatorState {
  IDLE,
  MOVING_UP,
  MOVING_DOWN,
  DOOR_OPENING,
  DOOR_WAITING,
  DOOR_CLOSING
};

Servo doorServo;
Stepper cabinStepper(MOTOR_STEPS_PER_REV, STEPPER_IN1, STEPPER_IN3, STEPPER_IN2, STEPPER_IN4);
Adafruit_SSD1306 oled(128, 64, &Wire, -1);
TM1637Display segmentDisplay(TM1637_CLK, TM1637_DIO);

ElevatorState state = IDLE;

// Ic butonlar hedef kat istegi, dis butonlar asansor cagirma istegidir.
bool internalRequests[FLOOR_COUNT] = {false, false, false, false};
bool externalRequests[FLOOR_COUNT] = {false, false, false, false};

// Debounce degiskenleri, mekanik buton titresiminden dogan hatali basmalari engeller.
bool lastExternalReading[FLOOR_COUNT] = {HIGH, HIGH, HIGH, HIGH};
bool lastInternalReading[FLOOR_COUNT] = {HIGH, HIGH, HIGH, HIGH};
bool stableExternalReading[FLOOR_COUNT] = {HIGH, HIGH, HIGH, HIGH};
bool stableInternalReading[FLOOR_COUNT] = {HIGH, HIGH, HIGH, HIGH};
unsigned long externalChangedAt[FLOOR_COUNT] = {0, 0, 0, 0};
unsigned long internalChangedAt[FLOOR_COUNT] = {0, 0, 0, 0};

byte currentFloor = 0;
byte targetFloor = 0;
int travelDirection = 0;
bool stopIsExternalCall = false;
long stepsToNextFloor = 0;
unsigned long doorTimer = 0;

void updateDisplays();
void readButtons();
void handleButton(byte floor, bool internalButton);
void clearStopRequest(byte floor);
void startDoorCycle();
void chooseNextTarget();
void moveToTarget();
void prepareNextFloorMove();
bool hasAnyRequest();
int firstInternalOnPath();
int firstExternalRequest();
const char *stateText();

void setup() {
  for (byte floor = 0; floor < FLOOR_COUNT; floor++) {
    pinMode(externalButtonPins[floor], INPUT_PULLUP);
    pinMode(internalButtonPins[floor], INPUT_PULLUP);
  }

  cabinStepper.setSpeed(12);
  doorServo.attach(SERVO_PIN);
  doorServo.write(DOOR_CLOSED_ANGLE);

  segmentDisplay.setBrightness(0x0f);

  oled.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  oled.clearDisplay();
  oled.setTextColor(SSD1306_WHITE);

  updateDisplays();
}

void loop() {
  readButtons();

  // Ana durum makinesi: her dongude asansorun mevcut durumuna gore islem yapilir.
  switch (state) {
    case IDLE:
      if (hasAnyRequest()) {
        chooseNextTarget();
      }
      break;

    case MOVING_UP:
    case MOVING_DOWN:
      moveToTarget();
      break;

    case DOOR_OPENING:
      if (millis() - doorTimer >= DOOR_MOVE_MS) {
        state = DOOR_WAITING;
        doorTimer = millis();
        updateDisplays();
      }
      break;

    case DOOR_WAITING:
      if (millis() - doorTimer >= DOOR_WAIT_MS) {
        state = DOOR_CLOSING;
        doorServo.write(DOOR_CLOSED_ANGLE);
        doorTimer = millis();
        updateDisplays();
      }
      break;

    case DOOR_CLOSING:
      if (millis() - doorTimer >= DOOR_MOVE_MS) {
        state = IDLE;
        updateDisplays();
      }
      break;
  }
}

// Tum butonlari debounce ile okur ve gecerli basmalari kaydeder.
void readButtons() {
  const unsigned long now = millis();

  for (byte floor = 0; floor < FLOOR_COUNT; floor++) {
    bool extReading = digitalRead(externalButtonPins[floor]);
    if (extReading != lastExternalReading[floor]) {
      externalChangedAt[floor] = now;
      lastExternalReading[floor] = extReading;
    }
    if (now - externalChangedAt[floor] > DEBOUNCE_MS && extReading != stableExternalReading[floor]) {
      stableExternalReading[floor] = extReading;
      if (stableExternalReading[floor] == LOW) {
        handleButton(floor, false);
      }
    }

    bool intReading = digitalRead(internalButtonPins[floor]);
    if (intReading != lastInternalReading[floor]) {
      internalChangedAt[floor] = now;
      lastInternalReading[floor] = intReading;
    }
    if (now - internalChangedAt[floor] > DEBOUNCE_MS && intReading != stableInternalReading[floor]) {
      stableInternalReading[floor] = intReading;
      if (stableInternalReading[floor] == LOW) {
        handleButton(floor, true);
      }
    }
  }
}

// Istegi dogru listeye ekler: kabin ici hedef veya dis cagri.
void handleButton(byte floor, bool internalButton) {
  if (floor >= FLOOR_COUNT) {
    return;
  }

  if (internalButton) {
    internalRequests[floor] = true;
  } else {
    externalRequests[floor] = true;
  }

  if (state == IDLE && floor == currentFloor) {
    stopIsExternalCall = !internalButton;
    clearStopRequest(floor);
    startDoorCycle();
  }

  updateDisplays();
}

// Siradaki hedef kati secer. Yol ustundeki kabin ici istekler onceliklidir.
void chooseNextTarget() {
  stopIsExternalCall = false;

  if (internalRequests[currentFloor] || externalRequests[currentFloor]) {
    stopIsExternalCall = externalRequests[currentFloor] && !internalRequests[currentFloor];
    clearStopRequest(currentFloor);
    startDoorCycle();
    return;
  }

  int nextInternal = firstInternalOnPath();
  if (nextInternal >= 0) {
    targetFloor = nextInternal;
  } else {
    int nextExternal = firstExternalRequest();
    if (nextExternal < 0) {
      return;
    }
    targetFloor = nextExternal;
    stopIsExternalCall = true;
  }

  travelDirection = targetFloor > currentFloor ? 1 : -1;
  state = travelDirection > 0 ? MOVING_UP : MOVING_DOWN;
  prepareNextFloorMove();
  updateDisplays();
}

// Yeni kabin ici istekleri yakalamak icin motoru kucuk adim paketleriyle ilerletir.
void moveToTarget() {
  int pathStop = firstInternalOnPath();
  if (pathStop >= 0 && pathStop != targetFloor) {
    targetFloor = pathStop;
    stopIsExternalCall = false;
  }

  const int stepChunk = 32;
  int stepsNow = stepsToNextFloor > stepChunk ? stepChunk : stepsToNextFloor;
  cabinStepper.step(stepsNow * travelDirection);
  stepsToNextFloor -= stepsNow;

  if (stepsToNextFloor > 0) {
    return;
  }

  currentFloor = currentFloor + travelDirection;

  bool reachedTarget = currentFloor == targetFloor;
  bool internalStop = internalRequests[currentFloor] && ((travelDirection > 0 && currentFloor <= targetFloor) || (travelDirection < 0 && currentFloor >= targetFloor));

  updateDisplays();

  if (internalStop || reachedTarget) {
    stopIsExternalCall = reachedTarget && stopIsExternalCall && !internalStop;
    clearStopRequest(currentFloor);
    startDoorCycle();
    return;
  }

  prepareNextFloorMove();
}

// Bir kat arasi 28BYJ-48 icin iki tam turdur: 2 * 2048 = 4096 adim.
void prepareNextFloorMove() {
  stepsToNextFloor = STEPS_PER_FLOOR;
}

// Bekleyen kabin ici veya dis cagri istegi olup olmadigini kontrol eder.
bool hasAnyRequest() {
  for (byte floor = 0; floor < FLOOR_COUNT; floor++) {
    if (internalRequests[floor] || externalRequests[floor]) {
      return true;
    }
  }
  return false;
}

// Mevcut hareket yonunde yol ustundeki ilk kabin ici istegi bulur.
int firstInternalOnPath() {
  if (travelDirection > 0) {
    for (byte floor = currentFloor + 1; floor < FLOOR_COUNT; floor++) {
      if (internalRequests[floor]) {
        return floor;
      }
      if (floor == targetFloor) {
        break;
      }
    }
  } else if (travelDirection < 0) {
    for (int floor = currentFloor - 1; floor >= 0; floor--) {
      if (internalRequests[floor]) {
        return floor;
      }
      if (floor == targetFloor) {
        break;
      }
    }
  } else {
    for (byte floor = 0; floor < FLOOR_COUNT; floor++) {
      if (internalRequests[floor]) {
        return floor;
      }
    }
  }

  return -1;
}

// Mevcut gorev bittikten sonra en yakin dis cagriyi secer.
int firstExternalRequest() {
  for (byte distance = 0; distance < FLOOR_COUNT; distance++) {
    if (currentFloor + distance < FLOOR_COUNT && externalRequests[currentFloor + distance]) {
      return currentFloor + distance;
    }
    if (currentFloor >= distance && externalRequests[currentFloor - distance]) {
      return currentFloor - distance;
    }
  }
  return -1;
}

// Kabinin durdugu katta tamamlanan istegi temizler.
void clearStopRequest(byte floor) {
  internalRequests[floor] = false;

  if (stopIsExternalCall) {
    externalRequests[floor] = false;
  }
}

// Kapi acma, bekleme ve kapama dongusunu baslatir.
void startDoorCycle() {
  travelDirection = 0;
  state = DOOR_OPENING;
  doorServo.write(DOOR_OPEN_ANGLE);
  doorTimer = millis();
  updateDisplays();
}

// TM1637 kat gostergesini ve OLED kabin ekranini gunceller.
void updateDisplays() {
  segmentDisplay.showNumberDec(currentFloor, false, 1, 3);

  oled.clearDisplay();
  oled.setTextSize(1);
  oled.setCursor(0, 0);
  oled.print(F("Akilli Asansor"));
  oled.setCursor(0, 14);
  oled.print(F("Kat: "));
  if (currentFloor == 0) {
    oled.print(F("Zemin"));
  } else {
    oled.print(currentFloor);
  }

  oled.setCursor(0, 28);
  oled.print(F("Durum: "));
  oled.print(stateText());

  oled.setCursor(0, 42);
  oled.print(F("Hedef: "));
  if (state == IDLE && !hasAnyRequest()) {
    oled.print(F("-"));
  } else if (targetFloor == 0) {
    oled.print(F("Zemin"));
  } else {
    oled.print(targetFloor);
  }

  oled.setCursor(0, 56);
  oled.print(F("Ic:"));
  for (byte floor = 0; floor < FLOOR_COUNT; floor++) {
    if (internalRequests[floor]) {
      oled.print(floor);
    }
  }
  oled.print(F(" Dis:"));
  for (byte floor = 0; floor < FLOOR_COUNT; floor++) {
    if (externalRequests[floor]) {
      oled.print(floor);
    }
  }
  oled.display();
}

// Mevcut durumu OLED ekranda gosterilecek metne cevirir.
const char *stateText() {
  switch (state) {
    case IDLE:
      return "Bekliyor";
    case MOVING_UP:
      return "Yukari";
    case MOVING_DOWN:
      return "Asagi";
    case DOOR_OPENING:
      return "Kapi aciliyor";
    case DOOR_WAITING:
      return "Kapi acik";
    case DOOR_CLOSING:
      return "Kapi kapaniyor";
  }
  return "";
}
