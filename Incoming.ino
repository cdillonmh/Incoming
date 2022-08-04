/*
 * Incoming!
 * for Move38 Blinks
 * By Dillon Hall
 */

/*
 *  KNOWN ISSUES:
 *  - Solo asteroid spawns not always sending to edge blinks.
 *  - Occasional missile request collisions causing lost requests (Comms timeout timers helping)
 *  - Asteroids can collide and cause lost comms (not a big issue, really)
 */

/*
 *  NEXT STEPS:
 *  - Find more storage space!
 *  
 *  - Gameplay testing and iterations
 *  
 *  - Improve targetting display?
 */

/* 
 *  IDEAS:
 *  - Single presses on launcher to start multiplayer instead?
 *  
 *  - Can use blinkState (currently only used in setup) during in-game for something else...
 *    + Global missile cooldown?
 *    + Increasing difficulty levels?
 *    + Other stuff for multiplayer mode?
 *    
 *  - Multiplayer improvements?
 *    + Create endgame trigger from launcher player, long timer after last asteroid is launched?
 *    + Spawn randomized asteroid loadout?
 *    + Show loadout remaining and next launch (in missileRequest position, blinking)?
 */

// Color Defaults
#define SPACECOLOR OFF
#define EARTHLAND GREEN
#define EARTHSEA BLUE
#define EARTHDISPLAYARRAY {EARTHLAND, EARTHLAND, EARTHSEA, EARTHLAND, EARTHSEA, EARTHSEA}
#define EARTHDAMAGEORDER {1, 4, 5, 3, 2, 0}
#define ASTEROIDCOLOR YELLOW
#define FASTEROIDCOLOR RED
#define MISSILECOLOR WHITE
#define EXPLOSIONCOLOR ORANGE
#define DAMAGECOLOR RED
#define SPAWNERCOLOR MAGENTA
#define CHARGETIMERCOLOR WHITE

// Game Balance
#define ASTEROIDTRANSITTIMEMS 1000
#define FASTEROIDTRANSITTIMEMS 1000
#define MISSILETRANSITTIMEMS 250
#define MISSILECOOLDOWNTIMEMS 1000
#define ASTEROIDCOOLDOWNTIMEMS 3000
#define FASTEROIDCOOLDOWNTIMEMS 6000
#define EXPLOSIONTIMEMS 500
#define EARTHFULLHEALTH 6
#define ASTEROIDDAMAGE 1
#define FASTEROIDDAMAGE 1

// Solo Game Balance
#define GAMESTARTDELAY 1200
#define GAMEENDDELAY 5000
#define BASEASTEROIDDELAYMS 1200
#define ADDITIONALDELAYMAXMS 1000
#define INITIALNUMASTEROIDS 8

// Solo Leveling Balance
#define SPEEDINCREASEPERLEVEL 50
#define NUMASTEROIDSINCREASEPERLEVEL 2
#define MAXLEVEL 9

// General presets
#define ANIMATIONTIMERMS 3000
#define REQUESTQUEUESIZE 12
#define REQUESTTIMEOUTTIMERMS 7500
#define COMMSTIMEOUTTIMERMS 7000
#define DEATHANIMATIONTIMEMS 2700

// Game States
enum gameStates {SETUP, SINGLEPLAYER, MULTIPLAYER, GAMEOVER};
byte gameState = SETUP;
byte cached_gameState[FACE_COUNT];

// Blink States
enum blinkStates {L0, L1, L2, L3};
byte blinkState = L0;
bool isEarth = false;
bool isSpawner = false;

// Projectile Types
enum projectiles {NOTHING, ASTFOUR, ASTTHREE, ASTTWO, ASTONE, FASTTWO, FASTONE, MISSILE};

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
byte leftOfMissileRequestFace = 8;
byte requestQueue[REQUESTQUEUESIZE];
byte requestQueueEmptyValue;
bool isExploding = false;
int earthHealth = EARTHFULLHEALTH;

// Solo Mode
bool levelStarted = false;
bool levelDone = false;
byte currentLevel = 0;
byte asteroidsRemaining;

