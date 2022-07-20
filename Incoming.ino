/*
 * Incoming!
 * for Move38 Blinks
 * By Dillon Hall
 */

/*
 *  KNOWN ISSUES:
 *  - Earth repeatedly sending missiles at cooldown rate once requested
 *  - original requester blink exploding with each received missile until passes a new request
 */

/*
 *  NEXT STEPS:
 *  - Create reset function to clear out game variables when game over
 */

/* 
 *  IDEAS:
 *  - Can use blinkState (currently only used in setup) during in-game for something else...
 *    + Global missile cooldown?
 *    + Increasing difficulty levels?
 *    + Other stuff for multiplayer mode?
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

// Game Balance
#define ASTEROIDTRANSITTIMEMS 350
#define MISSILETRANSITTIMEMS 250
#define MISSILECOOLDOWNTIMEMS 750
#define EXPLOSIONTIMEMS 500

// General presets
#define ANIMATIONTIMERMS 1000
//#define PROJECTILETIMEOUTTIMERMS 200

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

// Projectile Handling
byte incomingProjectiles[] = {NOTHING, NOTHING, NOTHING, NOTHING, NOTHING, NOTHING};
byte receivedProjectiles[] = {NOTHING, NOTHING, NOTHING, NOTHING, NOTHING, NOTHING};
byte ACKReceive[] = {0, 0, 0, 0, 0, 0};
byte outgoingProjectiles[] = {NOTHING, NOTHING, NOTHING, NOTHING, NOTHING, NOTHING};
byte projectilesBuffer[] = {NOTHING, NOTHING, NOTHING, NOTHING, NOTHING, NOTHING};
byte ACKSend[] = {0, 0, 0, 0, 0, 0};
byte asteroidType = NOTHING;
byte fasteroidType = NOTHING;
bool hasMissile = false;
bool missileRequested = false;
int missileRequestFace = -1;
byte requestQueue[6] = {7,7,7,7,7,7}; // 7 for none, keeps the array smaller and no chance of confusion with faces
bool isExploding = false;

// Projectile Timers
Timer asteroidTimer;
Timer fasteroidTimer;
Timer missileTimer;
Timer missileCooldownTimer;
Timer explosionTimer;

/*
 * BITWISE FACE COMMS
 * 
 *      128     64      32      16      8     4     2     1
 *     <-gameState->  <-blinkState-> <-projectileType-> <-ACK->
 */

// Directionality
enum faceDirections {EDGE, OUTWARD, INWARD, FORWARD, BACKWARD, UNDETERMINED};
byte faceDirection[FACE_COUNT];

// General Timers
Timer animationTimer;


void setup() {
  randomize(); //Seed RNG
  animationTimer.set(ANIMATIONTIMERMS);
  missileCooldownTimer.set(MISSILECOOLDOWNTIMEMS);
}

void loop() {
  switch (gameState) {
    case SETUP:
      checkBlinkState(); // Uses outside edge to help determine center Earth blink
      checkStartGame();
      break;
    case SINGLEPLAYER:
    case MULTIPLAYER:
      projectileTimerHandler();
      inputHandler();
      tempCheckEndGame();
      break;
    case GAMEOVER:
      checkResetGame();
      break;
  }

  incomingCommsHandler();
  projectileReceiver();
  projectileManager();
  outgoingCommsHandler();
  displayHandler();

  consumeErrantClicks();
}

// Figure out layer position based on relative position to edge
void checkBlinkState (){
  bool hasEdge = false;
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
        missileRequestFace = nextFace;
      }
      else {
        faceDirection[currentFace] = BACKWARD;
      }
    }
  }
}

// Check if game should start
void checkStartGame () {
  if (buttonSingleClicked() && isEarth && !hasWoken()){
    gameState = SINGLEPLAYER;
  }
  else if (buttonDoubleClicked() && isEarth && !hasWoken()) {
    gameState = MULTIPLAYER;
  }
}


// Check and handle expired projectile timers
void projectileTimerHandler () {

  // Handle Missiles
  if (hasMissile && missileTimer.isExpired()) {
    if (!missileRequested && requestQueue[0] != 7) {
      sendProjectileOnFace(MISSILE,getNextRequest());
    }
    else {
      startExplosion();
    }
  }

  // Handle Earth's Missile Cooldown and Queue
  if (isEarth) {
    //processRequestQueue();
    if ((requestQueue[0] != 7) && (missileCooldownTimer.isExpired())) {
      sendProjectileOnFace(MISSILE,getNextRequest());
      missileCooldownTimer.set(MISSILECOOLDOWNTIMEMS);
    }
  }

  // Handle Explosions
  if (isExploding && explosionTimer.isExpired()) {
    isExploding = false;
  }
}


