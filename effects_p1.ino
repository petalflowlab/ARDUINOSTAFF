
// -----------------------------------------------------------------------
// VORTEX OVERLAY — activated when the staff is spun rapidly on its axis.
//
// vortexPhase accumulates proportional to gyro[1] (Y-axis, staff long axis):
//   Positive roll → vortexPhase grows  → sin wave travels outward (center→edge)
//   Negative roll → vortexPhase shrinks → sin wave travels inward  (edge→center)
//
// The spiral pattern: distFromCenter is subtracted from vortexPhase, so the
// wave equation is  sin(ωt − kx)  which by conv ention travels in the +x
// (outward) direction when ω > 0.  Three spatial periods fit across half the
// staff, so you see three glowing rings sweeping simultaneously.
// -----------------------------------------------------------------------
void renderVortexOverlay() {
  for (int i = 0; i < STAFF_LENGTH; i++) {
    float pos           = (float)i / (float)(STAFF_LENGTH - 1);
    float distFromCenter = fabs(pos - 0.5f) * 2.0f;   // 0 = centre, 1 = edge

    // Spiral phase — subtraction ensures positive vortexPhase = outward sweep
    float spiralPhase = vortexPhase - distFromCenter * 3.0f;

    // Two-frequency wave gives richer, less uniform-looking rings
    float wave1 = sin(spiralPhase * 6.2832f) * 0.5f + 0.5f;          // 1 cycle / unit
    float wave2 = sin(spiralPhase * 10.472f + 1.2f) * 0.2f + 0.8f;   // 1.67 cycles / unit
    float wave  = wave1 * wave2;

    // Slightly soften towards edges so centre looks like the vortex origin
    wave *= (1.0f - distFromCenter * 0.3f);

    float wSq = wave * wave;
    // Colour: deep purple at troughs, electric blue-cyan at peaks, white core
    uint8_t hue = (uint8_t)(235.0f - wave * 50.0f);   // 235 purple → 185 cyan
    uint8_t sat = (uint8_t)(220.0f - wSq  * 140.0f);  // desaturate peaks → white glow
    uint8_t val = (uint8_t)(wSq           * 255.0f);

    CRGB vortexColor   = CHSV(hue, sat, val);
    uint8_t blendFrac  = (uint8_t)(vortexIntensity * 220.0f);
    leds[HEAD_LENGTH + i] = blend(leds[HEAD_LENGTH + i], vortexColor, blendFrac);
  }

  // Pulsing axial core at the centre of the staff (the "eye" of the vortex)
  if (vortexIntensity > 0.3f) {
    int ci        = HEAD_LENGTH + STAFF_LENGTH / 2;
    float pulse   = fabs(sin(vortexPhase * 18.85f)) * vortexIntensity;
    for (int j = -3; j <= 3; j++) {
      int idx = ci + j;
      if (idx < HEAD_LENGTH || idx >= HEAD_LENGTH + STAFF_LENGTH) continue;
      float f = (1.0f - fabs((float)j) / 4.0f) * pulse;
      CRGB coreGlow = CHSV(195, 170, (uint8_t)(f * 255));
      leds[idx] += coreGlow;
    }
  }
}

