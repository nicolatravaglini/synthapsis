#include <MozziGuts.h>
#include <Oscil.h>
#include <tables/sin2048_int8.h>
#include <EventDelay.h>

// Hardware

// Consts
enum Notes {
  DO,
  RE,
  MI,
  FA,
  SOL,
  LA,
  SI,

  NUM_NOTES
};
const int notesFrequencies[] = {262, 294, 330, 349, 392, 440, 494};
Oscil <SIN2048_NUM_CELLS, AUDIO_RATE> aSin(SIN2048_DATA);
const int updateControlFreq = 64;

// Effects
EventDelay noteDelay;

// Vars
int currentNote = DO;
float markovMatrix[NUM_NOTES][NUM_NOTES] = {0};
int bpm = 100;
int volume = 2;

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
  aSin.setFreq(notesFrequencies[currentNote]);
}

void setup() {
  markovMatrix[DO][MI] = 0.75;
  markovMatrix[DO][SOL] = 0.25;
  markovMatrix[MI][SOL] = 0.75;
  markovMatrix[MI][SI] = 0.25;
  markovMatrix[SOL][MI] = 0.1;
  markovMatrix[SOL][DO] = 0.9;
  markovMatrix[SI][MI] = 1;

  startMozzi(updateControlFreq);
  noteDelay.set(60000 / bpm);
  noteDelay.start();

  aSin.setFreq(notesFrequencies[currentNote]);

  Serial.begin(9600);
}

void updateControl() {
  if (noteDelay.ready()) {
    nextNote();
    noteDelay.start();
  }
}

int updateAudio() {
  return (int) (aSin.next() * volume);
}

void loop() {
  audioHook();
}
