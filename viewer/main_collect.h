#ifndef GEVB_COLLECT_OLD
#define GEVB_COLLECT_OLD 0   // -DGEVB_COLLECT_OLD=1 — билборды сборного и кольца выключателей в список GE, как раньше
#endif
// ---------- В5: карточки, осы, гэги уровня (viewer/collect.c — логика, общая с ботами; пакет collect-l1.shr рядом с EBOOT,
// tools/collect_export.py). Здесь — то, что видит и слышит игрок: модели SPR1 пакета (карточка крутится, оса летает,
// сцены гэгов — в городе или в своей комнате), блёстки ненайденных гэгов, звук (world.spt, звуки гэгов потоком),
// сообщения HUD из Scrooby (ItemCollected: «COLLECTOR CARD!», название, 3/7; WaspsDestroyed), ролик гэга (movie.c)
// и тряска камеры (GagSetCameraShake). Правила порта — viewer/collect.h и строки ниже.
static SceUID collStreamFd=-1;
static int collReadAt(void *user,unsigned off,void *data,unsigned size) {
  int fd=*(int *)user;return sceIoLseek32(fd,off,PSP_SEEK_SET)!=(int)off || sceIoRead(fd,data,size)!=(int)size;
}
// ТЕСТ90 П1 (SYNCREAD_P1_OLD / файл syncreadp1old / flag syncreadp1old 1): гэги читаются фоном по второму дескриптору файла пакета; кадр только запускает и принимает готовое
static SceUID collAsyncFd=-1;   // syncReadP1Old — в main_rooms.h (подключён раньше)
static unsigned collAsyncWant;
static int collAsyncStart(void *user,unsigned off,void *data,unsigned size){
  (void)user; if(collAsyncFd<0)return -1;
  if(sceIoLseek32(collAsyncFd,off,PSP_SEEK_SET)!=(int)off)return -1;
  collAsyncWant=size; return sceIoReadAsync(collAsyncFd,data,size)<0?-1:0;
}
static int collAsyncPoll(void *user){   // 1 — прочитано целиком, 0 — идёт, -1 — ошибка
  (void)user; SceInt64 res=-1; int r=sceIoPollAsync(collAsyncFd,&res);
  if(r>0)return 0; if(r<0)return -1; return res==(SceInt64)collAsyncWant?1:-1;
}
static void collAsyncCancel(void *user){(void)user; SceInt64 res; if(collAsyncFd<0)return; sceIoCancel(collAsyncFd); sceIoWaitAsync(collAsyncFd,&res);}   // как load_fuse_io.c: отмена, потом ожидание конца (после отмены оно не вечное)
static unsigned collAsyncNow(void *user){(void)user; return sceKernelGetSystemTimeLow()/1000u;}
static const CollectAsync collAsyncTab={collAsyncStart,collAsyncPoll,collAsyncCancel,collAsyncNow,0};
static void *collPak;   // Collect coll, haveColl — выше (правка 338: камера видит гэги)
enum { STREET_SOURCE_NONE,STREET_SOURCE_PED,STREET_SOURCE_NPC,STREET_SOURCE_DRIVER };
static int streetSource,streetSlot;static char streetWho[24];
static void streetSourceBind(const TrEvent *e,int said) {
  if(said!=1)return;
  streetSlot=e->index;
  streetSource=e->type==TR_EV_CAR_HIT?STREET_SOURCE_DRIVER:e->index<0?STREET_SOURCE_NPC:STREET_SOURCE_PED;
  const char *name=e->who;
  if(streetSource==STREET_SOURCE_DRIVER && streetSlot>=0 && streetSlot<TR_MAXCAR)name=traffic.model[traffic.car[streetSlot].model].name;
  snprintf(streetWho,sizeof streetWho,"%s",name);
  if(smkOn){char b[160];snprintf(b,sizeof b,"STREETPOS source=%d slot=%d who=%s at=%.2f,%.2f,%.2f t=%.1f\n",streetSource,streetSlot,streetWho,e->x,e->y,e->z,smkT);smkWrite(b);}
}
static void streetSourceFrame(void) {
  if(!streetSource)return;
  float p[3];int found=0;
  if(streetSource==STREET_SOURCE_NPC)found=mvNpcPosition(streetWho,p);
  else if(haveTraffic&&streetSource==STREET_SOURCE_PED&&streetSlot>=0&&streetSlot<TR_MAXPED){
    const TrPed *a=&traffic.ped[streetSlot];
    if(a->mode!=TRP_OFF&&a->model>=0&&a->model<(int)traffic.hd->nPed&&!strcmp(streetWho,traffic.pedm[a->model].name)){
      p[0]=a->x;p[1]=a->y;p[2]=a->z;found=1;
    }
  } else if(haveTraffic&&streetSource==STREET_SOURCE_DRIVER&&streetSlot>=0&&streetSlot<TR_MAXCAR){
    const TrCar *a=&traffic.car[streetSlot];
    if(a->mode!=TRM_OFF&&!a->car.destroyed&&a->model>=0&&a->model<(int)traffic.hd->nModel&&!strcmp(streetWho,traffic.model[a->model].name)){
      p[0]=a->car.x;p[1]=a->car.y;p[2]=a->car.z;found=1;
    }
  }
  if(!found){sndStreetStop();streetSource=0;}
  else if(!sndStreetMove(p[0],p[1],p[2]))streetSource=0;
}
// №561: гудок машин трафика (TR_EV_CAR_HONK) — звучит ограниченное время (~1,4 с),
// привязан к позиции машины трафика и прекращается при исчезновении/уничтожении/удалении (> 50 м).
#ifndef HORN_STOP_OLD
#define HORN_STOP_OLD 0
#endif
static int hornStopOld = HORN_STOP_OLD;
void trafficHornStopOld(int on) { hornStopOld = on != 0; }

// Сверка g11b, P10_звук_машины_и_пространство P10-4 (гудок трафика): по событию «машину ударили / мешают» — шанс 0,5; если у машины уже идёт серия, новый гудок ничего не делает; серия 1..3 гудков
// (число дополнительных 0, 1 или 2 поровну): гудок 0,25–0,5 с, пауза 0,25–0,5 с, ..., последний гудок (и единственный) 0,5–1 с; дальность 30–60 м (полная до 30). Клип общий (петля, обрезается таймером).
// HORN_SERIES_OLD=1 — один гудок 1,4 с без шанса, 4–60 м, новый перезапускает прежний (TEST81).
#ifndef HORN_SERIES_OLD
#define HORN_SERIES_OLD 0
#endif
typedef struct { int handle; float timer; float pause; int left; char hn[40]; } TrHornVoice;
static TrHornVoice trHorns[TR_MAXCAR];
static float hornRnd(float a, float b) { return a + (b - a) * (float)(rand() & 0x7fff) / 32768.f; }
static void trafficHornStart(int idx, const char *hn, float x, float y, float z) {   // вызывает main_loop.h на TR_EV_CAR_HONK (сверка g11b-collect)
  if (idx < 0 || idx >= TR_MAXCAR) return;
  TrHornVoice *v = &trHorns[idx];
  if (HORN_SERIES_OLD) { if (v->handle) audioStop(v->handle); v->handle = sndPlay(hn, x, y, z, 1.f); v->timer = 1.4f; v->pause = 0.f; v->left = 0; return; }
  if (v->handle || v->pause > 0.f || v->left > 0) return;   // серия идёт — новый ничего не делает
  if (rand() & 1) return;                                    // шанс 0,5 (traffic_horn)
  snprintf(v->hn, sizeof v->hn, "%s", hn);
  v->left = rand() % 3;                                      // дополнительных гудков 0, 1 или 2 поровну
  v->timer = v->left ? hornRnd(.25f, .5f) : hornRnd(.5f, 1.f); v->pause = 0.f;
  v->handle = sndPlay(hn, x, y, z, 1.f);
}

static void trafficHornFrame(float dt, float camx, float camy, float camz) {
#if !HORN_STOP_OLD
  if (hornStopOld) return;
  for (int i = 0; i < TR_MAXCAR; i++) {
    TrHornVoice *v = &trHorns[i];
    int gone = !haveTraffic || i >= traffic.maxCars || traffic.car[i].mode == TRM_OFF || traffic.car[i].car.destroyed;
    if (!v->handle && v->pause > 0.f) {   // пауза между гудками серии
      if (gone) { v->pause = 0.f; v->left = 0; continue; }
      if ((v->pause -= dt) <= 0.f) {
        v->pause = 0.f; v->left--;
        v->timer = v->left ? hornRnd(.25f, .5f) : hornRnd(.5f, 1.f);
        v->handle = sndPlay(v->hn, traffic.car[i].car.x, traffic.car[i].car.y, traffic.car[i].car.z, 1.f);
      }
      continue;
    }
    if (!v->handle) continue;
    v->timer -= dt;
    int stop = v->timer <= 0.f;
    if (gone) { stop = 1; v->left = 0; }
    float farD = HORN_SERIES_OLD ? 50.f : 60.f;
    if (!stop) {
      float dx = traffic.car[i].car.x - camx, dy = traffic.car[i].car.y - camy, dz = traffic.car[i].car.z - camz;
      if (dx * dx + dy * dy + dz * dz > farD * farD) { stop = 1; v->left = 0; }
    }
    if (stop) {
      audioStop(v->handle);
      v->handle = 0;
      if (!HORN_SERIES_OLD && v->left > 0) v->pause = hornRnd(.25f, .5f);
    } else {
      audioSetAt(v->handle, traffic.car[i].car.x, traffic.car[i].car.y, traffic.car[i].car.z, HORN_SERIES_OLD ? 4.f : 30.f, 60.f);
    }
  }
#endif
}
// №1048 (Роб 26.10 на TEST72: «есть NPC-машина, которая говорит "Vote Quimby", когда проезжает мимо — она должна это говорить»): фургон кампании Квимби (votetruc, в трафике L5, в миссиях гонки sr2) играет клип гудка
// votetruc_horn (sound/carsound/vote_quimby_03.rsd, SetLooping false, trim 0,75 в votetruc.spt) позиционно, пока она ближе VOTE_NEAR м: первый раз сразу при подъезде, дальше раз в VOTE_REPEAT с, пока рядом;
// ушла дальше VOTE_FAR м — счёт сбрасывается (следующий проезд снова с первой фразы). Громкость и затухание — как у любого позиционного звука порта (spatialPoint: полная до 4 м, затухание до 60 м), один голос на
// фургон, цена — перебор машин трафика раз в кадр. Клип уже в пакете звуков L5 (sfx.sab). VOTE1048_OLD=1 / файл vote1048old / «flag vote1048 0» — без голоса
#ifndef VOTE1048_OLD
#define VOTE1048_OLD 0
#endif
#define VOTE_NEAR 40.f
#define VOTE_FAR 60.f
#define VOTE_REPEAT 9.f
static int vote1048On = !VOTE1048_OLD; static float voteT[TR_MAXCAR]; static int voteId = -2, voteLvl = -1; static unsigned voteSaid;
static void trafficVoteFrame(float dt, float camx, float camy, float camz) {
  if (!vote1048On || !haveTraffic) return;
  if (voteLvl != gameLevel) { voteLvl = gameLevel; voteId = -2; memset(voteT, 0, sizeof voteT); }
  for (int i = 0; i < TR_MAXCAR && i < traffic.maxCars; i++) {
    const TrCar *a = &traffic.car[i];
    if ((a->mode != TRM_DRIVE && a->mode != TRM_PHYS) || a->car.destroyed || a->model < 0 || a->model >= (int)traffic.hd->nModel || strncmp(traffic.model[a->model].name, "votetruc", 8)) { voteT[i] = 0.f; continue; }
    float dx = a->car.x - camx, dy = a->car.y - camy, dz = a->car.z - camz, d2 = dx * dx + dy * dy + dz * dz;
    if (d2 > VOTE_FAR * VOTE_FAR) { voteT[i] = 0.f; continue; }
    if (d2 > VOTE_NEAR * VOTE_NEAR) continue;
    voteT[i] -= dt;
    if (voteT[i] > 0.f) continue;
    if (voteId == -2) voteId = sndId("votetruc_horn");
    voteT[i] = VOTE_REPEAT;
    if (voteId < 0) continue;
    int h_ = sndPlayId(voteId, a->car.x, a->car.y + 1.5f, a->car.z, 1.f); voteSaid++;
    if (smkOn) { float g_ = 0.f, p_ = 0.f; audioSpatial(a->car.x, a->car.y + 1.5f, a->car.z, 4.f, 60.f, &g_, &p_); char l_[160]; snprintf(l_, sizeof l_, "VOTE1048 t=%.1f car=%d dist=%.1f gain=%.2f handle=%d said=%u level=%d\n", smkT, i, sqrtf(d2), g_, h_, voteSaid, gameLevel); smkWrite(l_); }
  }
}

static void trafficHornClear(void) {
  for (int i = 0; i < TR_MAXCAR; i++) {
    if (trHorns[i].handle) { audioStop(trHorns[i].handle); trHorns[i].handle = 0; }
  }
}