// Projectile Timers
Timer asteroidTimer;
Timer fasteroidTimer;
Timer missileTimer;
Timer missileCooldownTimer;
Timer explosionTimer;
Timer requestTimeoutTimer;
Timer commsTimeoutTimer;

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
  
  // Seed request queue with empty value
  requestQueueEmptyValue = REQUESTQUEUESIZE + 1;
  clearRequestQueue();
}

void loop() {
  switch (gameState) {
    case SETUP:
      checkBlinkState(); // Uses outside edge to help determine center Earth blink
      resetEarthHealth();
      checkStartGame();
      break;
    case SINGLEPLAYER:
      checkSpawnAsteroid();
    case MULTIPLAYER:
      checkGameplayCollissions();
      projectileTimerHandler();
      inputHandler();
      tempCheckEndGame();
      break;
    case GAMEOVER:
      resetAll();
      checkResetGame();
      break;
  }

  processAllFaceBuffers();
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
  byte numEdges = 0;
  FOREACH_FACE(f) {
    if (isValueReceivedOnFaceExpired(f)) {
      // Found an open edge!
      hasEdge = true;
      numEdges++;
    }
  }
  if (hasEdge) {
    blinkState = L0;
    if (numEdges > 3) {
      isSpawner = true;
    }
    else {
      isSpawner = false;
    }
  }
  else {
    byte lowestAdjacentLayer = getBlinkStateOnFace(0);
    byte lastFaceValue = lowestAdjacentLayer;
    bool allTheSame = true;
    isSpawner = false;
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
    byte nextFace = (returnNextFace(f));
    if (!isSpawner) {
      if (faceDirection[currentFace] == UNDETERMINED) {
        if (faceDirection[nextFace] == INWARD) {
          faceDirection[currentFace] = FORWARD;
          missileRequestFace = nextFace;
          leftOfMissileRequestFace = currentFace;
        }
        else {
          faceDirection[currentFace] = BACKWARD;
        }
      }
    }
    else {
      //byte nextNextFace = returnNextFace(nextFace);
      if (faceDirection[currentFace] == EDGE && faceDirection[nextFace] != EDGE) {
        faceDirection[nextFace] = INWARD;
        missileRequestFace = nextFace;
        leftOfMissileRequestFace = currentFace;
        if (faceDirection[returnNextFace(nextFace)] != EDGE){
          faceDirection[returnNextFace(nextFace)] = INWARD;
        }
      }
    }
  }
}

// Check if game should start
void checkStartGame () {

  if (buttonSingleClicked() && !hasWoken()){ 
    if (isEarth) {
      gameState = SINGLEPLAYER;
    }
    else if (isSpawner) {
      gameState = MULTIPLAYER;
    }
  }
}

byte returnNextFace (byte face) {
  return (face+1)%6;
}

byte returnPrevFace (byte face) {
  return (face+5)%6;
}


// Check and handle solo mode asteroid spawns
void checkSpawnAsteroid () {
  if (isEarth && gameState == SINGLEPLAYER) {
    if (explosionTimer.isExpired()) {
      if (!levelStarted) {
        explosionTimer.set(GAMESTARTDELAY);
        levelStarted = true;
        asteroidsRemaining = INITIALNUMASTEROIDS + (currentLevel * NUMASTEROIDSINCREASEPERLEVEL);
      }
      else if (levelDone) {
        if (currentLevel == MAXLEVEL) {
          gameState = GAMEOVER;
        } else {
          currentLevel++;
          levelStarted = false;
          levelDone = false;
        }
      }
      else if (asteroidsRemaining < 1) {
        levelDone = true;
        explosionTimer.set(GAMEENDDELAY);
      }
      
      else {
        asteroidsRemaining--;

        sendProjectileOnFace(random(5)+1,random(5), false);
        
        explosionTimer.set((BASEASTEROIDDELAYMS - (currentLevel * SPEEDINCREASEPERLEVEL)) + random(ADDITIONALDELAYMAXMS - (currentLevel * SPEEDINCREASEPERLEVEL)));
      }
    }
  }
}