void startExplosion () {
  missileRequested = false;
  hasMissile = false;
  isExploding = true;
  explosionTimer.set(EXPLOSIONTIMEMS);
}

// Look for and interpret inputs during gameplay
void inputHandler () {
  if (buttonSingleClicked() && !isEarth && !hasWoken() && !missileRequested) {
    missileRequested = true;
    sendProjectileOnFace(MISSILE, missileRequestFace);
  }
}

// Temp end game on longpress
void tempCheckEndGame () {
  if (buttonLongPressed() && isEarth && !hasWoken()){
    gameState = GAMEOVER;
  }
}

// End GAMEOVER state and return to SETUP
void checkResetGame () {
  if (buttonSingleClicked() && isEarth && !hasWoken()) {
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


// Extract bitwise values from face input
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

byte getProjectileStateOnFace (byte face) {
  if (!isValueReceivedOnFaceExpired(face)){
    return ((getLastValueReceivedOnFace(face) >> 1) & 7);
  }
}

byte getACKStateOnFace (byte face) {
  if (!isValueReceivedOnFaceExpired(face)){
    return (getLastValueReceivedOnFace(face) & 1);
  }
}

// Parse bitwise values from stored face input
// WARNING! Slightly faster but less safe. Make sure
// you've already tested for expired face value!
byte parseBlinkState (byte input) {
  return ((input >> 4) & 3);
}

byte parseGameState (byte input) {
  return ((input >> 6) & 3);
}

byte parseProjectileState (byte input) {
  return ((input >> 1) & 7);
}

byte parseACKState (byte input) {
  return (input & 1);
}


void incomingCommsHandler() {
  FOREACH_FACE(f) {
    if (isValueReceivedOnFaceExpired(f)) continue; //Skip open faces
    
    byte face_value = getLastValueReceivedOnFace(f); // Read once, parse for each value of interest

    // Handle Game State Changes
    byte temp_value = parseGameState(face_value);
    if (cached_gameState[f] != temp_value) {
      // State changed! Update our own.
      gameState = temp_value;
    }
    cached_gameState[f] = temp_value;

    // Handle Incoming Projectiles
    if (incomingProjectiles[f] == NOTHING){
      incomingProjectiles[f] = parseProjectileState(face_value);
    }
    
    // Handle Incoming ACKs
    ACKReceive[f] = parseACKState(face_value);
    ACKHandler(f);
  }
}

void ACKHandler (int face) {
  if ((ACKReceive[face] == 1) && (outgoingProjectiles[face] != NOTHING)) {
    outgoingProjectiles[face] = NOTHING;
    processBufferOnFace(face);
  }
}

void projectileReceiver () {
  FOREACH_FACE(f) {
    if (isValueReceivedOnFaceExpired(f)) continue; //Skip open faces

    if ((receivedProjectiles[f] == NOTHING) && (incomingProjectiles[f] != NOTHING)) {       // If we have something to receive and room for it,
      if ((ACKSend[f] == 1) && (incomingProjectiles[f] != getProjectileStateOnFace(f))) {   // and have already ACK'd and been confirmed,
        receivedProjectiles[f] = incomingProjectiles[f];                                    // move projectile to received area for handling,
        incomingProjectiles[f] = NOTHING;                                                   // clear it from incoming,
        ACKSend[f] = 0;                                                                    // and clear ACK sender.
      }
      else { //if (ACKSend[f] != 1) { // May not be a disconfirming case, and no harm in double setting, so why check?
        ACKSend[f] = 1;
      }
    }
  }
}

void projectileManager () {
  FOREACH_FACE(f) {
    if (isValueReceivedOnFaceExpired(f)) continue; //Skip open faces

    byte tempProjectile = receivedProjectiles[f];
    
    if (tempProjectile != NOTHING) {
      byte tempDirection = faceDirection[f];
      
      switch (tempProjectile) {
        case MISSILE:
          if (tempDirection == OUTWARD) { // Missile request received! Pass on immediately inward if not Earth.
            if (!isEarth) {
              addMissileRequest(f);
              sendProjectileOnFace(MISSILE, missileRequestFace);
            }
            else if (missileCooldownTimer.isExpired()) { // If Earth and cooldown expired, bounce the request back.
              addMissileRequest(f);
              sendProjectileOnFace(MISSILE, getNextRequest());
              missileCooldownTimer.set(MISSILECOOLDOWNTIMEMS);
            }
            else { // If Earth and cooldown was running, add to request queue
               addMissileRequest(f);
            }
          }
          else { // Actual missile received!
            gained(MISSILE);
          }
          break;          
      }
      receivedProjectiles[f] = NOTHING; // Projectile handled, clear from receiving array.
    }
  }
}

void gained (byte proj) {
  switch (proj) {
    case MISSILE:
      hasMissile = true;
      missileTimer.set(MISSILETRANSITTIMEMS);
      break;
  }
}

void addMissileRequest (byte src) {
  FOREACH_FACE(f) {
    if (requestQueue[f] == 7) {
      requestQueue[f] = src;
      break;
    }
  }
}

byte getNextRequest () {
//  byte next = 7;
//  FOREACH_FACE(f) {
//    if (requestQueue[f] != 7){
//      next = requestQueue[f];
//      break;
//    }
//  }
  byte next = requestQueue[0];
  processRequestQueue();
  return next;
}

void processRequestQueue () {
  FOREACH_FACE(f) {
    byte nextPos = f+1;
    if ((f < 5) && (requestQueue[f] == 7) && (requestQueue[nextPos] != 7)) {
      requestQueue[f] = requestQueue[nextPos];
      requestQueue[nextPos] = 7;
    }
  }
//  if (requestQueue[0] == 7) {
//    processRequestQueue();
//  }
}

void sendProjectileOnFace (byte proj, int face) {
  processBufferOnFace(face);
  if (outgoingProjectiles[face] == NOTHING) {
    outgoingProjectiles[face] = proj;
    clearSentProjectile(proj);
  }
  else if (projectilesBuffer[face] == NOTHING) {
    projectilesBuffer[face] = proj;
    clearSentProjectile(proj);
  }
}

void clearSentProjectile (byte proj) {
  switch (proj) {
    case MISSILE:
      hasMissile = false;
      break;
  }
}

void processBufferOnFace (int f) {
  if ((projectilesBuffer[f] != NOTHING) && (outgoingProjectiles[f] == NOTHING)) {
    outgoingProjectiles[f] = projectilesBuffer[f];
    projectilesBuffer[f] = NOTHING;
  }
}

void outgoingCommsHandler() {

  FOREACH_FACE(f) {
    if (isValueReceivedOnFaceExpired(f)) continue; //Skip open faces

    setValueSentOnFace((gameState << 6) + (blinkState << 4) + (outgoingProjectiles[f] << 1) + ACKSend[f], f);
  }
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

  renderMissile();

  if (isExploding) {
    setColor (ORANGE);
  }
}

void renderMissile (){
  if (hasMissile) {
    if (!missileRequested) {
      if (missileTimer.getRemaining() >= (MISSILETRANSITTIMEMS / 2)) {
        setColorOnFace(MISSILECOLOR, missileRequestFace);
      }
      else {
        setColorOnFace(MISSILECOLOR, requestQueue[0]);
      }
    }
    else {
      setColorOnFace(MISSILECOLOR, missileRequestFace);
    }
  }

  if (missileRequested) {
    FOREACH_FACE (f) {
      if (f%2 > 0) {
        setColorOnFace(MISSILECOLOR, f);
      }
    }
  }
}

void commsDebugDisplay () {
  FOREACH_FACE(f) {
    if (outgoingProjectiles[f] == MISSILE){
      setColorOnFace (CYAN, f);
    }
    else if (incomingProjectiles[f] == MISSILE) {
      setColorOnFace (MAGENTA, f);
    }
    else if (ACKSend[f] == 1) {
      setColorOnFace (YELLOW, f);
    }
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
  if (animationTimer.isExpired()) animationTimer.set(ANIMATIONTIMERMS);
  
  switch (gameState) {
    case SETUP: 
      layerTestDisplay(); // Verifies layer functionality
      //directionalityTestDisplay(); // Verifies directionality functionality
      break;
    case SINGLEPLAYER:
    case MULTIPLAYER:
      inGameDisplay();
      commsDebugDisplay();
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
