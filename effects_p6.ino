
void renderNewtonsCradleEffect(float dt) {
  static float ballPos[5];
  static float ballVel[5];
  static bool ncInit = false;
  int ci = STAFF_LENGTH / 2;
  if (!ncInit) {
    ballPos[0] = ci - 20;
    ballPos[1] = ci - 10;
    ballPos[2] = ci;
    ballPos[3] = ci + 10;
    ballPos[4] = ci + 20;
    for(int i=0; i<5; i++) ballVel[i] = 0.0f;
    ballVel[0] = 30.0f;
    ncInit = true;
  }
  float tiltForce = mpu_ready ? clampf(accel[1] * 20.0f, -15.0f, 15.0f) : 0.0f;
  for (int b = 0; b < 5; b++) {
    ballVel[b] += tiltForce * dt;
    ballPos[b] += ballVel[b] * dt;
    
    if (ballPos[b] < 5) { 
      ballPos[b] = 5; 
      ballVel[b] = fabs(ballVel[b]) * 0.9f; 
      htState.headImpactIntensity = min(1.0f, fabs(ballVel[b])/30.0f);
    }
    if (ballPos[b] > STAFF_LENGTH-5) { 
      ballPos[b] = STAFF_LENGTH-5; 
      ballVel[b] = -fabs(ballVel[b]) * 0.9f; 
      htState.tailImpactIntensity = min(1.0f, fabs(ballVel[b])/30.0f);
    }
    
    ballVel[b] *= 0.998f;
    
    for (int b2 = b+1; b2 < 5; b2++) {
      float dist = fabs(ballPos[b] - ballPos[b2]);
      if (dist < 3.0f) {
        float tmp = ballVel[b]; 
        ballVel[b] = ballVel[b2] * 0.95f; 
        ballVel[b2] = tmp * 0.95f;
      }
    }
  }
  fadeToBlackBy(leds+HEAD_LENGTH, STAFF_LENGTH, 50);
  
  for(int i=0; i<STAFF_LENGTH; i++){
    float d = fabs((float)i - ci)/(float)ci;
    leds[HEAD_LENGTH+i] = CHSV(200, 80, (uint8_t)(20 - d*15));
  }
  
  for (int b = 0; b < 5; b++) {
    int idx = (int)ballPos[b];
    if(idx >= 0 && idx < STAFF_LENGTH){
      float spd = fabs(ballVel[b]);
      uint8_t bHue = 180 + b * 20;
      uint8_t bVal = 180 + (uint8_t)(min(spd, 30.0f) * 2.5f);
      
      leds[HEAD_LENGTH + idx] = CHSV(bHue, 220, bVal);
      if(idx > 0) leds[HEAD_LENGTH + idx - 1] += CHSV(bHue, 200, bVal/3);
      if(idx < STAFF_LENGTH-1) leds[HEAD_LENGTH + idx + 1] += CHSV(bHue, 200, bVal/3);
    }
  }
  finishEffect(dt,0.5f,0.3f, 190,200,200, 210,200,200);
}
void renderSlinkyEffect(float dt) {
  static float slinkyPhase = 0.0f;
  static float compression = 0.5f;
  uint32_t now = millis();
  int ci = STAFF_LENGTH / 2;
  float tilt = mpu_ready ? clampf(accel[1] * 0.8f, -1.0f, 1.0f) : sin(now * 0.001f) * 0.5f;
  slinkyPhase += dt * (1.5f + fabs(tilt) * 3.0f);
  if(slinkyPhase > 6.283f) slinkyPhase -= 6.283f;
  compression = compression * 0.95f + tilt * 0.05f;
  for (int i = 0; i < STAFF_LENGTH; i++) {
    float pos = (float)i / STAFF_LENGTH;
    float wave = sin(pos * 20.0f - slinkyPhase + compression * 5.0f) * 0.5f + 0.5f;
    float wave2 = sin(pos * 12.0f + slinkyPhase * 0.7f) * 0.5f + 0.5f;
    uint8_t hue = 180 + (uint8_t)(wave * 40);
    uint8_t sat = 200;
    uint8_t val = (uint8_t)((wave * wave2) * 220);
    leds[HEAD_LENGTH + i] = CHSV(hue, sat, val);
  }
  if (mpu_ready && fabs(tilt) > 0.6f) {
    htState.headImpactIntensity = max(htState.headImpactIntensity, fabs(tilt) * 0.8f);
    htState.tailImpactIntensity = max(htState.tailImpactIntensity, fabs(tilt) * 0.8f);
  }
  finishEffect(dt, 0.5f, tilt * 0.2f);
}
void renderInkDropEffect(float dt) {
  #define MAX_IDROPS 5
  #define MAX_ISPLAT 32
  struct IDrop  { float pos,vel; uint8_t hue; bool active,released; };
  struct ISplat { float pos,vel,life; uint8_t hue; bool active; };
  static IDrop   drops[MAX_IDROPS];
  static ISplat  splats[MAX_ISPLAT];
  static float   armExt[2]={0,0}, armVel[2]={0,0};
  static bool    armDriven[2]={false,false};
  static float   blobFizzle=0, fizzleH=0, fizzleT=0, blobPulse=0;
  static uint8_t impactHue=200, dropHue=0;
  static bool    inkInit=false;
  static float   spawnT=0, resetT=0;
  if (!inkInit) { memset(drops,0,sizeof(drops)); memset(splats,0,sizeof(splats)); inkInit=true; }

  // Periodic safety reset — clears accumulated physics every 8s to prevent overload
  resetT += dt;
  if (resetT > 8.0f) {
    memset(drops,  0, sizeof(drops));
    memset(splats, 0, sizeof(splats));
    armExt[0]=armExt[1]=armVel[0]=armVel[1]=0;
    armDriven[0]=armDriven[1]=false;
    blobFizzle=fizzleH=fizzleT=0;
    resetT=0;
  }

  int ci = STAFF_LENGTH/2;
  float swing  = mpu_ready ? clampf(sqrt(gyro[0]*gyro[0]+gyro[1]*gyro[1]+gyro[2]*gyro[2])/200.0f,0,1) : 0;
  float impact = mpu_ready ? clampf((sqrt(accel[0]*accel[0]+accel[1]*accel[1]+accel[2]*accel[2])-1.0f)*0.5f,0,1) : 0;
  float tilt   = mpu_ready ? clampf(accel[1],-1.5f,1.5f) : 0;
  blobPulse   += dt*2.5f;
  if (blobPulse > 628.0f) blobPulse -= 628.0f;  // wrap at 100*2π

  // Spawn drops
  spawnT += dt;
  int toSpawn = 0;
  if (spawnT > 0.5f-swing*0.3f) { spawnT=0; toSpawn=1; }
  if (impact > 0.45f) {
    toSpawn=3;
    htState.headImpactIntensity=max(htState.headImpactIntensity,impact);
    htState.tailImpactIntensity=max(htState.tailImpactIntensity,impact);
  }
  for (int n=0; n<toSpawn; n++) {
    for (int i=0; i<MAX_IDROPS; i++) {
      if (drops[i].active) continue;
      drops[i].pos=clampf(ci+tilt*12+random(11)-5, 4, STAFF_LENGTH-5);
      float spd=70+swing*60+random(40);
      drops[i].vel=(tilt>0.15f?1:tilt<-0.15f?-1:(random(2)?1:-1))*spd;
      drops[i].hue=dropHue; dropHue+=41;
      drops[i].active=true; drops[i].released=false; break;
    }
  }

  // Free arm spring physics — underdamped so it bounces
  for (int a=0; a<2; a++) {
    if (armDriven[a]) continue;
    float prev=armExt[a];
    armVel[a] += (-25.0f*armExt[a] - 2.5f*armVel[a]) * dt;
    armExt[a] += armVel[a]*dt;
    armExt[a] = clampf(armExt[a], -(float)(STAFF_LENGTH/2), (float)(STAFF_LENGTH/2));
    // Zero crossing = arm crashes back into blob → blob fizzle
    if (prev*armExt[a]<0 && fabs(armVel[a])>8.0f)
      blobFizzle=max(blobFizzle, clampf(fabs(armVel[a])/70.0f,0,1.0f));
    if (fabs(armExt[a])<0.3f && fabs(armVel[a])<0.4f) { armExt[a]=0; armVel[a]=0; }
  }

  // Update drops — elastic pull for 15 LEDs then violent release
  for (int i=0; i<MAX_IDROPS; i++) {
    if (!drops[i].active) continue;
    int   ai  = (drops[i].vel>0) ? 1 : 0;
    float dc  = drops[i].pos - ci;
    float str = (ai==1) ? dc : -dc;   // distance traveled in launch direction

    if (!drops[i].released) {
      if (str < 15.0f) {
        armDriven[ai]=true; armExt[ai]=dc;  // arm follows drop
      } else {
        // Violent snap-back on release
        drops[i].released=true; armDriven[ai]=false;
        armVel[ai]=-(dc/fabs(dc))*(fabs(drops[i].vel)*0.5f+50.0f);
        blobFizzle=max(blobFizzle,0.35f);
      }
    }
    drops[i].pos += drops[i].vel*dt;

    // Wall hit → splatter + fizzle
    if (drops[i].pos<1 || drops[i].pos>STAFF_LENGTH-2) {
      int    hp=(int)clampf(drops[i].pos,0,STAFF_LENGTH-1);
      float  dv=drops[i].vel;
      impactHue=drops[i].hue;
      if (!drops[i].released) {
        armDriven[ai]=false;
        armVel[ai]=-(dc/fabs(dc))*(fabs(dv)*0.5f+50.0f);
      }
      drops[i].active=false;
      if      (hp<STAFF_LENGTH/3)       fizzleH=max(fizzleH,1.2f);
      else if (hp>2*STAFF_LENGTH/3)     fizzleT=max(fizzleT,1.2f);
      else { fizzleH=max(fizzleH,0.7f); fizzleT=max(fizzleT,0.7f); }

      // Wall splatters
      int ns=5+random(5);
      for (int s=0; s<ns; s++) {
        for (int k=0; k<MAX_ISPLAT; k++) {
          if (splats[k].active) continue;
          splats[k].pos  = clampf(hp+random(7)-3, 0, STAFF_LENGTH-1);
          splats[k].vel  = -(dv*(0.05f+random(60)/100.0f)) + (random(41)-20)*2.0f;
          splats[k].life = 0.5f+random(90)/100.0f;
          splats[k].hue  = impactHue+random(30)-15;
          splats[k].active=true; break;
        }
      }
      // Blob fizzle splatters burst from center on impact
      for (int s=0; s<4; s++) {
        for (int k=0; k<MAX_ISPLAT; k++) {
          if (splats[k].active) continue;
          splats[k].pos  = clampf(ci+random(13)-6, 0, STAFF_LENGTH-1);
          splats[k].vel  = (random(41)-20)*1.5f;
          splats[k].life = 0.3f+random(60)/100.0f;
          splats[k].hue  = impactHue;
          splats[k].active=true; break;
        }
      }
      if (hp<STAFF_LENGTH/2) htState.headImpactIntensity=max(htState.headImpactIntensity,0.9f);
      else                   htState.tailImpactIntensity=max(htState.tailImpactIntensity,0.9f);
    }
  }

  // Release any arm whose driving drop is gone
  for (int a=0; a<2; a++) {
    if (!armDriven[a]) continue;
    bool driven=false;
    for (int i=0; i<MAX_IDROPS; i++)
      if (drops[i].active && !drops[i].released && (drops[i].vel>0?1:0)==a) { driven=true; break; }
    if (!driven) armDriven[a]=false;
  }

  // Update splatters
  for (int i=0; i<MAX_ISPLAT; i++) {
    if (!splats[i].active) continue;
    splats[i].pos+=splats[i].vel*dt; splats[i].vel*=(1.0f-dt*4.5f); splats[i].life-=dt;
    if (splats[i].life<=0||splats[i].pos<0||splats[i].pos>=STAFF_LENGTH) splats[i].active=false;
  }

  blobFizzle=max(0.0f,blobFizzle-dt*3.0f);
  fizzleH   =max(0.0f,fizzleH-dt*2.0f);
  fizzleT   =max(0.0f,fizzleT-dt*2.0f);

  fadeToBlackBy(leds+HEAD_LENGTH, STAFF_LENGTH, 12);

  // Center blob — big, elastic, pulsing
  float blobR=10.0f+sin(blobPulse*0.7f)*3.0f;
  for (int i=0; i<STAFF_LENGTH; i++) {
    float d=fabs((float)i-ci);
    if (d<blobR) {
      float f=1.0f-d/blobR; f=f*f*f;
      leds[HEAD_LENGTH+i]+=CHSV(210,(uint8_t)(220-f*100),(uint8_t)(f*f*235));
    }
  }

  // Blob fizzle — random sparks near center when arm crashes back in
  if (blobFizzle>0.05f) {
    for (int i=-14;i<=14;i++) {
      int idx=ci+i;
      if (idx<0||idx>=STAFF_LENGTH) continue;
      if (random(100)<(int)(blobFizzle*70))
        leds[HEAD_LENGTH+idx]+=CHSV(impactHue,200,(uint8_t)(blobFizzle*210));
    }
  }

  // Elastic arms — taffy stretch, bright at base, dim at tip
  for (int a=0; a<2; a++) {
    float ext=armExt[a];
    if (fabs(ext)<0.5f) continue;
    int dir=(ext>0)?1:-1, len=(int)fabs(ext);
    int maxJ=len<STAFF_LENGTH/2 ? len : STAFF_LENGTH/2;
    for (int j=0; j<=maxJ; j++) {
      int idx=ci+dir*j;
      if (idx<0||idx>=STAFF_LENGTH) break;
      float t=1.0f-(float)j/(float)(len+1); t=t*t;
      leds[HEAD_LENGTH+idx]+=CHSV(210,(uint8_t)(180-t*80),(uint8_t)(t*t*220));
    }
  }

  // Head/tail fizzle — random sparks at the end that was hit
  if (fizzleH>0.05f) {
    int fLen=(int)(fizzleH*28);
    for (int i=0;i<fLen&&i<STAFF_LENGTH;i++)
      if (random(100)<(int)(fizzleH*70))
        leds[HEAD_LENGTH+i]+=CHSV(impactHue,200,(uint8_t)(fizzleH*220));
  }
  if (fizzleT>0.05f) {
    int fLen=(int)(fizzleT*28);
    for (int i=0;i<fLen&&i<STAFF_LENGTH;i++)
      if (random(100)<(int)(fizzleT*70))
        leds[HEAD_LENGTH+STAFF_LENGTH-1-i]+=CHSV(impactHue,200,(uint8_t)(fizzleT*220));
  }

  // Splatters — single LEDs fading with life²
  for (int i=0; i<MAX_ISPLAT; i++) {
    if (!splats[i].active) continue;
    int idx=(int)splats[i].pos;
    if (idx>=0&&idx<STAFF_LENGTH)
      leds[HEAD_LENGTH+idx]+=CHSV(splats[i].hue,230,(uint8_t)(splats[i].life*splats[i].life*255));
  }

  // In-flight drop — single bright pixel + 2-LED trail
  for (int i=0; i<MAX_IDROPS; i++) {
    if (!drops[i].active) continue;
    int idx=(int)drops[i].pos;
    int dir=drops[i].vel>0?1:-1;
    if (idx>=0&&idx<STAFF_LENGTH) leds[HEAD_LENGTH+idx]=CHSV(drops[i].hue,180,255);
    int t1=idx-dir, t2=idx-dir*2;
    if (t1>=0&&t1<STAFF_LENGTH) leds[HEAD_LENGTH+t1]+=CHSV(drops[i].hue,210,150);
    if (t2>=0&&t2<STAFF_LENGTH) leds[HEAD_LENGTH+t2]+=CHSV(drops[i].hue,230,70);
  }

  finishEffect(dt,0.5f,0.0f);
}
void renderRippleTankEffect(float dt) {
  #define RT_SOURCES 4
  static float rtPos[RT_SOURCES]   = {0.2f, 0.4f, 0.6f, 0.8f};
  static float rtPhase[RT_SOURCES] = {0.0f, 1.57f, 3.14f, 4.71f};
  static float rtFreq[RT_SOURCES]  = {3.0f, 4.5f, 3.8f, 5.2f};
  uint32_t now = millis();
  for (int s = 0; s < RT_SOURCES; s++) {
    rtPhase[s] += dt * rtFreq[s];
    if (rtPhase[s] > 6.283f) rtPhase[s] -= 6.283f;
  }
  if (mpu_ready) {
    float tilt = clampf(accel[1] * 0.1f, -0.15f, 0.15f);
    for (int s = 0; s < RT_SOURCES; s++) {
      rtPos[s] = clampf(rtPos[s] + tilt * dt, 0.05f, 0.95f);
    }
  }
  for (int i = 0; i < STAFF_LENGTH; i++) {
    float pos = (float)i / STAFF_LENGTH;
    float superposition = 0.0f;
    for (int s = 0; s < RT_SOURCES; s++) {
      float dist = fabs(pos - rtPos[s]) * 30.0f;
      superposition += sin(dist - rtPhase[s]) * expDecay(dist * 0.1f);
    }
    superposition /= RT_SOURCES;
    float norm = superposition * 0.5f + 0.5f;
    uint8_t hue = 160 + (uint8_t)(norm * 60);
    uint8_t sat = 220;
    uint8_t val = (uint8_t)(norm * norm * 200);
    leds[HEAD_LENGTH + i] = CHSV(hue, sat, val);
  }
  finishEffect(dt, 0.5f, 0.0f);
}
void renderSupernovaEffect(float dt) {
  static float novaPhase   = 0.0f;
  static float novaRadius  = 0.0f;
  static float novaIntensity = 1.0f;
  static bool  expanding   = true;
  uint32_t now = millis();
  int ci = STAFF_LENGTH / 2;
  novaPhase += dt * 2.0f;
  if (expanding) {
    novaRadius += dt * 0.6f;
    novaIntensity = 1.0f - novaRadius * 0.8f;
    if (novaRadius >= 1.2f || novaIntensity < 0.05f) { expanding = false; novaRadius = 0.0f; }
  } else {
    novaRadius += dt * 0.2f;
    novaIntensity = novaRadius * 0.5f;
    if (novaRadius >= 0.6f) { expanding = true; novaRadius = 0.0f; novaIntensity = 1.0f; htState.headImpactIntensity = 1.5f; htState.tailImpactIntensity = 1.5f; }
  }
  if (mpu_ready) novaPhase += accel[1] * dt * 3.0f;
  if(novaPhase > 6.283f) novaPhase -= 6.283f;
  if(novaPhase < -6.283f) novaPhase += 6.283f;
  for (int i = 0; i < STAFF_LENGTH; i++) {
    float pos = (float)i / STAFF_LENGTH;
    float nebula = sin(pos * 12 + novaPhase) * 0.3f + 0.3f;
    leds[HEAD_LENGTH + i] = CHSV(200 + (uint8_t)(nebula * 40), 220, (uint8_t)(nebula * 50));
  }
  for (int i = 0; i < STAFF_LENGTH; i++) {
    float distFromCenter = fabs((float)i - ci) / (float)ci;
    float shellDist = fabs(distFromCenter - novaRadius);
    if (shellDist < 0.15f) {
      float ints = (1.0f - shellDist / 0.15f) * novaIntensity;
      ints = ints * ints;
      uint8_t h = expanding ? (uint8_t)(30 + ints * 20) : 200;
      leds[HEAD_LENGTH + i] += CHSV(h, 200, (uint8_t)(ints * 220));
    }
  }
  finishEffect(dt, 0.5f, 0.0f);
}
void renderBioluminescenceEffect(float dt) {
  #define MAX_BIO 20
  static float bioPos[MAX_BIO],bioBright[MAX_BIO],bioPhase[MAX_BIO],bioSpeed[MAX_BIO];
  static bool bioInit = false;
  uint32_t now = millis();
  int ci = STAFF_LENGTH / 2;
  if (!bioInit) {
    for (int i = 0; i < MAX_BIO; i++) {
      bioPos[i]    = random(100) / 100.0f;
      bioBright[i] = 0.0f;
      bioPhase[i]  = random(628) / 100.0f;
      bioSpeed[i]  = (random(2) ? 1.0f : -1.0f) * (0.05f + random(10) / 100.0f);
    }
    bioInit = true;
  }
  float tilt = mpu_ready ? clampf(accel[1] * 0.3f, -0.5f, 0.5f) : 0.0f;
  for (int i = 0; i < MAX_BIO; i++) {
    bioPhase[i] += dt * (1.0f + random(10) / 10.0f);
    if (bioPhase[i] > 6.283f) bioPhase[i] -= 6.283f;
    bioPos[i] += (bioSpeed[i] + tilt) * dt;
    if (bioPos[i] < 0.0f) bioPos[i] = 1.0f;
    if (bioPos[i] > 1.0f) bioPos[i] = 0.0f;
    bioBright[i] = (sin(bioPhase[i]) * 0.5f + 0.5f);
    if (bioBright[i] > 0.8f && random(100) < 5) {
      int nb = (i + 1) % MAX_BIO;
      bioPhase[nb] = bioPhase[i] * 0.5f;
    }
  }
  fadeToBlackBy(leds + HEAD_LENGTH, STAFF_LENGTH, 15);
  for (int i = 0; i < MAX_BIO; i++) {
    if (bioBright[i] < 0.1f) continue;
    int idx = HEAD_LENGTH + (int)(bioPos[i] * (STAFF_LENGTH - 1));
    uint8_t h = 140 + (uint8_t)(bioBright[i] * 40);
    uint8_t v = (uint8_t)(bioBright[i] * bioBright[i] * 220);
    for (int s = -2; s <= 2; s++) {
      int si = idx + s;
      if (si >= HEAD_LENGTH && si < HEAD_LENGTH + STAFF_LENGTH) {
        float f = 1.0f - fabs((float)s) / 3.0f;
        leds[si] += CHSV(h, 200, (uint8_t)(v * f));
      }
    }
  }
  if (mpu_ready && fabs(accel[1]) > 0.5f) {
    htState.headImpactIntensity = max(htState.headImpactIntensity, clampf(fabs(accel[1]) * 0.5f, 0.0f, 1.0f));
    htState.tailImpactIntensity = max(htState.tailImpactIntensity, clampf(fabs(accel[1]) * 0.5f, 0.0f, 1.0f));
  }
  finishEffect(dt, 0.5f, 0.0f);
}
void renderPlasmaWavesEffect(float dt) {
  static float phase1 = 0, phase2 = 0, phase3 = 0;
  float sm = globalSpeed / 128.0f;
  int ci = STAFF_LENGTH / 2;
  
  phase1 += dt * 2.5f * sm; if(phase1 > 6.283f) phase1 -= 6.283f;
  phase2 += dt * 1.8f * sm; if(phase2 > 6.283f) phase2 -= 6.283f;
  phase3 += dt * 3.2f * sm; if(phase3 > 6.283f) phase3 -= 6.283f;
  
  for(int i = 0; i < STAFF_LENGTH; i++){
    float d = fabs((float)i - ci) / (float)ci;
    float p1 = sin(d * 8 - phase1) * 0.5f + 0.5f;
    float p2 = sin(d * 12 + phase2) * 0.5f + 0.5f;
    float p3 = sin(d * 6 - phase3 * 0.7f) * 0.5f + 0.5f;
    
    float plasma = (p1 + p2 + p3) / 3.0f;
    uint8_t hue = (uint8_t)(plasma * 255);
    uint8_t sat = 220;
    uint8_t val = (uint8_t)(100 + plasma * 155);
    leds[HEAD_LENGTH + i] = CHSV(hue, sat, val);
  }
  
  finishEffect(dt,0.6f,0, 128,255,220, 200,255,220);
}
void renderElectronOrbitEffect(float dt) {
  static float electronAngle[3] = {0, 2.09f, 4.19f};
  static float orbitSpeed[3] = {2.0f, 1.5f, 2.5f};
  float sm = globalSpeed / 128.0f;
  int ci = STAFF_LENGTH / 2;
  
  fadeToBlackBy(leds + HEAD_LENGTH, STAFF_LENGTH, 40);
  
  for(int i = -3; i <= 3; i++){
    int idx = ci + i;
    if(idx >= 0 && idx < STAFF_LENGTH){
      uint8_t val = (uint8_t)(200 - abs(i) * 30);
      leds[HEAD_LENGTH + idx] = CHSV(60, 255, val);
    }
  }
  
  for(int e = 0; e < 3; e++){
    electronAngle[e] += dt * orbitSpeed[e] * sm;
    if(electronAngle[e] > 6.283f) electronAngle[e] -= 6.283f;
    
    float orbitRadius = 20 + e * 8;
    int pos = ci + (int)(sin(electronAngle[e]) * orbitRadius);
    
    if(pos >= 0 && pos < STAFF_LENGTH){
      uint8_t hue = 160 + e * 30;
      leds[HEAD_LENGTH + pos] = CHSV(hue, 255, 255);
      if(pos > 0) leds[HEAD_LENGTH + pos - 1] += CHSV(hue, 220, 100);
      if(pos < STAFF_LENGTH - 1) leds[HEAD_LENGTH + pos + 1] += CHSV(hue, 220, 100);
    }
  }
  
  finishEffect(dt,0.7f,0, 60,255,220, 60,255,220);
}
void renderDNAHelixEffect(float dt) {
  static float helixPhase = 0;
  float sm = globalSpeed / 128.0f;
  int ci = STAFF_LENGTH / 2;
  
  helixPhase += dt * 3.0f * sm; if(helixPhase > 6.283f) helixPhase -= 6.283f;
  
  fadeToBlackBy(leds + HEAD_LENGTH, STAFF_LENGTH, 30);
  
  for(int i = 0; i < STAFF_LENGTH; i++){
    float d = fabs((float)i - ci);
    
    float strand1 = sin(d * 0.4f - helixPhase);
    float strand2 = sin(d * 0.4f - helixPhase + 3.14f);
    
    if(strand1 > 0.6f){
      uint8_t val = (uint8_t)((strand1 - 0.6f) * 2.5f * 255);
      leds[HEAD_LENGTH + i] += CHSV(160, 255, val);
    }
    
    if(strand2 > 0.6f){
      uint8_t val = (uint8_t)((strand2 - 0.6f) * 2.5f * 255);
      leds[HEAD_LENGTH + i] += CHSV(200, 255, val);
    }
    
    if(fabs(strand1) < 0.2f || fabs(strand2) < 0.2f){
      leds[HEAD_LENGTH + i] += CHSV(0, 0, 80);
    }
  }
  
  finishEffect(dt,0.5f,0, 160,255,220, 200,255,220);
}
void renderMeteorShowerEffect(float dt) {
  static float meteors[8];
  static float meteorSpeeds[8];
  static bool meteorsInit = false;
  int ci = STAFF_LENGTH / 2;
  
  if(!meteorsInit){
    for(int i = 0; i < 8; i++){
      meteors[i] = random(ci);
      meteorSpeeds[i] = 20 + random(30);
    }
    meteorsInit = true;
  }
  
  fadeToBlackBy(leds + HEAD_LENGTH, STAFF_LENGTH, 35);
  
  for(int i = 0; i < STAFF_LENGTH; i++){
    if(random(1000) < 3){
      leds[HEAD_LENGTH + i] = CHSV(0, 0, random(80, 150));
    }
  }
  
  for(int m = 0; m < 8; m++){
    meteors[m] += dt * meteorSpeeds[m];
    
    if(meteors[m] > ci){
      meteors[m] = 0;
      meteorSpeeds[m] = 20 + random(30);
    }
    
    int pos1 = ci + (int)meteors[m];
    int pos2 = ci - (int)meteors[m];
    
    for(int t = 0; t < 8; t++){
      int trail1 = pos1 - t;
      int trail2 = pos2 + t;
      
      uint8_t brightness = (uint8_t)((8 - t) * 30);
      uint8_t hue = 20 + t * 5;
      
      if(trail1 >= 0 && trail1 < STAFF_LENGTH){
        leds[HEAD_LENGTH + trail1] += CHSV(hue, 255, brightness);
      }
      if(trail2 >= 0 && trail2 < STAFF_LENGTH){
        leds[HEAD_LENGTH + trail2] += CHSV(hue, 255, brightness);
      }
    }
  }
  
  finishEffect(dt,0.4f,0, 200,255,200, 200,255,200);
}
void renderMagneticPullEffect(float dt) {
  static float pullPhase = 0;
  int ci = STAFF_LENGTH / 2;
  pullPhase += dt * (globalSpeed / 64.0f);
  if(pullPhase >= 1.0f) pullPhase -= (float)(int)pullPhase;

  for(int i=0; i<STAFF_LENGTH; i++){
    uint8_t val = beatsin8(10, 10, 40, 0, i*2);
    leds[HEAD_LENGTH+i] = CHSV(160, 255, val);
  }

  for (int i=0; i<4; i++) {
    float pos = fmod(pullPhase + (i * 0.25f), 1.0f);
    int pLeft  = ci - (int)(pos * ci);
    int pRight = ci + (int)(pos * ci);
    if(pLeft  >= 0)           leds[HEAD_LENGTH+pLeft]  += CHSV(180, 100, 255);
    if(pRight < STAFF_LENGTH) leds[HEAD_LENGTH+pRight] += CHSV(180, 100, 255);
  }

  uint8_t coreVal = 100 + (uint8_t)(audioLevel * 155);
  for(int i=-3; i<=3; i++) {
    int idx = ci+i;
    if(idx >= 0 && idx < STAFF_LENGTH)
      leds[HEAD_LENGTH+idx] = CHSV(140, 200, coreVal);
  }
  finishEffect(dt, 0.5f, 0.0f);
}
void renderSolarFlareEffect(float dt) {
  static float flareAge = 0;
  int ci = STAFF_LENGTH / 2;
  flareAge += dt * (globalSpeed / 128.0f);
  if(flareAge > 6.283f) flareAge -= 6.283f;

  fadeToBlackBy(leds + HEAD_LENGTH, STAFF_LENGTH, 30);

  float eruption = audioLevel * 20.0f + (mpu_ready ? fabs(gyro[0])/50.0f : 0);
  for(int i=0; i < (int)eruption; i++) {
    float spd = 20 + random(60);
    float p = sin(flareAge + i) * spd * dt;
    int idxL = ci - (int)fabs(p * 10);
    int idxR = ci + (int)fabs(p * 10);
    if(idxL >= 0 && idxL < STAFF_LENGTH) leds[HEAD_LENGTH+idxL] += CHSV(random(20), 255, 200);
    if(idxR >= 0 && idxR < STAFF_LENGTH) leds[HEAD_LENGTH+idxR] += CHSV(random(20), 255, 200);
  }
  finishEffect(dt, 0.5f, 0.0f);
}
void renderMolecularVibrationEffect(float dt) {
  int ci = STAFF_LENGTH / 2;
  int jitter = (int)(audioLevel * 15.0f);

  for(int i=0; i<STAFF_LENGTH; i++) {
    int dist = (int)fabs((float)i - ci);
    if(dist % 8 == 0) {
      int offset = (jitter > 0) ? (random(jitter*2+1) - jitter) : 0;
      int idx = i + offset;
      if(idx >= 0 && idx < STAFF_LENGTH)
        leds[HEAD_LENGTH+idx] = CHSV(200, 255, 180);
    }
  }
  finishEffect(dt, 0.4f, 0.0f);
}
void renderBlackHoleEffect(float dt) {
  static float horizon = 0;
  int ci = STAFF_LENGTH / 2;
  horizon += dt; if(horizon > 750.0f) horizon -= 750.0f;  // keep fmod(d+horizon*20,15) accurate

  for(int i=0; i<STAFF_LENGTH; i++) {
    float d = fabs((float)i - ci);
    float pull = fmod(d + (horizon * 20), 15.0f);
    if(pull < 2.0f)
      leds[HEAD_LENGTH+i] = CHSV(230, 255, (uint8_t)((1.0f - (d/ci)) * 255));
  }
  for(int i=-2; i<=2; i++) {
    int idx = ci+i;
    if(idx >= 0 && idx < STAFF_LENGTH)
      leds[HEAD_LENGTH+idx] = CRGB::Black;
  }
  finishEffect(dt, 0.8f, 0.0f);
}