// Check and handle expired projectile timers
void projectileTimerHandler () {

  // Handle Asteroids
  if (asteroidType != NOTHING && asteroidTimer.isExpired()) {
    switch (asteroidType) {
      case ASTONE:
        sendProjectileOnFace(ASTFOUR,missileRequestFace, true);
        break;
      case ASTTWO:
        sendProjectileOnFace(ASTONE,leftOfMissileRequestFace, true);
        break;
      case ASTTHREE:
        sendProjectileOnFace(ASTTWO,leftOfMissileRequestFace, true);
        break;
      case ASTFOUR:
        sendProjectileOnFace(ASTTHREE,leftOfMissileRequestFace, true);
        break;
    }
  }

    // Handle Fasteroids
  if (fasteroidType != NOTHING && fasteroidTimer.isExpired()) {
    switch (fasteroidType) {
      case FASTONE:
        sendProjectileOnFace(FASTTWO,missileRequestFace, true);
        break;
      case FASTTWO:
        sendProjectileOnFace(FASTONE,leftOfMissileRequestFace, true);
        break;
    }
  }

  // Handle Missiles
  if (hasMissile && missileTimer.isExpired()) {
    if (!missileRequested && requestQueue[0] != requestQueueEmptyValue) {
      sendProjectileOnFace(MISSILE,getNextRequest(), true);
    }
    else {
      startExplosion();
    }
  }

  // Handle Comms Timeouts
  if ((requestQueue[0] != requestQueueEmptyValue) && commsTimeoutTimer.isExpired()) {
    getNextRequest(); // Clear out oldest request
    if (requestQueue[0] != requestQueueEmptyValue) {
      commsTimeoutTimer.set(COMMSTIMEOUTTIMERMS);
    }
  }

  // Handle Request Timeouts
  if (missileRequested && requestTimeoutTimer.isExpired()) {
    missileRequested = false; // Clear out missile request
  }

  // Handle Earth's Missile Cooldown and Queue
  if (isEarth) {
    //processRequestQueue();
    if ((requestQueue[0] != requestQueueEmptyValue) && (missileCooldownTimer.isExpired())) {
      sendProjectileOnFace(MISSILE,getNextRequest(), true);
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

// Check for destroyed asteroids or damage to Earth
void checkGameplayCollissions () {
  if (!isEarth) {
    if (isExploding && (asteroidType != NOTHING || fasteroidType != NOTHING)){
      asteroidType = NOTHING;
      fasteroidType = NOTHING;
    }
  }
  else if (asteroidType != NOTHING || fasteroidType != NOTHING) {
    if (asteroidType != NOTHING) {
      earthHealth = earthHealth - ASTEROIDDAMAGE;
      asteroidType = NOTHING;
    }
    if (fasteroidType != NOTHING) {
      earthHealth = earthHealth - FASTEROIDDAMAGE;
      fasteroidType = NOTHING;
    }
    if (earthHealth < 1) {
      gameState = GAMEOVER;
    }
  }
}

// Look for and interpret inputs during gameplay
void inputHandler () {
  if (buttonSingleClicked() && !isEarth && !hasWoken() && !missileRequested) {
    if (gameState == MULTIPLAYER && isSpawner) {
      if (missileCooldownTimer.isExpired() && earthHealth > 0) {
        gained(ASTONE);
        missileCooldownTimer.set(ASTEROIDCOOLDOWNTIMEMS);
        earthHealth--;
      }
    }
    else {
      missileRequested = true;
      sendProjectileOnFace(MISSILE, missileRequestFace, false);
      requestTimeoutTimer.set(REQUESTTIMEOUTTIMERMS);
    }
  }

  if (buttonDoubleClicked() && !hasWoken() && gameState == MULTIPLAYER && isSpawner && earthHealth > 0) {
    if (missileCooldownTimer.isExpired()) {
        gained(FASTONE);
        missileCooldownTimer.set(FASTEROIDCOOLDOWNTIMEMS);
        earthHealth--;
      }
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
  if (buttonSingleClicked() && !hasWoken()) {
    gameState = SETUP;
  }
}

void clearRequestQueue () {
  for (byte i; i<=(REQUESTQUEUESIZE-1); i++) {
    requestQueue[i] = requestQueueEmptyValue;
  }
}

void clearProjectileArray (byte target[]) {
  FOREACH_FACE(f) {
    target[f] = NOTHING;
  }
}

void clearACKArray (byte target[]) {
  FOREACH_FACE(f) {
    target[f] = 0;
  }
}

void resetEarthHealth () {
//  if (earthHealth != EARTHFULLHEALTH) setColor(WHITE); //Quick flash on game reset (didn't work)
  earthHealth = EARTHFULLHEALTH;
  isExploding = false;
  explosionTimer.set(0);
}

// Clear variables and arrays to reset for next game
void resetAll () {
  // Projectiles
  clearProjectileArray(incomingProjectiles);
  clearProjectileArray(receivedProjectiles);
  clearACKArray(ACKReceive);
  clearProjectileArray(outgoingProjectiles);
  clearProjectileArray(projectilesBuffer);
  clearACKArray(ACKSend);
  asteroidType = NOTHING;
  fasteroidType = NOTHING;
  hasMissile = false;
  missileRequested = false;
  missileRequestFace = -1;
  leftOfMissileRequestFace = 8;
  clearRequestQueue();
  //isExploding = false;
  isSpawner = false;
  //earthHealth = EARTHFULLHEALTH;

  // Projectile Timers
  asteroidTimer.set(0);
  fasteroidTimer.set(0);
  missileTimer.set(0);
  missileCooldownTimer.set(0);
  //explosionTimer.set(0);
  requestTimeoutTimer.set(0);
  commsTimeoutTimer.set(0);

  // Solo Mode
  levelStarted = false;
  levelDone = false;
  currentLevel = 0;
  asteroidsRemaining = 0;
}

void layerTestDisplay (){
  switch (blinkState) {
    case L0:
      if (isSpawner) {
        setColor(SPAWNERCOLOR);
      }
      else {
        setColor(WHITE);
      }
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
    if (f == missileRequestFace) {
      setColorOnFace(MAGENTA, f);
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


void incomingCommsHandler () {
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
          if (tempDirection != INWARD) { // Missile request received! Pass on immediately inward if not Earth.
            if (!isEarth) {
              addMissileRequest(f);
              sendProjectileOnFace(MISSILE, missileRequestFace, false);
            }
            else if (missileCooldownTimer.isExpired()) { // If Earth and cooldown expired, bounce the request back.
              addMissileRequest(f);
              sendProjectileOnFace(MISSILE, getNextRequest(), false);
              missileCooldownTimer.set(MISSILECOOLDOWNTIMEMS);
            }
            else { // If Earth and cooldown was running, add to request queue
               addMissileRequest(f);
            }
          }
          else if (missileRequested || (requestQueue[0] != requestQueueEmptyValue)){ // Actual missile received and we know what to do with it!
            gained(MISSILE);
          }
          break;
        case ASTONE:
        case ASTTWO:
        case ASTTHREE:
        case ASTFOUR:
        case FASTONE:
        case FASTTWO:
          byte opposingFace = faceDirection[(f+3)%6];
          if (tempDirection == INWARD && opposingFace != EDGE ){
            sendProjectileOnFace(tempProjectile, opposingFace, false);
          }
          else {
            gained (tempProjectile);
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
    case ASTONE:
      asteroidType = ASTONE;
      asteroidTimer.set(ASTEROIDTRANSITTIMEMS);
    break;
    case ASTTWO:
      asteroidType = ASTTWO;
      asteroidTimer.set(ASTEROIDTRANSITTIMEMS);
    break;
    case ASTTHREE:
      asteroidType = ASTTHREE;
      asteroidTimer.set(ASTEROIDTRANSITTIMEMS);
    break;
    case ASTFOUR:
      asteroidType = ASTFOUR;
      asteroidTimer.set(ASTEROIDTRANSITTIMEMS);
    break;
    case FASTONE:
      fasteroidType = FASTONE;fasteroidTimer.set(FASTEROIDTRANSITTIMEMS);
    break;
    case FASTTWO:
      fasteroidType = FASTTWO;
      fasteroidTimer.set(FASTEROIDTRANSITTIMEMS);
    break;
  }
}

void addMissileRequest (byte src) {
  for (byte f = 0; f<=(REQUESTQUEUESIZE-1); f++) {
    if (requestQueue[f] == requestQueueEmptyValue) {
      requestQueue[f] = src;
      commsTimeoutTimer.set(COMMSTIMEOUTTIMERMS);
      break;
    }
  }
}

byte getNextRequest () {
  byte next = requestQueue[0];
  requestQueue[0] = requestQueueEmptyValue;
  processRequestQueue();
  if (requestQueue[0] != requestQueueEmptyValue) {
    commsTimeoutTimer.set(COMMSTIMEOUTTIMERMS);
  }
  else {
    commsTimeoutTimer.set(0);
  }
  return next;
}

void processRequestQueue () {
  for (byte f = 0; f<=(REQUESTQUEUESIZE-2); f++) {
    int nextPos = f+1;
    if ((requestQueue[f] == requestQueueEmptyValue) && (requestQueue[nextPos] != requestQueueEmptyValue)) {
      requestQueue[f] = requestQueue[nextPos];
      requestQueue[nextPos] = requestQueueEmptyValue;
    }
  }
}

void sendProjectileOnFace (byte proj, byte face, bool isReal) {
  if (outgoingProjectiles[face] == NOTHING) {
    outgoingProjectiles[face] = proj;
    if (isReal) clearSentProjectile(proj);
  }
  else if (projectilesBuffer[face] == NOTHING) {
    projectilesBuffer[face] = proj;
    if (isReal) clearSentProjectile(proj);
  }
}

void clearSentProjectile (byte proj) {
  switch (proj) {
    case MISSILE:
      hasMissile = false;
      break;
    case ASTONE:
    case ASTTWO:
    case ASTTHREE:
    case ASTFOUR:
      asteroidType = NOTHING;
      break;
    case FASTONE:
    case FASTTWO:
      fasteroidType = NOTHING;
      break;
  }
}

void processBufferOnFace (byte f) {
  if ((projectilesBuffer[f] != NOTHING) && (outgoingProjectiles[f] == NOTHING)) {
    outgoingProjectiles[f] = projectilesBuffer[f];
    projectilesBuffer[f] = NOTHING;
  }
}

void processAllFaceBuffers() {
  
  FOREACH_FACE(f) {
    if (isValueReceivedOnFaceExpired(f)) continue; //Skip open faces

    if (parseACKState(getLastValueReceivedOnFace(f)) == 0) {
      processBufferOnFace(f);
    }
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
    renderEarth();
  }
  else {
    if (isSpawner && gameState != SINGLEPLAYER) {
      
      FOREACH_FACE(f) {
        byte currentFace = (missileRequestFace+f)%6;
        if (currentFace < earthHealth) {
          setColorOnFace (SPAWNERCOLOR, currentFace);
        }
        else {
          setColorOnFace (OFF, currentFace);
        }
      }
    }
    else if (!isSpawner && gameState == SETUP){
      int animTimer = animationTimer.getRemaining();

      if (animTimer < 500 && animTimer > 250) {
        setColorOnFace(makeColorRGB(animTimer-245,animTimer-245,animTimer-245), 3);
      } else if (animTimer < 250){
        animationTimer.set(animTimer + 2500 + random(3500));
      }
      else {
        setColor (SPACECOLOR);
      }
    }
    else {
      setColor (SPACECOLOR);
    }
    
  renderAsteroids();
    
  }

  renderMissile();

  rechargingDisplay();

  renderExplosion ();

}

void renderExplosion () {
    if (isExploding) {
      int explosionValue = constrain(explosionTimer.getRemaining(),0,255);
      setColor (makeColorRGB(explosionValue,explosionValue/2,0));
      setColorOnFace(makeColorRGB(explosionValue,explosionValue,0),animationTimer.getRemaining()%6);
  }
}

void renderEarth () {
  int startingFace;
  
  if (gameState != SETUP) {
    startingFace = int(animationTimer.getRemaining() / (ANIMATIONTIMERMS / FACE_COUNT));
  }
  else {
    startingFace = 0;
  }
  
  Color earthColors[] = EARTHDISPLAYARRAY;
  byte damageOrder[] = EARTHDAMAGEORDER;

  FOREACH_FACE(f) {
    if (f < (EARTHFULLHEALTH - earthHealth)){
      earthColors[damageOrder[f]] = DAMAGECOLOR;
    }
  }
  
  FOREACH_FACE(f) {
      // Set earth colors plus rotation (based on animation speed)
      setColorOnFace(earthColors[f], ((f+startingFace)%6));
    }
}

void renderAsteroids () {
    switch (asteroidType) {
      case ASTFOUR:
      case ASTTHREE:
          if (asteroidTimer.getRemaining() > (ASTEROIDTRANSITTIMEMS/2)) {
          setColorOnTwoFacesCWFromSource(ASTEROIDCOLOR,2,3,missileRequestFace);
        }
        else {
          setColorOnTwoFacesCWFromSource(ASTEROIDCOLOR,4,5,missileRequestFace);
        }
        break;
      case ASTTWO:
      case ASTONE:
        if (asteroidTimer.getRemaining() > (ASTEROIDTRANSITTIMEMS/2)) {
          setColorOnTwoFacesCWFromSource(ASTEROIDCOLOR,1,2,missileRequestFace);
        }
        else {
          setColorOnTwoFacesCWFromSource(ASTEROIDCOLOR,0,5,missileRequestFace);
        }
    }

    switch (fasteroidType) {
      case FASTTWO:
        if (fasteroidTimer.getRemaining() > (FASTEROIDTRANSITTIMEMS/2)) {
          setColorOnTwoFacesCWFromSource(FASTEROIDCOLOR,2,3,missileRequestFace);
        }
        else {
          setColorOnTwoFacesCWFromSource(FASTEROIDCOLOR,4,5,missileRequestFace);
        }
      break;
      case FASTONE:
        if (fasteroidTimer.getRemaining() > (FASTEROIDTRANSITTIMEMS/2)) {
        setColorOnTwoFacesCWFromSource(FASTEROIDCOLOR,1,2,missileRequestFace);
        }
        else {
          setColorOnTwoFacesCWFromSource(FASTEROIDCOLOR,0,5,missileRequestFace);
        }
      break;
    }
}

void setColorOnTwoFacesCWFromSource (Color col, byte face1, byte face2, byte source) {
  FOREACH_FACE (f) {
    if (f == ((source+face1)%6) || f == ((source+face2)%6)) {
      setColorOnFace(col,f);
    }
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
    if (outgoingProjectiles[f] != NOTHING){
      setColorOnFace (CYAN, f);
    }
    else if (incomingProjectiles[f] != NOTHING) {
      setColorOnFace (MAGENTA, f);
    }
    else if (ACKSend[f] == 1) {
      setColorOnFace (YELLOW, f);
    }
    if (projectilesBuffer[f] != NOTHING) {
      setColorOnFace (RED, f);
    }
    if (requestQueue[0] == f){
      setColorOnFace (GREEN, f);
    }
  }
}

void rechargingDisplay () {
  int cooldownValue = missileCooldownTimer.getRemaining();
  if (cooldownValue != 0) {
    if (isSpawner) {
      setColorOnFace(CHARGETIMERCOLOR,(missileRequestFace + (cooldownValue/(FASTEROIDCOOLDOWNTIMEMS / FACE_COUNT)))%6);
    }
    else if (isEarth) {
      setColorOnFace(CHARGETIMERCOLOR,(cooldownValue/(MISSILECOOLDOWNTIMEMS / FACE_COUNT))%6);
    }
    
  }
}

void gameoverDisplay () {
  if (isEarth) {
    if (earthHealth > 0){
      renderEarth();
    } else {
      if (earthHealth < -99) {
        renderExplosion();
      } else {
        earthHealth = -100;
        startExplosion();
        explosionTimer.set(DEATHANIMATIONTIMEMS);
      }
    }
  }
  else if (blinkState != L0 && earthHealth < -99) {
    renderExplosion();
  }
  else if (blinkState !=L0) {
    earthHealth = earthHealth - blinkState;
    if (earthHealth < -99) startExplosion();
  }
}

void displayHandler() {
  if (animationTimer.isExpired()) animationTimer.set(ANIMATIONTIMERMS);
  
  switch (gameState) {
    case SINGLEPLAYER:
    case MULTIPLAYER:
    case SETUP:
      inGameDisplay();
      //if (levelDone) setColor (WHITE);
      //commsDebugDisplay();
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
