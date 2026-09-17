/* SarGPU audio module. Compiled through sargpu.c; do not compile separately. */
#define DR_MP3_NO_STDIO
#define DR_MP3_NO_SIMD
#define DRMP3_ASSERT(x) ((void)0)
#ifndef INT_MAX
#define INT_MAX 2147483647
#endif
#define DRMP3_MALLOC(sz) MemAlloc((unsigned int)(sz))
#define DRMP3_REALLOC(p,sz) MemRealloc((p),(unsigned int)(sz))
#define DRMP3_FREE(p) MemFree(p)
#define DR_MP3_IMPLEMENTATION
#include "external/dr_mp3.h"
#undef DR_MP3_IMPLEMENTATION
#define DR_FLAC_NO_STDIO
#define DR_FLAC_NO_SIMD
#define DRFLAC_ASSERT(x) ((void)0)
#define DRFLAC_MALLOC(sz) MemAlloc((unsigned int)(sz))
#define DRFLAC_REALLOC(p,sz) MemRealloc((p),(unsigned int)(sz))
#define DRFLAC_FREE(p) MemFree(p)
#define DR_FLAC_IMPLEMENTATION
#include "external/dr_flac.h"
#undef DR_FLAC_IMPLEMENTATION
#define QOA_NO_STDIO
#define QOA_MALLOC(sz) MemAlloc((unsigned int)(sz))
#define QOA_FREE(p) MemFree(p)
#define QOA_IMPLEMENTATION
#include "external/qoa.h"
#undef QOA_IMPLEMENTATION
#ifdef __wasm__
#define malloc(sz) MemAlloc((unsigned int)(sz))
#define realloc(p,sz) MemRealloc((p),(unsigned int)(sz))
#define free(p) MemFree(p)
#define alloca(sz) __builtin_alloca(sz)
#define assert(x) ((void)0)
#define cos(x) cosf((float)(x))
#define sin(x) sinf((float)(x))
#define pow(x,y) mr_web_powf((float)(x),(float)(y))
#define log(x) mr_web_logf((float)(x))
#define exp(x) mr_web_expf((float)(x))
#define floor(x) mr_web_floorf((float)(x))
#define ldexp(x,e) mr_web_ldexpf((float)(x),(e))
#define abs(x) ((x)<0?-(x):(x))
#endif
#define STB_VORBIS_NO_STDIO
#define STB_VORBIS_NO_PUSHDATA_API
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wtautological-compare"
#endif
#include "external/stb_vorbis.c"
#ifdef __clang__
#pragma clang diagnostic pop
#endif
#ifdef __wasm__
#undef malloc
#undef realloc
#undef free
#undef alloca
#undef assert
#undef cos
#undef sin
#undef pow
#undef log
#undef exp
#undef floor
#undef ldexp
#undef abs
#endif
#undef L
#undef C
#undef R
#undef PLAYBACK_MONO
#undef PLAYBACK_LEFT
#undef PLAYBACK_RIGHT
/* Bundled tracker decoders. File I/O is disabled because SarGPU feeds memory. */
#define JARXM_MALLOC(sz) MemAlloc((unsigned int)(sz))
#define JARXM_FREE(p) MemFree(p)
#define JAR_XM_NO_STDIO
#ifdef __wasm__
#define JAR_XM_NO_CRT
typedef struct { int quot,rem; } mr_div_t;
#define div_t mr_div_t
#define div(a,b) ((mr_div_t){(a)/(b),(a)%(b)})
#define powf(x,y) mr_web_powf((x),(y))
#define floor(x) mr_web_floorf((float)(x))
#define fabs(x) ((x)<0?-(x):(x))
#endif
#define JAR_XM_IMPLEMENTATION
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wsign-compare"
#pragma clang diagnostic ignored "-Wunused-parameter"
#endif
#include "external/jar_xm.h"
#ifdef __clang__
#pragma clang diagnostic pop
#endif
#undef JAR_XM_IMPLEMENTATION
#ifdef __wasm__
#undef div_t
#undef div
#undef powf
#undef floor
#undef fabs
#endif
#define JARMOD_MALLOC(sz) MemAlloc((unsigned int)(sz))
#define JARMOD_FREE(p) MemFree(p)
#define JAR_MOD_NO_STDIO
#ifdef __wasm__
#define JAR_MOD_NO_CRT
#endif
#define JAR_MOD_IMPLEMENTATION
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-parameter"
#endif
#include "external/jar_mod.h"
#ifdef __clang__
#pragma clang diagnostic pop
#endif
#undef JAR_MOD_IMPLEMENTATION
static float mr_clamp01(float value);
struct rAudioBuffer {
    unsigned int id,frameCount,sampleRate,sampleSize,channels,dataSize;
    unsigned char *data; float volume,pitch,pan; bool paused;
    unsigned int startFrame; double startedAt,pausedAt; AudioCallback callback; bool streaming,streamStarted;
    rAudioProcessor *processors;
#ifdef _WIN32
    HWAVEOUT output; WAVEHDR header;
    WAVEHDR streamHeaders[2]; unsigned char *streamData[2]; unsigned int streamCapacity[2]; int streamNext;
#endif
};
struct rAudioProcessor { AudioCallback callback; rAudioProcessor *next; };
#define MR_MAX_SOUNDS 256
static rAudioBuffer *mr_sounds[MR_MAX_SOUNDS];
static unsigned int mr_next_sound;
static bool mr_audio_ready;
static float mr_master_volume=1.0f;
static int mr_stream_buffer_frames=4096;
static rAudioProcessor *mr_mixed_processors;
static unsigned int mr_u16(const unsigned char *p) { return (unsigned int)p[0]|((unsigned int)p[1]<<8); }
static unsigned int mr_u32(const unsigned char *p) { return mr_u16(p)|(mr_u16(p+2)<<16); }
bool IsWaveValid(Wave wave) { return wave.data && wave.frameCount>0 && wave.sampleRate>0 && wave.channels>0 && (wave.sampleSize==8||wave.sampleSize==16||wave.sampleSize==32); }
Wave LoadWaveFromMemory(const char *fileType,const unsigned char *data,int size) {
    if(!data||size<=0)return(Wave){0};
    if((fileType&&IsFileExtension(fileType,".ogg"))||(size>=4&&memcmp(data,"OggS",4)==0)){
        int error=0;stb_vorbis *vorbis=stb_vorbis_open_memory(data,size,&error,NULL);if(!vorbis)return(Wave){0};
        stb_vorbis_info info=stb_vorbis_get_info(vorbis);unsigned int frames=stb_vorbis_stream_length_in_samples(vorbis);
        if(!frames||info.channels<=0||info.sample_rate==0||frames>(unsigned int)INT_MAX/(unsigned int)info.channels){stb_vorbis_close(vorbis);return(Wave){0};}
        short *samples=MemAlloc(frames*(unsigned int)info.channels*sizeof(short));if(!samples){stb_vorbis_close(vorbis);return(Wave){0};}
        int decoded=stb_vorbis_get_samples_short_interleaved(vorbis,info.channels,samples,(int)(frames*(unsigned int)info.channels));stb_vorbis_close(vorbis);
        if(decoded<=0){MemFree(samples);return(Wave){0};}return(Wave){(unsigned int)decoded,info.sample_rate,16,(unsigned int)info.channels,samples};
    }
    if((fileType&&IsFileExtension(fileType,".mp3"))||(size>=3&&memcmp(data,"ID3",3)==0)){
        drmp3_config config={0};drmp3_uint64 frames=0;drmp3_int16 *samples=drmp3_open_memory_and_read_pcm_frames_s16(data,(size_t)size,&config,&frames,NULL);
        if(!samples||!frames||!config.channels||!config.sampleRate||frames>0xffffffffu){MemFree(samples);return(Wave){0};}
        return(Wave){(unsigned int)frames,config.sampleRate,16,config.channels,samples};
    }
    if((fileType&&IsFileExtension(fileType,".flac"))||(size>=4&&memcmp(data,"fLaC",4)==0)){
        unsigned int channels=0,rate=0;drflac_uint64 frames=0;drflac_int16 *samples=drflac_open_memory_and_read_pcm_frames_s16(data,(size_t)size,&channels,&rate,&frames,NULL);
        if(!samples||!frames||!channels||!rate||frames>0xffffffffu){MemFree(samples);return(Wave){0};}
        return(Wave){(unsigned int)frames,rate,16,channels,samples};
    }
    if((fileType&&IsFileExtension(fileType,".qoa"))||(size>=4&&memcmp(data,"qoaf",4)==0)){
        qoa_desc description={0};short *samples=qoa_decode(data,size,&description);
        if(!samples||!description.samples||!description.channels||!description.samplerate){MemFree(samples);return(Wave){0};}
        return(Wave){description.samples,description.samplerate,16,description.channels,samples};
    }
    if(fileType&&IsFileExtension(fileType,".xm")){
        jar_xm_context_t *context=NULL;if(jar_xm_create_context_safe(&context,(const char*)data,(size_t)size,48000)!=0||!context)return(Wave){0};
        jar_xm_set_max_loop_count(context,1);uint64_t frames=jar_xm_get_remaining_samples(context);if(!frames||frames>0xffffffffu||frames>SIZE_MAX/(sizeof(float)*2)){jar_xm_free_context(context);return(Wave){0};}
        float *samples=MemAlloc((unsigned int)(frames*2*sizeof(float)));if(!samples){jar_xm_free_context(context);return(Wave){0};}jar_xm_generate_samples(context,samples,(size_t)frames);jar_xm_free_context(context);return(Wave){(unsigned int)frames,48000,32,2,samples};
    }
    if(fileType&&IsFileExtension(fileType,".mod")){
        jar_mod_context_t context;if(!jar_mod_init(&context)||!jar_mod_setcfg(&context,48000,16,1,1,1))return(Wave){0};
        unsigned char *owned=MemAlloc((unsigned int)size);if(!owned)return(Wave){0};memcpy(owned,data,(size_t)size);if(!jar_mod_load(&context,owned,size)){MemFree(owned);return(Wave){0};}context.modfile=owned;context.modfilesize=(mulong)size;
        mulong length=jar_mod_max_samples(&context);if(!length||length>0x3fffffffu){jar_mod_unload(&context);return(Wave){0};}short *samples=MemAlloc((unsigned int)((size_t)length*2*sizeof(short)));if(!samples){jar_mod_unload(&context);return(Wave){0};}jar_mod_fillbuffer(&context,samples,length,NULL);jar_mod_unload(&context);return(Wave){(unsigned int)length,48000,16,2,samples};
    }
    if(size<44||memcmp(data,"RIFF",4)!=0||memcmp(data+8,"WAVE",4)!=0)return(Wave){0};
    unsigned int format=0,channels=0,rate=0,bits=0;const unsigned char *samples=NULL;unsigned int sampleBytes=0;
    for(int at=12;at+8<=size;){unsigned int length=mr_u32(data+at+4);int next=at+8+(int)length+(length&1);if(next<at||next>size)break;
        if(memcmp(data+at,"fmt ",4)==0&&length>=16){format=mr_u16(data+at+8);channels=mr_u16(data+at+10);rate=mr_u32(data+at+12);bits=mr_u16(data+at+22);if(format==0xfffe&&length>=40)format=mr_u16(data+at+32);}
        else if(memcmp(data+at,"data",4)==0){samples=data+at+8;sampleBytes=length;}
        at=next;
    }
    if(!samples||!channels||!rate||(format!=1&&format!=3)||(bits!=8&&bits!=16&&bits!=24&&bits!=32))return(Wave){0};
    unsigned int frames=sampleBytes/(channels*(bits/8));if(!frames)return(Wave){0};
    if(bits==24){float *converted=MemAlloc(frames*channels*sizeof(float));if(!converted)return(Wave){0};for(unsigned int i=0;i<frames*channels;i++){const unsigned char*p=samples+i*3;int value=(int)p[0]|((int)p[1]<<8)|((int)p[2]<<16);if(value&0x800000)value|=~0xffffff;converted[i]=value/8388608.0f;}return(Wave){frames,rate,32,channels,converted};}
    unsigned int bytes=frames*channels*(bits/8);void *copy=MemAlloc(bytes);if(!copy)return(Wave){0};memcpy(copy,samples,bytes);return(Wave){frames,rate,bits,channels,copy};
}
Wave LoadWave(const char *fileName) { int size=0;unsigned char *data=LoadFileData(fileName,&size);if(!data)return(Wave){0};Wave wave=LoadWaveFromMemory(GetFileExtension(fileName),data,size);UnloadFileData(data);return wave; }
void UnloadWave(Wave wave) { MemFree(wave.data); }
Wave WaveCopy(Wave wave) { if(!IsWaveValid(wave))return(Wave){0};unsigned int bytes=wave.frameCount*wave.channels*(wave.sampleSize/8);void*data=MemAlloc(bytes);if(!data)return(Wave){0};memcpy(data,wave.data,bytes);wave.data=data;return wave; }
void WaveCrop(Wave *wave,int first,int final) { if(!wave||!IsWaveValid(*wave))return;if(first<0)first=0;if(final>(int)wave->frameCount)final=(int)wave->frameCount;if(final<=first)return;unsigned int stride=wave->channels*(wave->sampleSize/8),frames=(unsigned int)(final-first);void*data=MemAlloc(frames*stride);if(!data)return;memcpy(data,(unsigned char*)wave->data+first*stride,frames*stride);MemFree(wave->data);wave->data=data;wave->frameCount=frames; }
float *LoadWaveSamples(Wave wave) { if(!IsWaveValid(wave))return NULL;unsigned int count=wave.frameCount*wave.channels;float*out=MemAlloc(count*sizeof(float));if(!out)return NULL;unsigned char*p=wave.data;for(unsigned int i=0;i<count;i++){if(wave.sampleSize==8)out[i]=((int)p[i]-128)/128.0f;else if(wave.sampleSize==16){int value=(int)(p[i*2]|(p[i*2+1]<<8));if(value&0x8000)value-=0x10000;out[i]=value/32768.0f;}else memcpy(&out[i],p+i*4,4);}return out; }
void UnloadWaveSamples(float *samples) { MemFree(samples); }
static void mr_w32(unsigned char *p,unsigned int value){p[0]=(unsigned char)value;p[1]=(unsigned char)(value>>8);p[2]=(unsigned char)(value>>16);p[3]=(unsigned char)(value>>24);}
static void mr_w16(unsigned char *p,unsigned int value){p[0]=(unsigned char)value;p[1]=(unsigned char)(value>>8);}
static unsigned char *mr_wave_file(Wave wave,unsigned int *outSize){
    if(outSize)*outSize=0;if(!IsWaveValid(wave))return NULL;unsigned int stride=wave.channels*(wave.sampleSize/8);
    if(wave.frameCount>0xffffffffu/stride)return NULL;unsigned int dataSize=wave.frameCount*stride;if(dataSize>0xffffffffu-44)return NULL;
    unsigned char *out=MemAlloc(dataSize+44);if(!out)return NULL;memcpy(out,"RIFF",4);mr_w32(out+4,dataSize+36);memcpy(out+8,"WAVEfmt ",8);mr_w32(out+16,16);
    mr_w16(out+20,wave.sampleSize==32?3:1);mr_w16(out+22,wave.channels);mr_w32(out+24,wave.sampleRate);mr_w32(out+28,wave.sampleRate*stride);mr_w16(out+32,stride);mr_w16(out+34,wave.sampleSize);memcpy(out+36,"data",4);mr_w32(out+40,dataSize);memcpy(out+44,wave.data,dataSize);if(outSize)*outSize=dataSize+44;return out;
}
bool ExportWave(Wave wave,const char *fileName){unsigned int size=0;unsigned char *data=mr_wave_file(wave,&size);if(!data)return false;bool result=SaveFileData(fileName,data,(int)size);MemFree(data);return result;}
bool ExportWaveAsCode(Wave wave,const char *fileName){
    unsigned int size=0;unsigned char *data=mr_wave_file(wave,&size);if(!data)return false;size_t capacity=(size_t)size*6+256;char *text=MemAlloc((unsigned int)capacity);if(!text){MemFree(data);return false;}
    const char *head="/* Wave data exported by SarGPU */\nstatic const unsigned char WAVE_DATA[] = {\n";size_t at=0;while(head[at]){text[at]=head[at];at++;}
    static const char hex[]="0123456789abcdef";for(unsigned int i=0;i<size;i++){if((i&15)==0){text[at++]=' ';text[at++]=' ';}text[at++]='0';text[at++]='x';text[at++]=hex[data[i]>>4];text[at++]=hex[data[i]&15];if(i+1<size)text[at++]=',';if((i&15)==15||i+1==size)text[at++]='\n';}
    const char *tail="};\nstatic const unsigned int WAVE_DATA_SIZE = sizeof(WAVE_DATA);\n";for(size_t i=0;tail[i];i++)text[at++]=tail[i];bool result=SaveFileData(fileName,text,(int)at);MemFree(text);MemFree(data);return result;
}
#ifdef _WIN32
static void mr_stop_sound(rAudioBuffer *buffer) { if(!buffer||!buffer->output)return;waveOutReset(buffer->output);if(buffer->streaming){for(int i=0;i<2;i++){if(buffer->streamHeaders[i].dwFlags&WHDR_PREPARED)waveOutUnprepareHeader(buffer->output,&buffer->streamHeaders[i],sizeof(WAVEHDR));memset(&buffer->streamHeaders[i],0,sizeof(WAVEHDR));}}else waveOutUnprepareHeader(buffer->output,&buffer->header,sizeof buffer->header);waveOutClose(buffer->output);buffer->output=NULL;memset(&buffer->header,0,sizeof buffer->header);buffer->paused=false;buffer->streamStarted=false; }
static void mr_sound_levels(rAudioBuffer *buffer) { if(!buffer||!buffer->output)return;float volume=mr_clamp01(buffer->volume*mr_master_volume),pan=mr_clamp01(buffer->pan);float left=volume*(pan<=0.5f?1.0f:2.0f*(1.0f-pan)),right=volume*(pan>=0.5f?1.0f:2.0f*pan);DWORD packed=(DWORD)(left*65535.0f)|((DWORD)(right*65535.0f)<<16);waveOutSetVolume(buffer->output,packed);waveOutSetPlaybackRate(buffer->output,(DWORD)(buffer->pitch*65536.0f)); }
static bool mr_open_audio_output(rAudioBuffer *b){if(b->output)return true;WAVEFORMATEX format={0};format.wFormatTag=b->sampleSize==32?WAVE_FORMAT_IEEE_FLOAT:WAVE_FORMAT_PCM;format.nChannels=(WORD)b->channels;format.nSamplesPerSec=b->sampleRate;format.wBitsPerSample=(WORD)b->sampleSize;format.nBlockAlign=(WORD)(b->channels*b->sampleSize/8);format.nAvgBytesPerSec=format.nSamplesPerSec*format.nBlockAlign;if(waveOutOpen(&b->output,WAVE_MAPPER,&format,0,0,CALLBACK_NULL)!=MMSYSERR_NOERROR){b->output=NULL;return false;}mr_sound_levels(b);return true;}
static bool mr_queue_audio_stream(rAudioBuffer *b,const void *data,unsigned int bytes){if(!b||!data||!bytes||!mr_open_audio_output(b))return false;for(int attempt=0;attempt<2;attempt++){int slot=(b->streamNext+attempt)&1;WAVEHDR *header=&b->streamHeaders[slot];if((header->dwFlags&WHDR_INQUEUE)&&!(header->dwFlags&WHDR_DONE))continue;if(header->dwFlags&WHDR_PREPARED)waveOutUnprepareHeader(b->output,header,sizeof*header);if(bytes>b->streamCapacity[slot]){unsigned char *grown=MemRealloc(b->streamData[slot],bytes);if(!grown)return false;b->streamData[slot]=grown;b->streamCapacity[slot]=bytes;}memcpy(b->streamData[slot],data,bytes);memset(header,0,sizeof*header);header->lpData=(LPSTR)b->streamData[slot];header->dwBufferLength=bytes;if(waveOutPrepareHeader(b->output,header,sizeof*header)!=MMSYSERR_NOERROR)return false;if(waveOutWrite(b->output,header,sizeof*header)!=MMSYSERR_NOERROR)return false;b->streamNext=(slot+1)&1;b->streamStarted=true;return true;}return false;}
#endif
void InitAudioDevice(void) { if(mr_audio_ready)return;
#ifdef __wasm__
    mr_audio_ready=mr_web_audio_init()!=0;
#else
    mr_audio_ready=true;
#endif
}
bool IsAudioDeviceReady(void) { return mr_audio_ready; }
float GetMasterVolume(void) { return mr_master_volume; }
void SetMasterVolume(float volume) { mr_master_volume=mr_clamp01(volume);
#ifdef __wasm__
    if(mr_audio_ready)mr_web_audio_command(0,7,mr_master_volume);
#else
    for(int i=0;i<MR_MAX_SOUNDS;i++)if(mr_sounds[i])mr_sound_levels(mr_sounds[i]);
#endif
}
Sound LoadSoundFromWave(Wave wave) { if(!mr_audio_ready||!IsWaveValid(wave))return(Sound){0};int slot=-1;for(int i=0;i<MR_MAX_SOUNDS;i++)if(!mr_sounds[i]){slot=i;break;}if(slot<0)return(Sound){0};rAudioBuffer*b=MemAlloc(sizeof*b);if(!b)return(Sound){0};memset(b,0,sizeof*b);b->id=++mr_next_sound;b->frameCount=wave.frameCount;b->sampleRate=wave.sampleRate;b->sampleSize=wave.sampleSize;b->channels=wave.channels;b->dataSize=wave.frameCount*wave.channels*(wave.sampleSize/8);b->data=MemAlloc(b->dataSize);if(!b->data){MemFree(b);return(Sound){0};}memcpy(b->data,wave.data,b->dataSize);b->volume=1;b->pitch=1;b->pan=0.5f;mr_sounds[slot]=b;
#ifdef __wasm__
    mr_web_audio_load(b->id,b->data,b->frameCount,b->sampleRate,b->sampleSize,b->channels);
#endif
    return(Sound){{b,NULL,b->sampleRate,b->sampleSize,b->channels},b->frameCount}; }
