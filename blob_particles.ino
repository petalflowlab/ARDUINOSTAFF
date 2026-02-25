
void initRedParticles() {
  for (int i=0;i<MAX_RED_PARTICLES;i++){
    redParticles[i].originalPos=random(100)/100.0f;
    redParticles[i].position=redParticles[i].originalPos;
    redParticles[i].velocity=0.0f;
    redParticles[i].charge=0.5f+random(50)/100.0f;
    redParticles[i].displacement=0.0f;
    redParticles[i].sparklePhase=random(628)/100.0f;
    redParticles[i].isPushed=false;
    redParticles[i].isPulled=false;
    redParticles[i].recoilVelocity=0.0f;
  }
}
void initTrailSystem() {
  for (int i=0;i<MAX_TRAIL_SEGMENTS;i++){
    trailSegments[i].active=false;
    trailSegments[i].intensity=0.0f;
    trailSegments[i].age=0.0f;
    trailSegments[i].isPullTrail=false;
    trailSegments[i].redInfluence=0.0f;
  }
}
void initFluidBlob() {
  blob.position=0.5f; blob.velocity=0.0f; blob.lastPosition=0.5f;
  blob.mass=0.3f; blob.size=0.25f;
  blob.pulsePhase=0.0f; blob.breathePhase=0.0f; blob.heat=0.5f;
  for (int i=0;i<5;i++) blob.waves[i].active=false;
  initRedParticles();
  initTrailSystem();
}
void addTrailSegment(float pos, float vel, float redInfluence=0.0f, bool isPull=false) {
  for (int i=0;i<MAX_TRAIL_SEGMENTS;i++){
    if (!trailSegments[i].active){
      trailSegments[i].position=pos;
      trailSegments[i].intensity=1.0f;
      trailSegments[i].hue=220+random(30)-15;
      trailSegments[i].size=0.02f+fabs(vel)*0.03f;
      trailSegments[i].age=0.0f;
      trailSegments[i].active=true;
      trailSegments[i].isPullTrail=isPull;
      trailSegments[i].redInfluence=redInfluence;
      break;
    }
  }
}
void updateTrailSystem(float dt) {
  for (int i=0;i<MAX_TRAIL_SEGMENTS;i++){
    if (!trailSegments[i].active) continue;
    trailSegments[i].age+=dt;
    float maxAge=trailSegments[i].isPullTrail?1.2f:0.8f;
    trailSegments[i].intensity-=(1.0f/maxAge)*dt;
    if (trailSegments[i].intensity<=0.0f) trailSegments[i].active=false;
  }
}
void updateRedParticles(float dt, float blobPos, float blobVel, float blobSize) {
  for (int i=0;i<MAX_RED_PARTICLES;i++){
    RedParticle &p=redParticles[i];
    float distToBlob=fabs(p.position-blobPos);
    float pushRadius=blobSize*1.3f;
    if (distToBlob<pushRadius){
      float ps=(1.0f-distToBlob/pushRadius); ps*=ps;
      float pd=(p.position>blobPos)?1.0f:-1.0f;
      if (fabs(blobVel)>0.05f) pd=(blobVel>0)?1.0f:-1.0f;
      p.velocity+=ps*(fabs(blobVel)*3.0f+0.5f)*pd*dt*20.0f;
      p.charge=clampf(p.charge+ps*0.3f,0.0f,2.0f);
      p.isPushed=true; p.displacement=fabs(p.position-p.originalPos);
      if (ps>0.6f && random(100)<30) p.sparklePhase=0.0f;
    } else p.isPushed=false;
    float pullRadius=blobSize*2.0f;
    float blobTail=blobPos-(blobVel>0?pullRadius*0.3f:-pullRadius*0.3f);
    float distTail=fabs(p.position-blobTail);
    if (distTail<pullRadius && distTail>pushRadius*0.5f){
      float ps=(1.0f-distTail/pullRadius); ps*=ps;
      float pd=(blobTail>p.position)?1.0f:-1.0f;
      p.velocity+=ps*fabs(blobVel)*2.5f*pd*dt*15.0f;
      p.isPulled=true; p.charge=clampf(p.charge+ps*0.2f,0.0f,2.0f);
      if (random(100)<20*ps) addTrailSegment(p.position,blobVel,ps,true);
    } else if (p.isPulled){
      p.recoilVelocity=-p.velocity*0.8f; p.isPulled=false;
      addTrailSegment(p.position,p.recoilVelocity,0.5f,true);
    }
    if (fabs(p.recoilVelocity)>0.01f){ p.velocity+=p.recoilVelocity; p.recoilVelocity*=0.85f; }
    p.position+=p.velocity*dt;
    p.velocity+=(p.originalPos-p.position)*2.0f*dt;
    p.velocity*=0.88f;
    p.displacement=fabs(p.position-p.originalPos);
    p.charge*=0.97f; p.charge=clampf(p.charge,0.3f,2.0f);
    p.sparklePhase+=dt*6.0f; if (p.sparklePhase>6.283f) p.sparklePhase-=6.283f;
    if (p.position<0.0f){p.position=0.0f;p.velocity=-p.velocity*0.5f;}
    if (p.position>1.0f){p.position=1.0f;p.velocity=-p.velocity*0.5f;}
  }
}
void updateFluidBlob(float dt) {
  static float lastAccelY=0, simulatedMotion=0.0f;
  blob.lastPosition=blob.position;
  if (!mpu_ready){
    simulatedMotion+=dt*0.3f;
    blob.position=0.5f+sin(simulatedMotion*0.8f)*0.1f;
    blob.velocity=cos(simulatedMotion*0.8f)*0.08f;
  } else {
    float accelY=clampf(accel[1]*0.5f,-1.0f,1.0f);
    float change=(accelY-lastAccelY)*0.4f; lastAccelY=accelY;
    blob.velocity=blob.velocity*0.88f+(accelY*1.2f+change*0.3f)*dt*10.0f;
    blob.position+=blob.velocity*dt*2.5f;
  }
  if (blob.position<0.1f){blob.position=0.1f+(0.1f-blob.position)*0.3f;blob.velocity=-blob.velocity*0.7f;}
  if (blob.position>0.9f){blob.position=0.9f-(blob.position-0.9f)*0.3f;blob.velocity=-blob.velocity*0.7f;}

  // ── INK DISPERSION TRIGGERS ──────────────────────────────────────────
  // 1) Sudden stop: blob was moving fast, now slow  →  residual momentum
  //    splashes forward as ink fizzle
  {
    float velDrop = fabs(lastBlobVelocity) - fabs(blob.velocity);
    float stopStrength = clampf(velDrop * 5.0f, 0.0f, 1.0f);
    if (stopStrength > 0.25f && fabs(lastBlobVelocity) > 0.08f) {
      spawnInkDispersion(blob.position, lastBlobVelocity, stopStrength);
    }
  }
  // 2) Strong lateral shake (accel[0] = X axis, perpendicular to staff long axis)
  {
    static float lastAccelX = 0.0f;
    float ax = mpu_ready ? accel[0] : 0.0f;
    float axDelta = fabs(ax - lastAccelX);
    lastAccelX = ax;
    float shakeStr = clampf((axDelta - 0.4f) * 2.5f, 0.0f, 1.0f);
    if (shakeStr > 0.1f) {
      spawnInkDispersion(blob.position, blob.velocity, shakeStr);
    }
  }
  lastBlobVelocity = blob.velocity;
  // ─────────────────────────────────────────────────────────────────────

  blob.pulsePhase+=dt*0.5f; if (blob.pulsePhase>6.283f) blob.pulsePhase-=6.283f;
  blob.breathePhase+=dt*0.8f; if (blob.breathePhase>6.283f) blob.breathePhase-=6.283f;
  if (fabs(blob.velocity)>0.05f && random(100)<60) addTrailSegment(blob.lastPosition,blob.velocity);
  for (int i=0;i<5;i++){
    if (!blob.waves[i].active) continue;
    blob.waves[i].position+=blob.waves[i].speed*dt;
    blob.waves[i].amplitude*=0.92f; blob.waves[i].age+=dt;
    if (blob.waves[i].age>3.0f||blob.waves[i].amplitude<0.01f) blob.waves[i].active=false;
  }
  if (fabs(blob.velocity)>0.2f && random(100)<10){
    for (int i=0;i<5;i++){
      if (!blob.waves[i].active){
        blob.waves[i].position=blob.position;
        blob.waves[i].amplitude=fabs(blob.velocity)*0.2f;
        blob.waves[i].wavelength=0.06f+random(40)/1000.0f;
        blob.waves[i].speed=(blob.velocity>0)?0.9f:-0.9f;
        blob.waves[i].age=0.0f; blob.waves[i].active=true; break;
      }
    }
  }
}
void updateHeadTailReactivity(float dt, float blobPos, float blobVel) {
  float headProx=blobPos, tailProx=1.0f-blobPos;
  const float IZ=0.15f, PZ=0.25f;
  bool wasHead=htState.blobAtHead, wasTail=htState.blobAtTail;
  htState.blobAtHead=(headProx<PZ);
  if (headProx<IZ){ if(fabs(blobVel)>0.08f&&!wasHead) htState.headImpactIntensity=clampf((IZ-headProx)*fabs(blobVel)*12.0f,0,1.5f);
    htState.headSustainedGlow=clampf(((IZ-headProx)/IZ)*((IZ-headProx)/IZ)*1.2f,0,1.0f);
  } else if (headProx<PZ){ htState.headSustainedGlow=(PZ-headProx)/PZ*0.5f;
  } else { htState.headSustainedGlow*=0.92f; if(htState.headSustainedGlow<0.01f) htState.headSustainedGlow=0; }
  htState.blobAtTail=(tailProx<PZ);
  if (tailProx<IZ){ if(fabs(blobVel)>0.08f&&!wasTail) htState.tailImpactIntensity=clampf((IZ-tailProx)*fabs(blobVel)*12.0f,0,1.5f);
    htState.tailSustainedGlow=clampf(((IZ-tailProx)/IZ)*((IZ-tailProx)/IZ)*1.2f,0,1.0f);
  } else if (tailProx<PZ){ htState.tailSustainedGlow=(PZ-tailProx)/PZ*0.5f;
  } else { htState.tailSustainedGlow*=0.92f; if(htState.tailSustainedGlow<0.01f) htState.tailSustainedGlow=0; }
  float decay=dt*4.0f;
  htState.headImpactIntensity=max(0.0f,htState.headImpactIntensity-decay);
  htState.tailImpactIntensity=max(0.0f,htState.tailImpactIntensity-decay);
  htState.headBreathePhase+=dt*1.2f; if(htState.headBreathePhase>6.283f) htState.headBreathePhase-=6.283f;
  htState.tailBreathePhase+=dt*1.0f; if(htState.tailBreathePhase>6.283f) htState.tailBreathePhase-=6.283f;
  htState.headPulsePhase+=dt*2.5f;   if(htState.headPulsePhase>6.283f)   htState.headPulsePhase-=6.283f;
  htState.tailPulsePhase+=dt*2.3f;   if(htState.tailPulsePhase>6.283f)   htState.tailPulsePhase-=6.283f;
}
void renderRedBackground() {
  uint32_t now=millis();
  for (int i=HEAD_LENGTH;i<HEAD_LENGTH+STAFF_LENGTH;i++){
    float pos=(float)(i-HEAD_LENGTH)/(float)STAFF_LENGTH;
    uint8_t h=235+(uint8_t)(sin(now*0.0003f+pos*4.0f)*8);
    uint8_t v=80+(uint8_t)(sin(now*0.0005f+pos*2.0f)*30);
    leds[i]=CHSV(h,200,v);
  }
  for (int i=0;i<MAX_RED_PARTICLES;i++){
    RedParticle &p=redParticles[i];
    if (p.position<0||p.position>1) continue;
    int idx=HEAD_LENGTH+(int)(p.position*(STAFF_LENGTH-1));
    if (idx<HEAD_LENGTH||idx>=HEAD_LENGTH+STAFF_LENGTH) continue;
    uint8_t h=235+(uint8_t)(sin(p.sparklePhase)*12);
    uint8_t s=200,v=100+(uint8_t)(p.charge*50);
    if (p.isPushed){ h=220+(uint8_t)(p.displacement*80); v=clampf(v+p.displacement*120,0,255); s=255;
      if (sin(p.sparklePhase*3.0f)>0.7f){v=255;h=245;} }
    if (p.isPulled){ h=230; v=clampf(v+80,0,200); s=220; }
    int spread=p.displacement>0.05f?2:1;
    for (int ss=-spread;ss<=spread;ss++){
      int si=idx+ss;
      if (si>=HEAD_LENGTH&&si<HEAD_LENGTH+STAFF_LENGTH){
        float f=1.0f-(abs(ss)/(float)(spread+1));
        CRGB c=CHSV(h,s,v); c.nscale8_video((uint8_t)(f*255));
        leds[si]=blend(leds[si],c,180);
      }
    }
  }
}
void renderTrailSystem() {
  for (int i=0;i<MAX_TRAIL_SEGMENTS;i++){
    if (!trailSegments[i].active) continue;
    TrailSegment &seg=trailSegments[i];
    if (seg.position<0||seg.position>1) continue;
    int ci=HEAD_LENGTH+(int)(seg.position*(STAFF_LENGTH-1));
    int r=(int)(seg.size*STAFF_LENGTH*(1.0f+seg.intensity*0.5f));
    if (r<1) r=1;
    for (int rr=-r;rr<=r;rr++){
      int idx=ci+rr;
      if (idx<HEAD_LENGTH||idx>=HEAD_LENGTH+STAFF_LENGTH) continue;
      float d=fabs(rr)/(float)(r+1);
      float ints=(1.0f-d)*seg.intensity;
      uint8_t h=seg.hue,s=200;
      if (seg.isPullTrail){h=235;s=220;}
      CRGB tc=CHSV(h,s,(uint8_t)(ints*160));
      leds[idx]+=tc;  // FastLED += is saturating; no overflow check needed
    }
  }
}
void renderFluidBlob() {
  uint32_t now=millis();
  for (int w=0;w<5;w++){
    if (!blob.waves[w].active) continue;
    float wPos=blob.waves[w].position,amp=blob.waves[w].amplitude;
    for (float off=-0.25f;off<=0.25f;off+=0.004f){
      float pos=wPos+off;
      if (pos<0||pos>1) continue;
      float wv=amp*sin(off*18.0f)*expDecay(fabs(off)*8.0f);
      int idx=HEAD_LENGTH+(int)(pos*(STAFF_LENGTH-1));
      if (idx>=HEAD_LENGTH&&idx<HEAD_LENGTH+STAFF_LENGTH){
        uint8_t wh=215+(uint8_t)(wv*20);
        uint8_t wval=(uint8_t)(fabs(wv)*180);
        CRGB wc=CHSV(wh,220,wval);
        if (wval>120) wc+=CRGB(40,30,50);
        leds[idx]=blend(leds[idx],wc,200);
      }
    }
  }
  float bSize=blob.size+0.03f*sin(blob.breathePhase)+0.04f*sin(blob.pulsePhase*2.0f);
  bool nearL=blob.position<0.15f,nearR=blob.position>0.85f;
  float edgeSpike=(nearL)?(0.15f-blob.position)*6.0f:(nearR?(blob.position-0.85f)*6.0f:0.0f);
  if (edgeSpike>0) bSize*=(1.0f+edgeSpike*0.3f);
  int ci=HEAD_LENGTH+(int)(blob.position*(STAFF_LENGTH-1));
  int br=(int)(bSize*STAFF_LENGTH); if (br<2) br=2;
  for (int i=-br;i<=br;i++){
    int idx=ci+i;
    if (idx<HEAD_LENGTH||idx>=HEAD_LENGTH+STAFF_LENGTH) continue;
    float d=fabs(i)/(float)br; if (d>1.0f) continue;
    float ints=(d<0.6f)?(1.0f-d*d*1.5f):smoothstep(1.0f,0.6f,d);
    ints*=(1.0f+edgeSpike*0.5f);
    ints*=sin(d*12.0f+now*0.003f)*0.2f+0.8f;
    CRGB fc=CHSV(215,220+(uint8_t)(ints*35),(uint8_t)(ints*180));
    if (d<0.4f){CRGB cw=CRGB(60,50,70);cw.nscale8_video((uint8_t)((0.4f-d)/0.4f*80));fc+=cw;}
    leds[idx]=blend(leds[idx],fc,220);
  }
  renderTrailSystem();
}
void renderHeadAndTail() {
  uint32_t now=millis();
  CRGB hBase=CHSV(headHue,headSat,headVal);
  CRGB tBase=CHSV(tailHue,tailSat,tailVal);
  for (int i=0;i<HEAD_LENGTH;i++){
    CRGB c=hBase;
    float b=sin(htState.headBreathePhase+i*0.3f)*0.15f+0.85f;
    c.nscale8_video((uint8_t)(b*255));
    if(sin(htState.headPulsePhase+i*0.8f)>0.7f) c+=CRGB(30,20,40);
    if(htState.headSustainedGlow>0){ CRGB g=CHSV(200,255,(uint8_t)(htState.headSustainedGlow*255)); c=blend(c,g,(uint8_t)(htState.headSustainedGlow*200)); }
    if(htState.headImpactIntensity>0){ CRGB f=CHSV(180,255,255); c=blend(c,f,(uint8_t)(htState.headImpactIntensity*255)); }
    leds[i]=c;
  }
  for (int i=0;i<TAIL_OFFSET;i++) {
    int idx = TAIL_START+i;
    if (idx < NUM_LEDS) leds[idx]=CRGB::Black;
  }
  
  for (int hi=0;hi<HEAD_LENGTH;hi++){
    int t1=TAIL_START+TAIL_OFFSET+(hi*2);
    int t2=t1+1;
    if (t1>=NUM_LEDS || t2>=NUM_LEDS) {
      static bool warned = false;
      if (!warned) {
        Serial.printf("WARNING: Tail rendering out of bounds! t1=%d, t2=%d, NUM_LEDS=%d\n", t1, t2, NUM_LEDS);
        warned = true;
      }
      continue;
    }
    CRGB c1=tBase,c2=tBase;
    float b1=sin(htState.tailBreathePhase+hi*0.4f)*0.1f+0.9f;
    float b2=sin(htState.tailBreathePhase+hi*0.4f+1.57f)*0.1f+0.9f;
    c1.nscale8_video((uint8_t)(b1*255)); c2.nscale8_video((uint8_t)(b2*255));
    if(htState.tailSustainedGlow>0){ CRGB g=CHSV(200,255,(uint8_t)(htState.tailSustainedGlow*255));
      c1=blend(c1,g,(uint8_t)(htState.tailSustainedGlow*150)); c2=blend(c2,g,(uint8_t)(htState.tailSustainedGlow*150)); }
    if(htState.tailImpactIntensity>0){ CRGB f=CHSV(180,255,255);
      c1=blend(c1,f,(uint8_t)(htState.tailImpactIntensity*255)); c2=blend(c2,f,(uint8_t)(htState.tailImpactIntensity*255)); }
    leds[t1]=c1; leds[t2]=c2;
  }
}
// ── finish helpers ─────────────────────────────────────────────────────────
// Replace the repeated 2-3 line "updateHeadTailReactivity / customColorMode /
// renderHeadAndTail" block at the end of every effect with one call.
// FastLED += already does saturating (qadd8) adds, so the old
//   if(leds[i].r>255)leds[i].r=255; ... triplets are redundant and removed.

