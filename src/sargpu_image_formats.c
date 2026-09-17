/* Extra image formats, mipmaps and image export. Compiled through sargpu.c. */
#ifdef _WIN32
typedef struct MRBitmapInfoHeader { DWORD size;LONG width,height;WORD planes,bits;DWORD compression,imageSize;LONG xppm,yppm;DWORD colorsUsed,colorsImportant; } MRBitmapInfoHeader;
typedef struct MRBitmapInfo { MRBitmapInfoHeader header; DWORD colors[3]; } MRBitmapInfo;
__declspec(dllimport) HDC WINAPI CreateCompatibleDC(HDC);
__declspec(dllimport) HBITMAP WINAPI CreateDIBSection(HDC,const MRBitmapInfo*,UINT,void**,HANDLE,DWORD);
__declspec(dllimport) HGDIOBJ WINAPI SelectObject(HDC,HGDIOBJ);
__declspec(dllimport) BOOL WINAPI BitBlt(HDC,int,int,int,int,HDC,int,int,DWORD);
__declspec(dllimport) BOOL WINAPI DeleteObject(HGDIOBJ);
__declspec(dllimport) BOOL WINAPI DeleteDC(HDC);
#endif
#define QOI_NO_STDIO
#define QOI_MALLOC(sz) MemAlloc((unsigned int)(sz))
#define QOI_FREE(p) MemFree(p)
#define QOI_IMPLEMENTATION
#include "external/qoi.h"
#undef QOI_IMPLEMENTATION
#undef QOI_MALLOC
#undef QOI_FREE

#define STBI_WRITE_NO_STDIO
#define STBIW_ASSERT(x) ((void)0)
#define STBIW_MALLOC(sz) MemAlloc((unsigned int)(sz))
#define STBIW_REALLOC(p,sz) MemRealloc((p),(unsigned int)(sz))
#define STBIW_FREE(p) MemFree(p)
#define STBIW_MEMMOVE(a,b,sz) memmove((a),(b),(sz))
#ifdef abs
#undef abs
#endif
#define abs(x) ((x)<0?-(x):(x))
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "external/stb_image_write.h"
#undef STB_IMAGE_WRITE_IMPLEMENTATION
#undef STBIW_MALLOC
#undef STBIW_REALLOC
#undef STBIW_FREE
#undef STBIW_MEMMOVE
#undef abs