Sound LoadSound(const char *fileName) { Wave wave=LoadWave(fileName);Sound sound=LoadSoundFromWave(wave);UnloadWave(wave);return sound; }
bool IsSoundValid(Sound sound) { return sound.stream.buffer&&sound.frameCount>0; }
void StopSound(Sound sound) { if(!IsSoundValid(sound))return;
#ifdef __wasm__
    mr_web_audio_command(sound.stream.buffer->id,1,0);
#else
    mr_stop_sound(sound.stream.buffer);
#endif
    sound.stream.buffer->streamStarted=false;
}
void PlaySound(Sound sound) { if(!mr_audio_ready||!IsSoundValid(sound))return;rAudioBuffer*b=sound.stream.buffer;
#ifdef __wasm__
    if(b->streaming)mr_web_audio_command(b->id,9,1);mr_web_audio_command(b->id,8,(float)b->startFrame/b->sampleRate);mr_web_audio_command(b->id,0,0);b->streamStarted=true;
#else
    if(b->streaming){if(!b->streamStarted)mr_queue_audio_stream(b,b->data,b->frameCount*b->channels*(b->sampleSize/8));}else{mr_stop_sound(b);if(!mr_open_audio_output(b))return;unsigned int stride=b->channels*b->sampleSize/8,offset=b->startFrame*stride;if(offset>b->dataSize)offset=b->dataSize;b->header.lpData=(LPSTR)(b->data+offset);b->header.dwBufferLength=b->dataSize-offset;waveOutPrepareHeader(b->output,&b->header,sizeof b->header);waveOutWrite(b->output,&b->header,sizeof b->header);}
#endif
    b->startedAt=GetTime();b->pausedAt=0;
}
void PauseSound(Sound sound) { if(!IsSoundValid(sound))return;sound.stream.buffer->paused=true;
#ifdef __wasm__
    mr_web_audio_command(sound.stream.buffer->id,2,0);
#else
    if(sound.stream.buffer->output)waveOutPause(sound.stream.buffer->output);
#endif
}
void ResumeSound(Sound sound) { if(!IsSoundValid(sound))return;sound.stream.buffer->paused=false;
#ifdef __wasm__
    mr_web_audio_command(sound.stream.buffer->id,3,0);
#else
    if(sound.stream.buffer->output)waveOutRestart(sound.stream.buffer->output);
#endif
}
bool IsSoundPlaying(Sound sound) { if(!IsSoundValid(sound)||sound.stream.buffer->paused)return false;
#ifdef __wasm__
    return mr_web_audio_playing(sound.stream.buffer->id)!=0;
#else
    if(!sound.stream.buffer->output)return false;if(sound.stream.buffer->streaming)return ((sound.stream.buffer->streamHeaders[0].dwFlags&WHDR_INQUEUE)&&!(sound.stream.buffer->streamHeaders[0].dwFlags&WHDR_DONE))||((sound.stream.buffer->streamHeaders[1].dwFlags&WHDR_INQUEUE)&&!(sound.stream.buffer->streamHeaders[1].dwFlags&WHDR_DONE));return(sound.stream.buffer->header.dwFlags&WHDR_DONE)==0;
#endif
}
void SetSoundVolume(Sound sound,float value) { if(!IsSoundValid(sound))return;sound.stream.buffer->volume=mr_clamp01(value);
#ifdef __wasm__
    mr_web_audio_command(sound.stream.buffer->id,4,sound.stream.buffer->volume);
#else
    mr_sound_levels(sound.stream.buffer);
#endif
}
void SetSoundPitch(Sound sound,float value) { if(!IsSoundValid(sound)||value<=0)return;sound.stream.buffer->pitch=value;
#ifdef __wasm__
    mr_web_audio_command(sound.stream.buffer->id,5,value);
#else
    mr_sound_levels(sound.stream.buffer);
#endif
}
void SetSoundPan(Sound sound,float value) { if(!IsSoundValid(sound))return;sound.stream.buffer->pan=mr_clamp01(value);
#ifdef __wasm__
    mr_web_audio_command(sound.stream.buffer->id,6,sound.stream.buffer->pan);
#else
    mr_sound_levels(sound.stream.buffer);
#endif
}
void UpdateSound(Sound sound,const void *data,int sampleCount) { if(!IsSoundValid(sound)||!data||sampleCount<=0)return;rAudioBuffer*b=sound.stream.buffer;StopSound(sound);unsigned int bytes=(unsigned int)sampleCount*b->channels*(b->sampleSize/8);if(bytes>b->dataSize)bytes=b->dataSize;memcpy(b->data,data,bytes);
#ifdef __wasm__
    mr_web_audio_load(b->id,b->data,b->frameCount,b->sampleRate,b->sampleSize,b->channels);
#endif
}
void UnloadSound(Sound sound) { if(!IsSoundValid(sound))return;rAudioBuffer*b=sound.stream.buffer;StopSound(sound);
#ifdef __wasm__
    mr_web_audio_unload(b->id);
#endif
    for(int i=0;i<MR_MAX_SOUNDS;i++)if(mr_sounds[i]==b){mr_sounds[i]=NULL;break;}while(b->processors){rAudioProcessor *next=b->processors->next;MemFree(b->processors);b->processors=next;}
#ifdef _WIN32
    MemFree(b->streamData[0]);MemFree(b->streamData[1]);
#endif
    MemFree(b->data);MemFree(b); }
