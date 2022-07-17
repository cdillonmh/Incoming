/*
 * Incoming!
 * for Move38 Blinks
 * By Dillon Hall
 */

// Color Defaults
#define SPACECOLOR OFF
#define EARTHLAND GREEN
#define EARTHSEA BLUE
#define ASTEROIDCOLOR YELLOW
#define MISSILECOLOR WHITE
#define EXPLOSIONCOLOR ORANGE
#define DAMAGECOLOR RED

// Blink States
enum blinkStates {L0, L1, L2, L3};
byte blinkState = L0;
bool isEarth = false;

// Game States
enum gameStates {SETUP, STARTING, GAME, ENDING};
byte gameState = SETUP;
byte cached_gameState[FACE_COUNT];


/*
 * SETUP MODE FACE COMMS
 * 
 *      128     64      32      16      8     4     2     1
 *     <-gameState->  <-blinkState-> 
 */

void setup() {
  randomize(); //Seed RNG
}

void loop() {
  switch (gameState) {
    case SETUP:
      checkBlinkState(); //  Uses outside edge to help determine center Earth blink
  }

  commsHandler();
  displayHandler();
}

// Figure out layer position based on relative position to edge
void checkBlinkState (){
  bool hasEdge = false; // Local variable to check for open edge
  FOREACH_FACE(f) {
    if (isValueReceivedOnFaceExpired(f)) {
      // Found an open edge!
      hasEdge = true;
    }
  }
  if (hasEdge) {
    blinkState = L0;
  }
  else {
    byte lowestAdjacentLayer = getBlinkStateOnFace(0);
    byte lastFaceValue = lowestAdjacentLayer;
    bool allTheSame = true;
    FOREACH_FACE(f) {
      byte faceState = getBlinkStateOnFace(f);
      if (faceState < lowestAdjacentLayer) {
        lowestAdjacentLayer = faceState;
      }
      if (allTheSame && faceState != lastFaceValue) {
        allTheSame = false;
      } else {
        lastFaceValue = faceState;
      }
    }
    if (lowestAdjacentLayer < L3) {
      switch (lowestAdjacentLayer) {
        case L0:
          blinkState = L1;
          break;
        case L1:
          blinkState = L2;
          break;
        case L2:
          blinkState = L3;
          break;
      }
      if (allTheSame) {
        isEarth = true;
      }
      else{
        isEarth = false;
      }
    }
    else{
      blinkState = L3;
      isEarth = false;
    }
  }

  
}

void layerTestDisplay (){
  switch (blinkState) {
    case L0:
      setColor(WHITE);
      break;
    case L1:
      setColor(YELLOW);
      break;
    case L2:
      setColor(ORANGE);
      break;
    case L3:
      setColor(RED);
      break;
  }
  if (isEarth) setColor(GREEN);
}

byte getBlinkStateOnFace (byte face) {
  if (!isValueReceivedOnFaceExpired(face)){
    return ((getLastValueReceivedOnFace(face) >> 4) & 3);
  }
}

byte getGameStateOnFace (byte face) {
  if (!isValueReceivedOnFaceExpired(face)){
    return ((getLastValueReceivedOnFace(face) >> 6) & 3);
  }
}

void commsHandler() {
  setValueSentOnAllFaces((gameState << 6) + (blinkState << 4));
}

void displayHandler() {
  layerTestDisplay();
}
