#include <MozziGuts.h>
#include <Oscil.h>
#include <tables/sin2048_int8.h>
#include <EventDelay.h>

class CircularArray {
  private:
    int array[256];
    int len;
    int start_index;
    int end_index;
  
  public:
    CircularArray(int length) {
      if (length > 256) length = 256;
      else if (length < 1) length = 1;
      
      len = length;
      start_index = 0;
      end_index = len-1;
    }

    void fillArray(int value) {
      for (int i=0; i<len; i++) {
        array[i] = value;
      }
    }

    void enqueue(int value) {
      start_index = (start_index + 1) % len;
      end_index = (end_index + 1) % len;
      array[end_index] = value;
    }

    int getIndexValue(int index) {
      if (index < 0 || index >= len) return -1;
      return array[(start_index + index) % len];
    }
};

// Hardware
const int BPM_POT_PIN = 34;

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
CircularArray lastNotes(8);
float markovMatrix[NUM_NOTES][NUM_NOTES] = {0};
int bpm;
int volume;

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
  currentNote = DO;
  lastNotes.fillArray(REST);
  bpm = 100;
  volume = 2;

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
  if (noteDuration.ready()) {
    nextNote();
    playCurrentNote();
    lastNotes.enqueue(currentNote);
    setBpm();
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