// No-colour variant (effects that don't set head/tail colours themselves)
void finishEffect(float dt, float p, float v) {
  updateHeadTailReactivity(dt, p, v); renderHeadAndTail();
}
// Colour-setting variant
void finishEffect(float dt, float p, float v,
                  uint8_t hH, uint8_t hS, uint8_t hV,
                  uint8_t tH, uint8_t tS, uint8_t tV) {
  if (!customColorMode) { headHue=hH;headSat=hS;headVal=hV;tailHue=tH;tailSat=tS;tailVal=tV; }
  updateHeadTailReactivity(dt, p, v); renderHeadAndTail();
}
// Colour + centre-zone variant
void finishEffectC(float dt, float p, float v,
                   uint8_t hH, uint8_t hS, uint8_t hV,
                   uint8_t tH, uint8_t tS, uint8_t tV,
                   uint8_t czH, uint8_t czS, float czL) {
  if (!customColorMode) { headHue=hH;headSat=hS;headVal=hV;tailHue=tH;tailSat=tS;tailVal=tV; }
  updateHeadTailReactivity(dt, p, v); renderHeadAndTail(); renderCenterZone(czH, czS, czL);
}
// ───────────────────────────────────────────────────────────────────────────

// =====================================================================
// INK DISPERSION — burst of white/purple fizzle particles
// =====================================================================
void spawnInkDispersion(float pos, float inheritedVel, float strength) {
  // Burst ~8-14 particles outward in both directions from pos
  int count = 6 + (int)(strength * 8);
  if (count > 14) count = 14;
  int spawned = 0;
  for (int i = 0; i < MAX_INK_PARTICLES && spawned < count; i++) {
    if (inkParticles[i].active) continue;
    // Half go forward, half backward; speed randomised by strength
    float dir = (spawned % 2 == 0) ? 1.0f : -1.0f;
    float spd = (0.3f + (random(100)/100.0f) * 0.7f) * strength;
    // Bias in the direction the blob was moving so it "splashes forward"
    if (fabs(inheritedVel) > 0.05f)
      spd += fabs(inheritedVel) * 0.4f * (dir * sign(inheritedVel) > 0 ? 1.0f : 0.3f);
    inkParticles[i].position = pos + (random(20)-10) * 0.003f;
    inkParticles[i].velocity = dir * spd;
    inkParticles[i].life     = 0.7f + random(30)/100.0f;  // 0.7-1.0
    inkParticles[i].hue      = 220 + random(40);           // white-purple band
    inkParticles[i].active   = true;
    spawned++;
  }
}

