
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
}
void renderRainbowPainterEffect(float dt) {
  uint32_t now=millis();
  if (!rainbowPainter.initialized){
    for(int i=0;i<STAFF_LENGTH;i++) rainbowPainter.mixingBuffer[i]=(float)(i*256)/STAFF_LENGTH;
    rainbowPainter.initialized=true;
  }
  if (mpu_ready){
    float ty=clampf(accel[1]*0.4f,-1,1);
    rainbowPainter.paintVelocity=rainbowPainter.paintVelocity*0.85f+ty*dt*6.0f;
    rainbowPainter.paintCenter+=rainbowPainter.paintVelocity*dt;
    if(rainbowPainter.paintCenter<0.1f){rainbowPainter.paintCenter=0.1f;rainbowPainter.paintVelocity=-rainbowPainter.paintVelocity*0.7f;htState.headImpactIntensity=clampf(fabs(rainbowPainter.paintVelocity)*3,0,1.5f);}
    if(rainbowPainter.paintCenter>0.9f){rainbowPainter.paintCenter=0.9f;rainbowPainter.paintVelocity=-rainbowPainter.paintVelocity*0.7f;htState.tailImpactIntensity=clampf(fabs(rainbowPainter.paintVelocity)*3,0,1.5f);}
  } else {
    static float dp=0; dp+=dt*0.5f;
    rainbowPainter.paintCenter=0.5f+sin(dp)*0.2f;
    rainbowPainter.paintVelocity=cos(dp)*0.1f;
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
    uint8_t ph=(uint8_t)rainbowPainter.mixingBuffer[i];
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
}
void renderFireStormEffect(float dt) {
  static byte heat[144];
  static uint32_t lu=0;
  static float fb=0,fbv=0;
  static float rPos[5]={0}; static unsigned long rT[5]={0}; static bool rA[5]={false};
  uint32_t now=millis();
  if(now-lu<50)return; lu=now;
  if(mpu_ready){
    float ty=clampf(accel[1]*0.5f,-1,1);
    fbv=fbv*0.88f+ty*dt*8; fb+=fbv*dt*2;
    if(fb<0.1f){fb=0.1f;fbv=-fbv*0.7f;htState.headImpactIntensity=1.5f;for(int i=0;i<5;i++)if(!rA[i]){rA[i]=true;rPos[i]=0.0f;rT[i]=now;break;}}
    if(fb>0.9f){fb=0.9f;fbv=-fbv*0.7f;htState.tailImpactIntensity=1.5f;for(int i=0;i<5;i++)if(!rA[i]){rA[i]=true;rPos[i]=1.0f;rT[i]=now;break;}}
  } else { static float sp=0;sp+=dt*0.5f;fb=0.5f+sin(sp)*0.3f;fbv=cos(sp)*0.2f; }
  if(random(100)<3)for(int i=0;i<5;i++)if(!rA[i]){rA[i]=true;rPos[i]=0.5f;rT[i]=now;break;}
  for(int i=0;i<STAFF_LENGTH;i++){int cd=random(10,25);heat[i]=(heat[i]>cd)?heat[i]-cd:0;}
  for(int k=STAFF_LENGTH-1;k>=2;k--) heat[k]=(byte)((heat[k-1]*0.6f+heat[k-2]*0.4f));
  int bi=(int)(fb*STAFF_LENGTH); int bri=8+(int)(fabs(fbv)*15);
  for(int i=-bri;i<=bri;i++){int idx=bi+i;if(idx>=0&&idx<STAFF_LENGTH){float d=fabs(i)/(float)bri;int nh=heat[idx]+(int)((1-d)*200);heat[idx]=(nh>255)?255:(byte)nh;}}
  for(int i=0;i<STAFF_LENGTH;i++){
    byte t=heat[i]; uint8_t r,g,b;
    if(t>200){r=255;g=255;b=random(30,80);}
    else if(t>150){r=255;g=random(80,120);b=0;}
    else if(t>100){r=255;g=random(30,80);b=0;}
    else if(t>50){r=200+random(55);g=random(20,40);b=0;}
    else{r=100+random(50);g=0;b=0;}
    leds[HEAD_LENGTH+i]=CRGB(r,g,b);
  }
  for(int r=0;r<5;r++){
    if(!rA[r])continue;
    float rr=(now-rT[r])/800.0f*0.5f;
    if(rr>0.5f){rA[r]=false;continue;}
    for(int i=0;i<STAFF_LENGTH;i++){
      float pos=(float)i/STAFF_LENGTH,dc=fabs(pos-rPos[r]),dr2=fabs(dc-rr);
      if(dr2<0.05f){float ints=(1-dr2/0.05f)*(1-rr*2);leds[HEAD_LENGTH+i]+=CRGB((uint8_t)(ints*255),(uint8_t)(ints*100),0);}
    }
  }
  finishEffect(dt,fb,fbv, 0,255,220, 10,255,200);
}
void renderOceanWavesEffect(float dt) {
  static float wp=0,ob=0.5f,obv=0;
  static float wgP[3]={0.2f,0.5f,0.8f},wgS[3]={0.15f,0.2f,0.18f},wgI[3]={0.8f,1.0f,0.9f};
  static unsigned long wgT[3]={0};
  uint32_t now=millis();
  if(mpu_ready){
    float ty=clampf(accel[1]*0.4f,-1,1);
    obv=obv*0.9f+ty*dt*6; ob+=obv*dt*2;
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
}
void renderCrystalPulseEffect(float dt) {
  static float cp=0,cb=0.5f,cbv=0;
  static float eP[6]={0},eS[6]={0},eI[6]={0};
  static uint8_t eH[6]={0};
  static unsigned long eT[6]={0};
  uint32_t now=millis();
  if(mpu_ready){
    float ty=clampf(accel[1]*0.3f,-1,1);
    cbv=cbv*0.92f+ty*dt*5; cb+=cbv*dt*1.8f;
    if(cb<0.1f){cb=0.1f;cbv=-cbv*0.75f;htState.headImpactIntensity=1.3f;for(int e=0;e<6;e++)if(eI[e]<0.2f){eP[e]=0.1f;eS[e]=0;eI[e]=1.0f;eH[e]=200+random(60);eT[e]=now;break;}}
    if(cb>0.9f){cb=0.9f;cbv=-cbv*0.75f;htState.tailImpactIntensity=1.3f;for(int e=0;e<6;e++)if(eI[e]<0.2f){eP[e]=0.9f;eS[e]=0;eI[e]=1.0f;eH[e]=200+random(60);eT[e]=now;break;}}
  } else { static float sp=0;sp+=dt*0.6f;cb=0.5f+sin(sp)*0.2f;cbv=cos(sp)*0.12f; }
  if(random(100)<5)for(int e=0;e<6;e++)if(eI[e]<0.2f){eP[e]=0.2f+random(60)/100.0f;eS[e]=0;eI[e]=0.7f+random(30)/100.0f;eH[e]=190+random(70);eT[e]=now;break;}
  for(int e=0;e<6;e++)if(eI[e]>0.1f){float as=(now-eT[e])/1000.0f;eS[e]=0.05f+(sin(as*4)*0.5f+0.5f)*0.25f;eI[e]*=0.96f;if(as>1.5f||eI[e]<0.1f)eI[e]=0;}
  cp+=dt*3; if(cp>6.283f)cp-=6.283f;
  for(int i=0;i<STAFF_LENGTH;i++){
    float pos=(float)i/STAFF_LENGTH,sh=sin(pos*20+cp)*0.3f+0.7f;
    leds[HEAD_LENGTH+i]=CHSV(200+(uint8_t)(pos*40),220,20+(uint8_t)(sh*30));
  }
  for(int e=0;e<6;e++){
    if(eI[e]<0.1f)continue;
    for(int i=0;i<STAFF_LENGTH;i++){
      float pos=(float)i/STAFF_LENGTH,de=fabs(pos-eP[e]);
      if(de<eS[e]){float ints=(eS[e]-de)/eS[e];ints*=ints;ints*=eI[e];
        leds[HEAD_LENGTH+i]+=CHSV(eH[e]+(uint8_t)(ints*20),255-(uint8_t)(ints*100),(uint8_t)(ints*255));}
    }
  }
  for(int i=0;i<STAFF_LENGTH;i++){
    float pos=(float)i/STAFF_LENGTH,dc=fabs(pos-cb);
    if(dc<0.15f){float bi=(0.15f-dc)/0.15f;bi*=bi;
      leds[HEAD_LENGTH+i]+=CHSV(210+(uint8_t)(bi*30),200,(uint8_t)(bi*200));}
  }
  finishEffect(dt,cb,cbv, 210,220,220, 220,200,200);
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
  spawnTimer+=dt;
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
}
