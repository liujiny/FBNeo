#include "../../src/burner/libretro/libretro-common/include/libretro.h"
#include <dlfcn.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdarg>
#include <cstdint>
#include <map>
#include <string>
#include <vector>
#include <chrono>
#include <filesystem>

static std::map<std::string,std::string> vars;
static std::string savedir;
static int frame, avmask=3, depth=16;
static unsigned pixel_format=RETRO_PIXEL_FORMAT_RGB565;
static uint64_t video_hash, audio_hash;
static FILE *hashes, *timings;
static std::vector<unsigned char> last_video;
static unsigned last_w,last_h;
static size_t last_pitch;
static double callback_seconds;
static bool playing, late_input;
static int renderCores=2;
static bool optionsChanged=false;
static double now() { return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count(); }
static uint64_t hash_bytes(const void *data,size_t len) {
 const unsigned char *p=(const unsigned char*)data;
 uint64_t h=1469598103934665603ULL;
 for(size_t i=0;i<len;i++) h=(h^p[i])*1099511628211ULL;
 return h;
}
static void logcb(enum retro_log_level level,const char *format,...) {
 if(level < RETRO_LOG_WARN) return;
 va_list ap; va_start(ap,format); vfprintf(stderr,format,ap); va_end(ap);
}
static bool environment(unsigned cmd,void *data) {
 switch(cmd) {
 case RETRO_ENVIRONMENT_GET_LOG_INTERFACE: ((retro_log_callback*)data)->log=logcb; return true;
 case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY:
 case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY: *(const char**)data=savedir.c_str(); return true;
 case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT:
   if(depth==16 && *(unsigned*)data==RETRO_PIXEL_FORMAT_XRGB8888) return false;
   pixel_format=*(unsigned*)data; return true;
 case RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION: *(unsigned*)data=0; return true;
 case RETRO_ENVIRONMENT_SET_VARIABLES: {
   for(retro_variable *v=(retro_variable*)data;v->key;v++) {
     std::string s=v->value; size_t p=s.find("; ");
     if(p!=std::string::npos) s=s.substr(p+2);
     s=s.substr(0,s.find('|')); vars[v->key]=s;
   } return true;
 }
 case RETRO_ENVIRONMENT_GET_VARIABLE: {
   retro_variable *v=(retro_variable*)data;
   if(!strcmp(v->key,"fbneo-xbox360-render-cores") || !strcmp(v->key,"fbneo-ps4-render-cores")) { static const char* values[]={"2","1","2","3"}; v->value=values[renderCores]; return true; }
   auto it=vars.find(v->key);
   v->value=it==vars.end()?nullptr:it->second.c_str(); return v->value!=nullptr;
 }
 case RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE: *(bool*)data=optionsChanged; optionsChanged=false; return true;
 case RETRO_ENVIRONMENT_GET_AUDIO_VIDEO_ENABLE: *(int*)data=avmask; return true;
 case RETRO_ENVIRONMENT_GET_CAN_DUPE: *(bool*)data=true; return true;
 case RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS:
 case RETRO_ENVIRONMENT_SET_CONTROLLER_INFO:
 case RETRO_ENVIRONMENT_SET_ROTATION:
 case RETRO_ENVIRONMENT_SET_GEOMETRY:
 case RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO:
 case RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME: return true;
 default: return false;
 }
}
static void video(const void *data,unsigned w,unsigned h,size_t pitch) {
 if(!data) return;
 double t=now();
 if(hashes) video_hash=hash_bytes(data,pitch*h);
 if(frame%1000==999) {
   last_video.assign((const unsigned char*)data,(const unsigned char*)data+pitch*h);
   last_w=w;last_h=h;last_pitch=pitch;
 }
 callback_seconds+=now()-t;
}
static size_t audio(const int16_t *data,size_t frames) {
 double t=now();if(hashes) audio_hash=hash_bytes(data,frames*4);callback_seconds+=now()-t;return frames;
}
static void audio_sample(int16_t,int16_t) {}
static void poll() {}
static int16_t input(unsigned port,unsigned device,unsigned,unsigned id) {
 if(!playing || port || device!=RETRO_DEVICE_JOYPAD) return 0;
 unsigned mask=0;
 if(late_input) {
   // CV1000 may ignore credits during the initial RAM test. Retry after
   // boot, with shooting and periodic bombs to exercise blended sprites.
   if(frame<600) return 0;
   int phase=frame%600;
   if(phase<5) mask |= 1<<RETRO_DEVICE_ID_JOYPAD_SELECT;
   if(phase>=30 && phase<35) mask |= 1<<RETRO_DEVICE_ID_JOYPAD_START;
   if(frame%30<25) mask |= 1<<RETRO_DEVICE_ID_JOYPAD_B;
   if(frame%240<5) mask |= 1<<RETRO_DEVICE_ID_JOYPAD_A;
   mask |= 1<<((frame/120)%2 ? RETRO_DEVICE_ID_JOYPAD_LEFT:RETRO_DEVICE_ID_JOYPAD_RIGHT);
   return id==RETRO_DEVICE_ID_JOYPAD_MASK ? mask : (mask>>id)&1;
 }
 if(frame<5) mask |= 1<<RETRO_DEVICE_ID_JOYPAD_SELECT;
 if(frame>=30 && frame<35) mask |= 1<<RETRO_DEVICE_ID_JOYPAD_START;
 if(frame>=70) mask |= 1<<RETRO_DEVICE_ID_JOYPAD_B;
 if(frame>=160) mask |= 1<<((frame/120)%2 ? RETRO_DEVICE_ID_JOYPAD_LEFT:RETRO_DEVICE_ID_JOYPAD_RIGHT);
 if(id==RETRO_DEVICE_ID_JOYPAD_MASK) return mask;
 return (mask>>id)&1;
}
static std::vector<unsigned char> readfile(const char *path) {
 FILE *f=fopen(path,"rb");if(!f) {perror(path);exit(2);}fseek(f,0,SEEK_END);long n=ftell(f);rewind(f);
 std::vector<unsigned char> b(n);if(fread(b.data(),1,n,f)!=(size_t)n) exit(2);fclose(f);return b;
}
static void writefile(const char *path,const void *p,size_t n) {
 FILE *f=fopen(path,"wb");if(!f) {perror(path);exit(2);}fwrite(p,1,n,f);fclose(f);
}
int main(int argc,char **argv) {
 if(argc<7) {fprintf(stderr,"core rom output_dir frames depth avmask [state_in|-] [play] [hash]\n");return 2;}
 late_input=getenv("FBNEO_REPLAY_LATE_INPUT")!=nullptr;
 renderCores=argc>13?atoi(argv[13]):2;
 if(renderCores<1 || renderCores>3) return 2;
 savedir=argv[3];std::filesystem::create_directories(savedir);
 if(getenv("SALVIA_TIMING_CSV")) timings=fopen(getenv("SALVIA_TIMING_CSV"),"w");
 int frames=atoi(argv[4]);depth=atoi(argv[5]);avmask=atoi(argv[6]);playing=argc>8 && atoi(argv[8]);
 if(argc>9 && atoi(argv[9])) hashes=fopen((savedir+"/hashes.txt").c_str(),"w");
 void *lib=dlopen(argv[1],RTLD_NOW|RTLD_LOCAL);if(!lib){fprintf(stderr,"%s\n",dlerror());return 2;}
#define LOAD(name) auto name=(decltype(&::name))dlsym(lib,#name)
 LOAD(retro_set_environment);LOAD(retro_set_video_refresh);LOAD(retro_set_audio_sample_batch);
 LOAD(retro_set_audio_sample);LOAD(retro_set_input_poll);LOAD(retro_set_input_state);
 LOAD(retro_init);LOAD(retro_load_game);LOAD(retro_run);LOAD(retro_unload_game);LOAD(retro_deinit);LOAD(retro_reset);
 LOAD(retro_serialize_size);LOAD(retro_serialize);LOAD(retro_unserialize);LOAD(retro_get_system_av_info);
 retro_set_environment(environment);retro_set_video_refresh(video);retro_set_audio_sample_batch(audio);
 retro_set_audio_sample(audio_sample);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();
 retro_game_info game={argv[2],nullptr,0,nullptr};
 if(!retro_load_game(&game)) {fprintf(stderr,"load failed\n");return 3;}
 retro_system_av_info info;retro_get_system_av_info(&info);printf("fps %.3f format %u\n",info.timing.fps,pixel_format);fflush(stdout);
 if(argc>7 && strcmp(argv[7],"-")) {auto state=readfile(argv[7]);if(!retro_unserialize(state.data(),state.size())) return 4;}
 if(argc>10 && atoi(argv[10])) retro_reset();
 double t=now(),core_time=0;
 std::vector<unsigned char> checkpoint;
 for(frame=0;frame<frames;frame++) {
   if(argc>14 && atoi(argv[14]) && (frame==700 || frame==1400 || frame==2100)) { renderCores=frame==700?3:frame==1400?1:2; optionsChanged=true; }
   if(argc>12 && atoi(argv[12])) {
    if(frame==300) retro_reset();
    if(frame==1200) { checkpoint.resize(retro_serialize_size()); if(!retro_serialize(checkpoint.data(),checkpoint.size())) return 7; }
    if(frame==1600 && !retro_unserialize(checkpoint.data(),checkpoint.size())) return 8;
    avmask=(frame>=1800 && frame<1900)?2:3;
   }
   double start=now();callback_seconds=0;retro_run();double elapsed=now()-start-callback_seconds;core_time+=elapsed;
   if(timings) fprintf(timings,"%d,%.9f\n",frame,elapsed*1000);
   if(hashes) fprintf(hashes,"%d %016llx %016llx\n",frame,(unsigned long long)video_hash,(unsigned long long)audio_hash);
   if(getenv("SALVIA_SNAPSHOTS") && frame%1000==999) {
     std::string checkpointdir=savedir+"/checkpoint-"+std::to_string(frame+1);std::filesystem::create_directories(checkpointdir);
     std::vector<unsigned char> snapshot(retro_serialize_size());
     if(retro_serialize(snapshot.data(),snapshot.size())) writefile((checkpointdir+"/end.state").c_str(),snapshot.data(),snapshot.size());
     if(!last_video.empty()) {
       writefile((checkpointdir+"/frame.raw").c_str(),last_video.data(),last_video.size());
       FILE *m=fopen((checkpointdir+"/frame.meta").c_str(),"w");fprintf(m,"%u %u %zu %u\n",last_w,last_h,last_pitch,pixel_format);fclose(m);
     }
   }
   if(frame%1000==999) {printf("frame %d core_ms %.3f wall %.2f\n",frame+1,core_time*1000/(frame+1),now()-t);fflush(stdout);}
 }
 if(hashes) fclose(hashes);
 if(timings) fclose(timings);
 if(argc>11 && atoi(argv[11])) { auto stress=(int (*)(int))dlsym(lib,"retro_sound_stress"); if(!stress || stress(atoi(argv[11]))) return 6; }
 std::vector<unsigned char> state(retro_serialize_size());
 if(retro_serialize(state.data(),state.size())) writefile((savedir+"/end.state").c_str(),state.data(),state.size());
 if(!last_video.empty()) {
   writefile((savedir+"/frame.raw").c_str(),last_video.data(),last_video.size());
   FILE *f=fopen((savedir+"/frame.meta").c_str(),"w");fprintf(f,"%u %u %zu %u\n",last_w,last_h,last_pitch,pixel_format);fclose(f);
 }
 printf("total frames %d core_ms %.4f core_fps %.1f\n",frames,core_time*1000/frames,frames/core_time);
 retro_unload_game();retro_deinit();dlclose(lib);return 0;
}
