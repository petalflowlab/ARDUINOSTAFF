
void updateButton(ButtonState &btn, int pin) {
  bool currentState = digitalRead(pin);
  uint32_t now = millis();
  if (currentState != btn.lastState) btn.lastDebounceTime = now;
  if ((now - btn.lastDebounceTime) > DEBOUNCE_DELAY) {
    if (currentState==LOW && btn.lastState==HIGH) {
      if (now - btn.lastClickTime < DOUBLE_CLICK_WINDOW) btn.clickCount++;
      else btn.clickCount = 1;
      btn.lastClickTime = now;
      btn.processed = false;
    }
  }
  btn.lastState = currentState;
  if (!btn.processed && btn.clickCount>0 && (now-btn.lastClickTime)>DOUBLE_CLICK_WINDOW) {
    btn.processed = true;
    if (btn.clickCount==1) {
      currentEffect = (currentEffect+1) % ((currentPage==0||currentPage==5)?6:5);
      fill_solid(leds, NUM_LEDS, CRGB::White); FastLED.show(); delay(50);
    } else if (btn.clickCount>=2) {
      currentPage = (currentPage+1) % NUM_PAGES;
      currentEffect = 0;
      for (int i=0;i<2;i++){fill_solid(leds,NUM_LEDS,CRGB::Cyan);FastLED.show();delay(100);FastLED.clear();FastLED.show();delay(100);}
    }
    btn.clickCount = 0;
  }
}
void updateIMU() {
  static uint32_t lastIMUCheck = 0;
  if (!mpu_ready) {
    if (millis()-lastIMUCheck>100) {
      lastIMUCheck = millis();
      if (mpu_ok) {
        mpu.update();
        if (abs(mpu.getAccY()) < 10.0f) { mpu_ready=true; Serial.println("MPU ready"); }
      }
    }
    return;
  }
  if (!mpu_ok) return;
  mpu.update();
  static float smoothAccel[3] = {0};
  float alpha = 0.2f;
  accelRaw[0]=mpu.getAccX(); accelRaw[1]=mpu.getAccY(); accelRaw[2]=mpu.getAccZ();
  float rad = IMU_TILT_COMPENSATION * 0.0174533f;
  float cY  = accelRaw[1]*cos(rad) - accelRaw[2]*sin(rad);
  float cZ  = accelRaw[1]*sin(rad) + accelRaw[2]*cos(rad);
  smoothAccel[0]=smoothAccel[0]*(1.0f-alpha)+accelRaw[0]*alpha;
  smoothAccel[1]=smoothAccel[1]*(1.0f-alpha)+cY*alpha;
  smoothAccel[2]=smoothAccel[2]*(1.0f-alpha)+cZ*alpha;
  accel[0]=smoothAccel[0]; accel[1]=smoothAccel[1]; accel[2]=smoothAccel[2];
  gyro[0]=mpu.getGyroX(); gyro[1]=mpu.getGyroY(); gyro[2]=mpu.getGyroZ();

  // === ROLL DETECTION (Y-axis = staff long axis) ===
  // gyro[1] is angular velocity around the staff's own axis (deg/sec).
  // Positive = roll one way, negative = roll the other way.
  {
    static uint32_t lastRollMs = 0;
    uint32_t rollNow = millis();
    if (lastRollMs > 0) {
      float rollDt = clampf((rollNow - lastRollMs) / 1000.0f, 0.0f, 0.05f);
      // Smooth roll speed for intensity threshold (avoids flickering on noise)
      rollRate = rollRate * 0.85f + gyro[1] * 0.15f;
      // Accumulate phase using raw gyro so animation is snappy and direction-accurate:
      //   positive gyro → vortexPhase grows → outward spiral
      //   negative gyro → vortexPhase shrinks → inward spiral
      vortexPhase += gyro[1] * rollDt * 0.018f;
      if (vortexPhase >  200.0f) vortexPhase -= 200.0f;
      if (vortexPhase < -200.0f) vortexPhase += 200.0f;
      // Intensity: threshold at 80 deg/sec, full vortex at ~300 deg/sec
      float rawIntensity = clampf((fabs(rollRate) - 80.0f) / 220.0f, 0.0f, 1.0f);
      vortexIntensity = vortexIntensity * 0.94f + rawIntensity * 0.06f;
    }
    lastRollMs = rollNow;
  }
}
void processAudio() {
  static float   simTime      = 0.0f;
  static float   beatTimer    = 0.0f;
  static float   beatInterval = 0.5f;
  static float   subBeatTimer = 0.0f;
  static uint32_t lastMs      = 0;
  uint32_t nowMs = millis();
  
  bool usingWebAudio = (nowMs - lastWebAudio) < 200;
  
  if (usingWebAudio) {
    audioLevel *= 0.95f;
    audioEnergy *= 0.93f;
    if(pseudoBeat) beatIntensity *= 0.9f;
    if(beatIntensity < 0.1f) pseudoBeat = false;
    
    if (audioLevel > audioLevelPeak) audioLevelPeak = audioLevel;
    return;
  }
  
  float dt = clampf((nowMs-lastMs)/1000.0f, 0.0f, 0.05f);
  lastMs = nowMs;
  simTime += dt; if(simTime > 1000.0f) simTime -= 1000.0f;  // prevent float precision loss
  beatTimer += dt; subBeatTimer += dt;
  float beatPhase = beatTimer/beatInterval;
  float bassThump = expf(-beatPhase*8.0f);
  if (beatTimer>=beatInterval) beatTimer = fmod(beatTimer,beatInterval);
  float melody = sin(simTime*5.3f)*0.3f + sin(simTime*7.1f)*0.2f
               + sin(simTime*11.7f)*0.15f + sin(simTime*3.9f)*0.25f;
  melody = (melody+0.9f)/1.9f;
  float hihat=0.0f;
  if (subBeatTimer>=beatInterval*0.25f) subBeatTimer=fmod(subBeatTimer,beatInterval*0.25f);
  float subPhase=subBeatTimer/(beatInterval*0.25f);
  hihat=(subPhase<0.15f)?(1.0f-subPhase/0.15f)*0.4f:0.0f;
  float swellPhase=simTime/(beatInterval*4.0f);
  float swell=(sin(swellPhase*6.2832f)*0.5f+0.5f)*0.6f;
  float combined=clampf(bassThump*0.7f+melody*0.3f+hihat*0.2f+swell*0.15f, 0.0f, 1.0f);
  beatInterval=0.43f+sin(simTime*0.07f)*0.07f;
  audioLevel=(combined>audioLevel)?(audioLevel*0.3f+combined*0.7f):(audioLevel*0.93f+combined*0.07f);
  static float envFollower=0.0f;
  envFollower=envFollower*0.97f+combined*0.03f;
  audioEnergy=envFollower;
  pseudoBeat=false; beatIntensity=0.0f;
  float spike=(envFollower>0.01f)?combined/envFollower:0.0f;
  if (spike>2.0f && bassThump>0.5f) {
    pseudoBeat=true;
    beatIntensity=clampf((spike-2.0f)/2.0f,0.0f,1.0f);
  }
  if (audioLevel>audioLevelPeak) audioLevelPeak=audioLevel;
  if (nowMs-lastAudioPrint>=2000) {
    lastAudioPrint=nowMs;
    Serial.printf("[Audio] BPM:%.0f level:%.3f beat:%s\n", 60.0f/beatInterval, audioLevel, pseudoBeat?"YES":"no");
  }
}