// №390: moving traffic uses authored engine/idle clips and the same spatial DSP.
// Keep a stable assignment for the three nearest audible cars, without increasing mixer capacity.
#ifndef TRAFFIC_SCAN_OLD
#define TRAFFIC_SCAN_OLD 0   // круг 6 (g08): 1 — поиск машины для слота каждый кадр, даже когда все пять слотов заняты
#endif
#ifndef TRAFFIC_HEARSE_OLD
#define TRAFFIC_HEARSE_OLD 0
#endif
// Сверка g08-sound (P10-1, P10-2, P10-21, Д2; audio.c audioSvEngine): трафик — до 5 слотов по порядку появления (вблизи слышимости), один клип «ferrari»,
// высота (0,5 + 0,5·v/16,67)·множитель (круг 7: эталонная скорость трафика 60 км/ч на всех уровнях = 16,6667 м/с) машины (0,8…1,2 при появлении); машины ИИ миссий — до 5 слотов, клип «mission_ai_vehicle», высота 0,3 + 0,7·v/TopSpeed;
// стоящим тоже играет, шин нет, оверлей у трафика по модели. WORLD_ENGINE_SV_OLD=1 или нет клипов в пакете — прежние 3 источника (audio.c)
static void trafficMotorFrameSv(float dt,float camx,float camy,float camz) {
  static int slotCar[AUDIO_SV_SLOTS]={-1,-1,-1,-1,-1}; static float slotMul[AUDIO_SV_SLOTS]; static unsigned rngS=0x2545F491u;
  const float R2=125.f*125.f; int taken[TR_MAXCAR]={0};
  for(int s=0;s<AUDIO_SV_SLOTS;s++) {   // слот держит свою машину, пока она в игре и слышна
    int i=slotCar[s];
    if(i>=0) {
      int ok=haveTraffic && i<traffic.maxCars;
      if(ok){const TrCar *a=&traffic.car[i];
        ok=(a->mode==TRM_DRIVE||a->mode==TRM_PHYS) && !a->car.destroyed;
        if(ok){float dx=a->car.x-camx,dy=a->car.ry-camy,dz=a->car.z-camz;ok=dx*dx+dy*dy+dz*dz<R2;}}
      if(!ok){slotCar[s]=-1;audioSvEngine(0,s,0,-1,0,0,0,0,1,1,dt);} else taken[i]=1;
    }
  }
  int freeSlot=0; for(int s=0;s<AUDIO_SV_SLOTS;s++) freeSlot|=slotCar[s]<0;   // круг 6 (g08, цена на PSP): все пять слотов заняты — поиск новой машины по всему трафику не нужен (было: обход всех машин каждый кадр, стало: только при свободном слоте)
  if(haveTraffic && (freeSlot || TRAFFIC_SCAN_OLD)) for(int i=0;i<traffic.maxCars&&i<TR_MAXCAR;i++) {
    if(taken[i])continue;
    const TrCar *a=&traffic.car[i];
    if((a->mode!=TRM_DRIVE && a->mode!=TRM_PHYS) || a->car.destroyed || a->model<0 || a->model>=(int)traffic.hd->nModel)continue;
    if(!TRAFFIC_HEARSE_OLD && strstr(traffic.model[a->model].name,"hearse"))continue;   // круг 2 (g11b P10-31): у катафалка мотора не слышно
    float dx=a->car.x-camx,dy=a->car.ry-camy,dz=a->car.z-camz; if(dx*dx+dy*dy+dz*dz>=R2)continue;
    for(int s=0;s<AUDIO_SV_SLOTS;s++) if(slotCar[s]<0){
      rngS^=rngS<<13;rngS^=rngS>>17;rngS^=rngS<<5;
      slotCar[s]=i;slotMul[s]=.8f+.4f*((rngS>>8)*(1.f/16777216.f));break;}
  }
  for(int s=0;s<AUDIO_SV_SLOTS;s++) {
    int i=slotCar[s]; if(i<0)continue;
    const TrCar *a=&traffic.car[i];
    float v=a->mode==TRM_DRIVE?a->v:a->car.v;
    audioSvEngine(0,s,1,audioCarProfile(traffic.model[a->model].name),a->car.x,a->car.ry,a->car.z,v,16.6667f,slotMul[s],dt);
  }
  const MsnCars *mc=mvCars();
  for(int s=0;s<AUDIO_SV_SLOTS;s++) {
    const McCar *a=mc && s<mc->n && mc->c[s].on && !mc->c[s].car.destroyed ? &mc->c[s] : NULL;
    if(!a){audioSvEngine(1,s,0,-1,0,0,0,0,1,1,dt);continue;}
    audioSvEngine(1,s,1,-1,a->car.x,a->car.ry,a->car.z,a->car.v,a->car.p.topSpeed,1.f,dt);
  }
}
static void trafficMotorFrame(float dt,float camx,float camy,float camz) {
  if(audioSvReady()){trafficMotorFrameSv(dt,camx,camy,camz);return;}
  static int assigned[AUDIO_WORLD_ENGINES]={-1,-1,-1};
  enum { SOURCES=TR_MAXCAR+MC_MAX };
  int selected[AUDIO_WORLD_ENGINES]={-1,-1,-1}, profile[SOURCES];
  const Car *body[SOURCES];const char *model[SOURCES];
  float distance[SOURCES], speed[SOURCES], throttle[SOURCES], slip[SOURCES];
  const MsnCars *mc=mvCars();
  for(int i=0;i<SOURCES;i++) {
    profile[i]=-1;distance[i]=1e30f;body[i]=NULL;model[i]="-";
    if(i<TR_MAXCAR) {
      if(!haveTraffic || i>=traffic.maxCars)continue;
      const TrCar *a=&traffic.car[i];
      if((a->mode!=TRM_DRIVE && a->mode!=TRM_PHYS) || a->car.destroyed || a->model<0 || a->model>=(int)traffic.hd->nModel)continue;
      body[i]=&a->car;model[i]=traffic.model[a->model].name;
      speed[i]=a->mode==TRM_DRIVE?a->v:a->car.v;
      throttle[i]=fminf(1.f,fabsf(speed[i])/fmaxf(1.f,a->car.p.topSpeed*.5f));
      slip[i]=a->car.grounded && a->car.slip ? fminf(1.f,fabsf(a->car.vl)/6.f+.3f) : 0.f;
    } else {
#ifndef CAR_NPC_AUDIO_OLD
      int j=i-TR_MAXCAR;
      if(!mc || j>=mc->n || !mc->c[j].on || mc->c[j].mode==MC_PARKED || mc->c[j].car.destroyed)continue;
      const McCar *a=&mc->c[j];body[i]=&a->car;model[i]=a->name;
      speed[i]=a->car.v;throttle[i]=a->lastIn.throttle;
      slip[i]=skidSoundLevel(&a->car,&a->lastIn);
#else
      continue;
#endif
    }
    profile[i]=audioCarProfile(model[i]);
    // сверка g11b, P10_звук_машины_и_пространство P10-31: настройки звука катафалка (обычная и в погоне) в данных не определены — у катафалка мотора не слышно (TRAFFIC_HEARSE_OLD=1 — обычный профиль).
    // coffin_posn (клип traffic_coffin, 10–100 м) — профиль и клип в audio.c, передано g08-sound
    if(!TRAFFIC_HEARSE_OLD && strstr(model[i],"hearse"))profile[i]=-1;
    float dx=body[i]->x-camx,dy=body[i]->ry-camy,dz=body[i]->z-camz;
    distance[i]=dx*dx+dy*dy+dz*dz;
    if(profile[i]<0 || distance[i]>=60.f*60.f)distance[i]=1e30f;
  }
  for(int s=0;s<AUDIO_WORLD_ENGINES;s++) {
    int best=-1;float score=1e30f;
    for(int i=0;i<SOURCES;i++) {
      int used=0;for(int k=0;k<s;k++)used|=selected[k]==i;if(used)continue;
      float d=distance[i];
      // 10% hysteresis prevents boundary swaps while two cars pass each other.
      for(int k=0;k<AUDIO_WORLD_ENGINES;k++)if(assigned[k]==i)d*=.9f;
      if(distance[i]<1e30f && d<score){best=i;score=d;}
    }
    selected[s]=best;
  }
  int next[AUDIO_WORLD_ENGINES]={-1,-1,-1};
  // Retained actors keep their decoder and envelope even when their rank changes.
  for(int s=0;s<AUDIO_WORLD_ENGINES;s++)for(int k=0;k<AUDIO_WORLD_ENGINES;k++)
    if(assigned[s]>=0 && assigned[s]==selected[k]){next[s]=assigned[s];selected[k]=-1;break;}
  for(int s=0;s<AUDIO_WORLD_ENGINES;s++)if(next[s]<0)for(int k=0;k<AUDIO_WORLD_ENGINES;k++)
    if(selected[k]>=0){next[s]=selected[k];selected[k]=-1;break;}
  for(int s=0;s<AUDIO_WORLD_ENGINES;s++) {
    int i=next[s];
    if(smkOn && assigned[s]!=i){char b[128];snprintf(b,sizeof b,"TRAFFICMOTOR slot=%d car=%d model=%s profile=%d t=%.2f\n",s,i,
      i>=0?model[i]:"-",i>=0?profile[i]:-1,smkT);smkWrite(b);}
    assigned[s]=i;
    if(i<0)audioWorldEngine(s,-1,-1,0,0,0,0,1,0,dt);
    else {
      const Car *a=body[i];
      audioWorldEngine(s,i,profile[i],a->x,a->ry,a->z,speed[i]*3.6f,a->p.topSpeed*3.6f,throttle[i],dt);
      audioWorldSkid(s,slip[i],0);
    }
  }
}
static float cardHudT, itemsHudT, waspsHudT;   // сколько ещё показывать сообщения HUD, с
#define COLL_HUD_S 3.f                          // правило порта: сообщение держится 3 с (длительности в данных нет)
// сверка g11b (M2_движение_в_игре №7, 8, 11, 12): карточка 0,5 (вылет) + 2,0 (держится) + 0,3 (уезд) = 2,8 с; «колода собрана» и «осы убиты» — рост 0,3 + 2,0 + гашение 0,3 = 2,6 с;
// «колода собрана» начинается в фазе ухода карточки (за 0,3 с до её конца). Рост/дуга/вылет по дуге — анимация Scrooby в ui (передано); здесь длительности. COLL_HUD_OLD=1 — 3 с / 6 с / 3 с
#ifndef COLL_HUD_OLD
#define COLL_HUD_OLD 0
#endif
#define COLL_HUD_CARD_S 2.8f
#define COLL_HUD_MSG_S 2.6f
#define COLL_HUD_CARD_LEAVE_S .3f
#ifndef TUT_GAG_ZONE_OLD
#define TUT_GAG_ZONE_OLD 0   // P15-09: подсказка про интерактивный гэг — когда игрок вошёл в зону действия гэга пешком вне миссии (было: по окончании первого гэга)
#endif
static int gagZoneIn;      // P15-09: игрок сейчас в зоне действия гэга (фронт входа)
#ifndef CARD_CELEBRATE_OLD
#define CARD_CELEBRATE_OLD 0
#endif
#ifndef FMV_STOPMUSIC_OLD
#define FMV_STOPMUSIC_OLD 0   // круг 2, g07 P15-05: ролик с «stopmusic» (fmv7, L7 m8) убивает музыку насовсем (было: музыка шла под роликом молча и после него продолжалась)
#endif
static float itemsWaitT;   // сколько ждать до показа «колода собрана» (конец вылета карточки)
static int gagStreamG = -1, gagStreamT = -1;    // гэг, чей звук сейчас в потоке диалогов, и его дорожка music.sap
#if !TICKET_CONTENT_C3_OLD
static TicketView ticketView;
#define TICKET_BUSY() ticketViewBusy(&ticketView)
#define TICKET_MOVIE_AVAILABLE (ticketView.st.flags&TICKET_ADMIT)
#else
#define TICKET_BUSY() 0
#define TICKET_MOVIE_AVAILABLE 0
#endif
#ifndef WASP_SND_OLD
#define WASP_SND_OLD 0
#endif
#define WASP_SLOTS 2
static int waspSnd2[3] = {-2, -2, -2};   // wasp_charging, wasp_charged_idle, wasp_attack (сверка g11b P10-19)
static struct { int wasp, stage, last, h; } wSlot[WASP_SLOTS] = {{-1, -1, -1, 0}, {-1, -1, -1, 0}};
static int waspLoop; static int waspSnd[6] = {-2};   // петля жужжания ближайшей живой осы; id звуков world.spt
static int shakeG = -1; static float shakeT;    // тряска камеры гэга: номер гэга и время с запуска
static void collLog(const char *what, int idx, const char *name) {
  if (!smkOn) return;
  char s[200]; int tc, tw, tg, c = collectCount(&coll, coll.level, COLL_CARDS, &tc), w = collectCount(&coll, coll.level, COLL_WASPS, &tw),
                 g = collectCount(&coll, coll.level, COLL_GAGS, &tg);
  snprintf(s, sizeof s, "COLLEV %s idx=%d name=%s cards=%d/%d wasps=%d/%d gags=%d/%d coins=%d t=%.1f\n", what, idx, name, c, tc, w, tw, g, tg,
           haveCoins ? coins.collected : 0, smkT);
  smkWrite(s);
}
static void collLoad(void) {   // один раз после fxInit и sndInit: пакет целиком, текстуры, частицы EFX — к fx.shr
  char fn[256]; levelFile(fn, "collect-l%d.shr");
  SceUID f = sceIoOpen(fn, PSP_O_RDONLY, 0); if (f < 0) return;   // нет пакета — уровень без карточек, ос и гэгов
  unsigned n = (unsigned)sceIoLseek32(f, 0, PSP_SEEK_END); sceIoLseek32(f, 0, PSP_SEEK_SET);
  unsigned char *b; int e=0;
  if(!gagSkinStreamOld){
    collStreamFd=f;unsigned mf0_=collStreamAllocFails;b=collStreamLoad(&coll,collReadAt,&collStreamFd,n);
    if(!b && collRetry1 && collStreamAllocFails>mf0_){   // ТЕСТ90 память (журнал карты TEST89, L7 после L4: отказ 1,6 МБ при 10,5 МБ свободных и куске 1,25 МБ — гэги не загрузились): экран загрузки закрывает мир — далёкие, затем все клетки уходят в кучу и пакет просится ещё раз
      unsigned need_=collStreamLastFail+(64u<<10),b0_=memMaxBlock?memMaxBlock():0;int ok_=mem1060Pre(need_,"collect");b=collStreamLoad(&coll,collReadAt,&collStreamFd,n);
      if(smkOn){char l_[160];snprintf(l_,sizeof l_,"COLLRETRY level=%d need=%.2f room=%d blk=%.2f->%.2f loaded=%d t=%.1f\n",gameLevel,need_/1048576.f,ok_,b0_/1048576.f,(memMaxBlock?memMaxBlock():0)/1048576.f,b!=0,smkT);smkWrite(l_);}
    }
    if(!b){sceIoClose(f);collStreamFd=-1;smkWrite("COLL stream error=load\n");return;}
    n=coll.size;
    if(!syncReadP1Old && !SYNCREAD_P1_OLD){collAsyncFd=sceIoOpen(fn,PSP_O_RDONLY,0); if(collAsyncFd>=0)collStreamSetAsync(&collAsyncTab);}
  } else {
    b=memalign(16,n);
    if(!b || sceIoRead(f,b,n)!=(int)n){sceIoClose(f);free(b);return;}
    sceIoClose(f);e=collectInit(&coll,b,n);
  }
  if (e) { char s[96]; snprintf(s, sizeof s, "COLL error=%s\n", collectError(e)); smkWrite(s); free(b); return; }
  cxTex = calloc(coll.ntex ? coll.ntex : 1, sizeof(Tex)); cxNtex = coll.ntex;
  if (!cxTex && !LOADMEM1170_OLD) { smkWrite("COLL error=memory cxTex\n"); cxNtex = 0; if (collStreamFd >= 0) { sceIoClose(collStreamFd); collStreamFd = -1; } free(b); return; }   // №1170: без таблицы текстур карточек уровень без карточек, а не запись в NULL
  unsigned p = (unsigned)(coll.tex - b);   // текстуры — формат level.shr (заголовок 16 Б, палитра 1024 Б, уровни в swizzle)
  for (unsigned i = 0; i < coll.ntex && p < n; i++) {
    const unsigned short *t2 = (const unsigned short *)(b + p); Tex *t = &cxTex[i];
    t->w = t2[0]; t->h = t2[1]; t->levels = t2[2]; t->pre = b[p + 7] == 1; t->t4 = b[p + 6] == 4 && t->pre; t->pal = b + p + 16; p += 16 + 1024;
    int lw = t->w, lh = t->h;
    for (int l = 0; l < t->levels; l++) {
      t->lv[l] = b + p; if (t->pre) t->slv[l] = b + p;
      p += texRowBytes(t->t4, t->pre, lw) * (t->pre ? (lh + 7) & ~7 : lh); p = (p + 15) & ~15u; lw >>= 1; lh = lh > 1 ? lh >> 1 : 1;
    }
  }
  sceKernelDcacheWritebackAll();
  for (unsigned k = 0; k < spFxNameCount(&coll.sp); k++) spFxBind(&coll.sp, k, fxFind(spFxName(&coll.sp, k)));
  #ifndef GAG_SKIN_OLD
  int ns = gagSkinStreamOld ? gagSkinInit(&coll) : ((const unsigned *)coll.pak)[14];
  if (smkOn) { char log[80]; snprintf(log, sizeof log, "GAGSKIN models=%d bytes=%u\n", ns, n); smkWrite(log); }
  #endif
  collPak = b; haveColl = 1; G.pColl = &coll;
  static const char *ws[6] = {"wasp_fade_in", "wasp_idle", "wasp_lasers", "laser_miss", "wasp_destroyed", "character_zapped"};
  for (int k = 0; k < 6; k++) waspSnd[k] = sndId(ws[k]);
  char s[160]; snprintf(s, sizeof s, "COLL ok cards=%u wasps=%u gags=%u tex=%u inst=%u bytes=%u\n", coll.ncard, coll.nwasp, coll.ngag, coll.ntex, coll.sp.ninst, n);
  smkWrite(s);
}
// звук гэга: дорожка music.sap с именем ресурса (варианты — «имя~k», tools/sound_export.py), поток диалогов, по месту гэга
static void gagSound(int g) {
  const CollGag *G = &coll.gag[g];
  if (!G->sound[0]) return;
  int t = audioTrack(G->sound), nv = 0; char nm[32];
  for (int k = 1; k < 8; k++) { snprintf(nm, sizeof nm, "%s~%d", G->sound, k); if (audioTrack(nm) < 0) break; nv = k; }
  if (nv) { int k = rand() % (nv + 1); if (k) { snprintf(nm, sizeof nm, "%s~%d", G->sound, k); t = audioTrack(nm); } }   // «AddFilename» ×N: случайный, как у звуков скриптов
  if (t < 0) { collLog("gag_nosound", g, G->sound); return; }
  if (sndVoiceBusy()) { collLog("gag_voicebusy", g, G->sound); return; }   // правило порта: реплику миссии гэг не обрывает (сверить с PS2)
  audioStream(AUDIO_ST_DIALOG, t, 0.f);
  audioStreamSet(AUDIO_ST_DIALOG, 1.f, 0.f);
  audioStreamAt(AUDIO_ST_DIALOG, G->x, G->y + 1.f, G->z, 4.f, 60.f);
  gagStreamG = g; gagStreamT = t;
  collLog("gag_sound", g, G->sound);
}
// ---- настройки игрока (settings.bin, ниже cfgLoad / cfgSave). ver 2 (29.09): + tutorial (строка Tutorial в Settings паузы) и tutSeen — показанные подсказки Барта последней игры (сохранение
// игры их не несёт: RESUME / LOAD GAME не повторяет уже показанные — правило порта; NEW GAME показывает все заново)
// movies — ролики VIEW MOVIES, которые игрок уже видел (бит k — MOVIES[k]; правило порта: ролик открыт, когда показан в игре;
// GAME INTRODUCTION — всегда: он идёт при каждом запуске)
// ver 3 (01.10, правка 144): + colour — насыщенность мира в сотых (строка Colour в Settings паузы, viewer/tone.h)
// + toon (№428, 16.10): строка Cartoon в Settings паузы (контурный режим персонажей и машины игрока; по умолчанию Off; в старых файлах короче — Off)
// ver 4 (05.10, правка 362): + invertCam — строка Invert Camera Control в Settings паузы (L/R камеры пешком наоборот; по умолчанию On)
// + treeShadow (Роб 17.10, №757/763): тени деревьев на земле L1/L2 — Off/On в настройках графики «чтобы можно было оптимизировать игру»; по умолчанию Off (как TEST32); в старых файлах короче — Off
// ver 5 (№777, Роб 18.10: подстраницы SETTINGS и панель мультика): + contrast (сотые, 50..200, 100 — как было), lineWidth (толщина контура мультика 0..10, 5),
// eyeLines (контур глаз 0 нет / 1 мягкий / 2 полный, 1), toonTex (доля родной текстуры в мультике 0..100 %, 35); в старых файлах короче — по умолчанию
// ver 6 (№795, Роб 18.10: даль «хотя бы 200, может немного больше»): + drawDist — строка Draw Distance в PICTURE (0 — 150 м, 1 — 200 м как было, 2 — 250 м).
// ver 7 (№800, Роб 18.10: «ощущение старого телевизора — виньетка, мягкий свет, выгоревшая плёнка»): + oldTv — строка Old TV на странице CARTOON
// (0 Off / 1 On; по умолчанию On — Роб просил этот вид; действует в режиме мультика). В старых файлах короче — по умолчанию
// ver 8 (№830, Роб 18.10: «виньетка нравится, но слишком мощная — в настройках сила Old TV»): oldTv — сила 0..10 (Off, 10 %..100 %; 100 % — как ver 7 On),
// по умолчанию 5 (50 %, до слова Роба о своём числе); в файле ver 7 On (1) — 5, Off (0) — 0
// ver 10 (№951): + ink — строка Ink на странице CARTOON: 0 Indigo / 1 Black / 2 Tone (skin.h skinInkAbgr); в старых файлах короче — INK951_DEF
// №794 (Роб 18.10, настройки по умолчанию из меню на PSP): Cartoon On, Line Width 4, Eye Lines Soft, Texture Detail 45 %, Tree Shadows Off — для нового файла;
// у кого settings.bin уже есть — его значения. DEFAULTS794_OLD=1 — прежние значения по умолчанию
#ifndef DEFAULTS794_OLD
#define DEFAULTS794_OLD 0
#endif
#ifndef CAMNAV_OPT_OLD
#define CAMNAV_OPT_OLD 0       // круг 5 (g10-flow, P08:22/P16:08): 1 — строки Jump Cameras и Navigation System серые, камеры и стрелки всегда включены, в запись не пишутся (как было)
#endif
typedef struct { char magic[4]; int ver, music, effects, vehicle, voice, radar, tutorial; unsigned tutSeen, movies; int colour, invertCam, toon, treeShadow;
                 int contrast, lineWidth, eyeLines, toonTex, drawDist, oldTv, texturesOn, ink, camHeight, camDist, carShadow, curbTint,
                 jumpCam, navSys,
                 carTilt, carFov, footHeight, footDist,
                 carAim; } Settings;   // №1160: carAim — Car Aim (сдвиг кадра), 0..4, 2 — как было, поле в конце: в старом settings.bin его нет — берётся 2;   // №1151 (раздел CAMERA паузы): carHeight = camHeight, carDist = camDist (машина), carTilt, carFov, footHeight, footDist (пешком) — ступени 0..4, середина 2; в старых файлах короче footHeight / footDist берут прежние camHeight / camDist   // круг 5 (сверка g10-flow, P08:22, P16:08): jumpCam — строка Jump Cameras (камеры зон у машины), navSys — Navigation System (стрелки пути); 0 Off / 1 On, по умолчанию On; в старых файлах короче — On   // ver 9 (№912): texturesOn — строка Textures на PICTURE: 0 Off — мир без текстур (заливка, мультик), 1 On — родные текстуры
