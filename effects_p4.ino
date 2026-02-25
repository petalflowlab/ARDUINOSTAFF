
void renderOceanBreezeEffect(float dt) {
  static float p1=0,p2=0,p3=0;
  float sm=globalSpeed/128.0f;
  int ci=STAFF_LENGTH/2;
  p1+=dt*0.4f*sm; if(p1>6.283f)p1-=6.283f;
  p2+=dt*0.6f*sm; if(p2>6.283f)p2-=6.283f;
  p3+=dt*0.3f*sm; if(p3>6.283f)p3-=6.283f;
  for(int i=0;i<STAFF_LENGTH;i++){
    float d=fabs((float)i-ci)/ci;
    float w1=sin(d*8-p1)*0.5f+0.5f,w2=sin(d*12+p2)*0.5f+0.5f,w3=sin(d*5-p3*0.7f)*0.5f+0.5f;
    uint8_t h=(uint8_t)(140+w1*60);
    uint8_t s=180+(uint8_t)(w2*60);
    uint8_t v=(140-(uint8_t)(d*40))+(uint8_t)(w1*w2*w3*80);
    leds[HEAD_LENGTH+i]=CHSV(h,s,v);
  }
  finishEffect(dt,0.5f,0.1f, 160,200,200, 180,220,200);
}
void renderSunsetFadeEffect(float dt) {
  static float sp=0;
  float sm=globalSpeed/128.0f;
  int ci=STAFF_LENGTH/2;
  sp+=dt*0.2f*sm; if(sp>6.283f)sp-=6.283f;
  for(int i=0;i<STAFF_LENGTH;i++){
    float d=fabs((float)i-ci)/ci;
    float hp=d+sin(sp+d*2)*0.15f;
    float hue;
    if(hp<0.4f) hue=hp*15;
    else if(hp<0.7f) hue=180+(hp-0.4f)*60;
    else hue=198+(hp-0.7f)*80;
    float br=sin(sp*0.5f+d*1.5f)*0.2f+0.8f;
    uint8_t s=220-(uint8_t)(d*60), v=(uint8_t)((180-d*60)*br);
    leds[HEAD_LENGTH+i]=CHSV((uint8_t)hue,s,v);
  }
  finishEffect(dt,0.5f,0, 10,255,200, 200,200,180);
}
void renderForestMistEffect(float dt) {
  static float mp1=0,mp2=0,cc[144]={0};
  float sm=globalSpeed/128.0f;
  int ci=STAFF_LENGTH/2;
  mp1+=dt*0.5f*sm; if(mp1>6.283f)mp1-=6.283f;
  mp2+=dt*0.35f*sm; if(mp2>6.283f)mp2-=6.283f;
  for(int i=0;i<STAFF_LENGTH;i++){
    float d=fabs((float)i-ci)/ci;
    float f1=sin(d*10-mp1)*0.5f+0.5f,f2=sin(d*12+mp2)*0.5f+0.5f;
    float col=f1*f2*4; if(col>1)col=1;
    cc[i]=cc[i]*0.9f+col*0.1f;
    float hue=(cc[i]>0.5f)?(60+cc[i]*100):(85+(160-85)*((f1+f2)/2));
    uint8_t s=200-(uint8_t)(cc[i]*80);
    uint8_t v=(120-(uint8_t)(d*40))+(uint8_t)((f1+f2)*50)+(uint8_t)(cc[i]*80);
    leds[HEAD_LENGTH+i]=CHSV((uint8_t)hue,s,v);
  }
  finishEffect(dt,0.5f,0, 85,220,200, 100,200,180);
}
void renderAuroraDreamsEffect(float dt) {
  static float dp=0,wp[3]={0,1.5f,3.0f};
  float sm=globalSpeed/128.0f;
  int ci=STAFF_LENGTH/2;
  dp+=dt*0.4f*sm; if(dp>6.283f)dp-=6.283f;
  for(int i=0;i<STAFF_LENGTH;i++){
    float d=fabs((float)i-ci)/ci;
    float c1=sin(d*12-wp[0]-dp)*0.5f+0.5f;
    float c2=sin(d*8+wp[1]+dp*0.8f)*0.5f+0.5f;
    float c3=sin(d*16-wp[2]-dp*1.2f)*0.5f+0.5f;
    float hue=85+d*100+((c1+c2+c3)/3)*30;
    float inter=c1*c2+c2*c3;
    uint8_t s=180+(uint8_t)(inter*70);
    uint8_t v=(100-(uint8_t)(d*30))+(uint8_t)((c1+c2+c3)*50);
    leds[HEAD_LENGTH+i]=CHSV((uint8_t)hue,s,v);
  }
  finishEffect(dt,0.5f,0, 120,200,200, 200,180,180);
}
void renderLavaFlowEffect(float dt) {
  static float lp=0,bp2=0;
  float sm=globalSpeed/128.0f;
  int ci=STAFF_LENGTH/2;
  lp+=dt*0.3f*sm; if(lp>6.283f)lp-=6.283f;
  bp2+=dt*2*sm; if(bp2>6.283f)bp2-=6.283f;
  for(int i=0;i<STAFF_LENGTH;i++){
    float d=fabs((float)i-ci)/ci;
    float f1=sin(d*6-lp)*0.5f+0.5f,f2=sin(d*10-lp*0.7f)*0.5f+0.5f;
    float bfreq=40-d*20;
    float bub=sin(d*bfreq+bp2)*0.5f+0.5f;
    bub=(bub>0.8f)?(bub-0.8f)*5:0;
    float hue=(bub>0.1f)?30:(d*20+(f1*f2)*10);
    uint8_t s=255-(uint8_t)(bub*100);
    uint8_t v=(100-(uint8_t)(d*30))+(uint8_t)((f1+f2)*60)+(uint8_t)(bub*100);
    leds[HEAD_LENGTH+i]=CHSV((uint8_t)hue,s,v);
  }
  finishEffect(dt,0.5f,0, 0,255,220, 20,255,200);
}
void renderSmokeyCloudstormEffect(float dt) {
  static float cloudPhase=0, turbulence=0;
  static uint32_t lastLightning=0;
  uint32_t now=millis();
  float sm=globalSpeed/128.0f;
  int ci=STAFF_LENGTH/2;
  
  cloudPhase+=dt*0.4f*sm; if(cloudPhase>6.283f)cloudPhase-=6.283f;
  turbulence+=dt*1.5f*sm; if(turbulence>6.283f)turbulence-=6.283f;
  
  for(int i=0;i<STAFF_LENGTH;i++){
    float d=fabs((float)i-ci)/(float)ci;
    
    float cloud1=sin(d*4-cloudPhase)*0.5f+0.5f;
    float cloud2=sin(d*7+cloudPhase*0.7f)*0.5f+0.5f;
    float turb=sin(d*15+turbulence)*0.3f+0.7f;
    
    uint8_t hue=180+(uint8_t)(cloud1*20);
    uint8_t sat=20+(uint8_t)(cloud2*30);
    uint8_t val=(60-(uint8_t)(d*20))+(uint8_t)((cloud1*cloud2*turb)*100);
    
    leds[HEAD_LENGTH+i]=CHSV(hue,sat,val);
  }
  
  if((now-lastLightning)>2000 && random(100)<5){
    lastLightning=now;
    htState.centerImpactIntensity=1.5f;
    htState.headImpactIntensity=1.0f;
    htState.tailImpactIntensity=1.0f;
  }
  
  if((now-lastLightning)<150){
    for(int i=-8;i<=8;i++){
      int idx=ci+i;
      if(idx>=0&&idx<STAFF_LENGTH){
        uint8_t flash=(uint8_t)((1.0f-(now-lastLightning)/150.0f)*255);
        leds[HEAD_LENGTH+idx]+=CHSV(0,0,flash);
      }
    }
  }
  
  finishEffectC(dt,0.3f,0, 180,50,180, 190,50,180, 200,60,0.3f);
}
void renderDandelionSeedsEffect(float dt) {
  static float seeds[20];
  static float seedAges[20];
  static bool seedsInit=false;
  float sm=globalSpeed/128.0f;
  int ci=STAFF_LENGTH/2;
  
  if(!seedsInit){
    for(int i=0;i<20;i++){seeds[i]=0;seedAges[i]=random(100)/100.0f;}
    seedsInit=true;
  }
  
  fadeToBlackBy(leds+HEAD_LENGTH,STAFF_LENGTH,40);
  for(int i=0;i<STAFF_LENGTH;i++){
    float d=fabs((float)i-ci)/(float)ci;
    uint8_t val=(uint8_t)((100-d*60)*0.6f);
    leds[HEAD_LENGTH+i]=CHSV(50,180,val);
  }
  
  for(int s=0;s<20;s++){
    seeds[s]+=dt*20*sm*(0.5f+seedAges[s]*0.5f);
    seedAges[s]+=dt*0.3f;
    
    if(seeds[s]>ci){
      seeds[s]=0;
      seedAges[s]=random(100)/100.0f;
    }
    
    int pos1=ci+(int)seeds[s];
    int pos2=ci-(int)seeds[s];
    
    if(pos1>=0&&pos1<STAFF_LENGTH){
      float fade=1.0f-(seeds[s]/ci);
      uint8_t brightness=(uint8_t)(fade*200);
      leds[HEAD_LENGTH+pos1]+=CHSV(40,100,brightness);
    }
    if(pos2>=0&&pos2<STAFF_LENGTH){
      float fade=1.0f-(seeds[s]/ci);
      uint8_t brightness=(uint8_t)(fade*200);
      leds[HEAD_LENGTH+pos2]+=CHSV(40,100,brightness);
    }
  }
  
  finishEffectC(dt,0.6f,0, 50,200,200, 45,180,200, 48,150,0.4f);
}
void renderTulipBouquetEffect(float dt) {
  static float swayPhase=0;
  static uint8_t tulipHues[10]={0,5,224,230,42,238,3,245,40,250};
  float sm=globalSpeed/128.0f;
  int ci=STAFF_LENGTH/2;
  
  swayPhase+=dt*0.8f*sm; if(swayPhase>6.283f)swayPhase-=6.283f;
  
  for(int i=0;i<STAFF_LENGTH;i++){
    float d=fabs((float)i-ci)/(float)ci;
    if(d>0.3f){
      uint8_t stemVal=(uint8_t)((d-0.3f)*200);
      leds[HEAD_LENGTH+i]=CHSV(90,220,stemVal);
    }
  }
  
  for(int t=0;t<10;t++){
    float offset=((t-5)/5.0f)*20;
    float sway=sin(swayPhase+t*0.5f)*3;
    int pos=ci+(int)(offset+sway);
    
    if(pos>=0&&pos<STAFF_LENGTH){
      uint8_t hue=tulipHues[t];
      uint8_t sat=240;
      uint8_t val=180+(uint8_t)(sin(swayPhase+t)*40);
      
      for(int n=-2;n<=2;n++){
        int bloomPos=pos+n;
        if(bloomPos>=0&&bloomPos<STAFF_LENGTH){
          uint8_t bloomVal=(uint8_t)(val*(1.0f-abs(n)*0.3f));
          leds[HEAD_LENGTH+bloomPos]=blend(leds[HEAD_LENGTH+bloomPos],CHSV(hue,sat,bloomVal),150);
        }
      }
    }
  }
  
  finishEffectC(dt,0.4f,0, 0,255,220, 94,255,220, 110,200,0.5f);
}
void renderSpaceRocketEffect(float dt) {
  static float rocketPos=0;
  static float exhaustTrail[30];
  static bool rocketDirection=true;
  static uint32_t lastLaunch=0;
  uint32_t now=millis();
  float sm=globalSpeed/128.0f;
  int ci=STAFF_LENGTH/2;
  
  fadeToBlackBy(leds+HEAD_LENGTH,STAFF_LENGTH,25);
  for(int i=0;i<STAFF_LENGTH;i++){
    if(random(100)<2){
      leds[HEAD_LENGTH+i]=CHSV(0,0,random(100,200));
    }
  }
  
  if((now-lastLaunch)>3000){
    rocketPos=0;
    rocketDirection=true;
    lastLaunch=now;
    htState.centerImpactIntensity=1.2f;
    for(int i=0;i<30;i++)exhaustTrail[i]=0;
  }
  
  if(rocketDirection){
    rocketPos+=dt*80*sm;
    if(rocketPos>ci-5){
      rocketDirection=false;
    }
  }else{
    rocketPos-=dt*60*sm;
    if(rocketPos<0)rocketPos=0;
  }
  
  for(int i=29;i>0;i--){
    exhaustTrail[i]=exhaustTrail[i-1]*0.92f;
  }
  exhaustTrail[0]=rocketPos;
  
  for(int t=0;t<30;t++){
    if(exhaustTrail[t]>0){
      int pos1=ci+(int)exhaustTrail[t];
      int pos2=ci-(int)exhaustTrail[t];
      uint8_t fade=(uint8_t)((1.0f-t/30.0f)*150);
      uint8_t hue=10+(t*3);
      
      if(pos1>=0&&pos1<STAFF_LENGTH)leds[HEAD_LENGTH+pos1]+=CHSV(hue,255,fade);
      if(pos2>=0&&pos2<STAFF_LENGTH)leds[HEAD_LENGTH+pos2]+=CHSV(hue,255,fade);
    }
  }
  
  int rPos1=ci+(int)rocketPos;
  int rPos2=ci-(int)rocketPos;
  if(rPos1>=0&&rPos1<STAFF_LENGTH)leds[HEAD_LENGTH+rPos1]=CHSV(0,0,255);
  if(rPos2>=0&&rPos2<STAFF_LENGTH)leds[HEAD_LENGTH+rPos2]=CHSV(0,0,255);
  
  finishEffectC(dt,0.5f,0, 200,255,200, 180,255,200, 180,255,0.6f);
}
void renderPumpkinPatchEffect(float dt) {
  static float glowPhase=0;
  static uint8_t pumpkinPositions[8];
  static bool pumpkinsInit=false;
  float sm=globalSpeed/128.0f;
  int ci=STAFF_LENGTH/2;
  
  if(!pumpkinsInit){
    for(int i=0;i<8;i++)pumpkinPositions[i]=ci-30+i*8+random(5);
    pumpkinsInit=true;
  }
  
  glowPhase+=dt*1.2f*sm; if(glowPhase>6.283f)glowPhase-=6.283f;
  
  for(int i=0;i<STAFF_LENGTH;i++){
    float d=fabs((float)i-ci)/(float)ci;
    uint8_t val=(uint8_t)(30-d*10);
    leds[HEAD_LENGTH+i]=CHSV(80,200,val);
  }
  
  for(int p=0;p<8;p++){
    int pos=pumpkinPositions[p];
    if(pos>=0&&pos<STAFF_LENGTH){
      float glow=sin(glowPhase+p*0.8f)*0.3f+0.7f;
      
      for(int w=-1;w<=1;w++){
        int pPos=pos+w;
        if(pPos>=0&&pPos<STAFF_LENGTH){
          uint8_t brightness=(uint8_t)(glow*200*(1.0f-abs(w)*0.3f));
          leds[HEAD_LENGTH+pPos]=blend(leds[HEAD_LENGTH+pPos],CHSV(20,255,brightness),200);
          
          if(abs(w)==0 && random(100)<15){
            leds[HEAD_LENGTH+pPos]+=CHSV(30,200,80);
          }
        }
      }
    }
  }
  
  finishEffectC(dt,0.7f,0, 20,255,220, 25,255,220, 30,255,0.8f);
}
