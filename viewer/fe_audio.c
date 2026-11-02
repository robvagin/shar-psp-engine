#include "fe_audio.h"
#include "audio.h"
#include <string.h>
#ifndef FE_AUDIO_OLD
#define FE_AUDIO_OLD 0
#endif
#ifndef FE_AUDIO_PREPARE_OLD
#define FE_AUDIO_PREPARE_OLD 0
#endif
static const char *const names[]={"FE_Gag_Grandpa","FE_Gag_Moleman","FE_Gag_Frink","FE_Gag_Barney","FE_Gag_Nick","FE_Gag_Snake","FE_Gag_Maggie",
 "FE_Gag_Homer_ScratchHead","FE_Gag_Homer_ScratchBum","FE_Gag_Homer_Yawn","FE_Gag_Homer_Nightmare","FE_Gag_Homer_Stretch"};
static int ids[12],loaded,pending=-1,selectedIndex=-1,selectedSegment=-1,status;
static unsigned serial;
static int choice(int index,int segment){return index==0?(segment>=0&&segment<5?7+segment:-1):(index>=1&&index<8?index-1:-1);}
const char *feAudioName(int index,int segment){int k=choice(index,segment);return k>=0?names[k]:"";}
int feAudioStatus(void){return status;}
int feAudioPrepared(void){return pending>=0&&serial==audioStreamRequestSerial(AUDIO_ST_DIALOG)&&audioStreamPrepared(AUDIO_ST_DIALOG,pending);}
void feAudioStop(void){
 if(pending>=0&&serial==audioStreamRequestSerial(AUDIO_ST_DIALOG)&&audioStreamTrack(AUDIO_ST_DIALOG)==pending)audioStream(AUDIO_ST_DIALOG,-1,0);
 pending=-1;selectedIndex=selectedSegment=-1;status=0;
}
void feAudioFree(void){feAudioStop();loaded=0;}
int feAudioLoad(const char *path){
 feAudioFree();int n=audioExtraStreams(path);if(n<12){status=-1;return -1;}   /* №1104: после 12 реплик меню в пакете могут идти реплики титров (KK_*) */
 for(int i=0;i<12;i++){ids[i]=audioExtraTrack(names[i]);if(ids[i]<0){status=-1;return -1;}}
 loaded=1;return n;
}
void feAudioEvent(int event,int index,int segment){
 status=0;if(FE_AUDIO_OLD)return;
 int k=choice(index,segment);if(!loaded||k<0){status=-1;return;}
 if(event==180){
  int active=audioStreamTrack(AUDIO_ST_DIALOG);
  if(active>=0&&active!=pending){status=-2;return;} /* never overwrite mission speech */
  selectedIndex=index;selectedSegment=index?0:segment;pending=ids[k];
  audioStreamSet(AUDIO_ST_DIALOG,1,0); /* authored trim/pitch1; missing trim is documented default */
  if(FE_AUDIO_PREPARE_OLD)audioStream(AUDIO_ST_DIALOG,pending,0);
  else if(!audioStreamPrepare(AUDIO_ST_DIALOG,pending)){pending=-1;status=-1;return;}
  serial=audioStreamRequestSerial(AUDIO_ST_DIALOG);status=1;
 }else if(event==181){
  if(index!=selectedIndex||(index==0&&segment!=selectedSegment)||pending!=ids[k]||serial!=audioStreamRequestSerial(AUDIO_ST_DIALOG)){status=-2;return;}
  status=audioStreamStartPrepared(AUDIO_ST_DIALOG,pending)?1:-2;
 }
}