// №794 (сверено 19.10 с settings.bin Роба на PSP: насыщенность 1,56, контраст 1,50): значения по умолчанию для нового файла. DEFAULTS794B_OLD=1 — 1,36/1,00
#ifndef DEFAULTS794B_OLD
#define DEFAULTS794B_OLD 0
#endif
// №1042б (Роб: «используй максимум возможностей, я хочу увидеть дальнюю прорисовку»): Draw Distance по умолчанию 250 м (было 200), settings.bin ver 13 один раз поднимает выбор до 250 у тех,
// у кого записано 150/200 (потолок 150 вне зон снят в FAR1042, выбор игрока = дальность). DRAWDEF1042_OLD=1 — по умолчанию 200, ver 12 не мигрирует
#ifndef DRAWDEF1042_OLD
#define DRAWDEF1042_OLD 0
#endif
// №1066 (Роб 28.10, фото меню CARTOON и PICTURE на PSP: «как у нас должны стоять настройки по умолчанию»): для НОВОГО settings.bin Saturation 1.28 (было 1.56), Contrast 1.30 (1.50), Eye Lines Off (Soft),
// Texture Detail 80 % (45), Old TV 20 % (50), Textures On (Off); Cartoon On, Line Width 4, Ink Black, Tree/Car Shadows Off, Draw Distance 250, Curbs Original — как было. Сохранения не трогаются (cfgLoad читает файл как есть).
// DEFAULTS1066_OLD=1 / файл defaults1066old рядом с EBOOT (до cfgLoad) — прежние значения
#ifndef DEFAULTS1066_OLD
#define DEFAULTS1066_OLD 0
#endif
static Settings cfg = {{'S', 'H', 'S', 'T'}, DRAWDEF1042_OLD ? 10 : 14, 1000, 1000, 1000, 1000, 1, 1, 0, 1, DEFAULTS1066_OLD ? (DEFAULTS794B_OLD ? TONE_SAT_DEF : 156) : 128, 1, DEFAULTS794_OLD ? 0 : 1, 0, DEFAULTS1066_OLD ? (DEFAULTS794B_OLD ? 100 : 150) : 130,
                       DEFAULTS794_OLD ? 5 : 4, DEFAULTS1066_OLD ? 1 : 0, DEFAULTS1066_OLD ? (DEFAULTS794_OLD ? 35 : 45) : 80, DRAWDEF1042_OLD ? 1 : 2, DEFAULTS1066_OLD ? 5 : 2, DEFAULTS1066_OLD ? 0 : 1, INK951_DEF, 2, 2, 0, 0, 1, 1, 2, 2, 2, 2, 2};   // ver 12 (№1003): carShadow — строка Car Shadows на PICTURE, 0 Off (по умолчанию) / 1 On; в старых файлах короче — 0   // ver 11 (№996): camHeight, camDist — строки Height и Distance страницы CAMERA, 0..4, 2 (Normal) — как было; в старых файлах короче — 2
