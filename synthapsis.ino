#include <MozziGuts.h>
#include <Oscil.h>
#include <tables/sin2048_int8.h>
#include <EventDelay.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_MCP23X17.h>

class Button {
  private:
    int pin;
    bool lastButtonState;

  public:
    Button(int pin) {
      this->pin = pin;
      this->lastButtonState = LOW;
    }

    void begin() {
      pinMode(pin, INPUT_PULLDOWN);
    }

    bool update() {
      bool currentButtonState = digitalRead(pin);
      bool wasPressed = (currentButtonState == HIGH && this->lastButtonState == LOW);
      this->lastButtonState = currentButtonState;
      return wasPressed;
    }
};

// Hardware
const int BPM_POT_PIN = 34;
const int LOOP_LED_PIN = 33;
const int LOOP_BTN_PIN = 32;
const int PAUSE_BTN_PIN = 4;
const int I2C_SDA_PIN = 21;
const int I2C_SCL_PIN = 22;

const int DISPLAY_WIDTH = 128;
const int DISPLAY_HEIGHT = 64;

const int DISPLAY_ADDR = 0x3C;
const int MCP1_ADDR = 0x27;
const int MCP2_ADDR = 0x26;

// Mutex
portMUX_TYPE matrixMux = portMUX_INITIALIZER_UNLOCKED;

// Consts
enum Note {
  DO,
  DO_SHARP,
  RE,
  RE_SHARP,
  MI,
  FA,
  FA_SHARP,
  SOL,
  SOL_SHARP,
  LA,
  LA_SHARP,
  SI,
  REST,

  NUM_NOTES
};
const int NOTES_FREQ[] = {262, 277, 294, 311, 330, 349, 370, 392, 415, 440, 466, 494, 0};
Oscil <SIN2048_NUM_CELLS, AUDIO_RATE> aSin(SIN2048_DATA);
const int UPDATE_CONTROL_FREQ = 64;
const int MIN_BPM = 50;
const int MAX_BPM = 1000;
const int BAR_LENGTH = 16;

// Effects
EventDelay noteDuration;

// Vars
Note currentNote;
float markovMatrix[NUM_NOTES][NUM_NOTES];
int bpm;
float volume;
bool isLoop;
bool triggeredLoop;
bool isPause;
Note barNotes[BAR_LENGTH];
int barIndex;
Button loopBtn(LOOP_BTN_PIN);
Button pauseBtn(PAUSE_BTN_PIN);
Adafruit_SSD1306 display(DISPLAY_WIDTH, DISPLAY_HEIGHT, &Wire, -1);
Adafruit_MCP23X17 mcp1;
Adafruit_MCP23X17 mcp2;

void nextNote() {
  float randomNumber = (float) esp_random() / (float) UINT32_MAX;
  // Get random note using CDF
  float sum = 0;
  for (int i=0; i<NUM_NOTES; i++) {
    sum += markovMatrix[currentNote][i];
    if (sum >= randomNumber || i == NUM_NOTES - 1) {
      currentNote = (Note)i;
      break;
    }
  }
}

void playCurrentNote() {
  if (currentNote != REST) {
    aSin.setFreq(NOTES_FREQ[currentNote]);
  }
}

void setBpm() {
  int bpmPotValue = analogRead(BPM_POT_PIN);
  float normalized = (float)bpmPotValue / 4095.0;
  float cubed = normalized * normalized * normalized;
  bpm = (cubed * (MAX_BPM - MIN_BPM)) + MIN_BPM;
  noteDuration.set(60000 / bpm);
}

const char* getNoteString(Note note) {
  switch (note) {
    case DO: return "DO";
    case DO_SHARP: return "DO#";
    case RE: return "RE";
    case RE_SHARP: return "RE#";
    case MI: return "MI";
    case FA: return "FA";
    case FA_SHARP: return "FA#";
    case SOL: return "SOL";
    case SOL_SHARP: return "SOL#";
    case LA: return "LA";
    case LA_SHARP: return "LA#";
    case SI: return "SI";
    case REST: return "-";
    default: return "";
  }
}

void displayCurrentNote() {
  const char* noteText = getNoteString(currentNote);
  display.setTextSize(2);
  display.setTextColor(WHITE);

  int16_t x1, y1;
  uint16_t w, h;
  display.getTextBounds(noteText, 0, 0, &x1, &y1, &w, &h);
  
  int16_t x = (DISPLAY_WIDTH - w) / 2;
  int16_t y = (DISPLAY_HEIGHT - h) / 2;
  
  display.setCursor(x, y);
  display.print(noteText);
}

void displayIsLoop() {
  display.setTextSize(1);
  if (!isLoop) display.setTextColor(WHITE);
  else display.setTextColor(BLACK, WHITE);
  display.setCursor(0, 0);
  display.print(" L ");
}

void displayIsPause() {
  if (isPause) {
    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.setCursor(DISPLAY_WIDTH - 3 * 6, 0);
    display.print(" P ");
  }
}

