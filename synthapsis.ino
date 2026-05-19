#include <MozziGuts.h>
#include <Oscil.h>
#include <tables/sin2048_int8.h>
#include <EventDelay.h>

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

// Consts
enum NOTES {
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
int currentNote;
CircularArray lastNotes(16);
float markovMatrix[NUM_NOTES][NUM_NOTES] = {0};
int bpm;
float volume;
bool isLoop;
int loopNoteIndex;
ToggleButton loopBtn(LOOP_BTN_PIN);

void nextNote() {
  float randomNumber = (float) esp_random() / (float) UINT32_MAX;
  // Get random note using CDF
  float sum = 0;
  for (int i=0; i<NUM_NOTES; i++) {
    sum += markovMatrix[currentNote][i];
    if (sum >= randomNumber || i == NUM_NOTES - 1) {
      currentNote = i;
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

void setup() {
  pinMode(LOOP_LED_PIN, OUTPUT);
  loopBtn.begin();

  currentNote = DO;
  lastNotes.fillArray(REST);
  bpm = 100;
  volume = 1.5;
  isLoop = false;
  loopNoteIndex = 0;

  markovMatrix[DO][RE] = 0.40;
  markovMatrix[DO][MI] = 0.20;
  markovMatrix[DO][SOL] = 0.20;
  markovMatrix[DO][REST] = 0.20;
  
  markovMatrix[DO_SHARP][RE] = 1.00;
  
  markovMatrix[RE][MI] = 0.30;
  markovMatrix[RE][FA] = 0.30;
  markovMatrix[RE][DO] = 0.20;
  markovMatrix[RE][REST] = 0.20;
  
  markovMatrix[RE_SHARP][MI] = 1.00;

  markovMatrix[MI][SOL] = 0.40;
  markovMatrix[MI][FA] = 0.20;
  markovMatrix[MI][DO] = 0.20;
  markovMatrix[MI][REST] = 0.20;

  markovMatrix[FA][SOL] = 0.40;
  markovMatrix[FA][RE] = 0.20;
  markovMatrix[FA][FA_SHARP] = 0.20;
  markovMatrix[FA][REST] = 0.20;
  
  markovMatrix[FA_SHARP][SOL] = 1.00;
  
  markovMatrix[SOL][LA] = 0.30;
  markovMatrix[SOL][MI] = 0.30;
  markovMatrix[SOL][DO] = 0.20;
  markovMatrix[SOL][REST] = 0.20;
  
  markovMatrix[SOL_SHARP][LA] = 1.00;
  
  markovMatrix[LA][DO] = 0.40;
  markovMatrix[LA][SOL] = 0.20;
  markovMatrix[LA][SI] = 0.20;
  markovMatrix[LA][REST] = 0.20;
  
  markovMatrix[LA_SHARP][SI] = 1.00;
  
  markovMatrix[SI][DO] = 0.60;
  markovMatrix[SI][LA] = 0.20;
  markovMatrix[SI][REST] = 0.20;

  markovMatrix[REST][DO] = 0.40;
  markovMatrix[REST][MI] = 0.30;
  markovMatrix[REST][SOL] = 0.30;

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
      currentNote = lastNotes.getIndexValue(loopNoteIndex);
      loopNoteIndex = (loopNoteIndex + 1) % lastNotes.length;
    }
    setBpm();
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