// №996: CAMCFG996_OLD=1 / файл camcfg996old рядом с EBOOT / «flag camcfg996 0» — множители камеры всегда 1 (как TEST70); шаги Height / Distance — множитель дальности и отступа глаза над точкой взгляда
#ifndef CAMCFG996_OLD
#define CAMCFG996_OLD 0
#endif
// №1003 (Роб 26.10 после TEST71: «давай уберём тени из-под машин — они мерцают, и это оптимизация (0,6–0,9 мс PSP); тогл в меню рядом с тенями деревьев»): тени всех машин (своя, трафик,
// стоящие, миссионные) по умолчанию не считаются и не рисуются; строка Car Shadows на PICTURE включает их как в TEST71. CARSHADOW1003_OLD=1 / файл carshadow1003old — тени всегда, как в TEST71
#ifndef CARSHADOW1003_OLD
#define CARSHADOW1003_OLD 0
#endif
static int carShadow1003Old = CARSHADOW1003_OLD, carShadow1003On;   // carShadow1003On ставит cfgApply: тени машин рисуются
static int camCfg996On = !CAMCFG996_OLD, camCfgSnap996;   // camCfgSnap996: Height / Distance сменились — сглаживание камеры (отпускание стены, опора глаза) сбросить: под паузой (dt = 0) оно иначе не сдвинется
#if defined(__has_include) && __has_include("camuser.h")
#include "camuser.h"        // g06-camera: camUserCarH/D/Tilt/Fov, camUserFootH/D (float; 1/1/0/1/1/1)
#else
static float camUserCarH = 1.f, camUserCarD = 1.f, camUserCarTilt = 0.f, camUserCarFov = 1.f, camUserFootH = 1.f, camUserFootD = 1.f;   // до слияния ветки g06-camera
#endif
static const float camCfgH996[5] = {.5f, .75f, 1.f, 1.25f, 1.5f}, camCfgD996[5] = {.7f, .85f, 1.f, 1.15f, 1.3f};
static int toonDevFile;   // №428: служебный файл toon рядом с EBOOT — режим включён поверх настройки игрока
static int camTurnOld, boomCamOld;   // правки 362 / 361: служебное, прежнее — L/R без инверсии / камера держит горящую машину (красный контроль)
static void cfgSave(void);
// строки ViewMovies.pag сверху вниз (IntroMovie, Movie1..8) = таблица роликов SLES_518.97 после логотипов: intro, fmv1a, fmv2..fmv8
static const char *const MOVIES[9] = {"intro", "fmv1a", "fmv2", "fmv3", "fmv4", "fmv5", "fmv6", "fmv7", "fmv8"};
static void movieSeen(const char *nm) {
  for (int k = 0; k < 9; k++) if (!strcmp(nm, MOVIES[k]) && !(cfg.movies >> k & 1u)) { cfg.movies |= 1u << k; cfgSave(); }
}
static int movieSeenBefore(const char *nm) {   // ролик уже открыт игроком (признак ставит movieSeen в конце показа)
  for (int k = 0; k < 9; k++) if (!strcmp(nm, MOVIES[k])) return cfg.movies >> k & 1u;
  return 0;
}
// Кадр ролика (правка Роба 49, 29.09, пересмотр 32: «рамка-телевизор на роликах слишком растянута; у роликов 4:3 — красивый
// фон по бокам»): картинка 4:3 без растяжки на всю высоту, по бокам — размытый фон из той же картинки; всё уже в кадре
// (tools/movie_export.py ambient), рамки поверх нет. Один вид у всех роликов: интро, гэги, цели «fmv», простой, VIEW MOVIES.
// Правка Роба 67 (видео IMG_7662): «видео, второе видео на фоне в блюре и рамку по периметру, как внутри меню» — поверх
// каждого ролика рамка-телевизор (boot.shr, экран tvmovie: та же TVFrame, разложенная на 16:9), поля — размытый фон того же
// кадра без тени, стык картинки мягкий (tools/movie_export.py AMB_FEATHER)
// Правка Роба 29 (30.09, «картинка на PSP заметно светлее и выцветшее, чем в эмуляторе»): экран PSP светлее монитора —
// палитры текстур мира (город, пропы) при загрузке проходят кривую тона: середины темнее (гамма PAL_GAMMA), цвет плотнее
// (PAL_SAT). ПРАВИЛО ПОРТА; флаг «tonoff» — как было
// Тело кривой — viewer/tone.c (правка 144: насыщенность — настройка cfg.colour, по умолчанию 1.36; гамма и контраст прежние).
// Блоки-источники палитр (TF_*, выше) регистрируются при чтении: при смене настройки палитры перечитываются из файла (toneApplyAll)
static int palToneOff;
static void palTone(void *pal) {
  if (palToneOff || !pal) return;
  tonePal((unsigned char *)pal);
  sceKernelDcacheWritebackRange(pal, 1024);
}
static int tvFrameOn = 1;
// правка Роба 89 (30.09): рамка меню на всех начальных экранах (лицензия, загрузка, титульный) — поверх экрана пакета boot.shr
static int scrTvOver;
#define SCR_CLOSE() (uiScrClose(), scrTvOver = 0)
static void movieDrawTv(const Movie *m) {
  movieDraw(m);
  if (tvFrameOn && uiScrReady()) { uiScrDraw(UI_BOOT_TVMOVIE, 0, 0.f, 0); resetState(); sceGuClutMode(GU_PSM_8888, 0, 0xff, 0); }
}
// ---- сверка g11b (02.11.2026): правила роликов — пропуск, затемнение вокруг ролика. MOVIE_RULES_OLD=1 — как в TEST81 (Start пропускает любой ролик с первого кадра, срез мира)
// M3_движение_меню №4/Д1, P07_миссии_цели №24, P14_меню_логика №13, P15_презентация P15-03/P15-04/Д1:
//  - пропуск X или Start, не раньше 0,125 с от начала; после нажатия картинка линейно гаснет в чёрное за 0,5 с, потом конец (звук затухает вместе — нужен movieVolume в movie.c, передано: пока звук идёт до обрыва)
//  - ролик новой игры (fmv1a) не пропускается вообще (Д1); ролики целей/просмотра fmv2..fmv8 — только если этот ролик уже открыт (cfg.movies); intro, логотипы, ролики гэгов — пропускаются
//  - ролик цели в игре и ролик гэга: HUD гасится (мир стоит), мир темнеет до чёрного за 0,5 с линейно и только потом стартует ролик; после ролика 10 кадров сплошного чёрного, потом срез в мир
#ifndef MOVIE_RULES_OLD
#define MOVIE_RULES_OLD 0
#endif
#define MOVIE_SKIP_MIN_S .125f
#define MOVIE_SKIPFADE_S .5f
#define MOVIE_WORLD_FADE_S .5f
#define MOVIE_BLACK_FRAMES 10
static int msnMovieWorld;   // 1 — ролик цели миссии в игре (ставит main_loop.h перед вызовом, «сверка g11b-collect»): мир темнеет до и 10 чёрных кадров после
static void movieVeil(float a) {   // чёрный прямоугольник на весь экран поверх нарисованного (внутри открытого списка GE)
  if (a <= .003f) return;
  typedef struct { unsigned color; short x, y, z, pad; } VeV;
  VeV *v = (VeV *)sceGuGetMemory(2 * sizeof(VeV)); unsigned c = (unsigned)(fminf(a, 1.f) * 255.f + .5f) << 24;
  v[0].color = v[1].color = c; v[0].x = 0; v[0].y = 0; v[1].x = SCR_W; v[1].y = SCR_H; v[0].z = v[1].z = 0;
  sceGuDisable(GU_TEXTURE_2D); sceGuDisable(GU_DEPTH_TEST); sceGuEnable(GU_BLEND); sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
  sceGuDrawArray(GU_SPRITES, GU_COLOR_8888 | GU_VERTEX_16BIT | GU_TRANSFORM_2D, 2, 0, v);
  resetState();
}
// мир стоит: два буфера кадра хранят последние кадры мира; в каждый по очереди добавляется чёрный слой так, чтобы накопленное затемнение шло линейно 0..1 за MOVIE_WORLD_FADE_S
static void movieWorldFade(void **fbp) {
  GE_NEST_GUARD("movieWorldFade");   // №1186/№1188
  void *buf[2] = {0, 0}; float cum[2] = {0.f, 0.f};
  unsigned t0 = sceKernelGetSystemTimeLow();
  for (;;) {
    float t = (sceKernelGetSystemTimeLow() - t0) * 1e-6f / MOVIE_WORLD_FADE_S; if (t > 1.f) t = 1.f;
    int k = (buf[0] == *fbp || !buf[0]) ? 0 : 1; buf[k] = *fbp;
    if (cum[k] < 1.f) { float a = cum[k] >= .999f ? 1.f : (t - cum[k]) / (1.f - cum[k]); if (a > 0.f) { sceGuStart(GU_DIRECT, list); movieVeil(a); sceGuFinish(); sceGuSync(0, 0); } cum[k] = t; }
    sceDisplayWaitVblankStart(); *fbp = swapBuf();
    if (cum[0] >= 1.f && cum[1] >= 1.f) break;
  }
}
static void movieBlackTail(void **fbp) {   // после ролика: 10 кадров сплошного чёрного (на 30 кадр/с около 0,33 с), потом чёрный снимается срезом
  GE_NEST_GUARD("movieBlackTail");   // №1186/№1188
  for (int k = 0; k < MOVIE_BLACK_FRAMES; k++) { sceGuStart(GU_DIRECT, list); movieVeil(1.f); sceGuFinish(); sceGuSync(0, 0); sceDisplayWaitVblankStart(); *fbp = swapBuf(); }
}
static void movieVolSync(void) { movieVolume = cfg.voice < 0 ? 0 : cfg.voice > 1000 ? PSP_AUDIO_VOLUME_MAX : cfg.voice * PSP_AUDIO_VOLUME_MAX / 1000; }   // сверка g09b-menu круг 2 (M3:4): звук ролика = ползунок голоса
static int movieSeenBefore(const char *nm);   // ниже, после MOVIES
// ролик гэга (GagPlayFMV): весь экран, мир стоит, START — пропустить; в сценарии смоука снимки берутся и здесь (как в интро)
// в поставке (tools/ship_rules.txt): "movies/loot.smv" — единственный ролик гэгов уровня 1 (gag_tele, Kwik-E-Mart)
static void gagMovie(int g, void **fbp) {
  GE_NEST_GUARD("gagMovie");   // №1186/№1188
  char fn[256], rel[64]; snprintf(rel, sizeof rel, "movies/%s.smv", coll.gag[g].fmv); sibling(fn, rel);
  if (movieSystem(1) < 0) { collLog("fmv_nojpeg", g, coll.gag[g].fmv); return; }
  if (waspLoop) { audioStop(waspLoop); waspLoop = 0; }
  if (!MOVIE_RULES_OLD) movieWorldFade(fbp);   // P15 Д1: мир темнеет до чёрного за 0,5 с, ролик гэга стартует после
  sndMaster(0.f);
  mem1060Pre(2560u << 10, "movie");   // №1060: кольцо кусков и кадр ролика (≈1,6 МБ) — цельным блоком; экран ролика закрывает мир
  int err = 0, shot = 0; movieVolSync(); Movie *m = movieOpen(fn, &err);
  int tvPak = tvFrameOn && !uiScrReady(); if (tvPak) { char bf[256]; sibling(bf, "boot.shr"); uiScrOpen(bf); }   // только отрицательный контроль (рамка правки 32)
  SceCtrlData pad; sceCtrlPeekBufferPositive(&pad, 1); unsigned prev = pad.Buttons | smokePad();
  unsigned last = sceKernelGetSystemTimeLow(); char s[160]; unsigned t00 = last; int skipping = 0; float skipT = 0.f;
  while (m && movieUpdate(m)) {
    unsigned t = sceKernelGetSystemTimeLow(); float dt = (t - last) * 1e-6f; last = t; if (dt > .05f) dt = .05f;
    sceCtrlPeekBufferPositive(&pad, 1); pad.Buttons |= smokePad();
    unsigned hit = pad.Buttons & ~prev; prev = pad.Buttons;
    if (MOVIE_RULES_OLD) { if (hit & PSP_CTRL_START) break; }
    else {   // M3 №4: X или Start не раньше 0,125 с, дальше картинка гаснет 0,5 с; у гэга пропуск включён всегда (P15 Д1)
      if (!skipping && (t - t00) * 1e-6f >= MOVIE_SKIP_MIN_S && (hit & (PSP_CTRL_START | PSP_CTRL_CROSS))) skipping = 1;
      if (skipping && (skipT += dt) >= MOVIE_SKIPFADE_S) break;
    }
    SmokeCmd sc; while (smokeNext(dt, &sc)) {}
    if (!!smkShot[0] != shot) {
      shot = !!smkShot[0]; moviePause(m, shot);
      if (shot) { snprintf(s, sizeof s, "GAGSHOT name=%s gag=%s frame=%d t=%.1f\n", smkShot, coll.gag[g].name, movieFrame(m), smkT); smkWrite(s); }
    }
    sceGuStart(GU_DIRECT, list); movieDrawTv(m); if (skipping) movieVeil(skipT / MOVIE_SKIPFADE_S); sceGuFinish(); sceGuSync(0, 0);
    sceDisplayWaitVblankStart(); *fbp = swapBuf();
  }
  if (tvPak) SCR_CLOSE();
  MovieStats st; memset(&st, 0, sizeof st); if (m) movieClose(m, &st); else st.err = err;
  if (!MOVIE_RULES_OLD) movieBlackTail(fbp);   // P15 Д1: 10 кадров сплошного чёрного
#if !TICKET_CONTENT_C3_OLD
  if(!TICKET_CONTENT_C3_OLD && st.frames>0 && coll.gag[g].pad==2) movieSeen(coll.gag[g].fmv);
#endif
  movieSystem(0); sndMaster(1.f);
  snprintf(s, sizeof s, "COLLEV fmv idx=%d name=%s frames=%d shown=%d dropped=%d under=%d err=%d t=%.1f\n", g, coll.gag[g].fmv, st.frames, st.shown,
           st.dropped, st.underruns, st.err, smkT); smkWrite(s);
}
// Ролик цели «fmv» миссии (SetFMVInfo, данные диска): L1 m5 «fmv2.rmv», L2 m8 «fmv3.rmv», L4 m5 «fmv4.rmv», L5 m8 «fmv5.rmv»,
// L6 m8 «fmv6.rmv», L7 m8 «fmv7.rmv» (у 2, 5, 6 — перед SetLevelOver, у 7 — перед SetGameOver: ролики концов уровней). Как ролик
// гэга: весь экран (4:3 с фоном по бокам, правка 49), мир стоит, START — пропустить; нет файла — цель засчитывается сразу.
// В поставке (tools/ship_rules.txt, tools/movie_export.py --clip fmvN): "movies/fmv2.smv" "movies/fmv3.smv" "movies/fmv4.smv"
// "movies/fmv5.smv" "movies/fmv6.smv" "movies/fmv7.smv"
// Правка Роба 279 (04.10: «нет музыки» — найдено по ходу): START, пропустивший ролик миссии, лежал в буфере пульта и в первом же
// кадре игры читался новым нажатием — открывалась пауза (тюнер «Pause» глушит музыку). После ролика пульт игры начинает заново
// с нынешнего состояния кнопок: зажатая кнопка — не нажатие. «flag padresyncold 1» — прежнее
static int padResync, padResyncOld;
#ifndef MELADSTUCK1192_OLD
#define MELADSTUCK1192_OLD 0   // 1 — ролики без проверки «ME замолчал на ступени лесенки» (как в TEST89: проверка была только в служебной сцене mefmv)
#endif
int meLadderStuck(void);
static void msnMovie1(const char *rmv, void **fbp);
static void msnMovie(const char *rmv, void **fbp) { unsigned t0_ = sceKernelGetSystemTimeLow(); msnMovie1(rmv, fbp); xnEnd("movie", t0_, (int)gameLevel); }   // №1206: время перехода
static void msnMovie1(const char *rmv, void **fbp) {
  GE_NEST_GUARD("msnMovie");   // №1186/№1188
  // №1192 (кандидат 2): ролик sceJpeg работает на ME; если лесенка ME оставила его «stuck» (ступень без ответа), задание прошивки повесило бы PSP целиком. Теперь так для ЛЮБОГО ролика (логотипы, fmv, интро), не только в сцене mefmv
  if (!MELADSTUCK1192_OLD && meLadderStuck()) { char s_[96]; snprintf(s_, sizeof s_, "MSNFMV skip: ME stuck name=%s t=%.1f\n", rmv, smkT); if (smkOn) smkWrite(s_); bbNote(s_); return; }
  char nm[32], rel[64], fn[256], s[192]; snprintf(nm, sizeof nm, "%s", rmv); char *dot = strrchr(nm, '.'); if (dot) *dot = 0;
  snprintf(rel, sizeof rel, "movies/%s.smv", nm); sibling(fn, rel);
  SceIoStat st; int have = sceIoGetstat(fn, &st) >= 0;
  if (!have || movieSystem(1) < 0) { snprintf(s, sizeof s, "MSNFMV name=%s have=%d played=0 t=%.1f\n", nm, have, smkT); smkWrite(s); return; }
  if (waspLoop) { audioStop(waspLoop); waspLoop = 0; }
  // сверка g11b: кто может пропустить (M3 №4/Д1, P07 №24, P14 №13, P15-03): ролик новой игры — никто; fmv2..fmv8 — если этот ролик уже открыт; остальные (intro, логотипы, простой) — да; X — не для логотипов
  int fmvWorld = !MOVIE_RULES_OLD && msnMovieWorld && !strncmp(nm, "fmv", 3);
  int canSkip = MOVIE_RULES_OLD || (strcmp(nm, "fmv1a") && (strncmp(nm, "fmv", 3) || movieSeenBefore(nm)));
  int crossOk = !MOVIE_RULES_OLD && (!strncmp(nm, "fmv", 3) || !strcmp(nm, "intro"));
#ifndef MSNBOTFMV_OLD
#define MSNBOTFMV_OLD 0   // 1 — как было: бот миссий (flag msnbot, смоук) ждёт ролик цели целиком, а команды сценария на это время теряются (06.11, L5: msnsel после fmv5 пропал)
#endif
  int botSkip = !MSNBOTFMV_OLD && msnBot && smkOn;   // бот — как игрок, видевший ролик: пропуск после MOVIE_SKIP_MIN_S (служебно)
  if (botSkip) canSkip = 1;
  if (fmvWorld && !FMV_STOPMUSIC_OLD && !strcmp(nm, "fmv7")) sndMusic(SND_MUS_OFF);   // SetFMVInfo("fmv7.rmv","stopmusic") в m8l.mfk уровня 7: второй аргумент в данных; до хоста он не доходит (fmvCb получает только имя) — по имени из данных
  if (fmvWorld) movieWorldFade(fbp);   // P15-04: мир темнеет до чёрного за 0,5 с линейно, ролик цели стартует после
  sndMaster(0.f);
  int err = 0, shot = 0; movieVolSync(); Movie *m = movieOpen(fn, &err);
  int tvPak = tvFrameOn && !uiScrReady(); if (tvPak) { char bf[256]; sibling(bf, "boot.shr"); uiScrOpen(bf); }   // только отрицательный контроль (рамка правки 32)
  SceCtrlData pad; sceCtrlPeekBufferPositive(&pad, 1); unsigned prev = pad.Buttons | smokePad();
  unsigned last = sceKernelGetSystemTimeLow(), t00 = last; int skipped = 0, skipping = 0; float skipT = 0.f;
  while (m && movieUpdate(m)) {
    unsigned tt = sceKernelGetSystemTimeLow(); float dt = (tt - last) * 1e-6f; last = tt; if (dt > .05f) dt = .05f;
    sceCtrlPeekBufferPositive(&pad, 1); pad.Buttons |= smokePad();
    unsigned hit = pad.Buttons & ~prev; prev = pad.Buttons;
    if (botSkip && (tt - t00) * 1e-6f >= MOVIE_SKIP_MIN_S + .5f) hit |= PSP_CTRL_START;
    // сведение дирижёра: правила пропуска g11b-collect (P15, задержка, ✕, затухание) + запрет пропуска ролика новой игры g10b-loop (P14:14)
    if (MOVIE_RULES_OLD) { if ((hit & PSP_CTRL_START) && !movieNoSkipSv) { skipped = 1; break; } }
    else {
      if (!skipping && canSkip && !movieNoSkipSv && (tt - t00) * 1e-6f >= MOVIE_SKIP_MIN_S && (hit & (PSP_CTRL_START | (crossOk ? PSP_CTRL_CROSS : 0u)))) { skipping = 1; skipped = 1; }
      if (skipping && (skipT += dt) >= MOVIE_SKIPFADE_S) break;
    }
    SmokeCmd sc; while (smokeNext(dt, &sc)) {}
    if (!!smkShot[0] != shot) {
      shot = !!smkShot[0]; moviePause(m, shot);
      if (shot) { snprintf(s, sizeof s, "MSNFMVSHOT name=%s clip=%s frame=%d t=%.1f\n", smkShot, nm, movieFrame(m), smkT); smkWrite(s); }
    }
    sceGuStart(GU_DIRECT, list); movieDrawTv(m); if (skipping) movieVeil(skipT / MOVIE_SKIPFADE_S); sceGuFinish(); sceGuSync(0, 0);
    sceDisplayWaitVblankStart(); *fbp = swapBuf();
  }
  if (tvPak) SCR_CLOSE();
  MovieStats ms; memset(&ms, 0, sizeof ms); if (m) movieClose(m, &ms); else ms.err = err;
  if (fmvWorld) movieBlackTail(fbp);   // P15-04: после ролика 10 кадров сплошного чёрного, потом срез в мир
  movieSystem(0); sndMaster(1.f);
  if (!padResyncOld) padResync = 1;   // правка 279: START пропуска ролика — не пауза в игре
  if (ms.frames > 0) movieSeen(nm);   // VIEW MOVIES: ролик открыт
  snprintf(s, sizeof s, "MSNFMV name=%s have=1 played=1 frames=%d shown=%d dropped=%d skipped=%d err=%d t=%.1f\n", nm, ms.frames, ms.shown,
           ms.dropped, skipped, ms.err, smkT); smkWrite(s);
}
// после gameAfterCar: события логики -> звук, частицы, HUD, ролик; частицы окон EFX пакета; петля жужжания; поток гэга
static void collFrame(float dt, void **fbp) {
  if (!haveColl) return;
#if !TICKET_CONTENT_C3_OLD
  ticketViewTick(&ticketView,&coll,dt);
#endif
  CollEvent e;
  while (collectPop(&coll, &e)) {
    float p[3] = {e.x, e.y, e.z};
    switch (e.type) {
      case CE_CARD: sndCardFound(e.x, e.y, e.z);   /* сверка g08-sound (P09 Д3): card_collect — в машине, пешком вставка OF_found_card */ fxBurst(fxFind("card_collect_psShape"), p, NULL); tutEvent(TUT_CARD);
        sndSay(SND_SAY_CARD, 0);   // аудит 09.10: W_Card («I can sell this on the internet»)
        if (!CARD_CELEBRATE_OLD && haveHomer && onFoot()) homerCelebrate(&homer, 1);   // круг 2, g04-homer P05:1: большая радость hom_victory_large при карточке (за рулём клипа нет: 0)
        cardHudT = COLL_HUD_OLD ? COLL_HUD_S : COLL_HUD_CARD_S; if (uiReady) uiHudSet(UI_F_CARD_TITLE, coll.card[e.idx].title);
        { char nm[16]; snprintf(nm, sizeof nm, "card%u", coll.card[e.idx].id); collLog("card", e.idx, nm); } break;
      case CE_CARDS_DONE: if (COLL_HUD_OLD) itemsHudT = 2.f * COLL_HUD_S;
        else { itemsWaitT = cardHudT > COLL_HUD_CARD_LEAVE_S ? cardHudT - COLL_HUD_CARD_LEAVE_S : 0.f; itemsHudT = itemsWaitT + COLL_HUD_MSG_S; }   // М2 №8: начинается в фазе ухода карточки
        collLog("cards_done", e.idx, "-");   // после сообщения карточки
        bonusOpenFromColl("cards"); break;   // правка 421: колода уровня собрана — «New bonus game track unlocked!» (Hud.pag ItemsUnlocked): трасса уровня открыта
      case CE_WASP_SPAWN: sndPlayId(waspSnd[0], e.x, e.y, e.z, 1.f); collLog("wasp_spawn", e.idx, coll.wasp[e.idx].name); if (WASP_NEAR_OLD) tutEvent(TUT_WASP); break;   // подсказка про ос (PS2 11:20); сверка g11b P15-07: теперь по CE_WASP_NEAR (оса ближе 5 м), не по появлению
      case CE_WASP_NEAR: if (!WASP_NEAR_OLD) tutEvent(TUT_WASP); break;
      case CE_WASP_CHARGE: collLog("wasp_charge", e.idx, coll.wasp[e.idx].name); break;
      case CE_WASP_FIRE: sndPlayId(waspSnd[2], e.x, e.y, e.z, 1.f); break;
#ifndef WASP_BOLT_OLD
      case CE_WASP_MISS: sndPlayId(waspSnd[3], e.x, e.y, e.z, 1.f); collLog("wasp_miss",e.idx,coll.wasp[e.idx].name); break;
#endif
      case CE_WASP_ZAP: sndPlayId(waspSnd[5], e.x, e.y, e.z, 1.f); collLog("wasp_zap", e.idx, coll.wasp[e.idx].name); break;
      case CE_WASP_DEAD: sndPlayId(waspSnd[4], e.x, e.y, e.z, 1.f); collLog("wasp_dead", e.idx, coll.wasp[e.idx].name); break;
      case CE_WASPS_DONE: { const char *tr_; sndMusicEvent("Destroy_Camera_Bonus", &tr_); } /* сверка g08 круг 3 (P09 Д4): все осы-камеры — вставка */ waspsHudT = COLL_HUD_OLD ? COLL_HUD_S : COLL_HUD_MSG_S; collLog("wasps_done", e.idx, "-"); break;   // М2 №11/12: 0,3 + 2,0 + 0,3 = 2,6 с
#if !TICKET_CONTENT_C3_OLD
      case CE_TICKET: if(ticketViewAction(&ticketView,&coll,e.idx)) gagMovie(e.idx,fbp); break;
#endif
      case CE_GAG_START: {
        const CollGag *G = &coll.gag[e.idx];
        collLog("gag_start", e.idx, G->name);
        if (G->shake[0] > 0.f || G->shake[1] > 0.f || G->shake[2] > 0.f) { shakeG = e.idx; shakeT = 0.f; }   // сверка g11b P08 №2: пауза может быть 0 — условие по любому аргументу
        if (G->fmv[0]) gagMovie(e.idx, fbp); else gagSound(e.idx);
      } break;
      case CE_GAG_END: collLog("gag_end", e.idx, coll.gag[e.idx].name); if (TUT_GAG_ZONE_OLD) tutEvent(TUT_GAG); break;   // подсказка про гэги — после первого (правило порта); сверка g11b P15-09: теперь по входу в зону действия, collHud
      case CE_GAG_FOUND: collLog("gag_found", e.idx, coll.gag[e.idx].name); break;
      case CE_GAGS_DONE: collLog("gags_done", e.idx, "-"); break;   // «LEVEL GAGS COMPLETE!» — строка 2 WaspsComplete: поля выбора в ui.shr нет
      default: break;
    }
  }
  coll.st.gagHold = gagStreamG >= 0 ? gagStreamG + 1 : 0; coll.st.inMission = mvInMission();   // сверка g11b: звук гэга продлевает «играет» (P16-14); в миссии гэги с роликом молчат (P16 Д7)
  SpFxOut o; while (spFxPop(&coll.sp, &o)) spFxOut(0, &o);   // окна EFX: частицы сцен гэгов и ос (родные системы fx.shr)
  // сверка g11b, P10_звук_машины_и_пространство P10-19: два голоса на ближайших живых ос, позиционно по wasp_settings (полная до 12 м, затухание до 30 м), по стадиям: появление wasp_fade_in (разово, CE_WASP_SPAWN),
  // дальше петля wasp_idle; зарядка — петля wasp_charging; заряжена — петля wasp_charged_idle; атака — wasp_attack разово, затем wasp_idle; уничтожена — wasp_destroyed (CE_WASP_DEAD), голос освобождается.
  // Звуков wasp_charging / wasp_charged_idle в пакете звуков нет (tools/sound_export.py:58) — пока их нет, на этих стадиях петля wasp_idle (передано в g08/g12). WASP_SND_OLD=1 — одна петля wasp_idle у ближайшей, 3–30 м
  if (!WASP_SND_OLD) {
    if (waspSnd2[0] == -2) { waspSnd2[0] = sndId("wasp_charging"); waspSnd2[1] = sndId("wasp_charged_idle"); waspSnd2[2] = sndId("wasp_attack"); }
    int paused = uiReady && uiPaused();
    for (int k = 0; k < WASP_SLOTS; k++) {   // голос держит свою осу, пока она жива и ближе 30 м
      int w = wSlot[k].wasp;
      if (w >= 0) { const SpRun *r = &coll.sp.run[coll.wasp[w].inst]; float dx = r->x - eyeX, dy = r->y - eyeY, dz = r->z - eyeZ;
        if (paused || coll.st.waspSt[w] != 1 || !r->alive || r->state == 5 || dx * dx + dy * dy + dz * dz > 30.f * 30.f) { if (wSlot[k].h) audioStop(wSlot[k].h); wSlot[k].h = 0; wSlot[k].wasp = -1; wSlot[k].stage = -1; } }
    }
    for (int k = 0; k < WASP_SLOTS && !paused; k++) {
      if (wSlot[k].wasp >= 0) continue;
      int best = -1; float bd = 30.f * 30.f;
      for (unsigned i = 0; i < coll.nwasp; i++) {
        if (coll.st.waspSt[i] != 1) continue;
        int taken = 0; for (int q = 0; q < WASP_SLOTS; q++) taken |= wSlot[q].wasp == (int)i;
        const SpRun *r = &coll.sp.run[coll.wasp[i].inst]; float dx = r->x - eyeX, dy = r->y - eyeY, dz = r->z - eyeZ, d = dx * dx + dy * dy + dz * dz;
        if (!taken && r->alive && r->state != 5 && d < bd) { bd = d; best = (int)i; }
      }
      wSlot[k].wasp = best; wSlot[k].stage = -1;
    }
    for (int k = 0; k < WASP_SLOTS; k++) {
      int w = wSlot[k].wasp; if (w < 0) continue;
      const SpRun *r = &coll.sp.run[coll.wasp[w].inst];
      int stage = r->state == 2 ? 1 : r->state == 3 ? 2 : 0;   // 0 жужжание, 1 зарядка, 2 заряжена
      if (r->state == 4 && wSlot[k].last != 4 && waspSnd2[2] >= 0) sndPlayId(waspSnd2[2], r->x, r->y, r->z, 1.f);   // wasp_attack разово
      wSlot[k].last = r->state;
      if (stage != wSlot[k].stage || !wSlot[k].h || !audioPlaying(wSlot[k].h)) {
        if (wSlot[k].h) audioStop(wSlot[k].h);
        int id = stage == 1 && waspSnd2[0] >= 0 ? waspSnd2[0] : stage == 2 && waspSnd2[1] >= 0 ? waspSnd2[1] : waspSnd[1];
        wSlot[k].h = id >= 0 ? audioPlayAt(id, r->x, r->y, r->z, 12.f, 30.f) : 0; wSlot[k].stage = stage;
      } else audioSetAt(wSlot[k].h, r->x, r->y, r->z, 12.f, 30.f);
    }
    if (waspLoop) { audioStop(waspLoop); waspLoop = 0; }
  } else {
  // жужжание: петля wasp_idle у ближайшей живой осы (дальше 30 м — тихо), в паузе — нет
  int best = -1; float bd = 30.f * 30.f;
  for (unsigned i = 0; i < coll.nwasp && !(uiReady && uiPaused()); i++) {
    if (coll.st.waspSt[i] != 1) continue;
    const SpRun *r = &coll.sp.run[coll.wasp[i].inst]; float dx = r->x - eyeX, dy = r->y - eyeY, dz = r->z - eyeZ, d = dx * dx + dy * dy + dz * dz;
    if (r->alive && r->state != 5 && d < bd) { bd = d; best = (int)i; }
  }
  if (best >= 0 && waspSnd[1] >= 0) {
    const SpRun *r = &coll.sp.run[coll.wasp[best].inst];
    if (!waspLoop || !audioPlaying(waspLoop)) waspLoop = audioPlayAt(waspSnd[1], r->x, r->y, r->z, 3.f, 30.f);
    else audioSetAt(waspLoop, r->x, r->y, r->z, 3.f, 30.f);
  } else if (waspLoop) { audioStop(waspLoop); waspLoop = 0; }
  }
  // поток гэга: громкость и панорама по месту сцены; кончился — поток диалогов снова по центру (реплики Гомера)
  if (gagStreamG >= 0) {
    if (audioStreamTrack(AUDIO_ST_DIALOG) != gagStreamT) { gagStreamG = -1; audioStreamSet(AUDIO_ST_DIALOG, 1.f, 0.f); }
    else { const CollGag *G = &coll.gag[gagStreamG]; audioStreamAt(AUDIO_ST_DIALOG, G->x, G->y + 1.f, G->z, 4.f, 60.f); }
  }
  if (cardHudT > 0.f) cardHudT -= dt;
  if (itemsHudT > 0.f) itemsHudT -= dt;
  if (waspsHudT > 0.f) waspsHudT -= dt;
  if (shakeG >= 0) shakeT += dt;
}
// тряска камеры гэга (GagSetCameraShake). Сверка g11b, P08_камера №2: аргументы по порядку — ПАУЗА в секундах, сила (шейкером не используется), ДЛИТЕЛЬНОСТЬ в секундах (пример данных 0,1 / 450 / 3,5);
// направление строго вверх, 58,9 рад/с. Без длительности: затухающая тряска 0,18 с с размахом до 1,1 м; с длительностью: без затухания, размах 1,0 м, до конца паузы плюс длительность.
// Размах понимаем как полный (от пика до пика): амплитуда глаза — половина. Прежнее (сила м, задержка мс, три оси 71/53/67 рад/с, линейное затухание) — SHAKE_ARGS_OLD=1.
#ifndef SHAKE_ARGS_OLD
#define SHAKE_ARGS_OLD 0
#endif
static void collShake(float *ex, float *ey, float *ez) {
  if (shakeG < 0) return;
  const CollGag *G = &coll.gag[shakeG];
  if (SHAKE_ARGS_OLD) {
    float t = shakeT - G->shake[1] * .001f;
    if (t > G->shake[2]) { shakeG = -1; return; }
    if (t < 0.f) return;
    float a = G->shake[0] * (1.f - t / G->shake[2]);
    *ex += a * sinf(t * 71.f); *ey += a * sinf(t * 53.f + 1.f); *ez += a * sinf(t * 67.f + 2.f);
    return;
  }
  float t = shakeT - G->shake[0], dur = G->shake[2];
  if (dur <= 0.f) {   // без длительности: затухающая 0,18 с
    if (t > .18f) { shakeG = -1; return; }
    if (t < 0.f) return;
    *ey += .55f * (1.f - t / .18f) * sinf(t * 58.9f);
    return;
  }
  if (t > dur) { shakeG = -1; return; }
  if (t < 0.f) return;
  *ey += .5f * sinf(t * 58.9f);
}
static void collHud(void) {
  if (!haveColl || !uiReady) return;
  int card = cardHudT > 0.f, tot;
  uiHudShow(UI_T_CARD_TEXT, card); uiHudShow(UI_T_CARDS, card);
  if (card) { uiHudSet(UI_F_CARDS, collectCount(&coll, coll.level, COLL_CARDS, &tot)); uiHudSet(UI_F_CARDS_TOTAL, tot); }
  uiHudShow(UI_T_ITEMS_COMPLETE, COLL_HUD_OLD ? (itemsHudT > 0.f && !card) : (itemsHudT > 0.f && itemsHudT <= COLL_HUD_MSG_S));   // сверка g11b М2 №8: с фазы ухода карточки, 2,6 с
  uiHudShow(UI_T_WASPS, waspsHudT > 0.f);
  { int act_ = onFoot() && !uiPaused() && collectWantsAction(&coll) >= 0;
    if (act_) { uiHudShow(UI_T_ACTION, 1); uiActionText("Use"); }   // △ у гэга
    if (act_ && !gagZoneIn && !TUT_GAG_ZONE_OLD && !mvInMission()) tutEvent(TUT_GAG);   // P15-09: вход в зону действия гэга в «воскресной» езде (подсказка показывается раз за игру)
    gagZoneIn = act_; }
}
// модели пакета: в городе — карточки, осы и уличные гэги; в комнате — только гэги этой комнаты
#ifndef SPARKLE_FADE_OLD
#define SPARKLE_FADE_OLD 0   // P16-13/Д8: блёстки гэга гаснут 10..20 м, размер 1,0 / 0,5 (1 — 45 м постоянной силы, размер 1,3)
#endif
static int gagSnap = -1;
static int glowFogOld;   // отрицательный контроль правки 76 (flag glowfog): свечение сложением в тумане, как раньше
// №1063 (Роб 27.10: «у карточек такой же блик, как в оригинале… точно векторное свечение, не старое»): данные оригинала — у карточки один билборд-квад 1,6 м со сложением, цвет вершины белый (его альфа — пульс),
// текстура — серый радиальный градиент 32×32 (по кольцам от центра: 0,89 → 0,6 (0,19 радиуса) → 0,23 (0,31) → 0,10 (0,44) → 0,03 (0,69) → 0 на краю квада). Пятно рисуется ВЕКТОРОМ: кольца вершин с альфой по этому профилю,
// без текстуры и без размытого спрайта; белое, сложением, как в оригинале (оранжево-розовый — от картинки карточки под ним). CARD1063_OLD=1 / файл card1063old / flag card1063old 1 — блик-звезда №839 прежний
#ifndef CARD1063_OLD
#define CARD1063_OLD 0
#endif
static int card1063Old = CARD1063_OLD;
static int cardGlowInst(unsigned i) { for (unsigned k = 0; k < coll.ncard; k++) if (coll.card[k].inst == i) return 1; return 0; }
static int cardSpotDraw(const float *v, unsigned col) {   // квад билборда (4 угла) -> векторное радиальное пятно (список треугольников); 0 — не нарисовано (альфа 0)
  float a0 = (float)(col >> 24) / 255.f; if (a0 < .01f) return 0;
  float cx = (v[0] + v[3] + v[6] + v[9]) * .25f, cy = (v[1] + v[4] + v[7] + v[10]) * .25f, cz = (v[2] + v[5] + v[8] + v[11]) * .25f;
  float rx = (v[3] - v[0] + v[9] - v[6]) * .25f, ry = (v[4] - v[1] + v[10] - v[7]) * .25f, rz = (v[5] - v[2] + v[11] - v[8]) * .25f;   // полуширина (вправо)
  float ux = (v[6] - v[0] + v[9] - v[3]) * .25f, uy = (v[7] - v[1] + v[10] - v[4]) * .25f, uz = (v[8] - v[2] + v[11] - v[5]) * .25f;   // полувысота (вверх)
  enum { SEG = 12, RINGS = 7, NV = SEG * 3 + SEG * (RINGS - 2) * 6 };
  static const float rf[RINGS] = {0.f, .19f, .31f, .44f, .69f, .88f, 1.f}, ra[RINGS] = {.89f, .6f, .23f, .10f, .03f, .01f, 0.f};
  typedef struct { unsigned c; float x, y, z; } SV;
  SV *q = (SV *)geVbAlloc(NV * sizeof(SV), GEVB_COLLECT_OLD); if (!q) return 0;
  unsigned n = 0;
#define SPV(r, k) do { float th_ = 6.2831853f * (float)((k) % SEG) / SEG, cs_ = cosf(th_) * rf[r], sn_ = sinf(th_) * rf[r]; \
    q[n].c = ((unsigned)(ra[r] * a0 * 255.f) << 24) | 0xffffffu; q[n].x = cx + rx * cs_ + ux * sn_; q[n].y = cy + ry * cs_ + uy * sn_; q[n].z = cz + rz * cs_ + uz * sn_; n++; } while (0)
  for (int k = 0; k < SEG; k++) { SPV(0, 0); SPV(1, k); SPV(1, k + 1); }
  for (int r = 1; r < RINGS - 1; r++) for (int k = 0; k < SEG; k++) { SPV(r, k); SPV(r + 1, k); SPV(r, k + 1); SPV(r, k + 1); SPV(r + 1, k); SPV(r + 1, k + 1); }