void updateInkDispersion(float dt) {
  for (int i = 0; i < MAX_INK_PARTICLES; i++) {
    if (!inkParticles[i].active) continue;
    InkParticle &p = inkParticles[i];
    // Move with increasing drag
    p.position += p.velocity * dt;
    p.velocity *= (1.0f - dt * 4.5f);   // drag: slows rapidly like real ink in water
    // Decay life; hue drifts from white-purple → deep purple as it fades
    p.life -= dt * 1.8f;
    p.hue  += dt * 15.0f;               // slowly shifts hue as it dies
    if (p.life <= 0.0f || p.position < 0.0f || p.position > 1.0f)
      p.active = false;
  }
}

void renderInkDispersion() {
  for (int i = 0; i < MAX_INK_PARTICLES; i++) {
    if (!inkParticles[i].active) continue;
    InkParticle &p = inkParticles[i];
    if (p.position < 0 || p.position > 1) continue;
    int idx = HEAD_LENGTH + (int)(p.position * (STAFF_LENGTH - 1));
    if (idx < HEAD_LENGTH || idx >= HEAD_LENGTH + STAFF_LENGTH) continue;
    float l = p.life;
    // Bright white-purple core that quickly diffuses to dimmer purple
    uint8_t sat = (uint8_t)(clampf(255.0f - l * l * 200.0f, 80.0f, 255.0f));
    uint8_t val = (uint8_t)(l * l * 255.0f);
    uint8_t hue = (uint8_t)fmod(p.hue, 256.0f);
    CRGB c = CHSV(hue, sat, val);
    // Small spread — 1-2 LED halo
    leds[idx] += c;
    if (idx-1 >= HEAD_LENGTH)                { CRGB s=c; s.nscale8(100); leds[idx-1] += s; }
    if (idx+1 < HEAD_LENGTH+STAFF_LENGTH)    { CRGB s=c; s.nscale8(100); leds[idx+1] += s; }
  }
}