typedef struct MRWriteBuffer { unsigned char *data; int size,capacity; bool failed; } MRWriteBuffer;
static void mr_image_write(void *context,void *data,int size){MRWriteBuffer *out=context;if(out->failed||size<=0)return;if(out->size>2147483647-size){out->failed=true;return;}int needed=out->size+size;if(needed>out->capacity){int capacity=out->capacity?out->capacity:4096;while(capacity<needed&&capacity<=1073741823)capacity*=2;if(capacity<needed){out->failed=true;return;}void *grown=MemRealloc(out->data,(unsigned)capacity);if(!grown){out->failed=true;return;}out->data=grown;out->capacity=capacity;}memcpy(out->data+out->size,data,(size_t)size);out->size=needed;}
static unsigned char mr_expand5(unsigned v){return(unsigned char)((v<<3)|(v>>2));}
static unsigned char mr_expand6(unsigned v){return(unsigned char)((v<<2)|(v>>4));}
static void mr_dds_color(unsigned short c,unsigned char *out){out[0]=mr_expand5((c>>11)&31);out[1]=mr_expand6((c>>5)&63);out[2]=mr_expand5(c&31);out[3]=255;}
static void mr_dds_alpha_block(const unsigned char *block,unsigned char values[16]){
    unsigned char table[8]={block[0],block[1]};
    if(table[0]>table[1])for(int i=1;i<=6;i++)table[i+1]=(unsigned char)(((7-i)*table[0]+i*table[1])/7);
    else{for(int i=1;i<=4;i++)table[i+1]=(unsigned char)(((5-i)*table[0]+i*table[1])/5);table[6]=0;table[7]=255;}
    uint64_t codes=0;for(int i=0;i<6;i++)codes|=(uint64_t)block[2+i]<<(8*i);for(int i=0;i<16;i++)values[i]=table[(codes>>(3*i))&7];
}
static Image mr_load_dds(const unsigned char *data,int size){
    if(!data||size<128||memcmp(data,"DDS ",4)||mr_u32(data+4)!=124)return(Image){0};int width=(int)mr_u32(data+16),height=(int)mr_u32(data+12);if(width<=0||height<=0||width>8192||height>8192)return(Image){0};
    uint32_t flags=mr_u32(data+80),fourcc=mr_u32(data+84),bits=mr_u32(data+88),rMask=mr_u32(data+92),gMask=mr_u32(data+96),bMask=mr_u32(data+100),aMask=mr_u32(data+104);unsigned char *rgba=MemAlloc((unsigned)((size_t)width*height*4));if(!rgba)return(Image){0};const unsigned char *src=data+128;int left=size-128;
    if((flags&0x40)&&bits==32){if(left<(int)((size_t)width*height*4)){MemFree(rgba);return(Image){0};}for(int i=0;i<width*height;i++){uint32_t p=mr_u32(src+i*4),masks[4]={rMask,gMask,bMask,aMask};for(int c=0;c<4;c++){uint32_t m=masks[c];if(!m){rgba[i*4+c]=(c==3)?255:0;continue;}int shift=0,widthBits=0;while(((m>>shift)&1)==0)shift++;while((m>>(shift+widthBits))&1)widthBits++;uint32_t v=(p&m)>>shift,max=(1u<<widthBits)-1;rgba[i*4+c]=(unsigned char)(v*255/max);}}return(Image){rgba,width,height,1,7};}
    if(fourcc==0x30315844u){if(left<20){MemFree(rgba);return(Image){0};}uint32_t format=mr_u32(src);src+=20;left-=20;if(format==71||format==72)fourcc=0x31545844u;else if(format==74||format==75)fourcc=0x33545844u;else if(format==77||format==78)fourcc=0x35545844u;else if(format==80)fourcc=0x31495441u;else if(format==83)fourcc=0x32495441u;else{MemFree(rgba);return(Image){0};}}
    bool bc4=fourcc==0x31495441u||fourcc==0x55344342u,bc5=fourcc==0x32495441u||fourcc==0x55354342u;
    int blockBytes=(fourcc==0x31545844u||bc4)?8:(fourcc==0x33545844u||fourcc==0x35545844u||bc5)?16:0;if(!blockBytes){MemFree(rgba);return(Image){0};}int blocksX=(width+3)/4,blocksY=(height+3)/4;if(left<blocksX*blocksY*blockBytes){MemFree(rgba);return(Image){0};}
    if(bc4||bc5){for(int by=0;by<blocksY;by++)for(int bx=0;bx<blocksX;bx++){const unsigned char *b=src+(by*blocksX+bx)*blockBytes;unsigned char red[16],green[16]={0};mr_dds_alpha_block(b,red);if(bc5)mr_dds_alpha_block(b+8,green);for(int py=0;py<4;py++)for(int px=0;px<4;px++){int x=bx*4+px,y=by*4+py,index=py*4+px;if(x>=width||y>=height)continue;unsigned char *d=rgba+((size_t)y*width+x)*4;d[0]=red[index];d[1]=bc5?green[index]:red[index];d[2]=bc4?red[index]:0;d[3]=255;}}return(Image){rgba,width,height,1,7};}
    for(int by=0;by<blocksY;by++)for(int bx=0;bx<blocksX;bx++){const unsigned char *b=src+(by*blocksX+bx)*blockBytes,*color=b+(blockBytes-8);unsigned char table[4][4];mr_dds_color((unsigned short)mr_u16(color),table[0]);mr_dds_color((unsigned short)mr_u16(color+2),table[1]);for(int c=0;c<3;c++){table[2][c]=(unsigned char)((2*table[0][c]+table[1][c])/3);table[3][c]=(unsigned char)((table[0][c]+2*table[1][c])/3);}table[2][3]=255;table[3][3]=255;if(fourcc==0x31545844u&&mr_u16(color)<=mr_u16(color+2)){for(int c=0;c<3;c++)table[2][c]=(unsigned char)((table[0][c]+table[1][c])/2);memset(table[3],0,4);}uint32_t codes=mr_u32(color+4);for(int py=0;py<4;py++)for(int px=0;px<4;px++){int x=bx*4+px,y=by*4+py;if(x>=width||y>=height)continue;unsigned char *d=rgba+((size_t)y*width+x)*4;memcpy(d,table[(codes>>(2*(py*4+px)))&3],4);if(fourcc==0x33545844u)d[3]=(unsigned char)(((b[(py*4+px)/2]>>((px&1)*4))&15)*17);else if(fourcc==0x35545844u){unsigned char at[8]={b[0],b[1]};if(at[0]>at[1])for(int i=1;i<=6;i++)at[i+1]=(unsigned char)(((7-i)*at[0]+i*at[1])/7);else{for(int i=1;i<=4;i++)at[i+1]=(unsigned char)(((5-i)*at[0]+i*at[1])/5);at[6]=0;at[7]=255;}uint64_t ac=0;for(int i=0;i<6;i++)ac|=(uint64_t)b[2+i]<<(8*i);d[3]=at[(ac>>(3*(py*4+px)))&7];}}}
    return(Image){rgba,width,height,1,7};
}
static Image mr_load_qoi(const unsigned char *data,int size){qoi_desc desc={0};unsigned char *pixels=qoi_decode(data,size,&desc,4);if(!pixels||!desc.width||!desc.height||desc.width>8192||desc.height>8192){MemFree(pixels);return(Image){0};}return(Image){pixels,(int)desc.width,(int)desc.height,1,7};}
Image LoadImageRaw(const char *fileName,int width,int height,int format,int headerSize){int size=0;unsigned char *file=LoadFileData(fileName,&size);if(!file||width<=0||height<=0||headerSize<0||headerSize>size){UnloadFileData(file);return(Image){0};}int bpp=format==1?1:format==2?2:format==3?2:format==4?3:(format==5||format==6)?2:format==7?4:0;if(!bpp||(size-headerSize)/(bpp*width)<height){UnloadFileData(file);return(Image){0};}Image image=GenImageColor(width,height,BLANK);if(!image.data){UnloadFileData(file);return image;}const unsigned char *src=file+headerSize;Color *dst=image.data;for(int i=0;i<width*height;i++){if(format==1)dst[i]=(Color){src[i],src[i],src[i],255};else if(format==2)dst[i]=(Color){src[i*2],src[i*2],src[i*2],src[i*2+1]};else if(format==4)dst[i]=(Color){src[i*3],src[i*3+1],src[i*3+2],255};else if(format==7)memcpy(&dst[i],src+i*4,4);else{unsigned v=mr_u16(src+i*2);if(format==3)dst[i]=(Color){mr_expand5(v>>11),mr_expand6((v>>5)&63),mr_expand5(v),255};if(format==5)dst[i]=(Color){mr_expand5(v>>11),mr_expand5(v>>6),mr_expand5(v>>1),(v&1)?255:0};if(format==6)dst[i]=(Color){(unsigned char)(((v>>12)&15)*17),(unsigned char)(((v>>8)&15)*17),(unsigned char)(((v>>4)&15)*17),(unsigned char)((v&15)*17)};}}UnloadFileData(file);return image;}
void ImageMipmaps(Image *image){if(!image||!IsImageValid(*image)||image->mipmaps>1)return;size_t total=(size_t)image->width*image->height*4;for(int w=image->width,h=image->height;w>1||h>1;){w=w>1?w/2:1;h=h>1?h/2:1;total+=(size_t)w*h*4;}if(total>4294967295u)return;unsigned char *all=MemAlloc((unsigned)total);if(!all)return;size_t offset=(size_t)image->width*image->height*4;memcpy(all,image->data,offset);Image level=ImageCopy(*image);int count=1;while(level.width>1||level.height>1){ImageResize(&level,level.width>1?level.width/2:1,level.height>1?level.height/2:1);size_t bytes=(size_t)level.width*level.height*4;memcpy(all+offset,level.data,bytes);offset+=bytes;count++;}UnloadImage(level);MemFree(image->data);image->data=all;image->mipmaps=count;}
unsigned char *ExportImageToMemory(Image image,const char *type,int *fileSize){if(fileSize)*fileSize=0;if(!IsImageValid(image)||!type)return NULL;if(IsFileExtension(type,".qoi")){qoi_desc d={(unsigned)image.width,(unsigned)image.height,4,QOI_SRGB};int n=0;unsigned char *out=qoi_encode(image.data,&d,&n);if(fileSize)*fileSize=n;return out;}MRWriteBuffer out={0};bool ok=false;if(IsFileExtension(type,".png"))ok=stbi_write_png_to_func(mr_image_write,&out,image.width,image.height,4,image.data,image.width*4)!=0;else if(IsFileExtension(type,".bmp"))ok=stbi_write_bmp_to_func(mr_image_write,&out,image.width,image.height,4,image.data)!=0;else if(IsFileExtension(type,".tga"))ok=stbi_write_tga_to_func(mr_image_write,&out,image.width,image.height,4,image.data)!=0;else if(IsFileExtension(type,".jpg;.jpeg"))ok=stbi_write_jpg_to_func(mr_image_write,&out,image.width,image.height,4,image.data,90)!=0;if(!ok||out.failed){MemFree(out.data);return NULL;}if(fileSize)*fileSize=out.size;return out.data;}
bool ExportImage(Image image,const char *fileName){int size=0;unsigned char *data=ExportImageToMemory(image,GetFileExtension(fileName),&size);if(!data)return false;bool result=SaveFileData(fileName,data,size);MemFree(data);return result;}
static void mr_write_uint(MRWriteBuffer *out,unsigned value){char text[16];int n=0;do{text[n++]=(char)('0'+value%10);value/=10;}while(value);for(int i=n-1;i>=0;i--)mr_image_write(out,&text[i],1);}
bool ExportImageAsCode(Image image,const char *fileName){if(!IsImageValid(image)||!fileName)return false;size_t bytes=(size_t)image.width*image.height*4;if(bytes>16777216)return false;MRWriteBuffer out={0};const char a[]="/* SarGPU image data */\n#define IMAGE_WIDTH ",b[]="\n#define IMAGE_HEIGHT ",c[]="\nstatic const unsigned char IMAGE_DATA[",d[]="] = {\n";mr_image_write(&out,(void*)a,(int)sizeof(a)-1);mr_write_uint(&out,(unsigned)image.width);mr_image_write(&out,(void*)b,(int)sizeof(b)-1);mr_write_uint(&out,(unsigned)image.height);mr_image_write(&out,(void*)c,(int)sizeof(c)-1);mr_write_uint(&out,(unsigned)bytes);mr_image_write(&out,(void*)d,(int)sizeof(d)-1);unsigned char *p=image.data;const char hex[]="0123456789abcdef";for(size_t i=0;i<bytes;i++){char value[5]={'0','x',hex[p[i]>>4],hex[p[i]&15],','};if(i+1==bytes)value[4]=' ';mr_image_write(&out,value,5);if((i&15)==15)mr_image_write(&out,"\n",1);}mr_image_write(&out,"\n};\n",4);bool result=!out.failed&&SaveFileData(fileName,out.data,out.size);MemFree(out.data);return result;}

