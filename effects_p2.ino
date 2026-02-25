
void renderAudioPulseEffect(float dt) {
  static float pp=0;
  static float explosionRadius = 0;
  static uint32_t lastExplosion = 0;
  static float darkWaveProgress = 0;
  static uint32_t lastDarkWave = 0;
  uint32_t now=millis();
  float sm=globalSpeed/128.0f;
  int ci=STAFF_LENGTH/2;
  pp+=dt*(3+audioLevel*5)*sm; if(pp>6.283f)pp-=6.283f;
  float pi2=(sin(pp)*0.5f+0.5f)*(0.3f+audioLevel*0.7f);
  
  for(int i=0;i<STAFF_LENGTH;i++){
    float d=fabs((float)i-ci)/(float)ci;
    uint8_t bh=220+(uint8_t)(sin(now*0.0005f+d*2)*10);
    uint8_t bv=80-(uint8_t)(d*30);
    float w=sin(d*15-pp*3)*0.5f+0.5f;
    uint8_t pv=(uint8_t)(pi2*w*180 + 25);
    uint8_t ph=215+(uint8_t)(audioLevel*40);
    leds[HEAD_LENGTH+i]=CHSV(bh,180,bv)+CHSV(ph,220,pv);
  }
  
  if(audioMid > 0.4f || audioBass > 0.5f) {
    if((now - lastDarkWave) > 1200) {
      darkWaveProgress = 0;
      lastDarkWave = now;
    }
  }
  
  if((now - lastDarkWave) < 2000) {
    darkWaveProgress += dt * 35 * sm;
    float progress = (now - lastDarkWave) / 2000.0f;
    float intensity = 1.0f - (progress * progress);
    
    for(int i=0; i<ci; i++){
      float distFromEnd = i;
      float waveDist = abs(distFromEnd - darkWaveProgress);
      
      if(waveDist < 12) {
        float waveIntensity = (1.0f - waveDist/12.0f) * intensity;
        
        uint8_t hue = 200 - (uint8_t)(waveIntensity * 30);
        uint8_t sat = 255;
        uint8_t val = (uint8_t)(waveIntensity * 100);
        
        float coreEffect = sin(waveDist * 0.5f) * 0.3f + 0.7f;
        val = (uint8_t)(val * coreEffect);
        
        int idxFromHead = HEAD_LENGTH + i;
        int idxFromTail = HEAD_LENGTH + STAFF_LENGTH - 1 - i;
        
        if(idxFromHead >= HEAD_LENGTH && idxFromHead < HEAD_LENGTH + STAFF_LENGTH) {
          leds[idxFromHead] = blend(leds[idxFromHead], CHSV(hue, sat, val), 200);
        }
        if(idxFromTail >= HEAD_LENGTH && idxFromTail < HEAD_LENGTH + STAFF_LENGTH) {
          leds[idxFromTail] = blend(leds[idxFromTail], CHSV(hue, sat, val), 200);
        }
      }
    }
  }
  
  if(pseudoBeat&&beatIntensity>0.18f){
    explosionRadius = 0;
    lastExplosion = now;
    htState.headImpactIntensity=beatIntensity*1.8f;
    htState.tailImpactIntensity=beatIntensity*1.8f;
    htState.centerImpactIntensity = beatIntensity * 2.0f;
  }
  
  if((now - lastExplosion) < 600) {
    explosionRadius += dt * 120 * sm;
    float progress = (now - lastExplosion) / 600.0f;
    float intensity = (1.0f - progress) * beatIntensity;
    
    for(int layer = 0; layer < 3; layer++) {
      float layerOffset = layer * 3.0f;
      float jitter = sin(now * 0.03f + layer) * 2.0f;
      
      for(int i=0; i<ci; i++){
        float d = abs(i);
        float ringDist = abs(d - (explosionRadius + layerOffset + jitter));
        
        if(ringDist < 4) {
          float ringIntensity = (1.0f - ringDist/4.0f) * intensity;
          
          uint8_t hue = 240 - (uint8_t)(ringIntensity * 60);
          uint8_t sat = 255 - (uint8_t)(ringIntensity * 100);
          uint8_t val = (uint8_t)(ringIntensity * 255);
          
          val += random(-20, 20);
          val = constrain(val, 0, 255);
          
          leds[HEAD_LENGTH+ci+i] += CHSV(hue, sat, val);
          leds[HEAD_LENGTH+ci-i] += CHSV(hue, sat, val);
        }
      }
    }
    
    for(int i=-5; i<=5; i++){
      int idx = ci + i;
      if(idx>=0 && idx<STAFF_LENGTH) {
        float coreDist = abs(i) / 5.0f;
        uint8_t darkness = (uint8_t)((1.0f - coreDist) * intensity * 180);
        leds[HEAD_LENGTH+idx].fadeToBlackBy(darkness);
      }
    }
  }
  
  if(!customColorMode){headHue=220;headSat=200;headVal=180+beatIntensity*75;tailHue=220;tailSat=200;tailVal=180+beatIntensity*75;}
  
  updateHeadTailReactivity(dt,0.7f,0);
  renderHeadAndTail();
  renderCenterZone(220, 220, audioLevel);
}
void renderBeatSparkleEffect(float dt) {
  static float beatRing = 0;
  static uint32_t lastBeat = 0;
  uint32_t now=millis();
  float sm=globalSpeed/128.0f;
  int ci=STAFF_LENGTH/2;
  fadeToBlackBy(leds+HEAD_LENGTH,STAFF_LENGTH,35);
  
  for(int i=0;i<STAFF_LENGTH;i++){
    float d=fabs((float)i-ci)/(float)ci;
    uint8_t val=(uint8_t)(80-d*40 + audioLevel*60);
    leds[HEAD_LENGTH+i]=CHSV(220,200,val);
  }
  
  if(pseudoBeat&&beatIntensity>0.15f){
    beatRing = 0;
    lastBeat = now;
    htState.headImpactIntensity=beatIntensity*1.5f;
    htState.tailImpactIntensity=beatIntensity*1.5f;
  }
  
  if((now - lastBeat) < 800) {
    beatRing += dt * 80 * sm;
    float ringFade = 1.0f - ((now - lastBeat) / 800.0f);
    
    for(int i=0;i<ci;i++){
      float d = abs(i);
      float ringDist = abs(d - beatRing);
      if(ringDist < 8) {
        float intensity = (1.0f - ringDist/8.0f) * ringFade;
        uint8_t val = (uint8_t)(intensity * 255);
        leds[HEAD_LENGTH+ci+i] += CHSV(240, 220, val);
        leds[HEAD_LENGTH+ci-i] += CHSV(240, 220, val);
      }
    }
  }
  
  if(pseudoBeat&&beatIntensity>0.15f){
    int ns=15+(int)(beatIntensity*20);
    for(int s=0;s<ns;s++){
      float sf=pow(random(100)/100.0f, 0.4f);
      int off=(int)(sf*ci);
      int sp=(random(2)==0)?(HEAD_LENGTH+ci+off):(HEAD_LENGTH+ci-off);
      if(sp>=HEAD_LENGTH&&sp<HEAD_LENGTH+STAFF_LENGTH){
        uint8_t hue=210+(uint8_t)(beatIntensity*45);
        leds[sp]+=CHSV(hue,220,(uint8_t)(beatIntensity*255));
      }
    }
  }
  
  if(audioMid>0.25f){
    float wave=(now*0.005f);
    for(int i=0;i<ci;i++){
      float d=i/(float)ci;
      float w=sin(wave+d*6.28f)*0.5f+0.5f;
      if(w>0.7f){
        uint8_t val=(uint8_t)((audioMid-0.25f)*w*255);
        leds[HEAD_LENGTH+ci+i]+=CHSV(180,220,val);
        leds[HEAD_LENGTH+ci-i]+=CHSV(180,220,val);
      }
    }
  }
  
  if(!customColorMode){headHue=220;headSat=200;headVal=200+beatIntensity*55;tailHue=220;tailSat=200;tailVal=200+beatIntensity*55;}
  
  if(pseudoBeat&&beatIntensity>0.15f){
    htState.centerImpactIntensity = beatIntensity * 1.3f;
  }
  
  updateHeadTailReactivity(dt,0.6f,0);
  renderHeadAndTail();
  renderCenterZone(230, 240, audioLevel);
}
void renderSoundWaveEffect(float dt) {
  static float wh[100]; static int wi=0;
  uint32_t now=millis();
  float sm=globalSpeed/128.0f;
  int ci=STAFF_LENGTH/2;
  wh[wi]=audioLevel*1.5f; wi=(wi+1)%100;
  
  for(int i=0;i<STAFF_LENGTH;i++){
    float d=fabs((float)i-ci);
    int hi=(wi-(int)(d*100.0f/ci)+100)%100;
    float lv=wh[hi];
    
    uint8_t hue;
    uint8_t sat;
    uint8_t val;
    
    if(lv > 0.6f) {
      hue = 180;
      sat = 255;
      val = (uint8_t)(lv*255);
    } else if(lv > 0.3f) {
      hue = 200;
      sat = 220;
      val = (uint8_t)(lv*200);
    } else {
      hue = 220;
      sat = 200;
      val = (uint8_t)(60 + lv*100);
    }
    
    leds[HEAD_LENGTH+i]=CHSV(hue, sat, val);
  }
  
  if(pseudoBeat&&beatIntensity>0.20f){
    static unsigned long lbt=0; static float be=0;
    if(now-lbt>100){lbt=now;be=0;}
    be+=dt*sm*40;
    for(int i=0;i<STAFF_LENGTH;i++){
      float d=fabs((float)i-ci),rd=fabs(d-be);
      if(rd<8&&be<ci){
        float ints=(1-rd/8)*beatIntensity;
        leds[HEAD_LENGTH+i]=CHSV(160,255,(uint8_t)(ints*255));
      }
    }
    htState.headImpactIntensity=beatIntensity*1.5f;
    htState.tailImpactIntensity=beatIntensity*1.5f;
  }
  if(!customColorMode){headHue=180;headSat=220;headVal=180+audioLevel*75;tailHue=200;tailSat=220;tailVal=180+audioLevel*75;}
  
  if(pseudoBeat&&beatIntensity>0.20f){
    htState.centerImpactIntensity = beatIntensity * 1.4f;
  }
  
  updateHeadTailReactivity(dt,0.7f,0);
  renderHeadAndTail();
  renderCenterZone(190, 230, audioLevel);
}
void renderColorShiftEffect(float dt) {
  static float cp=0;
  static float beatWaveOut = 0;
  static float beatWaveIn = 0;
  static uint32_t lastBeat = 0;
  static bool waveDirection = true;
  uint32_t now=millis();
  float sm=globalSpeed/128.0f;
  int ci=STAFF_LENGTH/2;
  
  cp+=dt*audioLevel*120*sm; if(cp>255)cp-=255;
  
  for(int i=0;i<STAFF_LENGTH;i++){
    float d=fabs((float)i-ci)/ci;
    uint8_t h=(uint8_t)(cp+d*80),s=200+(uint8_t)(audioLevel*55);
    uint8_t baseVal=100-(uint8_t)(d*50);
    uint8_t audioBoost=(uint8_t)(audioLevel*100);
    leds[HEAD_LENGTH+i]=CHSV(h,s,baseVal+audioBoost);
  }
  
  if(pseudoBeat&&beatIntensity>0.25f){
    beatWaveOut = 0;
    beatWaveIn = ci;
    lastBeat = now;
    waveDirection = !waveDirection;
    htState.headImpactIntensity=beatIntensity*1.8f;
    htState.tailImpactIntensity=beatIntensity*1.8f;
  }
  
  if((now - lastBeat) < 600) {
    float progress = (now - lastBeat) / 600.0f;
    beatWaveOut += dt * 100 * sm;
    beatWaveIn -= dt * 100 * sm;
    float intensity = 1.0f - progress;
    
    for(int i=0;i<ci;i++){
      float d = i;
      
      float ringDist1 = abs(d - beatWaveOut);
      if(ringDist1 < 10 && waveDirection) {
        float wave = (1.0f - ringDist1/10.0f) * intensity;
        uint8_t val = (uint8_t)(wave * 255);
        leds[HEAD_LENGTH+ci+i] = CHSV((uint8_t)(cp+128), 255, val);
        leds[HEAD_LENGTH+ci-i] = CHSV((uint8_t)(cp+128), 255, val);
      }
      
      float ringDist2 = abs(d - beatWaveIn);
      if(ringDist2 < 10 && !waveDirection) {
        float wave = (1.0f - ringDist2/10.0f) * intensity;
        uint8_t val = (uint8_t)(wave * 255);
        leds[HEAD_LENGTH+ci+i] = CHSV((uint8_t)(cp+128), 255, val);
        leds[HEAD_LENGTH+ci-i] = CHSV((uint8_t)(cp+128), 255, val);
      }
    }
  }
  
  if(audioMid>0.35f){
    for(int i=-5;i<=5;i++){
      int idx=ci+i;
      if(idx>=0&&idx<STAFF_LENGTH){
        uint8_t val=(uint8_t)((audioMid-0.35f)*255*4);
        leds[HEAD_LENGTH+idx]+=CHSV((uint8_t)(cp+64),255,val);
      }
    }
  }
  
  if(!customColorMode){headHue=(uint8_t)cp;headSat=220;headVal=180+beatIntensity*75;tailHue=(uint8_t)cp;tailSat=220;tailVal=180+beatIntensity*75;}
  
  if(pseudoBeat&&beatIntensity>0.25f){
    htState.centerImpactIntensity = beatIntensity * 1.6f;
  }
  
  updateHeadTailReactivity(dt,0.8f,0);
  renderHeadAndTail();
  renderCenterZone((uint8_t)(cp + 40), 240, audioLevel);
}
void renderAudioFireEffect(float dt) {
  static byte heat[144]; static uint32_t lu=0;
  uint32_t now=millis();
  int ci=STAFF_LENGTH/2;
  if(now-lu<50)return; lu=now;
  
  for(int i=0;i<STAFF_LENGTH;i++){int cd=random(8,18);heat[i]=(heat[i]>cd)?heat[i]-cd:0;}
  for(int k=0;k<ci-1;k++) heat[k]=(heat[k+1]+heat[k+2])/2;
  for(int k=STAFF_LENGTH-1;k>ci+1;k--) heat[k]=(heat[k-1]+heat[k-2])/2;
  
  for(int i=0;i<STAFF_LENGTH;i++){
    float d=fabs((float)i-ci)/(float)ci;
    heat[i]=max(heat[i],(byte)(60-d*20));
  }
  
  if(random(255)<180*(0.1f+audioLevel*0.9f)){
    int off=random(-4,5);int pos=ci+off;
    if(pos>=0&&pos<STAFF_LENGTH)heat[pos]=constrain(heat[pos]+random(160,220),0,255);
  }
  
  if(pseudoBeat&&beatIntensity>0.18f){
    for(int i=-8;i<=8;i++){
      int pos=ci+i;
      if(pos>=0&&pos<STAFF_LENGTH){
        float d = abs(i)/8.0f;
        heat[pos]=255-(uint8_t)(d*100);
      }
    }
    for(int s=0;s<10;s++){
      int pos=ci+random(-15,16);
      if(pos>=0&&pos<STAFF_LENGTH)heat[pos]=min(255,(int)(heat[pos]+random(180,255)));
    }
    htState.headImpactIntensity=beatIntensity*2.0f;
    htState.tailImpactIntensity=beatIntensity*2.0f;
  }
  
  for(int i=0;i<STAFF_LENGTH;i++){
    byte t=heat[i]; uint8_t r,g,b;
    if(t>200){r=255;g=255;b=random(80,150);}
    else if(t>150){r=255;g=random(100,180);b=0;}
    else if(t>100){r=255;g=random(40,100);b=0;}
    else if(t>50){r=220+random(35);g=random(15,35);b=0;}
    else{r=t*3;g=0;b=0;}
    leds[HEAD_LENGTH+i]=CRGB(r,g,b);
  }
  if(!customColorMode){headHue=0;headSat=255;headVal=180+beatIntensity*75;tailHue=20;tailSat=255;tailVal=180+beatIntensity*75;}
  
  if(pseudoBeat&&beatIntensity>0.18f){
    htState.centerImpactIntensity = beatIntensity * 2.0f;
  }
  
  updateHeadTailReactivity(dt,0.8f,0);
  renderHeadAndTail();
  renderCenterZone(10, 255, audioLevel * 1.5f);
}