enum { MR_MUSIC_PCM=1,MR_MUSIC_OGG,MR_MUSIC_MP3,MR_MUSIC_FLAC,MR_MUSIC_QOA,MR_MUSIC_XM,MR_MUSIC_MOD };
typedef struct MRMusicContext {
    AudioStream stream; unsigned int type,frameCount,cursor; unsigned char *encoded; int encodedSize;
    Wave pcm; stb_vorbis *ogg; drmp3 *mp3; drflac *flac; jar_xm_context_t *xm; jar_mod_context_t *mod;
    qoa_desc qoa; unsigned int qoaOffset,qoaFrameLength,qoaFrameAt; short *qoaFrame;
    double startTime,pauseTime; float seekSeconds; bool playing,paused;
} MRMusicContext;
static Sound mr_stream_sound(AudioStream stream){return(Sound){stream,stream.buffer?stream.buffer->frameCount:0};}
static void mr_process_audio(rAudioBuffer *buffer,void *data,unsigned int frames){
    if(!buffer||!data)return;if(buffer->callback)buffer->callback(data,frames);for(rAudioProcessor *p=buffer->processors;p;p=p->next)p->callback(data,frames);for(rAudioProcessor *p=mr_mixed_processors;p;p=p->next)p->callback(data,frames);
}
AudioStream LoadAudioStream(unsigned int sampleRate,unsigned int sampleSize,unsigned int channels){
    if(!sampleRate||!channels||(sampleSize!=8&&sampleSize!=16&&sampleSize!=32)||mr_stream_buffer_frames<=0)return(AudioStream){0};
    size_t bytes=(size_t)mr_stream_buffer_frames*channels*(sampleSize/8);if(bytes>0xffffffffu)return(AudioStream){0};void *silence=MemAlloc((unsigned int)bytes);if(!silence)return(AudioStream){0};memset(silence,sampleSize==8?128:0,bytes);
    Wave wave={(unsigned int)mr_stream_buffer_frames,sampleRate,sampleSize,channels,silence};Sound sound=LoadSoundFromWave(wave);MemFree(silence);if(IsSoundValid(sound))sound.stream.buffer->streaming=true;return sound.stream;
}
bool IsAudioStreamValid(AudioStream stream){return stream.buffer&&stream.sampleRate>0&&stream.channels>0;}
void UnloadAudioStream(AudioStream stream){if(IsAudioStreamValid(stream))UnloadSound(mr_stream_sound(stream));}
void UpdateAudioStream(AudioStream stream,const void *data,int frameCount){
    if(!IsAudioStreamValid(stream)||frameCount<=0)return;rAudioBuffer *b=stream.buffer;unsigned int stride=b->channels*(b->sampleSize/8);size_t required=(size_t)(unsigned int)frameCount*stride;if(required>0xffffffffu)return;unsigned int bytes=(unsigned int)required;
    if(bytes>b->dataSize){unsigned char *grown=MemRealloc(b->data,bytes);if(!grown)return;b->data=grown;b->dataSize=bytes;}
    if(data)memcpy(b->data,data,bytes);else memset(b->data,b->sampleSize==8?128:0,bytes);b->frameCount=(unsigned int)frameCount;mr_process_audio(b,b->data,b->frameCount);
#ifdef __wasm__
    if(b->streamStarted)mr_web_audio_stream_update(b->id,b->data,b->frameCount,b->sampleRate,b->sampleSize,b->channels);else mr_web_audio_load(b->id,b->data,b->frameCount,b->sampleRate,b->sampleSize,b->channels);
#else
    if(b->streamStarted)mr_queue_audio_stream(b,b->data,bytes);
#endif
}
bool IsAudioStreamProcessed(AudioStream stream){if(!IsAudioStreamValid(stream))return false;
#ifdef _WIN32
    rAudioBuffer *b=stream.buffer;if(!b->output)return true;for(int i=0;i<2;i++)if(!(b->streamHeaders[i].dwFlags&WHDR_INQUEUE)||(b->streamHeaders[i].dwFlags&WHDR_DONE))return true;return false;
#else
    return mr_web_audio_playing(stream.buffer->id)<2;
#endif
}
void PlayAudioStream(AudioStream stream){if(!IsAudioStreamValid(stream))return;if(stream.buffer->callback)UpdateAudioStream(stream,NULL,(int)stream.buffer->frameCount);PlaySound(mr_stream_sound(stream));}
void PauseAudioStream(AudioStream stream){PauseSound(mr_stream_sound(stream));}
void ResumeAudioStream(AudioStream stream){ResumeSound(mr_stream_sound(stream));}
bool IsAudioStreamPlaying(AudioStream stream){return IsSoundPlaying(mr_stream_sound(stream));}
void StopAudioStream(AudioStream stream){StopSound(mr_stream_sound(stream));}
void SetAudioStreamVolume(AudioStream stream,float volume){SetSoundVolume(mr_stream_sound(stream),volume);}
void SetAudioStreamPitch(AudioStream stream,float pitch){SetSoundPitch(mr_stream_sound(stream),pitch);}
void SetAudioStreamPan(AudioStream stream,float pan){SetSoundPan(mr_stream_sound(stream),pan);}
void SetAudioStreamBufferSizeDefault(int size){if(size>0)mr_stream_buffer_frames=size;}
void SetAudioStreamCallback(AudioStream stream,AudioCallback callback){if(IsAudioStreamValid(stream))stream.buffer->callback=callback;}
void AttachAudioStreamProcessor(AudioStream stream,AudioCallback callback){if(!IsAudioStreamValid(stream)||!callback)return;rAudioProcessor *p=MemAlloc(sizeof*p);if(!p)return;p->callback=callback;p->next=stream.buffer->processors;stream.buffer->processors=p;}
void DetachAudioStreamProcessor(AudioStream stream,AudioCallback callback){if(!IsAudioStreamValid(stream)||!callback)return;rAudioProcessor **at=&stream.buffer->processors;while(*at){if((*at)->callback==callback){rAudioProcessor *old=*at;*at=old->next;MemFree(old);return;}at=&(*at)->next;}}
void AttachAudioMixedProcessor(AudioCallback callback){if(!callback)return;rAudioProcessor *p=MemAlloc(sizeof*p);if(!p)return;p->callback=callback;p->next=mr_mixed_processors;mr_mixed_processors=p;}
void DetachAudioMixedProcessor(AudioCallback callback){rAudioProcessor **at=&mr_mixed_processors;while(*at){if((*at)->callback==callback){rAudioProcessor *old=*at;*at=old->next;MemFree(old);return;}at=&(*at)->next;}}
static void mr_unload_music_decoder(MRMusicContext *ctx){if(!ctx)return;if(ctx->ogg)stb_vorbis_close(ctx->ogg);if(ctx->mp3){drmp3_uninit(ctx->mp3);MemFree(ctx->mp3);}if(ctx->flac)drflac_close(ctx->flac);if(ctx->xm)jar_xm_free_context(ctx->xm);if(ctx->mod){jar_mod_unload(ctx->mod);MemFree(ctx->mod);}if(IsWaveValid(ctx->pcm))UnloadWave(ctx->pcm);MemFree(ctx->qoaFrame);MemFree(ctx->encoded);}
static bool mr_music_reset(MRMusicContext *ctx){if(!ctx)return false;ctx->cursor=0;ctx->qoaFrameAt=ctx->qoaFrameLength=0;ctx->qoaOffset=8;switch(ctx->type){case MR_MUSIC_PCM:return true;case MR_MUSIC_OGG:return stb_vorbis_seek_start(ctx->ogg)!=0;case MR_MUSIC_MP3:return drmp3_seek_to_pcm_frame(ctx->mp3,0)!=0;case MR_MUSIC_FLAC:return drflac_seek_to_pcm_frame(ctx->flac,0)!=0;case MR_MUSIC_QOA:return true;case MR_MUSIC_XM:jar_xm_reset(ctx->xm);return true;case MR_MUSIC_MOD:jar_mod_seek_start(ctx->mod);return true;default:return false;}}
static unsigned int mr_music_read(MRMusicContext *ctx,void *output,unsigned int frames){if(!ctx||!output||!frames)return 0;unsigned int got=0,channels=ctx->stream.channels;switch(ctx->type){
    case MR_MUSIC_PCM:{unsigned int left=ctx->frameCount-ctx->cursor;if(frames>left)frames=left;memcpy(output,(unsigned char*)ctx->pcm.data+(size_t)ctx->cursor*channels*(ctx->stream.sampleSize/8),(size_t)frames*channels*(ctx->stream.sampleSize/8));got=frames;}break;
    case MR_MUSIC_OGG:got=(unsigned int)stb_vorbis_get_samples_short_interleaved(ctx->ogg,(int)channels,output,(int)(frames*channels));break;
    case MR_MUSIC_MP3:got=(unsigned int)drmp3_read_pcm_frames_s16(ctx->mp3,frames,output);break;
    case MR_MUSIC_FLAC:got=(unsigned int)drflac_read_pcm_frames_s16(ctx->flac,frames,output);break;
    case MR_MUSIC_QOA:{short *dst=output;while(got<frames){if(ctx->qoaFrameAt>=ctx->qoaFrameLength){if(ctx->qoaOffset>=(unsigned)ctx->encodedSize)break;unsigned int length=0,bytes=qoa_decode_frame(ctx->encoded+ctx->qoaOffset,(unsigned)ctx->encodedSize-ctx->qoaOffset,&ctx->qoa,ctx->qoaFrame,&length);if(!bytes||!length)break;ctx->qoaOffset+=bytes;ctx->qoaFrameLength=length;ctx->qoaFrameAt=0;}unsigned int take=ctx->qoaFrameLength-ctx->qoaFrameAt;if(take>frames-got)take=frames-got;memcpy(dst+(size_t)got*channels,ctx->qoaFrame+(size_t)ctx->qoaFrameAt*channels,(size_t)take*channels*sizeof(short));ctx->qoaFrameAt+=take;got+=take;}}break;
    case MR_MUSIC_XM:{unsigned int left=ctx->frameCount-ctx->cursor;got=frames<left?frames:left;if(got)jar_xm_generate_samples(ctx->xm,output,got);}break;
    case MR_MUSIC_MOD:{unsigned int left=ctx->frameCount-ctx->cursor;got=frames<left?frames:left;if(got)jar_mod_fillbuffer(ctx->mod,output,got,NULL);}break;
    default:break;}ctx->cursor+=got;return got;}