void renderCenterZone(uint8_t baseHue, uint8_t baseSat, float audioReactivity) {
  #define CENTER_ZONE_SIZE 15
  
  int ci = STAFF_LENGTH / 2;
  
  htState.centerBreathePhase += 0.015f * audioReactivity;
  if(htState.centerBreathePhase > 6.283f) htState.centerBreathePhase -= 6.283f;
  
  htState.centerImpactIntensity *= 0.92f;
  
  for(int i = -CENTER_ZONE_SIZE; i <= CENTER_ZONE_SIZE; i++) {
    int idx = ci + i;
    if(idx < 0 || idx >= STAFF_LENGTH) continue;
    
    float distance = abs(i) / (float)CENTER_ZONE_SIZE;
    
    float edgeFade = 1.0f - (distance * distance);
    
    float breathe = sin(htState.centerBreathePhase + distance * 2.0f) * 0.15f + 0.85f;
    
    uint8_t val = (uint8_t)(edgeFade * breathe * (140 + audioReactivity * 115));
    CRGB centerColor = CHSV(baseHue, baseSat, val);
    
    if(htState.centerImpactIntensity > 0.1f) {
      float impactAmount = htState.centerImpactIntensity * edgeFade;
      CRGB impactColor = CHSV(baseHue + 20, 255, 255);
      centerColor = blend(centerColor, impactColor, (uint8_t)(impactAmount * 200));
    }
    
    leds[HEAD_LENGTH + idx] += centerColor;
  }
  
  #undef CENTER_ZONE_SIZE
}
