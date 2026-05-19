#include <MozziGuts.h>
#include <Oscil.h>
#include <tables/sin2048_int8.h>
#include <EventDelay.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

class CircularArray {
  private:
    int array[256];
    int start_index;
    int end_index;
  
  public:
    int length;
    
    CircularArray(int length) {
      if (length > 256) length = 256;
      else if (length < 1) length = 1;
      
      this->length = length;
      this->start_index = 0;
      this->end_index = this->length-1;
    }

    void fillArray(int value) {
      for (int i=0; i<this->length; i++) {
        this->array[i] = value;
      }
    }

    void enqueue(int value) {
      this->start_index = (this->start_index + 1) % this->length;
      this->end_index = (this->end_index + 1) % this->length;
      this->array[this->end_index] = value;
    }

    int getIndexValue(int index) {
      if (index < 0 || index >= this->length) return -1;
      return this->array[(this->start_index + index) % this->length];
    }
};

class ToggleButton {
  private:
    int pin;
    bool toggleState;
    bool lastButtonState;

  public:
    ToggleButton(int pin) {
      this->pin = pin;
      this->toggleState = false;
      this->lastButtonState = LOW;
    }

    void begin() {
      pinMode(pin, INPUT_PULLDOWN);
    }

    bool update() {
      bool currentButtonState = digitalRead(pin);
      if (currentButtonState == HIGH && this->lastButtonState == LOW) {
        this->toggleState = !this->toggleState;
      }
      this->lastButtonState = currentButtonState;
      return this->toggleState;
    }
};

// Hardware
const int BPM_POT_PIN = 34;
const int LOOP_LED_PIN = 33;
const int LOOP_BTN_PIN = 32;
const int DISPLAY_WIDTH = 128;
const int DISPLAY_HEIGHT = 64;

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

// Effects
EventDelay noteDuration;

// Vars
Note currentNote;
CircularArray lastNotes(16);
float markovMatrix[NUM_NOTES][NUM_NOTES];
int bpm;
float volume;
bool isLoop;
int loopNoteIndex;
ToggleButton loopBtn(LOOP_BTN_PIN);
Adafruit_SSD1306 display(DISPLAY_WIDTH, DISPLAY_HEIGHT, &Wire, -1);

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

void setup() {
  // Init loop led and button
  pinMode(LOOP_LED_PIN, OUTPUT);
  loopBtn.begin();

  // Init display
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;);
  }
  display.clearDisplay();
  
  currentNote = DO;
  lastNotes.fillArray(REST);
  bpm = 100;
  volume = 1.5;
  isLoop = false;
  loopNoteIndex = 0;

  for (int i=0; i<NUM_NOTES; i++) {
    for (int j=0; j<NUM_NOTES; j++) {
      markovMatrix[i][j] = 0.0;
    }
  }

  markovMatrix[DO][DO] = 0.40;
  markovMatrix[DO][REST] = 0.50;
  markovMatrix[DO][RE] = 0.10;
  
  markovMatrix[RE][MI] = 1.00;
  markovMatrix[MI][FA] = 1.00;

  markovMatrix[FA][FA] = 0.40;
  markovMatrix[FA][REST] = 0.40;
  markovMatrix[FA][SOL] = 0.20;

  markovMatrix[SOL][FA] = 0.50;
  markovMatrix[SOL][MI] = 0.40;
  markovMatrix[SOL][REST] = 0.10;

  markovMatrix[MI][FA] = 1.00;

  markovMatrix[REST][REST] = 0.70;
  markovMatrix[REST][DO] = 0.25;
  markovMatrix[REST][FA] = 0.05;

  startMozzi(UPDATE_CONTROL_FREQ);
  setBpm();

  noteDuration.start();
  playCurrentNote();

  Serial.begin(115200);
}

void updateControl() {
  isLoop = loopBtn.update();

  digitalWrite(LOOP_LED_PIN, isLoop ? HIGH : LOW);

  if (noteDuration.ready()) {
    if (!isLoop) {
      nextNote();
      lastNotes.enqueue(currentNote);
    } else {
      currentNote = (Note)lastNotes.getIndexValue(loopNoteIndex);
      loopNoteIndex = (loopNoteIndex + 1) % lastNotes.length;
    }
    setBpm();
    display.clearDisplay();
    displayCurrentNote();
    displayIsLoop();
    displayBpm();
    display.display();
    playCurrentNote();
    noteDuration.start();
  }
}

int updateAudio() {
  if (currentNote == REST) {
    return 0;
  }
  // FIXME: not handled correctly.
  return (int) (aSin.next() * volume);
}

void loop() {
  audioHook();
}
