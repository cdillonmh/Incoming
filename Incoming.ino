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
#define FASTEROIDCOLOR RED
#define MISSILECOLOR WHITE
#define EXPLOSIONCOLOR ORANGE
#define DAMAGECOLOR RED

// Game States
enum gameStates {SETUP, SINGLEPLAYER, MULTIPLAYER, GAMEOVER};
byte gameState = SETUP;
byte cached_gameState[FACE_COUNT];

// Blink States
enum blinkStates {L0, L1, L2, L3};
byte blinkState = L0;
bool isEarth = false;

// Projectile Types
enum projectiles {NOTHING, AST4, AST3, AST2, AST1, FAST2, FAST1, MISSILE};

/*
 * BITWISE FACE COMMS
 * 
 *      128     64      32      16      8     4     2     1
 *     <-gameState->  <-blinkState-> <-projectileType-> <-ACK->
 */

// Directionality
enum faceDirections {EDGE, OUTWARD, INWARD, FORWARD, BACKWARD, UNDETERMINED};
byte faceDirection[FACE_COUNT];

void setup() {
  randomize(); //Seed RNG
}

void loop() {
  switch (gameState) {
    case SETUP:
      checkBlinkState(); //  Uses outside edge to help determine center Earth blink
      checkStartGame();
      break;
    case SINGLEPLAYER:
    case MULTIPLAYER:
      tempCheckEndGame();
      break;
    case GAMEOVER:
      checkResetGame();
      break;
  }

  incomingCommsHandler();
  outgoingCommsHandler();
  displayHandler();

  consumeErrantClicks();
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
  determineDirectionality ();
}

void determineDirectionality () {
  // Clear directionality determinations
  FOREACH_FACE(f) {
    faceDirection[f] = UNDETERMINED;
  }

  // Find outwards and inwards
  FOREACH_FACE(f) {
    if (isValueReceivedOnFaceExpired(f)) {
      faceDirection[f] = EDGE;
    }
    else {
      byte faceState = getBlinkStateOnFace(f);
      
      if (faceState > blinkState) {
        faceDirection[f] = INWARD;
      }
      else if (faceState < blinkState) {
        faceDirection[f] = OUTWARD;
      }
    }
  }

  // Find forwards and backwards
  FOREACH_FACE(f) {
    byte currentFace = f;
    byte nextFace = ((f+1)%6);
    if (faceDirection[currentFace] == UNDETERMINED) {
      if (faceDirection[nextFace] == INWARD) {
        faceDirection[currentFace] = FORWARD;
      }
      else {
        faceDirection[currentFace] = BACKWARD;
      }
    }
  }
}

// Check if game should start
void checkStartGame () {
  if (buttonSingleClicked() && isEarth){
    gameState = SINGLEPLAYER;
  }
  else if (buttonDoubleClicked() && isEarth) {
    gameState = MULTIPLAYER;
  }
}

// Temp end game on longpress
void tempCheckEndGame () {
  if (buttonLongPressed() && isEarth){
    gameState = GAMEOVER;
  }
}

// End GAMEOVER state and return to SETUP
void checkResetGame () {
  if (buttonSingleClicked() && isEarth) {
    gameState = SETUP;
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

void directionalityTestDisplay (){
  FOREACH_FACE(f) {
    switch (faceDirection[f]) {
      case EDGE:
        setColorOnFace(WHITE, f);
        break;
      case OUTWARD:
        setColorOnFace(ORANGE, f);
        break;
      case INWARD:
        setColorOnFace(YELLOW, f);
        break;
      case FORWARD:
        setColorOnFace(GREEN, f);
        break;
      case BACKWARD:
        setColorOnFace(BLUE, f);
        break;
      case UNDETERMINED:
        setColorOnFace(OFF, f);
        break;
    }
  }
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

void incomingCommsHandler() {
  // Handle Game State Changes
  FOREACH_FACE(face) {
    if (isValueReceivedOnFaceExpired(face)) continue;

    byte face_value = getGameStateOnFace(face);

    if (cached_gameState[face] != face_value) {
      // State changed. Update our own.
      gameState = face_value;
    }
    cached_gameState[face] = face_value;
  }
}

void outgoingCommsHandler() {
  setValueSentOnAllFaces((gameState << 6) + (blinkState << 4));
}

void inGameDisplay () {
  if (isEarth) {
    FOREACH_FACE(f) {
      if (f%2 > 0) {
        setColorOnFace(EARTHLAND, f);
      }
      else {
        setColorOnFace(EARTHSEA, f);
      }
    }
  }
  else {
  setColor (SPACECOLOR);
  }
}

void gameoverDisplay () {
  if (isEarth) {
    setColor(CYAN);
  }
  else {
    setColor(MAGENTA);
  }
}

void displayHandler() {
  switch (gameState) {
    case SETUP: 
      layerTestDisplay(); // Verifies layer functionality
      //directionalityTestDisplay(); // Verifies directionality functionality
      break;
    case SINGLEPLAYER:
    case MULTIPLAYER:
      inGameDisplay();
      break;
    case GAMEOVER:
      gameoverDisplay();
      break;
  }
}

void consumeErrantClicks () {
  buttonPressed();
  buttonSingleClicked();
  buttonDoubleClicked();
  buttonLongPressed();
  hasWoken();
}