#ifdef _WIN32
typedef struct MRMapResult { bool done,ok; } MRMapResult;
static void mr_image_mapped(WGPUMapAsyncStatus status,WGPUStringView message,void *userdata1,void *userdata2){(void)message;(void)userdata2;MRMapResult *result=userdata1;result->ok=status==WGPUMapAsyncStatus_Success;result->done=true;}
static Image mr_read_gpu_texture(MRTexture *texture){
    if(!texture||!texture->texture||texture->width<=0||texture->height<=0)return(Image){0};uint32_t row=((uint32_t)texture->width*4+255)&~255u;uint64_t size=(uint64_t)row*(uint32_t)texture->height;
    WGPUBufferDescriptor bd=WGPU_BUFFER_DESCRIPTOR_INIT;bd.size=size;bd.usage=WGPUBufferUsage_CopyDst|WGPUBufferUsage_MapRead;WGPUBuffer buffer=wgpuDeviceCreateBuffer(mr.device,&bd);if(!buffer)return(Image){0};
    WGPUCommandEncoder encoder=wgpuDeviceCreateCommandEncoder(mr.device,NULL);WGPUTexelCopyTextureInfo source=WGPU_TEXEL_COPY_TEXTURE_INFO_INIT;source.texture=texture->texture;WGPUTexelCopyBufferInfo destination=WGPU_TEXEL_COPY_BUFFER_INFO_INIT;destination.buffer=buffer;destination.layout.bytesPerRow=row;destination.layout.rowsPerImage=(uint32_t)texture->height;WGPUExtent3D extent={(uint32_t)texture->width,(uint32_t)texture->height,1};wgpuCommandEncoderCopyTextureToBuffer(encoder,&source,&destination,&extent);WGPUCommandBuffer commands=wgpuCommandEncoderFinish(encoder,NULL);wgpuQueueSubmit(mr.queue,1,&commands);wgpuCommandBufferRelease(commands);wgpuCommandEncoderRelease(encoder);
    MRMapResult result={0};WGPUBufferMapCallbackInfo callback=WGPU_BUFFER_MAP_CALLBACK_INFO_INIT;callback.mode=MR_CALLBACK_MODE;callback.callback=mr_image_mapped;callback.userdata1=&result;wgpuBufferMapAsync(buffer,WGPUMapMode_Read,0,(size_t)size,callback);while(!result.done)mr_pump();
    Image image={0};if(result.ok){const unsigned char *mapped=wgpuBufferGetConstMappedRange(buffer,0,(size_t)size);unsigned char *pixels=MemAlloc((unsigned int)((size_t)texture->width*texture->height*4));if(mapped&&pixels){for(int y=0;y<texture->height;y++)memcpy(pixels+(size_t)y*texture->width*4,mapped+(size_t)y*row,(size_t)texture->width*4);if(texture->renderTarget&&(mr.config.format==WGPUTextureFormat_BGRA8Unorm||mr.config.format==WGPUTextureFormat_BGRA8UnormSrgb))for(int i=0;i<texture->width*texture->height;i++){unsigned char v=pixels[i*4];pixels[i*4]=pixels[i*4+2];pixels[i*4+2]=v;}image=(Image){pixels,texture->width,texture->height,1,7};}wgpuBufferUnmap(buffer);}wgpuBufferRelease(buffer);return image;
}
#endif
Image LoadImageFromTexture(Texture2D texture){MRTexture *entry=mr_texture(texture.id);if(!entry||!entry->pixels)return(Image){0};Image image={(void*)entry->pixels,entry->width,entry->height,1,7};return ImageCopy(image);}
#define MR_MAX_COMPLETED_READBACKS 16
typedef struct MRCompletedReadback { Image image; ImageLoadCallback callback; void *userData; } MRCompletedReadback;
static MRCompletedReadback mr_completed_readbacks[MR_MAX_COMPLETED_READBACKS];
static bool mr_queue_readback(Image image,ImageLoadCallback callback,void *userData){for(int i=0;i<MR_MAX_COMPLETED_READBACKS;i++)if(!mr_completed_readbacks[i].callback){mr_completed_readbacks[i]=(MRCompletedReadback){image,callback,userData};return true;}return false;}
static void mr_dispatch_readbacks(void){MRCompletedReadback ready[MR_MAX_COMPLETED_READBACKS];memcpy(ready,mr_completed_readbacks,sizeof ready);memset(mr_completed_readbacks,0,sizeof mr_completed_readbacks);for(int i=0;i<MR_MAX_COMPLETED_READBACKS;i++)if(ready[i].callback)ready[i].callback(ready[i].image,ready[i].userData);}
#ifdef __wasm__
#define MR_MAX_READBACKS 16
typedef struct MRReadbackRequest { unsigned int id; ImageLoadCallback callback; void *userData; } MRReadbackRequest;
static MRReadbackRequest mr_readbacks[MR_MAX_READBACKS];static unsigned int mr_next_readback;
MR_EXPORT("sargpu_readback_allocate") void *sargpu_readback_allocate(unsigned int requestId,unsigned int size){for(int i=0;i<MR_MAX_READBACKS;i++)if(mr_readbacks[i].id==requestId)return MemAlloc(size);return NULL;}
MR_EXPORT("sargpu_readback_complete") void sargpu_readback_complete(unsigned int requestId,void *pixels,int width,int height){for(int i=0;i<MR_MAX_READBACKS;i++)if(mr_readbacks[i].id==requestId){MRReadbackRequest request=mr_readbacks[i];mr_readbacks[i]=(MRReadbackRequest){0};Image image={pixels,width,height,1,7};if(!mr_queue_readback(image,request.callback,request.userData))MemFree(pixels);return;}MemFree(pixels);}
static unsigned int mr_begin_readback(ImageLoadCallback callback,void *userData){int occupied=0;for(int i=0;i<MR_MAX_COMPLETED_READBACKS;i++)if(mr_completed_readbacks[i].callback)occupied++;for(int i=0;i<MR_MAX_READBACKS;i++)if(mr_readbacks[i].id)occupied++;if(occupied>=MR_MAX_READBACKS)return 0;int slot=-1;for(int i=0;i<MR_MAX_READBACKS;i++)if(!mr_readbacks[i].id){slot=i;break;}if(slot<0)return 0;unsigned int id=++mr_next_readback;if(!id)id=++mr_next_readback;mr_readbacks[slot]=(MRReadbackRequest){id,callback,userData};return id;}
static void mr_cancel_readback(unsigned int id){for(int i=0;i<MR_MAX_READBACKS;i++)if(mr_readbacks[i].id==id){mr_readbacks[i]=(MRReadbackRequest){0};return;}}
#endif
static void mr_discard_readbacks(void){for(int i=0;i<MR_MAX_COMPLETED_READBACKS;i++){if(mr_completed_readbacks[i].callback)UnloadImage(mr_completed_readbacks[i].image);mr_completed_readbacks[i]=(MRCompletedReadback){0};}
#ifdef __wasm__
    memset(mr_readbacks,0,sizeof mr_readbacks);
#endif
}
bool LoadImageFromTextureAsync(Texture2D texture,ImageLoadCallback callback,void *userData){
    if(!callback)return false;MRTexture *entry=mr_texture(texture.id);if(!entry)return false;if(entry->pixels){Image image=LoadImageFromTexture(texture);if(!IsImageValid(image))return false;if(!mr_queue_readback(image,callback,userData)){UnloadImage(image);return false;}return true;}
#ifdef _WIN32
    Image image=mr_read_gpu_texture(entry);if(!IsImageValid(image))return false;if(!mr_queue_readback(image,callback,userData)){UnloadImage(image);return false;}return true;
#else
    unsigned int id=mr_begin_readback(callback,userData);if(!id)return false;if(!mr_web_texture_readback(entry->id,id)){mr_cancel_readback(id);return false;}return true;
#endif
}
void GenTextureMipmaps(Texture2D *texture){
    if(!texture||!IsTextureValid(*texture)||texture->mipmaps>1)return;MRTexture *entry=mr_texture(texture->id);if(!entry||!entry->pixels||entry->renderTarget)return;Image image={entry->pixels,entry->width,entry->height,1,7};image=ImageCopy(image);ImageMipmaps(&image);if(image.mipmaps<=1){UnloadImage(image);return;}
#ifdef _WIN32
    WGPUTextureDescriptor desc=WGPU_TEXTURE_DESCRIPTOR_INIT;desc.size=(WGPUExtent3D){(uint32_t)entry->width,(uint32_t)entry->height,1};desc.mipLevelCount=(uint32_t)image.mipmaps;desc.dimension=WGPUTextureDimension_2D;desc.format=WGPUTextureFormat_RGBA8Unorm;desc.usage=WGPUTextureUsage_TextureBinding|WGPUTextureUsage_CopyDst|WGPUTextureUsage_CopySrc;WGPUTexture gpu=wgpuDeviceCreateTexture(mr.device,&desc);if(!gpu){UnloadImage(image);return;}size_t offset=0;int w=entry->width,h=entry->height;for(int level=0;level<image.mipmaps;level++){WGPUTexelCopyTextureInfo destination=WGPU_TEXEL_COPY_TEXTURE_INFO_INIT;destination.texture=gpu;destination.mipLevel=(uint32_t)level;WGPUTexelCopyBufferLayout layout={0};layout.bytesPerRow=(uint32_t)w*4;layout.rowsPerImage=(uint32_t)h;WGPUExtent3D extent={(uint32_t)w,(uint32_t)h,1};size_t bytes=(size_t)w*h*4;wgpuQueueWriteTexture(mr.queue,&destination,(unsigned char*)image.data+offset,bytes,&layout,&extent);offset+=bytes;w=w>1?w/2:1;h=h>1?h/2:1;}WGPUTextureView view=wgpuTextureCreateView(gpu,NULL);WGPUBindGroupEntry bindings[2]={WGPU_BIND_GROUP_ENTRY_INIT,WGPU_BIND_GROUP_ENTRY_INIT};bindings[0].binding=0;bindings[0].sampler=entry->customSampler?entry->customSampler:mr.sampler;bindings[1].binding=1;bindings[1].textureView=view;WGPUBindGroupDescriptor groupDesc=WGPU_BIND_GROUP_DESCRIPTOR_INIT;groupDesc.layout=mr.textureLayout;groupDesc.entryCount=2;groupDesc.entries=bindings;WGPUBindGroup group=wgpuDeviceCreateBindGroup(mr.device,&groupDesc);if(!view||!group){if(group)wgpuBindGroupRelease(group);if(view)wgpuTextureViewRelease(view);wgpuTextureRelease(gpu);UnloadImage(image);return;}wgpuBindGroupRelease(entry->group);wgpuTextureViewRelease(entry->view);wgpuTextureRelease(entry->texture);entry->texture=gpu;entry->view=view;entry->group=group;
#else
    mr_web_texture_mipmaps(entry->id,image.data,image.width,image.height,image.mipmaps);
#endif
    MemFree(entry->pixels);entry->pixels=image.data;entry->mipmaps=image.mipmaps;texture->mipmaps=image.mipmaps;
}
#ifdef _WIN32
static Image mr_load_image_from_screen(void){
    if(!mr.window||mr.width<=0||mr.height<=0)return(Image){0};HDC screen=GetDC(mr.window),memory=CreateCompatibleDC(screen);MRBitmapInfo info={0};info.header.size=sizeof(MRBitmapInfoHeader);info.header.width=mr.width;info.header.height=-mr.height;info.header.planes=1;info.header.bits=32;void *bits=NULL;HBITMAP bitmap=CreateDIBSection(screen,&info,0,&bits,NULL,0);HGDIOBJ old=SelectObject(memory,bitmap);BitBlt(memory,0,0,mr.width,mr.height,screen,0,0,0x00CC0020u);Image image=GenImageColor(mr.width,mr.height,BLACK);if(image.data&&bits){unsigned char *source=bits,*dest=image.data;for(int i=0;i<mr.width*mr.height;i++){dest[i*4]=source[i*4+2];dest[i*4+1]=source[i*4+1];dest[i*4+2]=source[i*4];dest[i*4+3]=255;}}SelectObject(memory,old);DeleteObject(bitmap);DeleteDC(memory);ReleaseDC(mr.window,screen);return image;
}
#endif
bool LoadImageFromScreenAsync(ImageLoadCallback callback,void *userData){if(!callback)return false;
#ifdef _WIN32
    Image image=mr_load_image_from_screen();if(!IsImageValid(image))return false;if(!mr_queue_readback(image,callback,userData)){UnloadImage(image);return false;}return true;
#else
    unsigned int id=mr_begin_readback(callback,userData);if(!id)return false;if(!mr_web_screen_readback(id)){mr_cancel_readback(id);return false;}return true;
#endif
}
void TakeScreenshot(const char *fileName){if(!fileName)return;
#ifdef __wasm__
    mr_web_screenshot(fileName);
#else
    Image image=mr_load_image_from_screen();if(IsImageValid(image)){ExportImage(image,fileName);UnloadImage(image);}
#endif
}
