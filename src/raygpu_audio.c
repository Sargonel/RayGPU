/* RayGPU audio module. Compiled through raygpu.c; do not compile separately. */
static float mr_clamp01(float value);
struct rAudioBuffer {
    unsigned int id,frameCount,sampleRate,sampleSize,channels,dataSize;
    unsigned char *data; float volume,pitch,pan; bool paused;
#ifdef _WIN32
    HWAVEOUT output; WAVEHDR header;
#endif
};
struct rAudioProcessor { int unused; };
#define MR_MAX_SOUNDS 256
static rAudioBuffer *mr_sounds[MR_MAX_SOUNDS];
static unsigned int mr_next_sound;
static bool mr_audio_ready;
static float mr_master_volume=1.0f;
static unsigned int mr_u16(const unsigned char *p) { return (unsigned int)p[0]|((unsigned int)p[1]<<8); }
static unsigned int mr_u32(const unsigned char *p) { return mr_u16(p)|(mr_u16(p+2)<<16); }
bool IsWaveValid(Wave wave) { return wave.data && wave.frameCount>0 && wave.sampleRate>0 && wave.channels>0 && (wave.sampleSize==8||wave.sampleSize==16||wave.sampleSize==32); }
Wave LoadWaveFromMemory(const char *fileType,const unsigned char *data,int size) {
    (void)fileType; if(!data||size<44||memcmp(data,"RIFF",4)!=0||memcmp(data+8,"WAVE",4)!=0)return(Wave){0};
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
#ifdef _WIN32
static void mr_stop_sound(rAudioBuffer *buffer) { if(!buffer||!buffer->output)return;waveOutReset(buffer->output);waveOutUnprepareHeader(buffer->output,&buffer->header,sizeof buffer->header);waveOutClose(buffer->output);buffer->output=NULL;memset(&buffer->header,0,sizeof buffer->header);buffer->paused=false; }
static void mr_sound_levels(rAudioBuffer *buffer) { if(!buffer||!buffer->output)return;float volume=mr_clamp01(buffer->volume*mr_master_volume),pan=mr_clamp01(buffer->pan);float left=volume*(pan<=0.5f?1.0f:2.0f*(1.0f-pan)),right=volume*(pan>=0.5f?1.0f:2.0f*pan);DWORD packed=(DWORD)(left*65535.0f)|((DWORD)(right*65535.0f)<<16);waveOutSetVolume(buffer->output,packed);waveOutSetPlaybackRate(buffer->output,(DWORD)(buffer->pitch*65536.0f)); }
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
}
void PlaySound(Sound sound) { if(!mr_audio_ready||!IsSoundValid(sound))return;rAudioBuffer*b=sound.stream.buffer;
#ifdef __wasm__
    mr_web_audio_command(b->id,0,0);
#else
    mr_stop_sound(b);WAVEFORMATEX format={0};format.wFormatTag=b->sampleSize==32?WAVE_FORMAT_IEEE_FLOAT:WAVE_FORMAT_PCM;format.nChannels=(WORD)b->channels;format.nSamplesPerSec=b->sampleRate;format.wBitsPerSample=(WORD)b->sampleSize;format.nBlockAlign=(WORD)(b->channels*b->sampleSize/8);format.nAvgBytesPerSec=format.nSamplesPerSec*format.nBlockAlign;if(waveOutOpen(&b->output,WAVE_MAPPER,&format,0,0,CALLBACK_NULL)!=MMSYSERR_NOERROR){b->output=NULL;return;}b->header.lpData=(LPSTR)b->data;b->header.dwBufferLength=b->dataSize;waveOutPrepareHeader(b->output,&b->header,sizeof b->header);mr_sound_levels(b);waveOutWrite(b->output,&b->header,sizeof b->header);
#endif
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
    return sound.stream.buffer->output&&(sound.stream.buffer->header.dwFlags&WHDR_DONE)==0;
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
    for(int i=0;i<MR_MAX_SOUNDS;i++)if(mr_sounds[i]==b){mr_sounds[i]=NULL;break;}MemFree(b->data);MemFree(b); }
void CloseAudioDevice(void) { if(!mr_audio_ready)return;for(int i=0;i<MR_MAX_SOUNDS;i++)if(mr_sounds[i]){Sound sound={{mr_sounds[i],NULL,mr_sounds[i]->sampleRate,mr_sounds[i]->sampleSize,mr_sounds[i]->channels},mr_sounds[i]->frameCount};UnloadSound(sound);}
#ifdef __wasm__
    mr_web_audio_close();
#endif
    mr_audio_ready=false; }