#undef SPV
  ScePspFMatrix4 I; memset(&I, 0, sizeof I); I.x.x = I.y.y = I.z.z = I.w.w = 1.f; sceGuSetMatrix(GU_MODEL, &I);
  sceGuDisable(GU_TEXTURE_2D); sceGuDisable(GU_ALPHA_TEST); sceGuEnable(GU_BLEND); sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_FIX, 0, 0xffffffff); sceGuDepthMask(GU_TRUE); sceGuDisable(GU_FOG); setCull(0); curFlags = -1;
  sceGuDrawArray(GU_TRIANGLES, GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D, (int)n, 0, q);
  if (smkOn) { static unsigned nl_; if ((nl_++ & 63) == 0) { char l_[160]; snprintf(l_, sizeof l_, "CARDSPOT n=%u a=%.2f c=%.1f,%.1f,%.1f half=%.2f,%.2f\n", nl_, a0, cx, cy, cz, sqrtf(rx * rx + ry * ry + rz * rz), sqrtf(ux * ux + uy * uy + uz * uz)); smkWrite(l_); } }   // №1063 служебно
  return 1;
}
static int spBillUvOld;  // №370: отрицательный контроль прежнего переворота неона (flag spuvold 1)
#ifndef GRANDPA_GLASSES_OLD
#define GRANDPA_GLASSES_OLD 0
#endif
static int grandpaGlassesOld = GRANDPA_GLASSES_OLD;  // №518: отрицательный контроль розовых очков дедушки (flag grandpaold 1)
#ifndef FX_GLINT846_OLD
#define FX_GLINT846_OLD 0
#endif
static const char *gq944Site;   // №957б служебно: кто поставил квад в очередь бликов (spritelight.h)
#ifndef NEONARCH957_OLD
#define NEONARCH957_OLD 0
#endif
#define NEONARCH957_MAX 3.f   // м: свечение предмета со сложением крупнее — арка, вывеска (монета 2 м, карточка и приколы мельче); не рисуется и бликом не становится
static int neonArch957 = !NEONARCH957_OLD;
// №957б (Роб 25.10: «арки причала L6 — въезд, выезд, центр «Спрингфилд»: на неоне стоит блик «блинк-блинк» (№944), убрать»): неон арки — объект набора предметов (collNear*), и его квад-свечение
// шёл в блик как свечение предмета (№839) без предела по размеру (у стейтпропов уровня предел с №885: 2 м). Теперь у предметов тот же приём: крупный квад со сложением (не огни заграждений PVIS,
// не эффекты героя) — без свечения вовсе, сама вывеска — куски города. NEONARCH957_OLD=1 / «flag neonarch957 0» / файл neonarch957old — как было
static int neonArchBig957(const float *v, unsigned bfl) {
  if (!neonArch957 || ((bfl >> 8) & 0xFF) != 2 || (bfl & SP_BILL_PVIS)) return 0;
  float w = (v[3] - v[0]) * (v[3] - v[0]) + (v[4] - v[1]) * (v[4] - v[1]) + (v[5] - v[2]) * (v[5] - v[2]), h = (v[6] - v[0]) * (v[6] - v[0]) + (v[7] - v[1]) * (v[7] - v[1]) + (v[8] - v[2]) * (v[8] - v[2]);
  return (w > h ? w : h) > NEONARCH957_MAX * NEONARCH957_MAX;
}
static void glowQuadQ(const float *v, unsigned abgr, float ph);   // №839: spritelight.h (подключается ниже)
static void glowGlintFlush(float ex, float ey, float ez);
static void nitroGlintBatch1148(const float *P, const float *CY, int n, float ex, float ey, float ez);   // №1148: spritelight.h — блик нитро бонус-игры как у ключа
static void g944Bill(const char *site, int blm, int bt, const float *v, unsigned col, unsigned inst, unsigned b);   // №944 служебно: перепись картинок-квадов (spritelight.h)
// Разбор L3/L5 (журнал тура TEST55, COLLPROF inst 0,69 мс PSP на причале L3 при 111 экземплярах, на L5 0,11): collDraw два раза за кадр (тела и билборды) перебирает ВСЕ экземпляры набора коллекции
// (комната, живость, дальность) — рисуется из них единицы. Теперь список экземпляров в дальности + COLL_NEAR_MARGIN м от глаза строится раз в COLL_NEAR_AGE кадров или при сдвиге глаза на
// COLL_NEAR_MOVE м, сменой комнаты и дальности (отдельный список на каждую из двух: maxD и 60 м); экземпляры, что движутся сами (взмах ноги, удар током, молнии), в список входят всегда; порядок —
// по номеру, как в полном переборе. Рисуется тот же набор: условия room/spDrawable проверяются у каждого кандидата каждый кадр. «flag collverify 1» — сверка с полным перебором (строка COLLVER, гейт
// tests/collnear913). COLLNEAR913_OLD=1 — как было
#ifndef COLLNEAR913_OLD
#define COLLNEAR913_OLD 0
#endif
#ifndef COLL_NEAR_MARGIN
#define COLL_NEAR_MARGIN 14.f
#endif
#ifndef COLL_NEAR_MOVE
#define COLL_NEAR_MOVE 6.f
#endif
#ifndef COLL_NEAR_AGE
#define COLL_NEAR_AGE 10
#endif
#define COLL_NEAR_MAX 1024
typedef struct { const StateProps *sp; unsigned ninst, n, age; int room; float maxD, ex, ez; unsigned short idx[COLL_NEAR_MAX]; int valid; } CollNear;
static CollNear collNear[2]; static CollNear *collNearCur; static int collVerify; static unsigned collVerFrames, collVerMiss, collNearBuilds, collNearHits;
static void collNearVerify(const StateProps *sp, int room, float maxD, float ex, float ez, const CollNear *c) {   // служебное: все экземпляры, что рисует полный перебор, обязаны быть в списке
  static unsigned char in_[COLL_NEAR_MAX]; if (sp->ninst > COLL_NEAR_MAX) return; memset(in_, 0, sp->ninst);
  if (c->valid) for (unsigned k = 0; k < c->n; k++) in_[c->idx[k]] = 1; else memset(in_, 1, sp->ninst);
  unsigned miss = 0; for (unsigned i = 0; i < sp->ninst; i++) if (!in_[i] && collectInstRoom(&coll, i) == room && spDrawable(sp, i, ex, ez, maxD)) miss++;
  collVerFrames++; collVerMiss += miss;
}
static void collNearBuild0(const StateProps *sp, int room, float maxD, float ex, float ez, CollNear *c);
static void collNearBuild(const StateProps *sp, int room, float maxD, float ex, float ez) {
  CollNear *c = &collNear[maxD > 60.5f ? 0 : 1]; collNearCur = c;
  collNearBuild0(sp, room, maxD, ex, ez, c);
  if (collVerify) collNearVerify(sp, room, maxD, ex, ez, c);
}
static void collNearBuild0(const StateProps *sp, int room, float maxD, float ex, float ez, CollNear *c) {
  if (COLLNEAR913_OLD || sp->ninst > COLL_NEAR_MAX) { c->valid = 0; return; }
  float dx = ex - c->ex, dz = ez - c->ez;
  if (c->valid && c->sp == sp && c->ninst == sp->ninst && c->room == room && c->maxD == maxD && c->age < COLL_NEAR_AGE && dx * dx + dz * dz <= COLL_NEAR_MOVE * COLL_NEAR_MOVE) { c->age++; collNearHits++; return; }
  float lim = maxD + COLL_NEAR_MARGIN; lim *= lim; unsigned n = 0;
  for (unsigned i = 0; i < sp->ninst; i++) {
    int fx_ = (int)i == coll.kickFx || (int)i == coll.shockFx || (coll.bolt0 >= 0 && i >= (unsigned)coll.bolt0 && i < (unsigned)coll.bolt0 + coll.nbolt);
    const SpRun *r = &sp->run[i]; float ax = r->x - ex, az = r->z - ez;
    if (fx_ || ax * ax + az * az <= lim) c->idx[n++] = (unsigned short)i; }
  c->n = n; c->sp = sp; c->ninst = sp->ninst; c->room = room; c->maxD = maxD; c->ex = ex; c->ez = ez; c->age = 0; c->valid = 1; collNearBuilds++;
}
static unsigned collNearN(const StateProps *sp) { return !collNearCur->valid ? sp->ninst : collNearCur->n; }
static unsigned collNearAt(const StateProps *sp, unsigned k) { (void)sp; return !collNearCur->valid ? k : collNearCur->idx[k]; }
static void collDraw(float ex, float ey, float ez, int inR, float t) {
  if (!haveColl || !coll.haveSp) return; gloSite = 3;
  static unsigned cpN; static double cpA[4]; unsigned cp0 = smkOn ? sceKernelGetSystemTimeLow() : 0, cp1 = 0, cp2 = 0;   // COLLPROF: гэг-скины / карточки-осы-сцены / билборды, мс на кадр
  StateProps *sp = &coll.sp; int room = inR ? collectRoomId(roomName()) : -1;
  float maxD = inR ? 80.f : drawDist * .92f;
  if (gagSnap >= 0 && (unsigned)gagSnap < coll.ngag) {
    for (unsigned g = 0; g < coll.ngag; g++) coll.st.gagPick[g] = g == (unsigned)gagSnap;
    coll.st.gagSt[gagSnap] = 1; coll.st.gagT[gagSnap] = 1.f;
    int inst = coll.gag[gagSnap].inst;
    if (inst >= 0) { SpRun *r = coll.sp.run + inst; spEnter(sp, inst, 1); r->t = 1.f;
      r->poseF = sp->type[sp->inst[inst].type].fps; r->poseValid = 0; }
  }
  #ifndef GAG_SKIN_OLD
  if (gagSkinDraw(&coll, room, ex, ez, maxD)) resetState();
  #endif
  if (smkOn) cp1 = sceKernelGetSystemTimeLow();
  collNearBuild(sp, room, maxD, ex, ez);   // разбор L3/L5: список близких экземпляров (раз в несколько кадров)
  for (unsigned ii_ = 0; ii_ < collNearN(sp); ii_++) { unsigned i = collNearAt(sp, ii_);
    if (collectInstRoom(&coll, i) != room || !spDrawable(sp, i, ex, ez, maxD)) continue;
#ifndef GAG_SKIN_OLD
    int gag = collectGagOf(&coll, i);
    if (gag >= 0 && ((gagSnap >= 0 && gag != gagSnap) || (coll.gag[gag].random && !coll.st.gagPick[gag]))) continue;
#endif
    { float c[3], r; spBounds(sp, i, c, &r); float bb[6] = {c[0] - r, c[1] - r, c[2] - r, c[0] + r, c[1] + r, c[2] + r}; if (!visBox(bb, 0, 0, 0)) continue; }
    const SpType *T = &sp->type[sp->inst[i].type];
    spPose(sp, i);
    for (unsigned k = 0; k < T->nPiece; k++) {
      if (!spPieceOn(sp, i, k)) continue;
      const SpPiece *pp = &sp->piece[T->firstPiece + k];
      if (!pp->ni) continue;
#if !GRANDPA_GLASSES_OLD
      // №518: у дедушки в gag_s2..s5 очки — сустав 25 (gpa_high_glasses); кусок ni=144 — линзы без текстуры,
      // получившие из Maya розовую текстуру-заглушку. Оправа (ni=84) остаётся, сквозь неё видны глаза (как у grandp_m.p3d)
      if (!grandpaGlassesOld && ((pp->flags >> 8) & 0xFF) == 25 && pp->ni == 144) continue;
#endif
      float m[16]; spPieceMatrix(sp, i, k, m);
      Piece tmp; memset(&tmp, 0, sizeof tmp);
      tmp.r.tex = pp->tex; tmp.r.flags = pp->flags & (0xFF | F_ADDITIVE); tmp.r.ni = pp->ni;
      tmp.v = (Vert *)((char *)sp->vertBase + pp->vOff);
      tmp.idx = (unsigned short *)((char *)sp->idxBase + pp->iOff);
      drawPieceM(&tmp, (const ScePspFMatrix4 *)m);
    }
  }
  if (smkOn) cp2 = sceKernelGetSystemTimeLow();
  int nb = 0, spot_ = 0;   // билборды (свечение карточки, лучи осы, огонь/свет сцен): как у стейтпропов уровня
  collNearBuild(sp, room, 60.f, ex, ez);
  for (unsigned ii_ = 0; ii_ < collNearN(sp); ii_++) { unsigned i = collNearAt(sp, ii_);
    if (collectInstRoom(&coll, i) != room || !spDrawable(sp, i, ex, ez, 60.f)) continue;
#ifndef GAG_SKIN_OLD
    int gag = collectGagOf(&coll, i);
    if (gag >= 0 && ((gagSnap >= 0 && gag != gagSnap) || (coll.gag[gag].random && !coll.st.gagPick[gag]))) continue;
#endif
    const SpType *T = &sp->type[sp->inst[i].type];
    for (unsigned b = 0; b < T->nBill; b++) {
      float v[12]; uint32_t col, bfl; int bt;
      if (!spBillQuad(sp, i, b, ex, ey, ez, v, &col, &bt, &bfl) || !(col >> 24)) continue;
      // №846 (Роб 19.10, TEST39: «при ударе у героя блик, как будто удар магический — вместо взмаха подгрузился блик»): в этом же наборе живут
      // эффекты героя — взмах ноги (kickFx, 14 кадров диска), удар током (shockFx) и молнии (bolt) — их квады сложением остаются картинкой, бликом — только свечения предметов
      int fx_ = (int)i == coll.kickFx || (int)i == coll.shockFx || (coll.bolt0 >= 0 && i >= (unsigned)coll.bolt0 && i < (unsigned)coll.bolt0 + coll.nbolt);
      if (!fx_ && neonArchBig957(v, bfl)) continue;   // №957б: арка, вывеска — без свечения
      if (smkOn) { static unsigned char lg_[1024]; unsigned ok_ = bt >= 0 && (unsigned)bt < ntex; const Tex *tt_ = ok_ ? &tex[bt] : &tex[0]; unsigned k_ = i & 1023; if (!lg_[k_]) { lg_[k_] = 1; char l_[200]; snprintf(l_, sizeof l_, "CARDGLOW i=%u b=%u bt=%d mode=%d col=%08x stat=%u,%u,%u,%u,%u ok=%u p=%.1f,%.1f,%.1f w=%.2f\n", i, b, bt, (int)((bfl >> 8) & 0xFF), col, tt_->tstat[0], tt_->tstat[1], tt_->tstat[2], tt_->tstat[3], tt_->tstat[4], ok_ ? (unsigned)tt_->tstatOk : 99u, (v[0] + v[9]) * .5f, (v[1] + v[10]) * .5f, (v[2] + v[11]) * .5f, sqrtf((v[3] - v[0]) * (v[3] - v[0]) + (v[5] - v[2]) * (v[5] - v[2]))); smkWrite(l_); } }   // №1063 служебно: чем светится карточка (журнал смоука, раз на экземпляр)
      if (!card1063Old && !fx_ && ((bfl >> 8) & 0xFF) == 2 && cardGlowInst(i)) { if (cardSpotDraw(v, col)) spot_ = 1; continue; }   // №1063: свечение карточки — векторное пятно оригинала (1,6 м, радиальный градиент)
      if (!GLOW_GLINT_OLD && ((bfl >> 8) & 0xFF) == 2 && (FX_GLINT846_OLD || !fx_)) { gq944Site = "coll"; glowQuadQ(v, col, (float)i * .41f + (float)b); continue; }   // №839: свечение предмета — бликом
      if (!nb++) {
        ScePspFMatrix4 I; memset(&I, 0, sizeof I); I.x.x = I.y.y = I.z.z = I.w.w = 1.f;
        sceGuSetMatrix(GU_MODEL, &I); sceGuTexScale(1.f, 1.f);
        sceGuDisable(GU_ALPHA_TEST); sceGuEnable(GU_BLEND); sceGuDepthMask(GU_TRUE); curFlags = -1; setCull(0);
      }
      int blm = (bfl >> 8) & 0xFF; g944Bill("coll_bill", blm, bt, v, col, i, b);   // №944
      if (blm == 2) sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_FIX, 0, 0xffffffff);
      else if (blm == 3) sceGuBlendFunc(GU_REVERSE_SUBTRACT, GU_SRC_ALPHA, GU_FIX, 0, 0xffffffff);
      else sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
      if (blm == 2 && !glowFogOld) sceGuDisable(GU_FOG); else if (!inR) sceGuEnable(GU_FOG);   // правка 76: свечение сложением без тумана (см. ниже)
      typedef struct { float u, v; unsigned c; float x, y, z; } BV;
      BV *q = (BV *)geVbAlloc(4 * sizeof(BV), GEVB_COLLECT_OLD);
      for (int k = 0; k < 4; k++) { q[k].u = (float)(k & 1); q[k].v = spBillTexV(k, spBillUvOld); q[k].c = col; q[k].x = v[3 * k]; q[k].y = v[3 * k + 1]; q[k].z = v[3 * k + 2]; }
      bindTexLean(bt);
      sceGuDrawArray(GU_TRIANGLE_STRIP, GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D, 4, 0, q);
    }
  }
  if (nb || spot_) { sceGuTexScale(128.f, 128.f); if (!inR) sceGuEnable(GU_FOG); sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0); resetState(); }
  glowGlintFlush(ex, ey, ez);   // №839
  for (unsigned g = 0; g < coll.ngag; g++) {   // блёстки над ненайденными гэгами (GagSetSparkle): как у монет, слот частиц 0
    const CollGag *G = &coll.gag[g];
    if (G->room != room || !collectSparkle(&coll, g)) continue;
    float dx = G->tx - ex, dz = G->tz - ez; if (dx * dx + dz * dz > 45.f * 45.f) continue;
    float sk = 1.f, ssz = 1.3f;
    if (!SPARKLE_FADE_OLD) {   // сверка g11b P16-13/Д8: сила = 1 - зажим((расстояние до игрока - 10 м) x 0,1, 0..1) — полная до 10 м, нулевая с 20 м (до положения гэга); размер 1,0 на улице и 0,5 в комнате (было 1,3)
      float gx = G->x - ex, gz = G->z - ez, gd = sqrtf(gx * gx + gz * gz);   // расстояние от камеры; игрок рядом с камерой — порт приближает до игрока ≈ глаз камеры
      sk = 1.f - fminf(1.f, fmaxf(0.f, (gd - 10.f) * .1f)); if (sk <= 0.f) continue;
      ssz = room >= 0 ? .5f : 1.f;
    }
    fxSparkleCA(G->tx, G->ty + .6f, G->tz, ssz, t * .5f + g * .37f, 0xffc8f0ff, BLING989_A * sk);   // №989: блёстки гэга прозрачнее (было fxSparkle, 1)
  }
  if (smkOn) { unsigned c3 = sceKernelGetSystemTimeLow(); cpA[0] += (cp1 - cp0) * .001; cpA[1] += (cp2 - cp1) * .001; cpA[2] += (c3 - cp2) * .001; cpA[3] += sp->ninst;
    if (++cpN >= 30) { char l[200]; snprintf(l, sizeof l, "COLLPROF gagskin=%.3f inst=%.3f bill_spark=%.3f ninst=%.0f ngag=%u ms/frame\n", cpA[0] / cpN, cpA[1] / cpN, cpA[2] / cpN, cpA[3] / cpN, coll.ngag); smkWrite(l); cpN = 0; memset(cpA, 0, sizeof cpA); } }
}