static bool mr_music_seek_frame(MRMusicContext *ctx,unsigned int target){if(!ctx)return false;if(target>ctx->frameCount)target=ctx->frameCount;if(ctx->type==MR_MUSIC_OGG){if(!stb_vorbis_seek_frame(ctx->ogg,target))return false;ctx->cursor=target;return true;}if(ctx->type==MR_MUSIC_MP3){if(!drmp3_seek_to_pcm_frame(ctx->mp3,target))return false;ctx->cursor=target;return true;}if(ctx->type==MR_MUSIC_FLAC){if(!drflac_seek_to_pcm_frame(ctx->flac,target))return false;ctx->cursor=target;return true;}if(!mr_music_reset(ctx))return false;unsigned int stride=ctx->stream.channels*(ctx->stream.sampleSize/8),chunk=1024;void *discard=MemAlloc(chunk*stride);if(!discard)return target==0;while(ctx->cursor<target){unsigned int need=target-ctx->cursor;if(need>chunk)need=chunk;if(mr_music_read(ctx,discard,need)!=need){MemFree(discard);return false;}}MemFree(discard);return true;}
Music LoadMusicStreamFromMemory(const char *fileType,const unsigned char *data,int dataSize){
    if(!data||dataSize<=0)return(Music){0};MRMusicContext *ctx=MemAlloc(sizeof*ctx);if(!ctx)return(Music){0};memset(ctx,0,sizeof*ctx);ctx->encoded=MemAlloc((unsigned)dataSize);if(!ctx->encoded){MemFree(ctx);return(Music){0};}memcpy(ctx->encoded,data,(size_t)dataSize);ctx->encodedSize=dataSize;unsigned int rate=0,channels=0,bits=16;
    if((fileType&&IsFileExtension(fileType,".ogg"))||(dataSize>=4&&!memcmp(data,"OggS",4))){int error=0;ctx->ogg=stb_vorbis_open_memory(ctx->encoded,dataSize,&error,NULL);if(ctx->ogg){stb_vorbis_info info=stb_vorbis_get_info(ctx->ogg);ctx->type=MR_MUSIC_OGG;ctx->frameCount=stb_vorbis_stream_length_in_samples(ctx->ogg);rate=info.sample_rate;channels=(unsigned)info.channels;}}
    else if((fileType&&IsFileExtension(fileType,".mp3"))||(dataSize>=3&&!memcmp(data,"ID3",3))){ctx->mp3=MemAlloc(sizeof *ctx->mp3);if(ctx->mp3&&drmp3_init_memory(ctx->mp3,ctx->encoded,(size_t)dataSize,NULL)){drmp3_uint64 frames=drmp3_get_pcm_frame_count(ctx->mp3);if(frames&&frames<=0xffffffffu){ctx->type=MR_MUSIC_MP3;ctx->frameCount=(unsigned int)frames;rate=ctx->mp3->sampleRate;channels=ctx->mp3->channels;}}else{MemFree(ctx->mp3);ctx->mp3=NULL;}}
    else if((fileType&&IsFileExtension(fileType,".flac"))||(dataSize>=4&&!memcmp(data,"fLaC",4))){ctx->flac=drflac_open_memory(ctx->encoded,(size_t)dataSize,NULL);if(ctx->flac&&ctx->flac->totalPCMFrameCount<=0xffffffffu){ctx->type=MR_MUSIC_FLAC;ctx->frameCount=(unsigned int)ctx->flac->totalPCMFrameCount;rate=ctx->flac->sampleRate;channels=ctx->flac->channels;}}
    else if((fileType&&IsFileExtension(fileType,".qoa"))||(dataSize>=4&&!memcmp(data,"qoaf",4))){ctx->qoaOffset=qoa_decode_header(ctx->encoded,dataSize,&ctx->qoa);if(ctx->qoaOffset&&ctx->qoa.channels<=QOA_MAX_CHANNELS){ctx->qoaFrame=MemAlloc((unsigned int)((size_t)QOA_FRAME_LEN*ctx->qoa.channels*sizeof(short)));if(ctx->qoaFrame){ctx->type=MR_MUSIC_QOA;ctx->frameCount=ctx->qoa.samples;rate=ctx->qoa.samplerate;channels=ctx->qoa.channels;}}}
    else if(fileType&&IsFileExtension(fileType,".xm")){if(jar_xm_create_context_safe(&ctx->xm,(const char*)ctx->encoded,(size_t)dataSize,48000)==0&&ctx->xm){jar_xm_set_max_loop_count(ctx->xm,1);ctx->type=MR_MUSIC_XM;ctx->frameCount=(unsigned int)jar_xm_get_remaining_samples(ctx->xm);rate=48000;channels=2;bits=32;}}
    else if(fileType&&IsFileExtension(fileType,".mod")){ctx->mod=MemAlloc(sizeof *ctx->mod);if(ctx->mod&&jar_mod_init(ctx->mod)&&jar_mod_setcfg(ctx->mod,48000,16,1,1,1)&&jar_mod_load(ctx->mod,ctx->encoded,dataSize)){ctx->mod->modfile=ctx->encoded;ctx->mod->modfilesize=(mulong)dataSize;ctx->encoded=NULL;ctx->type=MR_MUSIC_MOD;ctx->frameCount=(unsigned int)jar_mod_max_samples(ctx->mod);jar_mod_seek_start(ctx->mod);rate=48000;channels=2;}}
    if(!ctx->type){ctx->pcm=LoadWaveFromMemory(fileType,data,dataSize);if(IsWaveValid(ctx->pcm)){ctx->type=MR_MUSIC_PCM;ctx->frameCount=ctx->pcm.frameCount;rate=ctx->pcm.sampleRate;channels=ctx->pcm.channels;bits=ctx->pcm.sampleSize;}}
    if(!ctx->type||!ctx->frameCount||!rate||!channels){mr_unload_music_decoder(ctx);MemFree(ctx);return(Music){0};}ctx->stream=LoadAudioStream(rate,bits,channels);if(!IsAudioStreamValid(ctx->stream)){mr_unload_music_decoder(ctx);MemFree(ctx);return(Music){0};}return(Music){ctx->stream,ctx->frameCount,true,(int)ctx->type,ctx};
}
Music LoadMusicStream(const char *fileName){int size=0;unsigned char *data=LoadFileData(fileName,&size);if(!data)return(Music){0};Music music=LoadMusicStreamFromMemory(GetFileExtension(fileName),data,size);UnloadFileData(data);return music;}
bool IsMusicValid(Music music){MRMusicContext *ctx=(MRMusicContext*)music.ctxData;return ctx&&ctx->type&&ctx->frameCount&&IsAudioStreamValid(ctx->stream);}
void UnloadMusicStream(Music music){if(!IsMusicValid(music))return;MRMusicContext *ctx=music.ctxData;UnloadAudioStream(ctx->stream);mr_unload_music_decoder(ctx);MemFree(ctx);}
static bool mr_queue_music(MRMusicContext *ctx,bool looping){if(!ctx)return false;rAudioBuffer *buffer=ctx->stream.buffer;unsigned int stride=ctx->stream.channels*(ctx->stream.sampleSize/8),capacity=buffer->dataSize/stride;if(!capacity)return false;void *chunk=MemAlloc(capacity*stride);if(!chunk)return false;unsigned int filled=0,restarts=0;while(filled<capacity){unsigned int got=mr_music_read(ctx,(unsigned char*)chunk+(size_t)filled*stride,capacity-filled);filled+=got;if(got==0){if(!looping||restarts++>=64||!mr_music_reset(ctx))break;}}if(filled)UpdateAudioStream(ctx->stream,chunk,(int)filled);MemFree(chunk);return filled>0;}
void PlayMusicStream(Music music){if(!IsMusicValid(music))return;MRMusicContext *ctx=music.ctxData;StopAudioStream(ctx->stream);unsigned int target=(unsigned int)(ctx->seekSeconds*ctx->stream.sampleRate);if(target>=ctx->frameCount)target=0;if(!mr_music_seek_frame(ctx,target)||!mr_queue_music(ctx,music.looping))return;PlayAudioStream(ctx->stream);ctx->startTime=GetTime();ctx->playing=true;ctx->paused=false;if(IsAudioStreamProcessed(ctx->stream))mr_queue_music(ctx,music.looping);}
bool IsMusicStreamPlaying(Music music){return IsMusicValid(music)&&((MRMusicContext*)music.ctxData)->playing&&IsAudioStreamPlaying(((MRMusicContext*)music.ctxData)->stream);}
void UpdateMusicStream(Music music){if(!IsMusicValid(music))return;MRMusicContext *ctx=music.ctxData;if(!ctx->playing||ctx->paused)return;if(ctx->cursor>=ctx->frameCount&&!music.looping&&!IsAudioStreamPlaying(ctx->stream)){ctx->playing=false;ctx->seekSeconds=GetMusicTimeLength(music);return;}if(IsAudioStreamProcessed(ctx->stream))mr_queue_music(ctx,music.looping);}
void StopMusicStream(Music music){if(!IsMusicValid(music))return;MRMusicContext *ctx=music.ctxData;StopAudioStream(ctx->stream);ctx->seekSeconds=0;mr_music_reset(ctx);ctx->playing=ctx->paused=false;}
void PauseMusicStream(Music music){if(!IsMusicValid(music))return;MRMusicContext *ctx=music.ctxData;if(ctx->playing&&!ctx->paused){ctx->seekSeconds=GetMusicTimePlayed(music);PauseAudioStream(ctx->stream);ctx->paused=true;ctx->pauseTime=GetTime();}}
void ResumeMusicStream(Music music){if(!IsMusicValid(music))return;MRMusicContext *ctx=music.ctxData;if(ctx->paused){ResumeAudioStream(ctx->stream);ctx->startTime=GetTime();ctx->paused=false;ctx->playing=true;}}
void SeekMusicStream(Music music,float position){if(!IsMusicValid(music))return;float length=GetMusicTimeLength(music);if(position<0)position=0;if(position>length)position=length;MRMusicContext *ctx=music.ctxData;bool wasPlaying=ctx->playing&&!ctx->paused;StopAudioStream(ctx->stream);ctx->seekSeconds=position;mr_music_seek_frame(ctx,(unsigned int)(position*ctx->stream.sampleRate));ctx->startTime=GetTime();if(wasPlaying)PlayMusicStream(music);}
void SetMusicVolume(Music music,float volume){if(IsMusicValid(music))SetAudioStreamVolume(((MRMusicContext*)music.ctxData)->stream,volume);}
void SetMusicPitch(Music music,float pitch){if(IsMusicValid(music))SetAudioStreamPitch(((MRMusicContext*)music.ctxData)->stream,pitch);}
void SetMusicPan(Music music,float pan){if(IsMusicValid(music))SetAudioStreamPan(((MRMusicContext*)music.ctxData)->stream,pan);}
float GetMusicTimeLength(Music music){return IsMusicValid(music)?music.frameCount/(float)music.stream.sampleRate:0;}
float GetMusicTimePlayed(Music music){if(!IsMusicValid(music))return 0;MRMusicContext *ctx=music.ctxData;float time=ctx->seekSeconds;if(ctx->playing&&!ctx->paused)time+=(float)(GetTime()-ctx->startTime)*ctx->stream.buffer->pitch;float length=GetMusicTimeLength(music);if(music.looping&&length>0)while(time>=length)time-=length;return time<length?time:length;}
void CloseAudioDevice(void) { if(!mr_audio_ready)return;for(int i=0;i<MR_MAX_SOUNDS;i++)if(mr_sounds[i]){Sound sound={{mr_sounds[i],NULL,mr_sounds[i]->sampleRate,mr_sounds[i]->sampleSize,mr_sounds[i]->channels},mr_sounds[i]->frameCount};UnloadSound(sound);}
#ifdef __wasm__
    mr_web_audio_close();
#endif
    while(mr_mixed_processors){rAudioProcessor *next=mr_mixed_processors->next;MemFree(mr_mixed_processors);mr_mixed_processors=next;}mr_audio_ready=false; }
