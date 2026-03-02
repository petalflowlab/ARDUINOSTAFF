
void renderPlasmaStormEffect(float dt) {
  static float p1=0,p2=0,p3=0;
  float ty = mpu_ready ? clampf(accel[1], -1.0f, 1.0f) : 0.0f;
  static float lastAccelYP3 = 0;
  float jerk = mpu_ready ? clampf((ty - lastAccelYP3)*5.0f, -3.0f, 3.0f) : 0.0f;
  lastAccelYP3 = ty;
  float rollSpd = mpu_ready ? fabs(rollRate)*0.05f : 0.0f;
  p1+=dt*(2.5f + rollSpd); if(p1>6.283f)p1-=6.283f;
  p2+=dt*(1.8f + rollSpd); if(p2>6.283f)p2-=6.283f;
  p3+=dt*(3.2f + rollSpd); if(p3>6.283f)p3-=6.283f;
  
  if (fabs(jerk) > 1.5f) {
    htState.headImpactIntensity = max(htState.headImpactIntensity, fabs(jerk)*0.8f);
    htState.tailImpactIntensity = max(htState.tailImpactIntensity, fabs(jerk)*0.8f);
    for (int j=0; j<5; j++) leds[HEAD_LENGTH+random(STAFF_LENGTH)] = CRGB(255,255,255);
  }
  for(int i=0;i<STAFF_LENGTH;i++){
    float pos=(float)i/STAFF_LENGTH;
    float c=(sin(pos*10+p1)+sin(pos*15-p2)+sin(pos*8+p3))/3.0f;
    leds[HEAD_LENGTH+i]=CHSV(200+(uint8_t)(c*40),255,120+(uint8_t)(c*135));
  }
  if(random8()<30){int ap=HEAD_LENGTH+random(STAFF_LENGTH-5);for(int j=0;j<3;j++)leds[ap+j]=CHSV(180,200,255);}
  finishEffect(dt,0.5f,0, 200,255,220, 220,255,200);
}
void renderLightningStrikeEffect(float dt) {
  #define MAX_LIGHTNING 12
  static float si[MAX_LIGHTNING]={0};
  static int sp[MAX_LIGHTNING]={0},sl[MAX_LIGHTNING]={0},bp[MAX_LIGHTNING]={0};
  static bool sb[MAX_LIGHTNING]={false};
  static unsigned long st[MAX_LIGHTNING]={0};
  uint32_t now=millis();
  float sm=globalSpeed/128.0f;
  float ty = mpu_ready ? clampf(accel[1], -1.0f, 1.0f) : 0.0f;
  static float lastAccelYL = 0;
  float jerk = mpu_ready ? clampf((ty - lastAccelYL)*5.0f, -3.0f, 3.0f) : 0.0f;
  lastAccelYL = ty;
  for(int i=0;i<STAFF_LENGTH;i++){
    float pos=(float)i/STAFF_LENGTH;
    leds[HEAD_LENGTH+i]=blend(leds[HEAD_LENGTH+i],CHSV(160+(uint8_t)(sin(now*0.001f+pos*5)*20+(mpu_ready?vortexPhase*20:0)),200,40+(uint8_t)(sin(now*0.0008f+pos*3)*30)),50);
  }
  if(random(100)<8*sm || fabs(jerk) > 1.2f){
    for(int s=0;s<MAX_LIGHTNING;s++)if(si[s]<0.1f){
      if (ty > 0.4f) sp[s] = random(STAFF_LENGTH/2, STAFF_LENGTH);
      else if (ty < -0.4f) sp[s] = random(0, STAFF_LENGTH/2);
      else sp[s]=random(STAFF_LENGTH);
      
      sl[s]=random(15,40); si[s]=1.0f+random(50)/100.0f + fabs(jerk)*0.8f;
      sb[s]=(random(100)<40); if(sb[s])bp[s]=random(-10,10);
      st[s]=now;
      if(random(100)<30 || fabs(jerk) > 1.2f) {
          htState.headImpactIntensity=max(htState.headImpactIntensity, 1.5f);
          htState.tailImpactIntensity=max(htState.tailImpactIntensity, 1.5f);
      }
      break;
    }
  }
  for(int s=0;s<MAX_LIGHTNING;s++){
    if(si[s]<=0)continue;
    si[s]-=dt*(4+random(20)/10.0f);
    for(int i=-sl[s]/2;i<=sl[s]/2;i++){
      int idx=sp[s]+i;
      if(idx>=0&&idx<STAFF_LENGTH){
        float d=fabs(i)/(float)(sl[s]/2);
        uint8_t br=(uint8_t)((1-d*d)*si[s]*255);
        CRGB bc=CRGB(br,br,min(255,br+30));
        leds[HEAD_LENGTH+idx]=blend(leds[HEAD_LENGTH+idx],bc,200);
      }
    }
    if(sb[s]&&si[s]>0.5f){
      int bl=sl[s]/3;
      for(int i=0;i<bl;i++){
        int idx=sp[s]+bp[s]+i;
        if(idx>=0&&idx<STAFF_LENGTH){float d=(float)i/bl;uint8_t br=(uint8_t)((1-d)*si[s]*180);CRGB bc=CRGB(br,br,min(255,br+30));leds[HEAD_LENGTH+idx]=blend(leds[HEAD_LENGTH+idx],bc,150);}
      }
    }
  }
  if(random(100)<30) leds[HEAD_LENGTH+random(STAFF_LENGTH)]=CRGB(200,200,255);
  finishEffect(dt,0.5f,0, 170,200,230, 180,180,210);
}
void renderCometTrailEffect(float dt) {
  #define MAX_COMETS 5
  static float cp[MAX_COMETS]={0.2f,0.5f,0.8f,0.3f,0.7f};
  static float cv[MAX_COMETS]={0.4f,-0.5f,0.6f,-0.45f,0.55f};
  static uint8_t ch[MAX_COMETS]={0,85,170,42,200};
  #define MAX_EXPLOSIONS 8
  static float ep[MAX_EXPLOSIONS]={0},es[MAX_EXPLOSIONS]={0},ei[MAX_EXPLOSIONS]={0};
  static uint8_t eh[MAX_EXPLOSIONS]={0};
  uint32_t now=millis();
  float sm=globalSpeed/128.0f;
  float ty = mpu_ready ? clampf(accel[1], -1.0f, 1.0f) : 0.0f;
  static float lastAccelYC = 0;
  float jerk = mpu_ready ? clampf((ty - lastAccelYC)*5.0f, -3.0f, 3.0f) : 0.0f;
  lastAccelYC = ty;
  if(fabs(jerk) > 1.5f) {
     htState.headImpactIntensity = max(htState.headImpactIntensity, fabs(jerk)*0.8f);
     htState.tailImpactIntensity = max(htState.tailImpactIntensity, fabs(jerk)*0.8f);
     for(int e=0;e<MAX_EXPLOSIONS;e++)if(ei[e]<0.1f){ep[e]=0.5f+random(-30,30)/100.0f;es[e]=0;ei[e]=2.0f;eh[e]=random(256);break;}
  }

  for(int c=0;c<MAX_COMETS;c++){
    cp[c]+=(cv[c] + ty*0.8f)*dt*sm;
    if(cp[c]<0){cp[c]=0;cv[c]=-cv[c];ch[c]=random(256);htState.headImpactIntensity=1.2f;}
    if(cp[c]>1){cp[c]=1;cv[c]=-cv[c];ch[c]=random(256);htState.tailImpactIntensity=1.2f;}
    for(int c2=c+1;c2<MAX_COMETS;c2++){
      if(fabs(cp[c]-cp[c2])<0.05f){
        for(int e=0;e<MAX_EXPLOSIONS;e++)if(ei[e]<0.1f){ep[e]=(cp[c]+cp[c2])/2;es[e]=0;ei[e]=1.5f;eh[e]=(ch[c]+ch[c2])/2;break;}
        cv[c]=-cv[c]*0.8f;cv[c2]=-cv[c2]*0.8f;ch[c]=random(256);ch[c2]=random(256);
      }
    }
  }
  for(int e=0;e<MAX_EXPLOSIONS;e++)if(ei[e]>0.1f){es[e]+=dt*0.3f;ei[e]*=0.92f;}
  fadeToBlackBy(leds+HEAD_LENGTH,STAFF_LENGTH,15);
  for(int c=0;c<MAX_COMETS;c++){
    int ci=(int)(cp[c]*STAFF_LENGTH);
    for(int i=-4;i<=4;i++){int idx=ci+i;if(idx>=0&&idx<STAFF_LENGTH){float d=fabs(i)/4.0f;leds[HEAD_LENGTH+idx]+=CHSV(ch[c],200,(uint8_t)((1-d*d)*255));}}
    int tl=60;
    for(int i=1;i<=tl;i++){
      int ti=ci-(int)(cv[c]>0?i:-i);
      if(ti>=0&&ti<STAFF_LENGTH){float f=(float)(tl-i)/tl;f=f*f*f;leds[HEAD_LENGTH+ti]+=CHSV(ch[c],160+(uint8_t)(f*60),(uint8_t)(f*180));}
    }
  }
  for(int e=0;e<MAX_EXPLOSIONS;e++){
    if(ei[e]<0.1f)continue;
    int ei2=(int)(ep[e]*STAFF_LENGTH),er=(int)(es[e]*STAFF_LENGTH/2);
    for(int i=-er;i<=er;i++){int idx=ei2+i;if(idx>=0&&idx<STAFF_LENGTH){float d=fabs(i)/(float)(er+1),ints=(1-d)*ei[e];leds[HEAD_LENGTH+idx]+=CHSV(eh[e],200,(uint8_t)(ints*255));}}
  }
  float avg=0;for(int c=0;c<MAX_COMETS;c++)avg+=cp[c];avg/=MAX_COMETS;
  finishEffect(dt,avg,0.5f, 20,255,220, 40,220,200);
}
void renderAuroraFlowEffect(float dt) {
  static float ap=0,fd=1.0f;
  uint32_t now=millis();
  float ty = mpu_ready ? clampf(accel[1], -1.0f, 1.0f) : 0.0f;
  static float lastAccelYA = 0;
  float jerk = mpu_ready ? clampf((ty - lastAccelYA)*5.0f, -3.0f, 3.0f) : 0.0f;
  lastAccelYA = ty;
  float rollSpd = mpu_ready ? rollRate*0.01f : 0.0f;
  
  ap+=dt*(0.8f*fd + rollSpd); if(ap>6.283f)ap-=6.283f; if(ap<-6.283f)ap+=6.283f;
  if (fabs(jerk) > 1.5f) {
     htState.headImpactIntensity = max(htState.headImpactIntensity, fabs(jerk)*0.8f);
     htState.tailImpactIntensity = max(htState.tailImpactIntensity, fabs(jerk)*0.8f);
     fd = -fd; // Reverse direction on hit
  }
  if(random(1000)<5)fd=-fd;
  for(int i=0;i<STAFF_LENGTH;i++){
    float pos=(float)i/STAFF_LENGTH;
    float tiltVal = (pos - 0.5f) * ty * 2.0f; 
    float w1=sin(pos*8+ap)*0.5f+0.5f,w2=sin(pos*12-ap*1.5f)*0.5f+0.5f,w3=sin(pos*6+ap*0.8f)*0.5f+0.5f;
    uint8_t baseBright = (uint8_t)(100+(uint8_t)(w3*130));
    baseBright = (uint8_t)clampf((float)baseBright + tiltVal*100.0f, 0.0f, 255.0f);
    leds[HEAD_LENGTH+i]=CHSV(120+(uint8_t)(w1*60),180+(uint8_t)(w2*75),baseBright);
    if(sin(now*0.005f+pos*10)>0.85f) leds[HEAD_LENGTH+i]+=CHSV(120+(uint8_t)(w1*60),150,80);
  }
  finishEffect(dt,0.5f,0, 120,200,210, 140,200,190);
}
void renderGalaxySwirlEffect(float dt) {
  static float gr=0,sp[20]; static bool sinit=false;
  uint32_t now=millis();
  float ty = mpu_ready ? clampf(accel[1], -1.0f, 1.0f) : 0.0f;
  static float lastAccelYG = 0;
  float jerk = mpu_ready ? clampf((ty - lastAccelYG)*5.0f, -3.0f, 3.0f) : 0.0f;
  lastAccelYG = ty;
  float rollSpd = mpu_ready ? rollRate*0.015f : 0.0f;

  if(!sinit){for(int i=0;i<20;i++)sp[i]=random(628)/100.0f;sinit=true;}
  gr+=dt*(0.5f + rollSpd); if(gr>6.283f)gr-=6.283f; if(gr<0)gr+=6.283f;
  
  if (fabs(jerk) > 1.5f) {
     htState.headImpactIntensity = max(htState.headImpactIntensity, fabs(jerk)*0.8f);
     htState.tailImpactIntensity = max(htState.tailImpactIntensity, fabs(jerk)*0.8f);
     for(int s=0;s<5;s++) {
        int pp=HEAD_LENGTH+random(STAFF_LENGTH);
        leds[pp] = CRGB::White;
     }
  }
  for(int i=0;i<STAFF_LENGTH;i++){
    float pos=(float)i/STAFF_LENGTH,s=sin(pos*15+gr)*0.5f+0.5f;
    leds[HEAD_LENGTH+i]=CHSV(200+(uint8_t)(s*40),220,30+(uint8_t)(s*80));
  }
  for(int s=0;s<20;s++){
    sp[s]+=dt*(1.5f+(s%3)*0.5f); if(sp[s]>6.283f)sp[s]-=6.283f;
    float tw=sin(sp[s])*0.5f+0.5f;
    if(tw>0.7f){int pp=HEAD_LENGTH+(s*STAFF_LENGTH/20);if(pp<HEAD_LENGTH+STAFF_LENGTH)leds[pp]=CHSV(0,0,(uint8_t)(tw*255));}
  }
  float centerPos = STAFF_LENGTH/2 + ty*(STAFF_LENGTH/2.5f);
  int cs=HEAD_LENGTH+(int)centerPos-8, ce=HEAD_LENGTH+(int)centerPos+8;
  for(int i=cs;i<=ce;i++){
    if(i>=HEAD_LENGTH&&i<HEAD_LENGTH+STAFF_LENGTH){
      float d=fabs((float)(i-HEAD_LENGTH-centerPos)/8.0f);
      leds[i]+=CHSV(220,200,(uint8_t)((1-d)*200));
    }
  }
  finishEffect(dt,0.5f,0, 200,220,220, 210,200,200);
}