// Правка Роба 462: красное кольцо на земле у выключателя объекта (△ — подъёмник Duff, платформы АЭС). Без текстуры: плоский пояс из 24 звеньев
// в мире, пульс альфы 0,05–0,85 (≈0,8 Гц), видно до AO_RING_FAR. AO_SWITCH_OLD=1 — без кольца
typedef struct { unsigned c; float x, y, z; } AoRingV;
static void aoDrawRings(float ex, float ey, float ez, float t) {
  if (AO_SWITCH_OLD || !haveAo) return;
  (void)ey;
  for (unsigned k = 0; k < ao.nobj; k++) {
    float c[3], r; if (!aoSwitchRing(&ao, k, c, &r)) continue;
    float dx = c[0] - ex, dz = c[2] - ez; if (dx * dx + dz * dz > AO_RING_FAR * AO_RING_FAR) continue;
    enum { N = 24 };
    AoRingV *v = (AoRingV *)geVbAlloc((unsigned)(sizeof(AoRingV) * (N + 1) * 2), GEVB_COLLECT_OLD);
    float a = .45f + .4f * sinf(t * 5.f);
    unsigned col = ((unsigned)(a * 255.f) << 24) | 0x002020ffu;   // ABGR: красный
    for (int i = 0; i <= N; i++) {
      float an = (float)i * (2.f * GU_PI / N), s = sinf(an), co = cosf(an);
      v[2 * i].c = col;     v[2 * i].x = c[0] + co * r;         v[2 * i].y = c[1] + .08f; v[2 * i].z = c[2] + s * r;
      v[2 * i + 1].c = col; v[2 * i + 1].x = c[0] + co * r * .7f; v[2 * i + 1].y = c[1] + .08f; v[2 * i + 1].z = c[2] + s * r * .7f;
    }
    ScePspFMatrix4 I; memset(&I, 0, sizeof I); I.x.x = I.y.y = I.z.z = I.w.w = 1.f;
    sceGuSetMatrix(GU_MODEL, &I);
    sceGuDisable(GU_TEXTURE_2D); sceGuDisable(GU_CULL_FACE); sceGuEnable(GU_BLEND);
    sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
    sceGuDepthMask(GU_TRUE);
    sceGuDrawArray(GU_TRIANGLE_STRIP, GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D, (N + 1) * 2, 0, v);
    sceGuDepthMask(GU_FALSE); sceGuDisable(GU_BLEND); sceGuEnable(GU_TEXTURE_2D);
  }
}
