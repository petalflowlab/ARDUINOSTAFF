
float getFruitySwingIntensity() {
  if(!mpu_ready)return 0;
  float gp=sqrt(gyro[0]*gyro[0]+gyro[1]*gyro[1]+gyro[2]*gyro[2]);
  return clampf(gp/200.0f,0,1);
}
float getFruityAccelImpact() {
  if(!mpu_ready)return 0;
  float am=sqrt(accel[0]*accel[0]+accel[1]*accel[1]+accel[2]*accel[2]);
  return clampf((am-1.0f)*0.5f,0,1);
}
void renderWatermelonEffect(float dt) {
  uint32_t now=millis();
  float sm=globalSpeed/128.0f;
  int ci=STAFF_LENGTH/2;
  float sw=getFruitySwingIntensity(),ah=getFruityAccelImpact();
  if(ah>0.3f){htState.headImpactIntensity=max(htState.headImpactIntensity,ah*1.2f);htState.tailImpactIntensity=max(htState.tailImpactIntensity,ah*1.2f);}
  static const int so[]={5,12,20,28,35,42,50,57,64}; static const int ns=9;
  static float fp=0; fp+=dt*1.2f*sm; if(fp>6.283f)fp-=6.283f;
  for(int i=0;i<STAFF_LENGTH;i++){
    float d=fabs((float)i-ci)/ci;
    float hue=252-d*8,sat=230+d*25;
    float br=(200-d*60)*(sin(fp+d*3)*0.15f+0.85f);
    br=clampf(br+sw*50,0,255);
    leds[HEAD_LENGTH+i]=CHSV((uint8_t)hue,(uint8_t)sat,(uint8_t)br);
    for(int s=0;s<ns;s++){
      int sl=ci-so[s],sr=ci+so[s];
      if(abs(i-sl)<=1||abs(i-sr)<=1){
        float sd=min(abs(i-sl),abs(i-sr))/2.0f;
        leds[HEAD_LENGTH+i]=blend(leds[HEAD_LENGTH+i],CHSV(85,200,15),(uint8_t)((1-sd)*240));
      }
    }
    if(d>0.88f){float rf=(d-0.88f)/0.12f;leds[HEAD_LENGTH+i]=blend(leds[HEAD_LENGTH+i],CHSV(85+(uint8_t)(sin(now*0.001f+i*0.3f)*10),220,80+(uint8_t)(rf*80)),(uint8_t)(rf*220));}
  }
  if(!customColorMode){headHue=90;headSat=220;headVal=180;tailHue=88;tailSat=215;tailVal=160;if(sw>0.2f){headHue=(uint8_t)(90-sw*60);headSat=230;headVal=(uint8_t)(180+sw*75);tailHue=headHue;tailVal=headVal-20;}}
  updateHeadTailReactivity(dt,0.5f,0);
  renderHeadAndTail();
}
void renderCitrusBurstEffect(float dt) {
  uint32_t now=millis();
  float sm=globalSpeed/128.0f;
  int ci=STAFF_LENGTH/2;
  float sw=getFruitySwingIntensity(),ah=getFruityAccelImpact();
  static float sgp=0,glp=0;
  sgp+=dt*0.8f*sm; if(sgp>6.283f)sgp-=6.283f;
  glp+=dt*2*sm; if(glp>6.283f)glp-=6.283f;
  if(ah>0.4f){htState.headImpactIntensity=max(htState.headImpactIntensity,ah*1.5f);htState.tailImpactIntensity=max(htState.tailImpactIntensity,ah*1.5f);}
  for(int i=0;i<STAFF_LENGTH;i++){
    float d=fabs((float)i-ci)/ci;
    int si=(int)(d*ci/8); bool even=(si%2==0);
    float sb=sin(glp+si*0.8f)*0.2f+0.8f;
    float hue,sat,val;
    if(even){hue=15+sin(sgp+d*2)*5;sat=255;val=180*sb+d*30;}
    else{hue=30+sin(sgp*1.2f)*4;sat=220;val=140*sb;}
    if(d<0.1f){hue=40;sat=200;val=240;}
    float sf=fmod(d*ci,8.0f);
    if(sf<1||sf>7){val=clampf(val+60,0,255);sat=clampf(sat-80,100,255);}
    val=clampf(val+sw*40,0,255);
    leds[HEAD_LENGTH+i]=CHSV((uint8_t)hue,(uint8_t)sat,(uint8_t)val);
  }
  if(!customColorMode){headHue=12;headSat=255;headVal=(uint8_t)(160+sw*80);tailHue=18;tailSat=240;tailVal=(uint8_t)(140+sw*80);if(ah>0.3f){headHue=35;headVal=255;}}
  updateHeadTailReactivity(dt,0.5f,0);
  renderHeadAndTail();
}
void renderBerryBlastEffect(float dt) {
  uint32_t now=millis();
  float sm=globalSpeed/128.0f;
  int ci=STAFF_LENGTH/2;
  float sw=getFruitySwingIntensity(),ah=getFruityAccelImpact();
  #define MAX_BERRIES 8
  static float bp2[MAX_BERRIES]={0},bi2[MAX_BERRIES]={0},br2[MAX_BERRIES]={0};
  static uint8_t bh2[MAX_BERRIES]={0};
  static float base=0; base+=dt*1.5f*sm; if(base>6.283f)base-=6.283f;
  if(sw>0.3f||ah>0.3f){
    for(int b=0;b<MAX_BERRIES;b++)if(bi2[b]<0.1f){
      bp2[b]=clampf((float)(random(40)-20)/ci,-0.3f,0.3f);
      bi2[b]=0.7f+sw*0.5f; bh2[b]=190+random(50); br2[b]=0.05f; break;
    }
    if(ah>0.4f){htState.headImpactIntensity=max(htState.headImpactIntensity,ah*1.3f);htState.tailImpactIntensity=max(htState.tailImpactIntensity,ah*1.3f);}
  }
  for(int b=0;b<MAX_BERRIES;b++)if(bi2[b]>0.05f){br2[b]+=dt*0.4f*sm;bi2[b]*=0.94f;}
  for(int i=0;i<STAFF_LENGTH;i++){
    float d=fabs((float)i-ci)/ci;
    float bw=sin(base-d*8)*0.3f+0.7f;
    leds[HEAD_LENGTH+i]=CHSV(200+(uint8_t)(d*40),240,(uint8_t)((80-d*40)*bw));
  }
  for(int b=0;b<MAX_BERRIES;b++){
    if(bi2[b]<0.05f)continue;
    float bc=(float)ci+bp2[b]*ci;
    int bci=(int)bc,bri=(int)(br2[b]*STAFF_LENGTH);
    if(bri<2)bri=2;
    for(int i=max(0,bci-bri);i<=min(STAFF_LENGTH-1,bci+bri);i++){
      float d=fabs((float)i-bc)/bri,ints=(1-d*d)*bi2[b];
      leds[HEAD_LENGTH+i]+=CHSV(bh2[b],240-(uint8_t)(ints*80),(uint8_t)(ints*220));
    }
  }
  if(sw>0.1f&&random(100)<(int)(sw*40)){int so=(random(2)?1:-1)*random(10,ci);int si=ci+so;if(si>=0&&si<STAFF_LENGTH){leds[HEAD_LENGTH+si]=CHSV(210+random(30),200,220);leds[HEAD_LENGTH+STAFF_LENGTH-1-si]=leds[HEAD_LENGTH+si];}}
  if(!customColorMode){headHue=205;headSat=240;headVal=(uint8_t)(160+sw*80);tailHue=215;tailSat=230;tailVal=(uint8_t)(140+sw*80);if(ah>0.3f){headHue=230;headVal=255;tailHue=220;tailVal=240;}}
  updateHeadTailReactivity(dt,0.5f,0);
  renderHeadAndTail();
}
void renderMangoSwirlEffect(float dt) {
  uint32_t now=millis();
  float sm=globalSpeed/128.0f;
  int ci=STAFF_LENGTH/2;
  float sw=getFruitySwingIntensity(),ah=getFruityAccelImpact();
  static float sph=0,rph=0;
  sph+=dt*(1+sw*4)*sm; if(sph>6.283f)sph-=6.283f;
  rph+=dt*3*sm; if(rph>6.283f)rph-=6.283f;
  if(ah>0.3f){htState.headImpactIntensity=max(htState.headImpactIntensity,ah*1.4f);htState.tailImpactIntensity=max(htState.tailImpactIntensity,ah*1.4f);}
  for(int i=0;i<STAFF_LENGTH;i++){
    float d=fabs((float)i-ci)/ci;
    float s1=sin(d*20-sph)*0.5f+0.5f,s2=sin(d*14+sph*0.7f)*0.5f+0.5f,s3=sin(d*8-sph*1.3f)*0.5f+0.5f;
    float sw2=(s1+s2+s3)/3;
    float hue=30-d*12+sw2*8,sat=255-d*30,val=(200-d*50)*(0.7f+sw2*0.4f);
    if(d<0.08f){hue=35;sat=220;val=255;}
    if(sw>0.15f){float rip=sin(d*25-rph)*0.5f+0.5f;val=clampf(val+rip*sw*80,0,255);}
    leds[HEAD_LENGTH+i]=CHSV((uint8_t)hue,(uint8_t)sat,(uint8_t)val);
  }
  if(!customColorMode){headHue=(uint8_t)(22-sw*8);headSat=255;headVal=(uint8_t)(170+sw*80);tailHue=(uint8_t)(18-sw*6);tailSat=250;tailVal=(uint8_t)(150+sw*80);}
  updateHeadTailReactivity(dt,0.5f,0);
  renderHeadAndTail();
}
void renderKiwiSparkEffect(float dt) {
  uint32_t now=millis();
  float sm=globalSpeed/128.0f;
  int ci=STAFF_LENGTH/2;
  float sw=getFruitySwingIntensity(),ah=getFruityAccelImpact();
  static float kp=0,skp=0;
  kp+=dt*0.6f*sm; if(kp>6.283f)kp-=6.283f;
  skp+=dt*4*sm; if(skp>6.283f)skp-=6.283f;
  if(ah>0.35f){htState.headImpactIntensity=max(htState.headImpactIntensity,ah*1.3f);htState.tailImpactIntensity=max(htState.tailImpactIntensity,ah*1.3f);}
  static const int kso[]={4,9,14,20,26,33,40,48,56}; static const int nks=9;
  for(int i=0;i<STAFF_LENGTH;i++){
    float d=fabs((float)i-ci)/ci;
    float fw=sin(d*12-kp)*0.2f+0.8f;
    float hue=80+d*15,sat=220+d*20,val=(160-d*50)*fw;
    if(d<0.07f){hue=50;sat=150;val=240;}
    float spw=sin(d*30-skp)*0.5f+0.5f;
    if(sw>0.2f&&spw>0.8f){hue=90+random(20);sat=255;val=clampf(val+sw*120,0,255);}
    leds[HEAD_LENGTH+i]=CHSV((uint8_t)hue,(uint8_t)sat,(uint8_t)val);
    for(int s=0;s<nks;s++){
      int kl=ci-kso[s],kr=ci+kso[s];
      if(abs(i-kl)<=1||abs(i-kr)<=1){float sd=(float)min(abs(i-kl),abs(i-kr))/2.0f;leds[HEAD_LENGTH+i]=blend(leds[HEAD_LENGTH+i],CHSV(25,180,18),(uint8_t)((1-sd)*220));}
    }
    if(d>0.90f){float sf=(d-0.90f)/0.10f;leds[HEAD_LENGTH+i]=blend(leds[HEAD_LENGTH+i],CHSV(20,200,60),(uint8_t)(sf*200));}
  }
  if(!customColorMode){headHue=20;headSat=200;headVal=(uint8_t)(120+sw*100);tailHue=22;tailSat=195;tailVal=(uint8_t)(100+sw*100);if(sw>0.3f){headHue=(uint8_t)(80-sw*30);headSat=255;headVal=(uint8_t)(160+sw*90);tailHue=headHue+5;tailVal=headVal-20;}}
  updateHeadTailReactivity(dt,0.5f,0);
  renderHeadAndTail();
}