void displayBpm() {
  display.setTextSize(1);
  display.setTextColor(WHITE);
  int digits = (int)log10(bpm) + 1;
  int textWidth = (5 + digits) * 6;
  int x = DISPLAY_WIDTH - textWidth - 2;
  display.setCursor(x, 54);
  display.print("BPM: ");
  display.print(bpm);
}

void displayBarIndex() {
  display.setTextSize(1);
  display.setTextColor(WHITE);
  int digits = ((int)log10(barIndex+1) + 1) + ((int)log10(BAR_LENGTH) + 1);
  int textWidth = (1 + digits) * 6;
  display.setCursor(2, 54); 
  display.print(barIndex+1);
  display.print("/");
  display.print(BAR_LENGTH);
}

void updateDisplay() {
  display.clearDisplay();
  displayCurrentNote();
  displayIsLoop();
  displayIsPause();
  displayBpm();
  displayBarIndex();
  display.display();
}

void updateMarkovMatrix() {
  // Reset all the mcp1 pins
  mcp1.writeGPIOAB(0x0000);

  float tmpMatrix[NUM_NOTES][NUM_NOTES];

  for (int i=0; i<NUM_NOTES; i++) {
    mcp1.writeGPIOAB(1 << i);

    uint16_t mcp2States = mcp2.readGPIOAB();
    int counter = 0;

    for (int j=0; j<NUM_NOTES; j++) {
      int linked = (mcp2States >> j) & 0x01;
      tmpMatrix[i][j] = linked;
      counter += linked;
    }

    // Normalize
    if (counter > 0) {
      float invCounter = 1.0 / (float)counter;
      for (int j=0; j<NUM_NOTES; j++) tmpMatrix[i][j] *= invCounter;
    } else {
      // Fallback to REST if nothing is connected
      tmpMatrix[i][REST] = 1;
    }
  }

  // Update the real Markov matrix
  portENTER_CRITICAL(&matrixMux);
  for(int i=0; i<NUM_NOTES; i++) {
    for(int j=0; j<NUM_NOTES; j++) {
      markovMatrix[i][j] = tmpMatrix[i][j];
    }
  }
  portEXIT_CRITICAL(&matrixMux);

  mcp1.writeGPIOAB(0x0000);
}

void hardwareUpdateTask(void* pvParameters) {
  while (true) {
    updateMarkovMatrix();
    setBpm();
    updateDisplay();
    vTaskDelay(pdMS_TO_TICKS(50));
  }
}

void setup() {
  // Env begins
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  Wire.setClock(400000);
  Serial.begin(115200);

  // Init loop led and button
  pinMode(LOOP_LED_PIN, OUTPUT);
  loopBtn.begin();

  // Init display
  if (!display.begin(SSD1306_SWITCHCAPVCC, DISPLAY_ADDR)) {
    Serial.println(F("SSD1306 initialization failed"));
    while (1);
  }
  display.clearDisplay();

  // Init MCPs
  if (!mcp1.begin_I2C(MCP1_ADDR)) {
    Serial.println("MCP1 initialization failed");
    while (1);
  }

  if (!mcp2.begin_I2C(MCP2_ADDR)) {
    Serial.println("MCP2 initialization failed");
    while (1);
  }

  for (int i=0; i<NUM_NOTES; i++) {
    mcp1.pinMode(i, OUTPUT);
    mcp2.pinMode(i, INPUT);
  }
  
  // Init vars
  currentNote = REST;
  bpm = 100;
  volume = 1.5;
  isLoop = false;
  triggeredLoop = false;
  isPause = true;
  for (int i=0; i<BAR_LENGTH; i++) barNotes[i] = REST;
  barIndex = 0;

  // Init Markov matrix
  for (int i=0; i<NUM_NOTES; i++) {
    for (int j=0; j<NUM_NOTES; j++) {
      markovMatrix[i][j] = 0.0;
    }
  }

  xTaskCreatePinnedToCore(hardwareUpdateTask, "HW_Task", 10000, NULL, 1, NULL, 0);

  startMozzi(UPDATE_CONTROL_FREQ);

  noteDuration.start();
  playCurrentNote();
}

void updateControl() {
  if (loopBtn.update()) {
    triggeredLoop = !triggeredLoop;
  }

  if (pauseBtn.update()) {
    isPause = !isPause;
    barIndex = 0;
  }

  digitalWrite(LOOP_LED_PIN, isLoop);

  if (noteDuration.ready() && !isPause) {
    barIndex = (barIndex + 1) % BAR_LENGTH;

    if (triggeredLoop && barIndex == 0) {
      isLoop = !isLoop;
      triggeredLoop = false;
    } else if (triggeredLoop) {
      digitalWrite(LOOP_LED_PIN, !isLoop);
    }

    if (!isLoop) {
      portENTER_CRITICAL(&matrixMux);
      nextNote();
      portEXIT_CRITICAL(&matrixMux);

      barNotes[barIndex] = currentNote;
    } else {
      currentNote = barNotes[barIndex];
    }
    
    playCurrentNote();
    noteDuration.start();
  }
}

int updateAudio() {
  if (currentNote == REST || isPause) {
    return 0;
  }
  // FIXME: not handled correctly.
  return (int) (aSin.next() * volume);
}

void loop() {
  audioHook();
}
