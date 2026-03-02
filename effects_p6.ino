
void render80sBlocksEffect(float dt) {
  #define BK_N 4
  struct Block { float pos, vel, size, impactFlash; uint8_t hue; };
  static Block bk[BK_N];
  static bool bkInit = false;

  if (!bkInit) {
    const uint8_t hues[BK_N]  = { 224, 128, 64, 20 };   // hot-pink, cyan, yellow, orange
    const float   sizes[BK_N] = { 14.0f, 16.0f, 13.0f, 15.0f };
    const float   vels[BK_N]  = { 40.0f, -38.0f, 32.0f, -44.0f };
    for (int b = 0; b < BK_N; b++) {
      bk[b].pos         = (float)STAFF_LENGTH * (0.1f + b * 0.27f);
      bk[b].vel         = vels[b];
      bk[b].size        = sizes[b];
      bk[b].hue         = hues[b];
      bk[b].impactFlash = 0.0f;
    }
    bkInit = true;
  }

  static float lastAccelYBK = 0.0f;
  float accelYBK = mpu_ready ? clampf(accel[1] * 0.8f, -1.2f, 1.2f) : 0.0f;
  float jerkBK   = clampf((accelYBK - lastAccelYBK) * 5.0f, -3.0f, 3.0f);
  lastAccelYBK   = accelYBK;

  float tiltPush = mpu_ready ? accelYBK * 100.0f + jerkBK * 80.0f : 0.0f;

  // Big jerk: scatter blocks in opposite directions for maximum collisions
  if (fabs(jerkBK) > 1.8f) {
    for (int b = 0; b < BK_N; b++)
      bk[b].vel += jerkBK * (b % 2 == 0 ? 1.0f : -1.0f) * 25.0f;
    htState.headImpactIntensity = min(1.5f, fabs(jerkBK) * 0.4f);
    htState.tailImpactIntensity = min(1.5f, fabs(jerkBK) * 0.4f);
  }

  // Physics update
  for (int b = 0; b < BK_N; b++) {
    bk[b].vel  += tiltPush * dt;
    bk[b].vel  *= (1.0f - dt * 0.15f);              // light air friction
    bk[b].vel   = clampf(bk[b].vel, -220.0f, 220.0f);
    bk[b].pos  += bk[b].vel * dt;
    bk[b].impactFlash = max(0.0f, bk[b].impactFlash - dt * 5.0f);

    float half = bk[b].size * 0.5f;
    if (bk[b].pos - half < 0.0f) {
      bk[b].pos = half;
      if (bk[b].vel < 0.0f) {
        float spd = fabs(bk[b].vel);
        htState.headImpactIntensity = min(1.5f, spd / 80.0f);
        bk[b].impactFlash = clampf(spd / 100.0f, 0.3f, 1.0f);
        bk[b].vel = spd * 0.85f;
      }
    }
    if (bk[b].pos + half > (float)(STAFF_LENGTH - 1)) {
      bk[b].pos = (float)(STAFF_LENGTH - 1) - half;
      if (bk[b].vel > 0.0f) {
        float spd = fabs(bk[b].vel);
        htState.tailImpactIntensity = min(1.5f, spd / 80.0f);
        bk[b].impactFlash = clampf(spd / 100.0f, 0.3f, 1.0f);
        bk[b].vel = -spd * 0.85f;
      }
    }
  }

  // Block-block collisions — elastic swap with damping
  for (int a = 0; a < BK_N; a++) {
    for (int b = a + 1; b < BK_N; b++) {
      float minGap = (bk[a].size + bk[b].size) * 0.5f;
      float delta  = bk[b].pos - bk[a].pos;
      if (fabs(delta) < minGap && fabs(delta) > 0.01f) {
        float push = (minGap - fabs(delta)) * 0.5f;
        float dir  = (delta > 0.0f) ? 1.0f : -1.0f;
        bk[a].pos -= dir * push;
        bk[b].pos += dir * push;
        float va = bk[a].vel, vb = bk[b].vel;
        bk[a].vel  = vb * 0.9f;
        bk[b].vel  = va * 0.9f;
        float relSpd = fabs(va - vb);
        float fl = clampf(relSpd / 80.0f, 0.2f, 1.0f);
        bk[a].impactFlash = max(bk[a].impactFlash, fl);
        bk[b].impactFlash = max(bk[b].impactFlash, fl);
      }
    }
  }

  // Render — solid blocks on black background
  for (int i = 0; i < STAFF_LENGTH; i++) leds[HEAD_LENGTH + i] = CRGB::Black;
  for (int b = 0; b < BK_N; b++) {
    int center = (int)bk[b].pos;
    int half   = (int)(bk[b].size * 0.5f);
    for (int i = center - half; i <= center + half; i++) {
      if (i < 0 || i >= STAFF_LENGTH) continue;
      float edge = 1.0f - fabs((float)(i - center)) / (float)(half + 1) * 0.2f;
      leds[HEAD_LENGTH + i] = CHSV(bk[b].hue, 255, (uint8_t)(215.0f * edge));
    }
    // Impact flash: whole block brightens + white edge burst
    if (bk[b].impactFlash > 0.05f) {
      uint8_t fv = (uint8_t)(bk[b].impactFlash * 220.0f);
      for (int i = center - half; i <= center + half; i++) {
        if (i >= 0 && i < STAFF_LENGTH) leds[HEAD_LENGTH + i] += CHSV(0, 0, fv / 2);
      }
      for (int e = -3; e <= 3; e++) {
        float ef = 1.0f - fabs((float)e) / 4.0f;
        uint8_t ev = (uint8_t)(fv * ef);
        int ie1 = center - half + e, ie2 = center + half + e;
        if (ie1 >= 0 && ie1 < STAFF_LENGTH) leds[HEAD_LENGTH + ie1] += CHSV(0, 0, ev);
        if (ie2 >= 0 && ie2 < STAFF_LENGTH) leds[HEAD_LENGTH + ie2] += CHSV(0, 0, ev);
      }
    }
  }

  // Head/tail color: nearest block to each end
  float hDist = (float)STAFF_LENGTH, tDist = (float)STAFF_LENGTH;
  uint8_t hHue = bk[0].hue, tHue = bk[0].hue;
  for (int b = 0; b < BK_N; b++) {
    if (bk[b].pos < hDist)                       { hDist = bk[b].pos;                       hHue = bk[b].hue; }
    if ((float)STAFF_LENGTH - bk[b].pos < tDist) { tDist = (float)STAFF_LENGTH - bk[b].pos; tHue = bk[b].hue; }
  }
  if (!customColorMode) { headHue = hHue; headSat = 255; headVal = 220; tailHue = tHue; tailSat = 255; tailVal = 220; }
  updateHeadTailReactivity(dt, 0.5f, accelYBK * 0.15f);
  renderHeadAndTail();

  // Roll: rainbow scan across all blocks at spin speed
  if (mpu_ready && fabs(rollRate) > 55.0f) {
    float str = clampf((fabs(rollRate) - 55.0f) / 145.0f, 0.0f, 1.0f);
    for (int i = 0; i < STAFF_LENGTH; i++) {
      uint8_t h = (uint8_t)(fmod((float)i * 1.8f + vortexPhase * 90.0f, 256.0f));
      leds[HEAD_LENGTH + i] = blend(leds[HEAD_LENGTH + i], CHSV(h, 255, 255), (uint8_t)(str * 160));
    }
  }
  applyRollGlow(224, 255);   // hot-pink spin glow
}
void renderSlinkyEffect(float dt) {
  static float slinkyPhase = 0.0f;
  static float compression = 0.5f;
  uint32_t now = millis();
  int ci = STAFF_LENGTH / 2;
  static float lastAccelYSL=0.0f;
  float accelYSL=mpu_ready?clampf(accel[1]*0.8f,-1.2f,1.2f):0.0f;
  float jerkSL=clampf((accelYSL-lastAccelYSL)*5.0f,-3.0f,3.0f); lastAccelYSL=accelYSL;
  float tilt=mpu_ready?accelYSL:sin(now*0.001f)*0.5f;
  slinkyPhase+=dt*(2.0f+fabs(tilt)*5.0f+fabs(jerkSL)*4.0f);
  if(slinkyPhase>6.283f) slinkyPhase-=6.283f;
  compression=compression*0.88f+(tilt+jerkSL*0.3f)*0.12f;
  for (int i = 0; i < STAFF_LENGTH; i++) {
    float pos = (float)i / STAFF_LENGTH;
    float wave = sin(pos * 20.0f - slinkyPhase + compression * 5.0f) * 0.5f + 0.5f;
    float wave2 = sin(pos * 12.0f + slinkyPhase * 0.7f) * 0.5f + 0.5f;
    uint8_t hue = 180 + (uint8_t)(wave * 40);
    uint8_t sat = 200;
    uint8_t val = (uint8_t)((wave * wave2) * 220);
    leds[HEAD_LENGTH + i] = CHSV(hue, sat, val);
  }
  if (mpu_ready) {
    float impSL=clampf(fabs(accelYSL)*0.8f+fabs(jerkSL)*0.5f,0.0f,1.5f);
    if(impSL>0.4f){htState.headImpactIntensity=max(htState.headImpactIntensity,impSL);htState.tailImpactIntensity=max(htState.tailImpactIntensity,impSL);}
  }
  finishEffect(dt, 0.5f, accelYSL * 0.2f);
  if (mpu_ready && fabs(rollRate) > 55.0f) {
    // Compression ripple: tight rapid waves like a slinky being shaken fast
    float str = clampf((fabs(rollRate)-55.0f)/145.0f, 0.0f, 1.0f);
    for (int i = 0; i < STAFF_LENGTH; i++) {
      float pos = (float)i/(float)(STAFF_LENGTH-1);
      float wave = sin(pos*25.13f + vortexPhase*5.0f)*0.5f+0.5f; wave*=wave*str;
      leds[HEAD_LENGTH+i] += CHSV(155, 200, (uint8_t)(wave*190));
    }
  }
  applyRollGlow(155, 200);   // spring green-cyan coil when spinning
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
  static uint8_t impactHue=200, dropHue=0, headImpactHue=200, tailImpactHue=200;
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
  static float lastAccelYID=0.0f;
  float accelYID=mpu_ready?clampf(accel[1]*0.8f,-1.2f,1.2f):0.0f;
  float jerkID=clampf((accelYID-lastAccelYID)*5.0f,-3.0f,3.0f); lastAccelYID=accelYID;
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
  // Strong jerk shakes ink loose: burst of extra drops + immediate fizzle
  if (fabs(jerkID) > 1.0f) {
    toSpawn+=2;
    blobFizzle=max(blobFizzle,fabs(jerkID)*0.4f);
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
      if (hp<STAFF_LENGTH/2) { htState.headImpactIntensity=max(htState.headImpactIntensity,0.9f); headImpactHue=impactHue; }
      else                   { htState.tailImpactIntensity=max(htState.tailImpactIntensity,0.9f); tailImpactHue=impactHue; }
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

  // Head/tail fizzle — sparks use the hue of the drop that hit THAT specific end
  if (fizzleH>0.05f) {
    int fLen=(int)(fizzleH*28);
    for (int i=0;i<fLen&&i<STAFF_LENGTH;i++)
      if (random(100)<(int)(fizzleH*70))
        leds[HEAD_LENGTH+i]+=CHSV(headImpactHue,220,(uint8_t)(fizzleH*220));
  }
  if (fizzleT>0.05f) {
    int fLen=(int)(fizzleT*28);
    for (int i=0;i<fLen&&i<STAFF_LENGTH;i++)
      if (random(100)<(int)(fizzleT*70))
        leds[HEAD_LENGTH+STAFF_LENGTH-1-i]+=CHSV(tailImpactHue,220,(uint8_t)(fizzleT*220));
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

  // Head and tail flowers mirror the ink colour that splashed at each end.
  // Brightness is proportional to fizzle intensity so they fade in sync with the staff sparks.
  float hFizz = clampf(fizzleH, 0.0f, 1.0f);
  float tFizz = clampf(fizzleT, 0.0f, 1.0f);
  if (!customColorMode) {
    headHue = headImpactHue;  headSat = (uint8_t)(180 + hFizz*75);  headVal = (uint8_t)(55 + hFizz*200);
    tailHue = tailImpactHue;  tailSat = (uint8_t)(180 + tFizz*75);  tailVal = (uint8_t)(55 + tFizz*200);
  }
  finishEffect(dt,0.5f,0.0f);
  if (mpu_ready && fabs(rollRate) > 55.0f) {
    // Ink rings: two concentric rings expand from center like ink dropped in water
    float str = clampf((fabs(rollRate)-55.0f)/145.0f, 0.0f, 1.0f);
    int ci = STAFF_LENGTH/2;
    for (int r = 0; r < 2; r++) {
      float ring = fmod(vortexPhase*0.8f + r*0.5f, 1.0f) * (float)(STAFF_LENGTH/2);
      for (int i = 0; i < STAFF_LENGTH; i++) {
        float d = fabs(fabs((float)i-ci) - ring);
        if (d < 4.5f) { float f=(1.0f-d/4.5f)*str; leds[HEAD_LENGTH+i]+=CHSV(210,220,(uint8_t)(f*210)); }
      }
    }
  }
  applyRollGlow(210, 200);   // purple-blue ink swirl when spinning
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
  {
    static float lastAccelYRT=0.0f;
    float accelYRT=mpu_ready?clampf(accel[1]*0.8f,-1.2f,1.2f):0.0f;
    float jerkRT=clampf((accelYRT-lastAccelYRT)*5.0f,-3.0f,3.0f); lastAccelYRT=accelYRT;
    if (mpu_ready) {
      for (int s = 0; s < RT_SOURCES; s++) {
        // Strong tilt sweeps sources; jerk snaps them dramatically (alternating directions)
        rtPos[s]=clampf(rtPos[s]+accelYRT*0.5f*dt+jerkRT*0.04f*(s%2==0?1:-1),0.05f,0.95f);
        // Jerk spikes frequency for a visible ripple burst
        if(fabs(jerkRT)>0.5f) rtFreq[s]=clampf(rtFreq[s]+fabs(jerkRT)*3.0f*dt,rtFreq[s],15.0f);
      }
    }
    // Freq decays back toward base values (~0.5 s time constant)
    const float baseF[RT_SOURCES]={3.0f,4.5f,3.8f,5.2f};
    for (int s=0;s<RT_SOURCES;s++) rtFreq[s]=rtFreq[s]*0.98f+baseF[s]*0.02f;
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
  if (mpu_ready && fabs(rollRate) > 55.0f) {
    // Interference burst: two counter-propagating waves produce rapid beat pattern
    float str = clampf((fabs(rollRate)-55.0f)/145.0f, 0.0f, 1.0f);
    for (int i = 0; i < STAFF_LENGTH; i++) {
      float pos = (float)i/(float)STAFF_LENGTH;
      float w1 = sin(pos*25.13f + vortexPhase*5.0f)*0.5f+0.5f;
      float w2 = sin(pos*18.85f - vortexPhase*3.5f)*0.5f+0.5f;
      float inter = w1*w2*str;
      leds[HEAD_LENGTH+i] += CHSV(160, 220, (uint8_t)(inter*200));
    }
  }
  applyRollGlow(160, 220);   // teal wave surge when spinning
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
  {
    static float lastAccelYSN=0.0f;
    float accelYSN=mpu_ready?clampf(accel[1]*0.8f,-1.2f,1.2f):0.0f;
    float jerkSN=clampf((accelYSN-lastAccelYSN)*5.0f,-3.0f,3.0f); lastAccelYSN=accelYSN;
    if (mpu_ready) {
      novaPhase+=(accelYSN*10.0f+jerkSN*6.0f)*dt;
      // Strong jerk triggers immediate nova burst
      if(fabs(jerkSN)>1.5f){expanding=true;novaRadius=0.0f;novaIntensity=1.0f;htState.headImpactIntensity=1.5f;htState.tailImpactIntensity=1.5f;}
    }
  }
  if(novaPhase > 6.283f) novaPhase -= 6.283f;
  if(novaPhase < -6.283f) novaPhase += 6.283f;
  for (int i = 0; i < STAFF_LENGTH; i++) {
    float pos    = (float)i / STAFF_LENGTH;
    float dist   = fabs(pos - 0.5f) * 2.0f;                  // 0=center, 1=edge
    float nebula = sin(pos * 12 + novaPhase) * 0.3f + 0.7f;  // much brighter base
    float corona = (1.0f - dist * dist) * (expanding ? 0.65f : 0.9f);
    float ripple = sin(pos * 22 + novaPhase * 1.6f) * 0.12f + 0.88f;
    uint8_t h    = expanding ? (uint8_t)(20 + dist * 10) : (uint8_t)(15 + dist * 12);
    uint8_t v    = (uint8_t)min(255.0f, nebula * 110.0f + corona * 145.0f * ripple);
    leds[HEAD_LENGTH + i] = CHSV(h, 230, v);
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
  if (mpu_ready && fabs(rollRate) > 55.0f) {
    // Nova rings: three rapid expanding shells of orange-yellow light from center
    float str = clampf((fabs(rollRate)-55.0f)/145.0f, 0.0f, 1.0f);
    int ci = STAFF_LENGTH/2;
    for (int r = 0; r < 3; r++) {
      float ring = fmod(vortexPhase*1.1f + (float)r/3.0f, 1.0f) * (float)(STAFF_LENGTH/2);
      for (int i = 0; i < STAFF_LENGTH; i++) {
        float d = fabs(fabs((float)i-ci) - ring);
        if (d < 5.0f) { float f=(1.0f-d/5.0f)*str; uint8_t h=30-(uint8_t)(ring/(STAFF_LENGTH/2)*15); leds[HEAD_LENGTH+i]+=CHSV(h,255,(uint8_t)(f*230)); }
      }
    }
  }
  applyRollGlow(25, 255);    // orange nova burst when spinning
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
  static float bioFizzle = 0.0f;
  static float planktonFlash  = 0.0f;  // white flash intensity on jerk
  static float bioJerkStrobe  = 0.0f;  // head/tail strobe trigger, longer-lived
  static float bioStrobePhase = 0.0f;  // 0-1 cycles at ~10 Hz for white/purple alternation
  static float lastAccelYBL=0.0f;
  float accelYBL=mpu_ready?clampf(accel[1]*0.8f,-1.2f,1.2f):0.0f;
  float jerkBL=clampf((accelYBL-lastAccelYBL)*5.0f,-3.0f,3.0f); lastAccelYBL=accelYBL;
  float tilt = mpu_ready ? accelYBL : 0.0f;
  if (fabs(jerkBL) > 1.0f) {
    bioFizzle     = max(bioFizzle,     fabs(jerkBL) * 0.55f);
    planktonFlash = max(planktonFlash, fabs(jerkBL) * 0.8f);
    bioJerkStrobe = max(bioJerkStrobe, fabs(jerkBL) * 0.9f);
  }
  for (int i = 0; i < MAX_BIO; i++) {
    bioPhase[i] += dt * (1.5f + random(10) / 10.0f + fabs(jerkBL)*2.0f);
    if (bioPhase[i] > 6.283f) bioPhase[i] -= 6.283f;
    if (fabs(jerkBL) > 1.0f) bioPhase[i] = 1.5708f;  // snap all to peak brightness on jerk
    bioPos[i] += (bioSpeed[i] + tilt*0.4f) * dt;
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
  // Bio fizzle — each bright organism emits sparks on jerk, like ink drop fizzle
  if (bioFizzle > 0.05f) {
    for (int i = 0; i < MAX_BIO; i++) {
      if (bioBright[i] < 0.3f) continue;
      int center = HEAD_LENGTH + (int)(bioPos[i] * (STAFF_LENGTH - 1));
      for (int s = -8; s <= 8; s++) {
        int si = center + s;
        if (si < HEAD_LENGTH || si >= HEAD_LENGTH + STAFF_LENGTH) continue;
        if (random(100) < (int)(bioFizzle * bioBright[i] * 65))
          leds[si] += CHSV(140 + (uint8_t)(bioBright[i] * 40), 180, (uint8_t)(bioFizzle * bioBright[i] * 210));
      }
    }
    bioFizzle = max(0.0f, bioFizzle - dt * 2.5f);
  }
  // Plankton flash — white sparkling eruption across the whole staff on jerk
  if (planktonFlash > 0.05f) {
    for (int i = 0; i < STAFF_LENGTH; i++) {
      if (random(100) < (int)(planktonFlash * 65))
        leds[HEAD_LENGTH + i] += CHSV(180, (uint8_t)random(35), (uint8_t)(planktonFlash * 210));
    }
  }
  planktonFlash = max(0.0f, planktonFlash - dt * 3.0f);
  bioJerkStrobe = max(0.0f, bioJerkStrobe - dt * 1.5f);
  bioStrobePhase += dt * 10.0f;
  if (bioStrobePhase >= 1.0f) bioStrobePhase -= 1.0f;
  if (mpu_ready) {
    float impBL=clampf(fabs(accelYBL)*0.8f+fabs(jerkBL)*0.5f,0.0f,1.5f);
    if(impBL>0.3f){htState.headImpactIntensity=max(htState.headImpactIntensity,impBL);htState.tailImpactIntensity=max(htState.tailImpactIntensity,impBL);}
  }
  finishEffect(dt, 0.5f, 0.0f);
  // White/purple strobe on head and tail during jerk disturbance
  if (bioJerkStrobe > 0.05f) {
    CRGB sc = (bioStrobePhase < 0.5f) ? CRGB(CRGB::White) : CRGB(CHSV(200, 255, 255));
    uint8_t ba = (uint8_t)(bioJerkStrobe * 210.0f);
    for (int i = 0; i < HEAD_LENGTH; i++)
      leds[i] = blend(leds[i], sc, ba);
    for (int hi = 0; hi < HEAD_LENGTH; hi++) {
      int t1 = TAIL_START + TAIL_OFFSET + hi*2;
      int t2 = t1 + 1;
      if (t1 < NUM_LEDS) leds[t1] = blend(leds[t1], sc, ba);
      if (t2 < NUM_LEDS) leds[t2] = blend(leds[t2], sc, ba);
    }
  }
  if (mpu_ready && fabs(rollRate) > 55.0f) {
    // Bio bloom wave: slow sinusoidal glow sweeps the whole staff like a bloom pulse
    float str = clampf((fabs(rollRate)-55.0f)/145.0f, 0.0f, 1.0f);
    for (int i = 0; i < STAFF_LENGTH; i++) {
      float pos = (float)i/(float)(STAFF_LENGTH-1);
      float bloom = sin(pos*9.42f + vortexPhase*2.5f)*0.5f+0.5f; bloom*=bloom*str;
      leds[HEAD_LENGTH+i] += CHSV(148, 180, (uint8_t)(bloom*165));
    }
  }
  applyRollGlow(148, 220);   // teal bioluminescence bloom when spinning
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
  static float orbitSpeed[3]    = {2.0f, 1.5f, 2.5f};
  static float eoNebPhase       = 0.0f;
  float sm = globalSpeed / 128.0f;
  int ci = STAFF_LENGTH / 2;

  eoNebPhase += dt * 0.35f; if (eoNebPhase > 6.2832f) eoNebPhase -= 6.2832f;

  // Fade electron trails
  fadeToBlackBy(leds + HEAD_LENGTH, STAFF_LENGTH, 40);

  // Murky dark purple background — slow undulating nebula across full strip
  for (int i = 0; i < STAFF_LENGTH; i++) {
    float p = (float)i / STAFF_LENGTH;
    float n = sin(p * 9.0f + eoNebPhase) * 0.35f
            + sin(p * 3.5f - eoNebPhase * 0.6f) * 0.40f + 0.55f;
    if (n < 0.0f) n = 0.0f; if (n > 1.0f) n = 1.0f;
    leds[HEAD_LENGTH + i] += CHSV((uint8_t)(198 + (uint8_t)(n * 22)), 235, (uint8_t)(n * 110 + 40));
  }

  // Center nucleus — 5x bigger (±17 LEDs, was ±3), soft quadratic falloff
  for (int i = -17; i <= 17; i++) {
    int idx = ci + i;
    if (idx < 0 || idx >= STAFF_LENGTH) continue;
    float d = fabs((float)i) / 17.0f;
    float f = 1.0f - d * d;
    leds[HEAD_LENGTH + idx] += CHSV(60, 200, (uint8_t)(f * f * 235));
  }

  // Electrons — 2x orbit distance (40/56/72 was 20/28/36), wider glow ±4 LEDs
  for (int e = 0; e < 3; e++) {
    electronAngle[e] += dt * orbitSpeed[e] * sm;
    if (electronAngle[e] > 6.283f) electronAngle[e] -= 6.283f;

    float orbitRadius = (20.0f + e * 8.0f) * 2.0f;  // 40, 56, 72
    int pos = ci + (int)(sin(electronAngle[e]) * orbitRadius);
    uint8_t hue = 160 + e * 30;

    for (int w = -4; w <= 4; w++) {
      int widx = pos + w;
      if (widx < 0 || widx >= STAFF_LENGTH) continue;
      float fd = fabs((float)w) / 5.0f;
      float f  = 1.0f - fd * fd;
      leds[HEAD_LENGTH + widx] += CHSV(hue, (w == 0) ? 255 : 220, (uint8_t)(f * (w == 0 ? 255 : 160)));
    }
  }

  finishEffect(dt, 0.7f, 0, 60, 255, 220, 60, 255, 220);
}
void renderDNAHelixEffect(float dt) {
  static float helixPhase = 0;
  float sm = globalSpeed / 128.0f;
  float ty = mpu_ready ? clampf(accel[1], -1.0f, 1.0f) : 0.0f;
  static float lastAccelYDNA = 0.0f;
  float jerk = mpu_ready ? clampf((ty - lastAccelYDNA)*5.0f, -3.0f, 3.0f) : 0.0f;
  lastAccelYDNA = ty;
  int ci = STAFF_LENGTH / 2 + (int)(ty * (STAFF_LENGTH/2.5f));
  ci = constrain(ci, 4, STAFF_LENGTH-5);
  float rollSpd = mpu_ready ? fabs(rollRate)*0.015f : 0.0f;
  
  if (fabs(jerk) > 1.5f) {
     htState.headImpactIntensity = max(htState.headImpactIntensity, fabs(jerk)*0.8f);
     htState.tailImpactIntensity = max(htState.tailImpactIntensity, fabs(jerk)*0.8f);
  }
  
  helixPhase += dt * (3.0f * sm + rollSpd*2.0f); if(helixPhase > 6.283f) helixPhase -= 6.283f;
  
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
  float ty = mpu_ready ? clampf(accel[1], -1.0f, 1.0f) : 0.0f;
  static float lastAccelYMP = 0.0f;
  float jerk = mpu_ready ? clampf((ty - lastAccelYMP)*5.0f, -3.0f, 3.0f) : 0.0f;
  lastAccelYMP = ty;
  int ci = STAFF_LENGTH / 2 + (int)(ty * (STAFF_LENGTH/2.5f));
  ci = constrain(ci, 4, STAFF_LENGTH-5);
  float rollSpd = mpu_ready ? fabs(rollRate)*0.015f : 0.0f;
  
  if (fabs(jerk) > 1.5f) {
     htState.headImpactIntensity = max(htState.headImpactIntensity, fabs(jerk)*0.8f);
     htState.tailImpactIntensity = max(htState.tailImpactIntensity, fabs(jerk)*0.8f);
  }
  
  pullPhase += dt * (globalSpeed / 64.0f + rollSpd);
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
  static float lastAccelYSF=0.0f;
  float accelYSF=mpu_ready?clampf(accel[1]*0.8f,-1.2f,1.2f):0.0f;
  float jerkSF=clampf((accelYSF-lastAccelYSF)*5.0f,-3.0f,3.0f); lastAccelYSF=accelYSF;
  flareAge += dt * (globalSpeed/128.0f + (mpu_ready?fabs(accelYSF)*2.0f:0.0f));
  if(flareAge > 6.283f) flareAge -= 6.283f;

  fadeToBlackBy(leds + HEAD_LENGTH, STAFF_LENGTH, 20);

  // Persistent solar corona — many LEDs always lit, bright center fading to edges
  for (int i = 0; i < STAFF_LENGTH; i++) {
    float pos    = (float)i / STAFF_LENGTH;
    float dist   = fabs(pos - 0.5f) * 2.0f;
    float corona = 1.0f - dist * dist;
    float flicker = sin(flareAge * 4.5f + pos * 8.0f) * 0.18f + 0.82f;
    float plasma  = sin(flareAge * 2.2f + pos * 15.0f) * 0.12f + 0.88f;
    uint8_t h    = (uint8_t)(8 + dist * 10);
    uint8_t v    = (uint8_t)(corona * corona * flicker * plasma * 185.0f + 12.0f);
    leds[HEAD_LENGTH + i] += CHSV(h, 255, v);
  }
  // Solar granulation — random bright speckles concentrated near center
  if (random(3) == 0) {
    int si = random(STAFF_LENGTH);
    float sdist = fabs((float)(si - ci)) / (float)(STAFF_LENGTH / 2);
    if (sdist < 0.9f) leds[HEAD_LENGTH + si] += CHSV(random(25), 230, (uint8_t)((1.0f - sdist) * 160 + 60));
  }

  float eruption = audioLevel*20.0f + (mpu_ready?(fabs(accelYSF)*30.0f+fabs(jerkSF)*50.0f):0.0f);
  for(int i=0; i < (int)eruption; i++) {
    float spd = 20 + random(60);
    float p = sin(flareAge + i) * spd * dt;
    int idxL = ci - (int)fabs(p * 10);
    int idxR = ci + (int)fabs(p * 10);
    if(idxL >= 0 && idxL < STAFF_LENGTH) leds[HEAD_LENGTH+idxL] += CHSV(random(20), 255, 200);
    if(idxR >= 0 && idxR < STAFF_LENGTH) leds[HEAD_LENGTH+idxR] += CHSV(random(20), 255, 200);
  }
  finishEffect(dt, 0.5f, 0.0f);
  if (mpu_ready && fabs(rollRate) > 55.0f) {
    // Solar streaks: flare jets radiate from center with frequency proportional to spin
    float str = clampf((fabs(rollRate)-55.0f)/145.0f, 0.0f, 1.0f);
    for (int i = 0; i < STAFF_LENGTH; i++) {
      float pos = (float)i/(float)(STAFF_LENGTH-1);
      float flare = sin(pos*15.71f + vortexPhase*6.0f)*0.5f+0.5f;
      flare *= flare * (1.0f - fabs(pos-0.5f)) * str;  // falloff toward ends
      leds[HEAD_LENGTH+i] += CHSV(15, (uint8_t)(255-flare*110), (uint8_t)(flare*230));
    }
  }
  applyRollGlow(15, 255);    // solar orange-red corona when spinning
}
void renderMolecularVibrationEffect(float dt) {
  float ty = mpu_ready ? clampf(accel[1], -1.0f, 1.0f) : 0.0f;
  static float lastAccelYMV = 0.0f;
  float jerk = mpu_ready ? clampf((ty - lastAccelYMV)*5.0f, -3.0f, 3.0f) : 0.0f;
  lastAccelYMV = ty;
  int ci = STAFF_LENGTH / 2 + (int)(ty * (STAFF_LENGTH/2.5f));
  ci = constrain(ci, 4, STAFF_LENGTH-5);
  
  if (fabs(jerk) > 1.5f) {
     htState.headImpactIntensity = max(htState.headImpactIntensity, fabs(jerk)*0.8f);
     htState.tailImpactIntensity = max(htState.tailImpactIntensity, fabs(jerk)*0.8f);
  }
  
  int jitter = (int)(audioLevel * 15.0f + (mpu_ready?fabs(rollRate)*0.2f:0));

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
  float ty = mpu_ready ? clampf(accel[1], -1.0f, 1.0f) : 0.0f;
  static float lastAccelYBH = 0.0f;
  float jerk = mpu_ready ? clampf((ty - lastAccelYBH)*5.0f, -3.0f, 3.0f) : 0.0f;
  lastAccelYBH = ty;
  int ci = STAFF_LENGTH / 2 + (int)(ty * (STAFF_LENGTH/2.5f));
  ci = constrain(ci, 4, STAFF_LENGTH-5);
  
  if (fabs(jerk) > 1.5f) {
     htState.headImpactIntensity = max(htState.headImpactIntensity, fabs(jerk)*0.8f);
     htState.tailImpactIntensity = max(htState.tailImpactIntensity, fabs(jerk)*0.8f);
  }
  
  horizon += dt*(1.0f + (mpu_ready?fabs(rollRate)*0.05f:0)); if(horizon > 750.0f) horizon -= 750.0f;  // keep fmod(d+horizon*20,15) accurate

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