void renderPurpleBlobEffect(float dt) {
  updateRedParticles(dt,blob.position,blob.velocity,blob.size);
  updateFluidBlob(dt);
  updateTrailSystem(dt);
  updateInkDispersion(dt);
  updateHeadTailReactivity(dt,blob.position,blob.velocity);
  renderRedBackground();
  renderFluidBlob();
  renderInkDispersion();
  // Overlay vortex when the staff is spinning rapidly on its long axis
  if (vortexIntensity > 0.01f) renderVortexOverlay();
  if(!customColorMode){headHue=235;headSat=200;headVal=220;tailHue=240;tailSat=200;tailVal=200;}
  renderHeadAndTail();
  applyRollStrobePastel();
}
void renderRainbowPainterEffect(float dt) {
  uint32_t now=millis();
  if (!rainbowPainter.initialized){
    for(int i=0;i<STAFF_LENGTH;i++) rainbowPainter.mixingBuffer[i]=(float)(i*256)/STAFF_LENGTH;
    rainbowPainter.initialized=true;
  }
  {
    static float lastAccelYRP=0.0f;
    float accelY=mpu_ready?clampf(accel[1]*0.8f,-1.2f,1.2f):0.0f;
    float jerk=clampf((accelY-lastAccelYRP)*5.0f,-3.0f,3.0f); lastAccelYRP=accelY;
    if (mpu_ready){
      rainbowPainter.paintVelocity=rainbowPainter.paintVelocity*0.85f+(accelY*0.8f+jerk*1.2f)*dt*8.0f;
      rainbowPainter.paintCenter+=rainbowPainter.paintVelocity*dt;
      if(rainbowPainter.paintCenter<0.1f){
        float spd=fabs(rainbowPainter.paintVelocity);
        rainbowPainter.paintCenter=0.1f; rainbowPainter.paintVelocity=-rainbowPainter.paintVelocity*0.7f;
        htState.headImpactIntensity=clampf(spd*3,0,1.5f);
        if(spd>0.3f){rainbowPainter.crushDir=-1;rainbowPainter.crushFactor=min(rainbowPainter.crushFactor+spd*0.8f,1.0f);}
      }
      if(rainbowPainter.paintCenter>0.9f){
        float spd=fabs(rainbowPainter.paintVelocity);
        rainbowPainter.paintCenter=0.9f; rainbowPainter.paintVelocity=-rainbowPainter.paintVelocity*0.7f;
        htState.tailImpactIntensity=clampf(spd*3,0,1.5f);
        if(spd>0.3f){rainbowPainter.crushDir=1;rainbowPainter.crushFactor=min(rainbowPainter.crushFactor+spd*0.8f,1.0f);}
      }
    } else {
      static float dp=0; dp+=dt*0.5f;
      rainbowPainter.paintCenter=0.5f+sin(dp)*0.2f;
      rainbowPainter.paintVelocity=cos(dp)*0.1f;
    }
    // Crush spring: underdamped (ζ≈0.64), period ~1.1 s — slight overshoot = decompression visual
    rainbowPainter.crushVel+=(-30.0f*rainbowPainter.crushFactor-7.0f*rainbowPainter.crushVel)*dt;
    rainbowPainter.crushFactor=clampf(rainbowPainter.crushFactor+rainbowPainter.crushVel*dt,-0.15f,1.0f);
    if(rainbowPainter.crushFactor<0.005f&&fabs(rainbowPainter.crushVel)<0.05f){rainbowPainter.crushFactor=0.0f;rainbowPainter.crushVel=0.0f;}
  }
  rainbowPainter.centerHue+=dt*120; if(rainbowPainter.centerHue>=256) rainbowPainter.centerHue-=256;
  float bSize=0.12f+fabs(rainbowPainter.paintVelocity)*0.15f;
  int ci=(int)(rainbowPainter.paintCenter*STAFF_LENGTH);
  int br=(int)(bSize*STAFF_LENGTH); if(br<3) br=3;
  // === ROLL: shift colour bands and grow dominant zone ===
  {
    bool rolling = mpu_ready && fabs(rollRate) > 30.0f;
    if (rolling) {
      // Capture centre hue as dominant on first rolling frame
      if (rainbowPainter.dominantFade <= 0.0f)
        rainbowPainter.dominantHue = rainbowPainter.mixingBuffer[ci];
      rainbowPainter.dominantFade = 2.0f;
      // Grow zone proportional to roll speed, up to half the staff
      rainbowPainter.dominantZoneWidth = clampf(
        rainbowPainter.dominantZoneWidth + dt * fabs(rollRate) * 0.05f,
        0.0f, (float)(STAFF_LENGTH / 2 - 2));
      // Accumulate fractional pixel shift (positive rollRate = shift toward tail)
      rainbowPainter.rollShiftAccum += rollRate * dt * 0.12f;
    } else {
      rainbowPainter.dominantFade = clampf(rainbowPainter.dominantFade - dt, 0.0f, 2.0f);
      rainbowPainter.dominantZoneWidth *= (1.0f - dt * 0.4f);
      rainbowPainter.rollShiftAccum *= (1.0f - dt * 3.0f); // decay residual
    }
    // Apply integer shifts — positive accum shifts bands toward tail
    while (rainbowPainter.rollShiftAccum >= 1.0f) {
      float sv = rainbowPainter.mixingBuffer[STAFF_LENGTH - 1];
      for (int i = STAFF_LENGTH - 1; i > 0; i--)
        rainbowPainter.mixingBuffer[i] = rainbowPainter.mixingBuffer[i - 1];
      rainbowPainter.mixingBuffer[0] = sv;
      rainbowPainter.rollShiftAccum -= 1.0f;
    }
    while (rainbowPainter.rollShiftAccum <= -1.0f) {
      float sv = rainbowPainter.mixingBuffer[0];
      for (int i = 0; i < STAFF_LENGTH - 1; i++)
        rainbowPainter.mixingBuffer[i] = rainbowPainter.mixingBuffer[i + 1];
      rainbowPainter.mixingBuffer[STAFF_LENGTH - 1] = sv;
      rainbowPainter.rollShiftAccum += 1.0f;
    }
  }
  for(int i=0;i<STAFF_LENGTH;i++){
    int d=abs(i-ci);
    if(d<=br){
      float ms=1.0f-(float)d/(br+1); ms=ms*ms*ms;
      float hd=rainbowPainter.centerHue-rainbowPainter.mixingBuffer[i];
      if(hd>128)hd-=256; if(hd<-128)hd+=256;
      float mr=10.0f+fabs(rainbowPainter.paintVelocity)*15;
      rainbowPainter.mixingBuffer[i]+=hd*ms*dt*mr;
      while(rainbowPainter.mixingBuffer[i]>=256)rainbowPainter.mixingBuffer[i]-=256;
      while(rainbowPainter.mixingBuffer[i]<0)rainbowPainter.mixingBuffer[i]+=256;
    }
  }
  static float tb[144]; memcpy(tb,rainbowPainter.mixingBuffer,sizeof(tb));
  float dr=0.03f*dt;
  for(int i=1;i<STAFF_LENGTH-1;i++){
    float ld=tb[i-1]-tb[i],rd=tb[i+1]-tb[i];
    if(ld>128)ld-=256; if(ld<-128)ld+=256;
    if(rd>128)rd-=256; if(rd<-128)rd+=256;
    rainbowPainter.mixingBuffer[i]+=(ld+rd)*dr;
    while(rainbowPainter.mixingBuffer[i]>=256)rainbowPainter.mixingBuffer[i]-=256;
    while(rainbowPainter.mixingBuffer[i]<0)rainbowPainter.mixingBuffer[i]+=256;
  }
  // Dominant colour zone: pull mixing buffer toward captured hue over growing window
  if (rainbowPainter.dominantFade > 0.0f && rainbowPainter.dominantZoneWidth > 0.5f) {
    float str = clampf(rainbowPainter.dominantFade / 2.0f, 0.0f, 1.0f);
    int dw = (int)(rainbowPainter.dominantZoneWidth + 0.5f);
    for (int i = max(0, ci - dw); i <= min(STAFF_LENGTH - 1, ci + dw); i++) {
      float dist = fabs((float)(i - ci)) / (float)(dw + 1);
      float inf = (1.0f - dist * dist) * str * 0.5f;
      float hd = rainbowPainter.dominantHue - rainbowPainter.mixingBuffer[i];
      if (hd > 128.0f) hd -= 256.0f;
      if (hd < -128.0f) hd += 256.0f;
      rainbowPainter.mixingBuffer[i] += hd * inf;
      while (rainbowPainter.mixingBuffer[i] >= 256.0f) rainbowPainter.mixingBuffer[i] -= 256.0f;
      while (rainbowPainter.mixingBuffer[i] < 0.0f) rainbowPainter.mixingBuffer[i] += 256.0f;
    }
  }
  for(int i=0;i<STAFF_LENGTH;i++){
    // Crush warp: compress bands toward impact end on slam, decompress on spring-back
    int bufIdx=i;
    if(fabs(rainbowPainter.crushFactor)>0.01f){
      float t=(float)i/(float)(STAFF_LENGTH-1);
      // exponent < 1 packs bands toward tail (crushDir=+1); > 1 packs toward head (crushDir=-1)
      float ex=1.0f-(float)rainbowPainter.crushDir*rainbowPainter.crushFactor*0.7f;
      if(ex<0.1f) ex=0.1f;
      float warped=(t>0.0f)?powf(t,ex):0.0f;
      bufIdx=(int)(clampf(warped,0.0f,1.0f)*(STAFF_LENGTH-1));
    }
    uint8_t ph=(uint8_t)rainbowPainter.mixingBuffer[bufIdx];
    uint8_t s=255,v=120;
    float noise=sin(i*0.2f+now*0.0008f)*0.3f+0.7f; v=(uint8_t)(v*noise);
    int d=abs(i-ci);
    if(d<=br){
      float hi=1.0f-(float)d/(br+1); hi*=hi;
      v=clampf(v+hi*120,0,255); s=255;
      if(fabs(rainbowPainter.paintVelocity)>0.15f){float sh=sin(now*0.012f+i*0.4f)*0.5f+0.5f;if(sh>0.75f){v=255;s=clampf(s-80,120,255);}}
      if(d<2){v=255;s=clampf(s-50,150,255);}
    }
    // Dominant colour zone: boost brightness so the growing zone is visually distinct
    if (rainbowPainter.dominantFade > 0.0f) {
      int dw = (int)(rainbowPainter.dominantZoneWidth + 0.5f);
      if (d <= dw) {
        float dist = (float)d / (float)(dw + 1);
        float boost = (1.0f - dist * dist) * clampf(rainbowPainter.dominantFade / 2.0f, 0.0f, 1.0f);
        v = clampf(v + boost * 55.0f, 0, 255);
      }
    }
    leds[HEAD_LENGTH+i]=CHSV(ph,s,v);
  }
  if(fabs(rainbowPainter.paintVelocity)>0.2f&&random8()<70){
    int dd=(rainbowPainter.paintVelocity>0)?-1:1;
    int di=ci+dd*(br+random(3));
    if(di>=0&&di<STAFF_LENGTH){
      float dh=rainbowPainter.centerHue+random(-20,20);
      while(dh>=256)dh-=256; while(dh<0)dh+=256;
      CRGB drp=CHSV((uint8_t)dh,255,200);
      leds[HEAD_LENGTH+di]=blend(leds[HEAD_LENGTH+di],drp,180);
      if(di>0&&di<STAFF_LENGTH-1){leds[HEAD_LENGTH+di-1]=blend(leds[HEAD_LENGTH+di-1],drp,120);leds[HEAD_LENGTH+di+1]=blend(leds[HEAD_LENGTH+di+1],drp,120);}
    }
  }
  updateHeadTailReactivity(dt,rainbowPainter.paintCenter,rainbowPainter.paintVelocity);
  if(!customColorMode){
    uint8_t ch=(uint8_t)rainbowPainter.centerHue;
    headHue=ch;headSat=255;headVal=220;
    tailHue=(ch+30)%256;tailSat=255;tailVal=200;
  }
  renderHeadAndTail();
  if (mpu_ready && fabs(rollRate) > 55.0f) {
    // Rainbow scan: hue sweeps across the staff at spin speed, energising the bands
    float str = clampf((fabs(rollRate)-55.0f)/145.0f, 0.0f, 1.0f);
    for (int i = 0; i < STAFF_LENGTH; i++) {
      uint8_t h = (uint8_t)(fmod((float)i * 1.77f + vortexPhase * 100.0f, 256.0f));
      leds[HEAD_LENGTH+i] = blend(leds[HEAD_LENGTH+i], CHSV(h, 255, 255), (uint8_t)(str * 160));
    }
  }
  applyRollStrobeRainbow();
}
void renderFireStormEffect(float dt) {
  #define FS_FG_MAX 25
  struct FSFG { float pos, vel, life; bool active; };
  static byte     heat[144];
  static uint32_t lu = 0;
  static float    fb = 0, fbv = 0;
  static float    rPos[5] = {0}; static unsigned long rT[5] = {0}; static bool rA[5] = {false};
  static FSFG     fg[FS_FG_MAX];
  static bool     fgInit = false;
  static float    lastFbv       = 0.0f;
  static float    bgDriftPhase  = 0.0f;   // slow hue+brightness drift for background embers
  static float    blobStrobePh  = 0.0f;   // 10 Hz strobe on the moving blob only
  static float    fsHtStrobePh  = 0.0f;   // 5 Hz red/orange strobe on head+tail during roll
  if (!fgInit) { memset(fg, 0, sizeof(fg)); fgInit = true; }
  uint32_t now = millis();

  // Blob physics — fast, matching purple blob responsiveness
  if (mpu_ready) {
    static float lastAccelYFS = 0.0f;
    float ty = clampf(accel[1]*0.8f, -1.2f, 1.2f);
    float jk = clampf((ty-lastAccelYFS)*5.0f, -3.0f, 3.0f); lastAccelYFS = ty;
    fbv = fbv*0.88f + (ty*1.8f + jk*1.5f)*dt*10.0f; fb += fbv*dt*2.5f;
    if (fb < 0.1f) { fb=0.1f; fbv=-fbv*0.7f; htState.headImpactIntensity=1.5f; for(int i=0;i<5;i++) if(!rA[i]){rA[i]=true;rPos[i]=0.0f;rT[i]=now;break;} }
    if (fb > 0.9f) { fb=0.9f; fbv=-fbv*0.7f; htState.tailImpactIntensity=1.5f; for(int i=0;i<5;i++) if(!rA[i]){rA[i]=true;rPos[i]=1.0f;rT[i]=now;break;} }
  } else { static float sp=0; sp+=dt*0.5f; fb=0.5f+sin(sp)*0.3f; fbv=cos(sp)*0.2f; }

  // Advance continuous phases every frame
  bgDriftPhase += dt * 0.4f;  if (bgDriftPhase >= 6.2832f) bgDriftPhase -= 6.2832f;
  blobStrobePh += dt * 10.0f; if (blobStrobePh >= 1.0f)    blobStrobePh -= 1.0f;
  if (mpu_ready && fabs(rollRate) > 55.0f)
    { fsHtStrobePh += dt * 5.0f; if (fsHtStrobePh >= 1.0f) fsHtStrobePh -= 1.0f; }

  // Fire glitter: spawn on sharp blob velocity kick (jerk proxy)
  float dvBlob = fbv - lastFbv; lastFbv = fbv;
  if (mpu_ready && fabs(dvBlob) > 0.2f) {
    float gDir = (dvBlob > 0.0f) ? 1.0f : -1.0f;
    int count = min(3 + (int)(fabs(dvBlob) * 25.0f), 15);
    for (int n = 0; n < count; n++) {
      for (int p = 0; p < FS_FG_MAX; p++) {
        if (fg[p].active) continue;
        fg[p].pos    = clampf(fb + (random(20)-10)*0.004f, 0.05f, 0.95f);
        fg[p].vel    = gDir * (0.4f + random(60)/100.0f);
        fg[p].life   = 0.5f + random(50)/100.0f;
        fg[p].active = true; break;
      }
    }
  }
  // Update glitter particles
  for (int p = 0; p < FS_FG_MAX; p++) {
    if (!fg[p].active) continue;
    fg[p].pos  += fg[p].vel * dt;
    fg[p].vel  *= (1.0f - dt * 2.0f);
    fg[p].life -= dt * 1.8f;
    if (fg[p].life <= 0.0f || fg[p].pos < 0.0f || fg[p].pos > 1.0f) fg[p].active = false;
  }

  // Heat simulation + render: rate-limited to 20 Hz
  if (now - lu >= 50) {
    lu = now;
    if (random(100) < 3) for (int i=0;i<5;i++) if(!rA[i]){rA[i]=true;rPos[i]=0.5f;rT[i]=now;break;}
    for (int i=0; i<STAFF_LENGTH; i++) { int cd=random(10,25); heat[i]=(heat[i]>cd)?heat[i]-cd:0; }
    for (int k=STAFF_LENGTH-1; k>=2; k--) heat[k]=(byte)((heat[k-1]*0.6f+heat[k-2]*0.4f));
    int bi=(int)(fb*STAFF_LENGTH); int bri=8+(int)(fabs(fbv)*15);
    for (int i=-bri; i<=bri; i++) { int idx=bi+i; if(idx>=0&&idx<STAFF_LENGTH){float d=fabs(i)/(float)bri;int nh=heat[idx]+(int)((1-d)*200);heat[idx]=(nh>255)?255:(byte)nh;} }
    // Background heat render: slow hue drift through deep reds + gentle brightness wave
    float bgH = sin(bgDriftPhase) * 6.0f + 5.0f;          // hue 0–11, red↔dark-orange-red
    float bgV = sin(bgDriftPhase * 1.7f) * 0.10f + 0.90f; // ±10% brightness swell
    for (int i=0; i<STAFF_LENGTH; i++) {
      byte t = heat[i]; uint8_t hue, sat, val;
      if (t > 200)      { hue=(uint8_t)(bgH+12); sat=160; val=(uint8_t)(255*bgV); }
      else if (t > 150) { hue=(uint8_t)(bgH+7);  sat=215; val=(uint8_t)((160+random(40))*bgV); }
      else if (t > 100) { hue=(uint8_t)(bgH+3);  sat=240; val=(uint8_t)((110+random(40))*bgV); }
      else if (t > 50)  { hue=(uint8_t)(bgH);    sat=255; val=(uint8_t)((55+random(35))*bgV); }
      else              { hue=0;                  sat=255; val=(uint8_t)((20+random(20))*bgV); }
      leds[HEAD_LENGTH+i] = CHSV(hue, sat, val);
    }
    for (int r=0; r<5; r++) {
      if (!rA[r]) continue;
      float rr = (now-rT[r])/800.0f*0.5f;
      if (rr > 0.5f) { rA[r]=false; continue; }
      for (int i=0; i<STAFF_LENGTH; i++) {
        float pos=(float)i/STAFF_LENGTH, dc=fabs(pos-rPos[r]), dr2=fabs(dc-rr);
        if (dr2 < 0.05f) { float ints=(1-dr2/0.05f)*(1-rr*2); leds[HEAD_LENGTH+i]+=CRGB((uint8_t)(ints*255),(uint8_t)(ints*100),0); }
      }
    }
  }

  // Center blob — every frame, additive, grows with roll speed
  {
    int ci = STAFF_LENGTH / 2;
    float rollAbs = mpu_ready ? fabs(rollRate) : 0.0f;
    float blobR = 6.0f + clampf(rollAbs / 5.0f, 0.0f, 45.0f);
    float blobB = 150.0f + clampf((rollAbs - 55.0f) / 1.5f, 0.0f, 105.0f);
    for (int i = 0; i < STAFF_LENGTH; i++) {
      float d = fabs((float)(i - ci));
      if (d < blobR) {
        float f = 1.0f - d / blobR; f *= f;
        leds[HEAD_LENGTH + i] += CHSV((uint8_t)(15 + (1.0f-f)*12), 240, (uint8_t)(blobB * f));
      }
    }
  }

  // Glitter render — every frame, additive
  for (int p = 0; p < FS_FG_MAX; p++) {
    if (!fg[p].active) continue;
    int idx = (int)(fg[p].pos * (STAFF_LENGTH - 1));
    if (idx < 0 || idx >= STAFF_LENGTH) continue;
    float l = fg[p].life;
    uint8_t h = (uint8_t)(5 + (1.0f - l) * 20);
    uint8_t v = (uint8_t)(l * l * 240);
    leds[HEAD_LENGTH + idx] += CHSV(h, 255, v);
    if (idx > 0)               leds[HEAD_LENGTH + idx - 1] += CHSV(h, 230, v/3);
    if (idx < STAFF_LENGTH-1)  leds[HEAD_LENGTH + idx + 1] += CHSV(h, 230, v/3);
  }

  // Blob strobe — 10 Hz sharp flash only at the moving blob position
  {
    float pulse = (blobStrobePh < 0.35f) ? (blobStrobePh / 0.35f) : 0.0f;  // brief on-spike, dark the rest
    pulse = pulse * pulse * pulse;  // snappy burst shape
    if (pulse > 0.01f) {
      int bi2 = (int)(fb * (STAFF_LENGTH - 1));
      int bw  = 7 + (int)(fabs(fbv) * 10);
      for (int i = -bw; i <= bw; i++) {
        int idx = bi2 + i;
        if (idx < 0 || idx >= STAFF_LENGTH) continue;
        float d = fabs((float)i) / (float)bw;
        float f = (1.0f - d*d) * pulse;
        leds[HEAD_LENGTH + idx] += CHSV(10, 180, (uint8_t)(f * 220));
      }
    }
  }

  finishEffect(dt, fb, fbv, 0, 255, 220, 10, 255, 200);

  // Roll rings — slowed down (0.3f)
  if (mpu_ready && fabs(rollRate) > 55.0f) {
    float str = clampf((fabs(rollRate)-55.0f)/145.0f, 0.0f, 1.0f);
    int ci = STAFF_LENGTH/2;
    for (int r = 0; r < 2; r++) {
      float ring = fmod(vortexPhase*0.3f + r*0.5f, 1.0f) * (float)(STAFF_LENGTH/2);
      for (int i = 0; i < STAFF_LENGTH; i++) {
        float d = fabs(fabs((float)i-ci) - ring);
        if (d < 5.0f) { float f=(1.0f-d/5.0f)*str; leds[HEAD_LENGTH+i]+=CHSV(random(25),255,(uint8_t)(f*220)); }
      }
    }
    // Head and tail: ~0.2 s red/orange strobe (5 Hz)
    CRGB htsc = (fsHtStrobePh < 0.5f) ? CRGB(CHSV(0, 255, 255)) : CRGB(CHSV(18, 255, 240));
    uint8_t ba = (uint8_t)(str * 210);
    for (int i = 0; i < HEAD_LENGTH; i++)
      leds[i] = blend(leds[i], htsc, ba);
    for (int hi = 0; hi < HEAD_LENGTH; hi++) {
      int t1 = TAIL_START + TAIL_OFFSET + hi*2;
      int t2 = t1 + 1;
      if (t1 < NUM_LEDS) leds[t1] = blend(leds[t1], htsc, ba);
      if (t2 < NUM_LEDS) leds[t2] = blend(leds[t2], htsc, ba);
    }
  }
  applyRollGlow(5, 255);
}
void renderOceanWavesEffect(float dt) {
  static float wp=0,ob=0.5f,obv=0;
  static float wgP[3]={0.2f,0.5f,0.8f},wgS[3]={0.15f,0.2f,0.18f},wgI[3]={0.8f,1.0f,0.9f};
  static unsigned long wgT[3]={0};
  uint32_t now=millis();
  if(mpu_ready){
    static float lastAccelYOW=0.0f;
    float ty=clampf(accel[1]*0.8f,-1.2f,1.2f);
    float jk=clampf((ty-lastAccelYOW)*5.0f,-3.0f,3.0f); lastAccelYOW=ty;
    obv=obv*0.88f+(ty*1.2f+jk*1.0f)*dt*7; ob+=obv*dt*2;
    if(ob<0.1f){ob=0.1f;obv=-obv*0.8f;htState.headImpactIntensity=1.2f;wgP[0]=0;wgS[0]=0.1f;wgI[0]=1.0f;wgT[0]=now;}
    if(ob>0.9f){ob=0.9f;obv=-obv*0.8f;htState.tailImpactIntensity=1.2f;wgP[2]=1;wgS[2]=0.1f;wgI[2]=1.0f;wgT[2]=now;}
  } else { static float sp=0;sp+=dt*0.4f;ob=0.5f+sin(sp)*0.25f;obv=cos(sp)*0.15f; }
  wp+=dt*(2+fabs(obv)*3); if(wp>6.283f)wp-=6.283f;
  for(int w=0;w<3;w++){
    if(wgI[w]>0.1f){
      wgS[w]+=dt*0.3f;
      if(wgP[w]<0.5f)wgP[w]-=dt*0.4f; else wgP[w]+=dt*0.4f;
      wgI[w]*=0.97f;
      if(wgP[w]<-0.2f||wgP[w]>1.2f)wgI[w]=0;
    }
  }
  if(random(100)<2)for(int w=0;w<3;w++)if(wgI[w]<0.2f){wgP[w]=0.4f+random(20)/100.0f;wgS[w]=0.08f+random(10)/100.0f;wgI[w]=0.6f+random(40)/100.0f;wgT[w]=now;break;}
  for(int i=0;i<STAFF_LENGTH;i++){
    float pos=(float)i/STAFF_LENGTH;
    uint8_t bh=160+(uint8_t)(pos*30),bs=255,bv=60+(uint8_t)(pos*80);
    float wave=sin(pos*20+wp)*0.3f+0.7f; bv=(uint8_t)(bv*wave);
    for(int w=0;w<3;w++){
      float dw=fabs(pos-wgP[w]);
      if(dw<wgS[w]&&wgI[w]>0.1f){
        float wi=(wgS[w]-dw)/wgS[w]; wi*=wi; wi*=wgI[w];
        bv=clampf(bv+wi*180,0,255); bs=clampf(bs-wi*120,100,255); bh=170+(uint8_t)(wi*20);
        if(wi>0.7f){bs=100;bv=255;}
      }
    }
    float db=fabs(pos-ob);
    if(db<0.15f){float bi=(0.15f-db)/0.15f;bi*=bi;bv=clampf(bv+bi*150,0,255);bs=clampf(bs-bi*100,150,255);if(fabs(obv)>0.3f&&bi>0.7f){bv=255;bs=150;}}
    leds[HEAD_LENGTH+i]=CHSV(bh,bs,bv);
  }
  if(fabs(obv)>0.2f&&random8()<50) leds[HEAD_LENGTH+random(STAFF_LENGTH)]=CHSV(180,200,220);
  finishEffect(dt,ob,obv, 160,220,200, 170,220,180);
  if (mpu_ready && fabs(rollRate) > 55.0f) {
    // Wave sweep: whitecap surge rolls from one end to the other at spin speed
    float str = clampf((fabs(rollRate)-55.0f)/145.0f, 0.0f, 1.0f);
    float sweep = fmod(fabs(vortexPhase)*1.8f, (float)STAFF_LENGTH);
    if (rollRate < 0) sweep = STAFF_LENGTH-1-sweep;
    for (int i = 0; i < STAFF_LENGTH; i++) {
      float d = fabs((float)i - sweep);
      if (d < 14.0f) { float f=(1.0f-d/14.0f)*str; f*=f; leds[HEAD_LENGTH+i]+=CHSV(170,(uint8_t)(120-f*110),(uint8_t)(f*230)); }
    }
  }
  applyRollGlow(165, 170);   // seafoam white surge when spinning
}
void renderForestFairiesEffect(float dt) {
  #define FF_N 4
  #define FF_TR 100
  struct FFairy { float pos, vel, hue, size, phase, wander; };
  struct FFTrail { float pos, vel, life; bool active; };
  static FFairy  ff[FF_N];
  static FFTrail ft[FF_TR];
  static bool    ffInit       = false;
  static float   bgPhase      = 0.0f;
  static float   lastAccelYFF = 0.0f;
  static float   ffStrobePh   = 0.0f;  // 10 Hz strobe — alternates which particles are visible

  if (!ffInit) {
    const float hues[FF_N]  = {88.0f, 96.0f, 78.0f, 105.0f};
    const float sizes[FF_N] = {0.04f, 0.045f, 0.035f, 0.04f};
    for (int f = 0; f < FF_N; f++) {
      ff[f].pos    = 0.15f + f * (0.70f / (FF_N - 1));  // spread evenly 0.15-0.85
      ff[f].vel    = 0.0f;
      ff[f].hue    = hues[f];  ff[f].size  = sizes[f];
      ff[f].phase  = f * 1.57f; ff[f].wander = f * 1.1f;
    }
    memset(ft, 0, sizeof(ft));
    ffInit = true;
  }

  bgPhase    += dt * 0.2f;  if (bgPhase    > 6.2832f) bgPhase    -= 6.2832f;
  ffStrobePh += dt * 10.0f; if (ffStrobePh >= 1.0f)   ffStrobePh -= 1.0f;

  // IMU — purple-blob style for fast response
  float accelY = mpu_ready ? clampf(accel[1]*0.8f, -1.2f, 1.2f) : 0.0f;
  float jerkFF = mpu_ready ? clampf((accelY - lastAccelYFF)*5.0f, -3.0f, 3.0f) : 0.0f;
  lastAccelYFF = accelY;

  // Jerk burst — spawn a volley of sparkles shooting to both ends from every fairy
  if (mpu_ready && fabs(jerkFF) > 1.2f) {
    int spawnN = 1 + (int)(fabs(jerkFF) * 1.5f);
    for (int f = 0; f < FF_N; f++) {
      for (int dir = -1; dir <= 1; dir += 2) {
        for (int n = 0; n < spawnN; n++) {
          for (int t = 0; t < FF_TR; t++) {
            if (ft[t].active) continue;
            ft[t].pos    = ff[f].pos + (random(14)-7) * 0.003f;
            ft[t].vel    = dir * (0.5f + random(55)/100.0f);
            ft[t].life   = 1.1f + random(40)/100.0f;   // longer-lived burst particles
            ft[t].active = true;
            break;
          }
        }
      }
    }
    htState.headImpactIntensity = max(htState.headImpactIntensity, fabs(jerkFF)*0.7f);
    htState.tailImpactIntensity = max(htState.tailImpactIntensity, fabs(jerkFF)*0.7f);
  }

  // Update fairies — IMU dominant, wander is a tiny nudge for spread only
  for (int f = 0; f < FF_N; f++) {
    ff[f].wander += dt * (0.35f + f * 0.18f); if (ff[f].wander > 6.2832f) ff[f].wander -= 6.2832f;
    ff[f].phase  += dt * (3.0f  + f * 0.5f);  if (ff[f].phase  > 6.2832f) ff[f].phase  -= 6.2832f;

    float imuForce    = mpu_ready ? (accelY * 1.8f + jerkFF * 1.5f) * dt * 9.0f : 0.0f;
    float wanderNudge = sin(ff[f].wander) * (mpu_ready ? 0.03f : 0.08f);  // subtle spread
    ff[f].vel  = ff[f].vel * 0.88f + imuForce + wanderNudge;
    ff[f].pos += ff[f].vel * dt * 2.5f;

    if (ff[f].pos < 0.05f) { ff[f].pos = 0.05f; ff[f].vel =  fabs(ff[f].vel) * 0.6f; }
    if (ff[f].pos > 0.95f) { ff[f].pos = 0.95f; ff[f].vel = -fabs(ff[f].vel) * 0.6f; }

    // Continuous outward trail stream in both directions
    for (int dir = -1; dir <= 1; dir += 2) {
      if (random(100) < 5) {
        for (int t = 0; t < FF_TR; t++) {
          if (ft[t].active) continue;
          ft[t].pos    = ff[f].pos + (random(8)-4) * 0.003f;
          ft[t].vel    = dir * (0.42f + random(40)/100.0f);  // 0.42-0.82 units/sec outward
          ft[t].life   = 0.75f + random(40)/100.0f;
          ft[t].active = true;
          break;
        }
      }
    }
  }

  // Update trail particles — simple decay, no artificial edge kill
  for (int t = 0; t < FF_TR; t++) {
    if (!ft[t].active) continue;
    ft[t].pos  += ft[t].vel * dt;
    ft[t].life -= dt * 0.85f;   // slow decay — lets them reach the ends
    if (ft[t].life <= 0.0f || ft[t].pos <= 0.0f || ft[t].pos >= 1.0f) ft[t].active = false;
  }

  // Background: dark forest floor
  for (int i = 0; i < STAFF_LENGTH; i++) {
    float p = (float)i / STAFF_LENGTH;
    float b = sin(p * 6.0f + bgPhase) * 0.28f + sin(p * 2.2f - bgPhase * 0.55f) * 0.30f + 0.45f;
    if (b < 0.0f) b = 0.0f; if (b > 1.0f) b = 1.0f;
    leds[HEAD_LENGTH + i] = CHSV((uint8_t)(88 + b * 10), 245, (uint8_t)(b * 30 + 5));
  }

  // Trail particles — alternating strobe: even-indexed on first half-cycle, odd on second
  bool strobeA = (ffStrobePh < 0.5f);
  for (int t = 0; t < FF_TR; t++) {
    if (!ft[t].active) continue;
    // Every other particle is dark on this half-cycle — creates the sparkling alternation
    if ((t % 2 == 0) != strobeA) continue;
    int idx = (int)(ft[t].pos * (STAFF_LENGTH - 1));
    if (idx < 0 || idx >= STAFF_LENGTH) continue;
    float l = ft[t].life;
    // Quadratic fizzle starts 30% from each end — clear dissipation visible at the tips
    float edgeDist = ft[t].pos < 0.5f ? ft[t].pos : (1.0f - ft[t].pos);
    float ef = edgeDist / 0.30f; if (ef > 1.0f) ef = 1.0f; ef = ef * ef;
    float bri = l * ef;
    if (bri > 1.0f) bri = 1.0f;
    uint8_t v = (uint8_t)(bri * 175);
    CRGB sc = CRGB(v, (uint8_t)(v*18/20), (uint8_t)(v*10/16));
    leds[HEAD_LENGTH + idx] = blend(leds[HEAD_LENGTH + idx], sc, v);
  }

  // Fairy blobs — small, distinct green dots
  for (int f = 0; f < FF_N; f++) {
    int     fi      = (int)(ff[f].pos * (STAFF_LENGTH - 1));
    int     srad    = max(1, (int)(ff[f].size * STAFF_LENGTH));
    float   flicker = sin(ff[f].phase) * 0.15f + 0.85f;
    uint8_t hue     = (uint8_t)ff[f].hue;
    for (int w = -srad; w <= srad; w++) {
      int idx = fi + w;
      if (idx < 0 || idx >= STAFF_LENGTH) continue;
      float d   = fabs((float)w) / (float)(srad + 1);
      float fac = (1.0f - d*d) * flicker;
      leds[HEAD_LENGTH + idx] += CHSV(hue, (uint8_t)(255 - fac*50), (uint8_t)(fac * 165));
    }
  }

  finishEffect(dt, 0.5f, 0.0f, 96, 255, 200, 96, 255, 200);

  // Roll: sweeping green shimmer rings
  if (mpu_ready && fabs(rollRate) > 55.0f) {
    float str = clampf((fabs(rollRate)-55.0f)/145.0f, 0.0f, 1.0f);
    int ci = STAFF_LENGTH/2;
    for (int r = 0; r < 2; r++) {
      float ring = fmod(vortexPhase*0.4f + r*0.5f, 1.0f) * (float)(STAFF_LENGTH/2);
      for (int i = 0; i < STAFF_LENGTH; i++) {
        float d = fabs(fabs((float)i-ci) - ring);
        if (d < 5.0f) { float fac=(1.0f-d/5.0f)*str; leds[HEAD_LENGTH+i]+=CHSV(96+random(20),200,(uint8_t)(fac*210)); }
      }
    }
  }
  applyRollGlow(96, 200);
}
void renderPingPongEffect(float dt) {
  #define MAX_BALLS 12
  struct Ball { float pos,vel; uint8_t hue; float trail[10]; bool active; int bounces; };
  static Ball balls[MAX_BALLS];
  static bool init=false;
  static float spawnTimer=0;
  uint32_t now=millis();
  float sm=globalSpeed/128.0f;
  if(!init){for(int b=0;b<MAX_BALLS;b++){balls[b].active=false;balls[b].bounces=0;for(int t=0;t<10;t++)balls[b].trail[t]=-1;}init=true;}
  float tf=mpu_ready?clampf(accel[1]*1.5f,-3,3):0;
  static float lastAccelYPP=0.0f;
  float ty = mpu_ready ? accel[1] : 0.0f;
  float jerkPP = mpu_ready ? clampf((ty-lastAccelYPP)*5.0f,-3.0f,3.0f) : 0.0f;
  lastAccelYPP = ty;
  if (fabs(jerkPP) > 1.5f) {
    spawnTimer += dt * 10.0f * fabs(jerkPP);
    htState.headImpactIntensity = max(htState.headImpactIntensity, fabs(jerkPP)*0.6f);
    htState.tailImpactIntensity = max(htState.tailImpactIntensity, fabs(jerkPP)*0.6f);
  } else {
    spawnTimer+=dt;
  }
  int ac=0; for(int b=0;b<MAX_BALLS;b++) if(balls[b].active)ac++;
  float si=(ac==0)?0.05f:0.4f;
  if(spawnTimer>=si&&ac<MAX_BALLS){
    spawnTimer=0;
    for(int b=0;b<MAX_BALLS;b++)if(!balls[b].active){
      int st=random(3);
      if(st==0){balls[b].pos=0.02f;balls[b].vel=0.5f+random(60)/100.0f;}
      else if(st==1){balls[b].pos=0.98f;balls[b].vel=-(0.5f+random(60)/100.0f);}
      else{balls[b].pos=0.1f+random(80)/100.0f;balls[b].vel=(random(2)?1:-1)*(0.4f+random(70)/100.0f);}
      balls[b].vel*=sm; balls[b].hue=random(256); balls[b].bounces=0; balls[b].active=true;
      for(int t=0;t<10;t++)balls[b].trail[t]=balls[b].pos; break;
    }
  }
  for(int b=0;b<MAX_BALLS;b++){
    if(!balls[b].active)continue;
    for(int t=9;t>0;t--)balls[b].trail[t]=balls[b].trail[t-1]; balls[b].trail[0]=balls[b].pos;
    balls[b].vel+=tf*dt*2;
    if(balls[b].bounces>=4&&fabs(tf)<0.3f){balls[b].vel+=(0.5f-balls[b].pos)*1.2f*dt;balls[b].vel*=0.97f;}
    balls[b].pos+=balls[b].vel*dt;
    if(balls[b].pos<=0){balls[b].pos=0;balls[b].vel=fabs(balls[b].vel)*0.9f;balls[b].hue=random(256);balls[b].bounces++;htState.headImpactIntensity=1.2f;for(int i=0;i<HEAD_LENGTH;i++)leds[i]=CHSV(balls[b].hue,200,255);if(balls[b].vel<0.05f)balls[b].active=false;}
    if(balls[b].pos>=1){balls[b].pos=1;balls[b].vel=-fabs(balls[b].vel)*0.9f;balls[b].hue=random(256);balls[b].bounces++;htState.tailImpactIntensity=1.2f;if(balls[b].vel>-0.05f)balls[b].active=false;}
    if(fabs(balls[b].vel)<0.02f&&balls[b].bounces>3)balls[b].active=false;
  }
  for(int i=HEAD_LENGTH;i<HEAD_LENGTH+STAFF_LENGTH;i++)leds[i]=CRGB::Black;
  int cx=HEAD_LENGTH+STAFF_LENGTH/2;
  for(int i=-2;i<=2;i++){float f=1-fabs((float)i)/3.0f;leds[cx+i]=CHSV(180,200,(uint8_t)(12*f));}
  for(int b=0;b<MAX_BALLS;b++){
    if(!balls[b].active)continue;
    for(int t=9;t>=1;t--){
      if(balls[b].trail[t]<0)continue;
      int ti=HEAD_LENGTH+(int)(balls[b].trail[t]*(STAFF_LENGTH-1));
      if(ti<HEAD_LENGTH||ti>=HEAD_LENGTH+STAFF_LENGTH)continue;
      float f=1-(float)t/10.0f; f=f*f*f;
      leds[ti]+=CHSV(balls[b].hue,(uint8_t)(200+f*55),(uint8_t)(f*160));
    }
    int bi=HEAD_LENGTH+(int)(balls[b].pos*(STAFF_LENGTH-1));
    if(bi>=HEAD_LENGTH&&bi<HEAD_LENGTH+STAFF_LENGTH){
      leds[bi]=CRGB::White;
      if(bi-1>=HEAD_LENGTH)leds[bi-1]+=CHSV(balls[b].hue,220,140);
      if(bi+1<HEAD_LENGTH+STAFF_LENGTH)leds[bi+1]+=CHSV(balls[b].hue,220,140);
      if(bi-2>=HEAD_LENGTH)leds[bi-2]+=CHSV(balls[b].hue,240,50);
      if(bi+2<HEAD_LENGTH+STAFF_LENGTH)leds[bi+2]+=CHSV(balls[b].hue,240,50);
    }
  }
  if(!customColorMode){float hp=sin(now*0.002f)*0.3f+0.7f,tp=sin(now*0.002f+1.5f)*0.3f+0.7f;headHue=180;headSat=200;headVal=(uint8_t)(150+hp*80);tailHue=200;tailSat=200;tailVal=(uint8_t)(130+tp*80);}
  updateHeadTailReactivity(dt,0.5f,tf*0.1f);
  renderHeadAndTail();
  if (mpu_ready && fabs(rollRate) > 55.0f) {
    float str = clampf((fabs(rollRate)-55.0f)/145.0f, 0.0f, 1.0f);
    for (int b=0; b<MAX_BALLS; b++) {
      if (!balls[b].active) continue;
      int bi = HEAD_LENGTH + (int)(balls[b].pos*(STAFF_LENGTH-1));
      for (int i=-5; i<=5; i++) {
        int idx = bi+i;
        if (idx>=HEAD_LENGTH && idx<HEAD_LENGTH+STAFF_LENGTH) {
          float d = fabs((float)i)/5.0f;
          leds[idx] += CHSV(balls[b].hue, 200, (uint8_t)((1.0f-d)*str*150.0f));
        }
      }
    }
  }
}
