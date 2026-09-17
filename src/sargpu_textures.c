/* SarGPU textures module. Compiled through sargpu.c; do not compile separately. */
static Image mr_load_qoi(const unsigned char *data,int size);
static Image mr_load_dds(const unsigned char *data,int size);
#ifdef __INTELLISENSE__
/* IntelliSense can lose preprocessor state inside stb_image's deeply nested
 * implementation and then report false missing-#endif errors for this file.
 * It only needs the decoder entry point to understand SarGPU below. */
unsigned char *stbi_load_from_memory(const unsigned char *buffer,int length,
    int *width,int *height,int *channelsInFile,int desiredChannels);
unsigned char *stbi_load_gif_from_memory(const unsigned char *buffer,int length,
    int **delays,int *width,int *height,int *frames,int *channels,int desiredChannels);
#else
/* Bundled stb_image v2.30, public domain/MIT. */
#define STBI_NO_STDIO
#define STBI_NO_HDR
#define STBI_NO_LINEAR
#define STBI_NO_THREAD_LOCALS
#define STBI_NO_SIMD
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#define STBI_ONLY_BMP
#define STBI_ONLY_TGA
#define STBI_ONLY_GIF
#define STBI_ASSERT(x) ((void)0)
#define STBI_MALLOC(sz) MemAlloc((unsigned int)(sz))
#define STBI_REALLOC(p,sz) MemRealloc((p),(unsigned int)(sz))
#define STBI_FREE(p) MemFree(p)
#ifdef abs
#undef abs
#endif
#define abs(x) ((x)<0 ? -(x) : (x))
#define STB_IMAGE_IMPLEMENTATION
#include "external/stb_image.h"

#undef STB_IMAGE_IMPLEMENTATION
#undef abs
#undef STBI_MALLOC
#undef STBI_REALLOC
#undef STBI_FREE
/* End stb_image configuration. */
#endif /* __INTELLISENSE__ */

Image LoadImageFromMemory(const char *fileType,const unsigned char *fileData,int dataSize) {
    if (!fileType || !fileData || dataSize<=0) return (Image){0};
    if(IsFileExtension(fileType,".qoi"))return mr_load_qoi(fileData,dataSize);
    if(IsFileExtension(fileType,".dds"))return mr_load_dds(fileData,dataSize);
    int width=0,height=0,channels=0;
    unsigned char *decoded=stbi_load_from_memory(fileData,dataSize,&width,&height,&channels,4);
    if (!decoded || width<=0 || height<=0) return (Image){0};
    return (Image){decoded,width,height,1,7};
}
Image LoadImage(const char *fileName) {
    int size=0; unsigned char *data=LoadFileData(fileName,&size); if (!data) return (Image){0};
    Image image=LoadImageFromMemory(GetFileExtension(fileName),data,size); UnloadFileData(data); return image;
}
Image LoadImageAnimFromMemory(const char *fileType,const unsigned char *fileData,int dataSize,int *frames) {
    if (frames) *frames=0;
    if (!fileType || !fileData || dataSize<=0) return (Image){0};
    if (dataSize>=6 && (memcmp(fileData,"GIF87a",6)==0 || memcmp(fileData,"GIF89a",6)==0)) {
        int width=0,height=0,count=0,channels=0;
        unsigned char *pixels=stbi_load_gif_from_memory(fileData,dataSize,NULL,&width,&height,&count,&channels,4);
        if (!pixels || width<=0 || height<=0 || count<=0) { MemFree(pixels); return (Image){0}; }
        if (frames) *frames=count;
        return (Image){pixels,width,height,1,7};
    }
    Image image=LoadImageFromMemory(fileType,fileData,dataSize);
    if (frames && IsImageValid(image)) *frames=1;
    return image;
}
Image LoadImageAnim(const char *fileName,int *frames) {
    if (frames) *frames=0;
    if (!fileName) return (Image){0};
    int size=0;
    unsigned char *data=LoadFileData(fileName,&size);
    if (!data) return (Image){0};
    Image image=LoadImageAnimFromMemory(GetFileExtension(fileName),data,size,frames);
    UnloadFileData(data);
    return image;
}
bool IsImageValid(Image image) { return image.data && image.width>0 && image.height>0 && image.format==7; }
void UnloadImage(Image image) { MemFree(image.data); }
Image GenImageColor(int width,int height,Color color) {
    if (width<=0 || height<=0 || width>8192 || height>8192) return (Image){0};
    Color *pixels=MemAlloc((unsigned int)((size_t)width*height*sizeof *pixels)); if (!pixels) return (Image){0};
    for (int i=0;i<width*height;i++) pixels[i]=color;
    return (Image){pixels,width,height,1,7};
}
Image GenImageGradientLinear(int width,int height,int direction,Color start,Color end) {
    Image image=GenImageColor(width,height,start); if (!IsImageValid(image)) return image;
    float angle=direction*MR_DEG2RAD,dx=sinf(angle),dy=cosf(angle);
    float extent=(width>1 ? (width-1)*(dx<0?-dx:dx) : 0)+(height>1 ? (height-1)*(dy<0?-dy:dy) : 0);
    Color *pixels=image.data;
    for (int y=0;y<height;y++) for (int x=0;x<width;x++) {
        float projection=x*dx+y*dy;
        if (dx<0) projection-=(width-1)*dx; if (dy<0) projection-=(height-1)*dy;
        pixels[y*width+x]=ColorLerp(start,end,extent>0 ? projection/extent : 0);
    }
    return image;
}
Image GenImageGradientRadial(int width,int height,float density,Color inner,Color outer) {
    Image image=GenImageColor(width,height,outer); if (!IsImageValid(image)) return image;
    float cx=(width-1)*0.5f,cy=(height-1)*0.5f,maxDistance=sqrtf(cx*cx+cy*cy);
    Color *pixels=image.data; density=mr_clamp01(density);
    for (int y=0;y<height;y++) for (int x=0;x<width;x++) {
        float dx=x-cx,dy=y-cy,t=maxDistance>0 ? sqrtf(dx*dx+dy*dy)/maxDistance : 0;
        t=(t-density)/(1-density>0.0001f ? 1-density : 0.0001f);
        pixels[y*width+x]=ColorLerp(inner,outer,mr_clamp01(t));
    }
    return image;
}
Image GenImageGradientSquare(int width,int height,float density,Color inner,Color outer) {
    Image image=GenImageColor(width,height,BLANK); if(!IsImageValid(image))return image; Color*p=image.data;
    float cx=(width-1)*0.5f,cy=(height-1)*0.5f,maxd=cx>cy?cx:cy;if(maxd<1)maxd=1;if(density<0)density=0;if(density>1)density=1;
    for(int y=0;y<height;y++)for(int x=0;x<width;x++){float dx=x-cx;if(dx<0)dx=-dx;float dy=y-cy;if(dy<0)dy=-dy;float t=(dx>dy?dx:dy)/maxd;t=(t-density)/(1-density+0.00001f);p[y*width+x]=ColorLerp(inner,outer,mr_clamp01(t));}
    return image;
}
static unsigned int mr_noise_hash(int x,int y) { unsigned int h=(unsigned int)x*374761393u+(unsigned int)y*668265263u;h=(h^(h>>13))*1274126177u;return h^(h>>16); }
static float mr_value_noise(float x,float y) { int x0=(int)x,y0=(int)y;float fx=x-x0,fy=y-y0;float sx=fx*fx*(3-2*fx),sy=fy*fy*(3-2*fy);float a=(mr_noise_hash(x0,y0)&65535)/65535.0f,b=(mr_noise_hash(x0+1,y0)&65535)/65535.0f,c=(mr_noise_hash(x0,y0+1)&65535)/65535.0f,d=(mr_noise_hash(x0+1,y0+1)&65535)/65535.0f;return(a+(b-a)*sx)*(1-sy)+(c+(d-c)*sx)*sy; }
Image GenImagePerlinNoise(int width,int height,int offsetX,int offsetY,float scale) {
    Image image=GenImageColor(width,height,BLACK);if(!IsImageValid(image))return image;if(scale<=0)scale=1;Color*p=image.data;
    for(int y=0;y<height;y++)for(int x=0;x<width;x++){float amplitude=.5714286f,total=0,weight=0,frequency=1;for(int octave=0;octave<4;octave++){total+=mr_value_noise((x+offsetX)*frequency/scale,(y+offsetY)*frequency/scale)*amplitude;weight+=amplitude;amplitude*=.5f;frequency*=2;}unsigned char v=mr_byte(total/weight*255);p[y*width+x]=(Color){v,v,v,255};}return image;
}
Image GenImageCellular(int width,int height,int tileSize) {
    Image image=GenImageColor(width,height,BLACK);if(!IsImageValid(image))return image;if(tileSize<2)tileSize=2;Color*p=image.data;
    for(int y=0;y<height;y++)for(int x=0;x<width;x++){int cellX=x/tileSize,cellY=y/tileSize;float best=(float)(tileSize*tileSize*8);for(int cy=cellY-1;cy<=cellY+1;cy++)for(int cx=cellX-1;cx<=cellX+1;cx++){unsigned int h=mr_noise_hash(cx,cy);float px=cx*tileSize+(h&65535)*(tileSize/65535.0f),py=cy*tileSize+((h>>16)&65535)*(tileSize/65535.0f);float dx=x-px,dy=y-py,d=dx*dx+dy*dy;if(d<best)best=d;}unsigned char v=mr_byte(sqrtf(best)/tileSize*255);p[y*width+x]=(Color){v,v,v,255};}return image;
}
Image GenImageChecked(int width,int height,int checksX,int checksY,Color a,Color b) {
    Image image=GenImageColor(width,height,a); if (!IsImageValid(image)) return image;
    if (checksX<1) checksX=1; if (checksY<1) checksY=1; Color *pixels=image.data;
    for (int y=0;y<height;y++) for (int x=0;x<width;x++) pixels[y*width+x]=((x/checksX+y/checksY)&1)?b:a;
    return image;
}
Image GenImageWhiteNoise(int width,int height,float factor) {
    Image image=GenImageColor(width,height,WHITE); if (!IsImageValid(image)) return image;
    factor=mr_clamp01(factor); Color *pixels=image.data;
    for (int i=0;i<width*height;i++) if ((mr_random()&0xffffffu)<(unsigned int)(factor*16777215.0f)) pixels[i]=BLACK;
    return image;
}
Image ImageCopy(Image image) {
    if (!IsImageValid(image)) return (Image){0}; size_t bytes=(size_t)image.width*image.height*4;
    void *data=MemAlloc((unsigned int)bytes); if (!data) return (Image){0}; memcpy(data,image.data,bytes);
    return (Image){data,image.width,image.height,1,7};
}
Image ImageFromImage(Image image,Rectangle rec) {
    if (!IsImageValid(image)) return (Image){0};
    int x=(int)rec.x,y=(int)rec.y,w=(int)rec.width,h=(int)rec.height;
    if (x<0) { w+=x; x=0; } if (y<0) { h+=y; y=0; }
    if (x+w>image.width) w=image.width-x; if (y+h>image.height) h=image.height-y;
    Image result=GenImageColor(w,h,BLANK); if (!IsImageValid(result)) return result;
    for (int row=0;row<h;row++) memcpy((Color *)result.data+row*w,(Color *)image.data+(y+row)*image.width+x,(size_t)w*4);
    return result;
}
void ImageCrop(Image *image,Rectangle crop) {
    if (!image) return; Image result=ImageFromImage(*image,crop); if (!IsImageValid(result)) return;
    MemFree(image->data); *image=result;
}
void ImageResizeNN(Image *image,int newWidth,int newHeight) {
    if (!image || !IsImageValid(*image) || newWidth<=0 || newHeight<=0 || newWidth>8192 || newHeight>8192) return;
    Color *output=MemAlloc((unsigned int)((size_t)newWidth*newHeight*4)); if (!output) return; Color *input=image->data;
    for (int y=0;y<newHeight;y++) for (int x=0;x<newWidth;x++) output[y*newWidth+x]=input[(y*image->height/newHeight)*image->width+x*image->width/newWidth];
    MemFree(image->data); image->data=output; image->width=newWidth; image->height=newHeight; image->mipmaps=1; image->format=7;
}
void ImageResize(Image *image,int newWidth,int newHeight) {
    if (!image || !IsImageValid(*image) || newWidth<=0 || newHeight<=0 || newWidth>8192 || newHeight>8192) return;
    Color *output=MemAlloc((unsigned int)((size_t)newWidth*newHeight*4)); if (!output) return; Color *input=image->data;
    for (int y=0;y<newHeight;y++) for (int x=0;x<newWidth;x++) {
        float sx=(x+0.5f)*image->width/newWidth-0.5f,sy=(y+0.5f)*image->height/newHeight-0.5f;
        int x0=(int)sx,y0=(int)sy; float fx=sx-x0,fy=sy-y0; if (x0<0){x0=0;fx=0;} if(y0<0){y0=0;fy=0;}
        int x1=x0+1<image->width?x0+1:x0,y1=y0+1<image->height?y0+1:y0;
        Color a=ColorLerp(input[y0*image->width+x0],input[y0*image->width+x1],fx);
        Color b=ColorLerp(input[y1*image->width+x0],input[y1*image->width+x1],fx);
        output[y*newWidth+x]=ColorLerp(a,b,fy);
    }
    MemFree(image->data); image->data=output; image->width=newWidth; image->height=newHeight; image->mipmaps=1; image->format=7;
}
void ImageResizeCanvas(Image *image,int newWidth,int newHeight,int offsetX,int offsetY,Color fill) {
    if (!image || !IsImageValid(*image) || newWidth<=0 || newHeight<=0 || newWidth>8192 || newHeight>8192) return;
    Image canvas=GenImageColor(newWidth,newHeight,fill); if (!IsImageValid(canvas)) return;
    Color *src=image->data,*dst=canvas.data;
    for (int y=0;y<image->height;y++) for (int x=0;x<image->width;x++) {
        int dx=x+offsetX,dy=y+offsetY; if (dx>=0&&dy>=0&&dx<newWidth&&dy<newHeight) dst[dy*newWidth+dx]=src[y*image->width+x];
    }
    MemFree(image->data); *image=canvas;
}
void ImageFlipVertical(Image *image) {
    if (!image || !IsImageValid(*image)) return; Color *p=image->data;
    for (int y=0;y<image->height/2;y++) for (int x=0;x<image->width;x++) { int a=y*image->width+x,b=(image->height-1-y)*image->width+x; Color t=p[a];p[a]=p[b];p[b]=t; }
}
void ImageFlipHorizontal(Image *image) {
    if (!image || !IsImageValid(*image)) return; Color *p=image->data;
    for (int y=0;y<image->height;y++) for (int x=0;x<image->width/2;x++) { int a=y*image->width+x,b=y*image->width+image->width-1-x; Color t=p[a];p[a]=p[b];p[b]=t; }
}
void ImageRotateCW(Image *image) {
    if (!image || !IsImageValid(*image)) return; int oldW=image->width,oldH=image->height; Color *src=image->data;
    Color *dst=MemAlloc((unsigned int)((size_t)oldW*oldH*4)); if (!dst) return;
    for (int y=0;y<oldH;y++) for (int x=0;x<oldW;x++) dst[x*oldH+(oldH-1-y)]=src[y*oldW+x];
    MemFree(src); image->data=dst; image->width=oldH; image->height=oldW;
}
void ImageRotateCCW(Image *image) {
    if (!image || !IsImageValid(*image)) return; int oldW=image->width,oldH=image->height; Color *src=image->data;
    Color *dst=MemAlloc((unsigned int)((size_t)oldW*oldH*4)); if (!dst) return;
    for (int y=0;y<oldH;y++) for (int x=0;x<oldW;x++) dst[(oldW-1-x)*oldH+y]=src[y*oldW+x];
    MemFree(src); image->data=dst; image->width=oldH; image->height=oldW;
}
void ImageRotate(Image *image,int degrees) {
    if(!image||!IsImageValid(*image))return;degrees%=360;if(degrees<0)degrees+=360;
    if(degrees==0)return;if(degrees==90){ImageRotateCW(image);return;}if(degrees==180){ImageFlipHorizontal(image);ImageFlipVertical(image);return;}if(degrees==270){ImageRotateCCW(image);return;}
    float angle=degrees*MR_DEG2RAD,c=cosf(angle),s=sinf(angle),ac=c<0?-c:c,as=s<0?-s:s;int oldW=image->width,oldH=image->height,newW=(int)(oldW*ac+oldH*as+0.9999f),newH=(int)(oldW*as+oldH*ac+0.9999f);
    Color*src=image->data,*dst=MemAlloc((unsigned int)((size_t)newW*newH*4));if(!dst)return;float ocx=(oldW-1)*.5f,ocy=(oldH-1)*.5f,ncx=(newW-1)*.5f,ncy=(newH-1)*.5f;
    for(int y=0;y<newH;y++)for(int x=0;x<newW;x++){float dx=x-ncx,dy=y-ncy;int sx=(int)(dx*c+dy*s+ocx+.5f),sy=(int)(-dx*s+dy*c+ocy+.5f);dst[y*newW+x]=(sx>=0&&sy>=0&&sx<oldW&&sy<oldH)?src[sy*oldW+sx]:BLANK;}
    MemFree(image->data);image->data=dst;image->width=newW;image->height=newH;image->mipmaps=1;
}
void ImageKernelConvolution(Image *image,const float *kernel,int kernelSize) {
    if(!image||!IsImageValid(*image)||!kernel||kernelSize<1||!(kernelSize&1))return;int w=image->width,h=image->height,r=kernelSize/2;Color*src=image->data,*dst=MemAlloc((unsigned int)((size_t)w*h*4));if(!dst)return;
    for(int y=0;y<h;y++)for(int x=0;x<w;x++){float red=0,green=0,blue=0,alpha=0;for(int ky=0;ky<kernelSize;ky++)for(int kx=0;kx<kernelSize;kx++){int sx=x+kx-r,sy=y+ky-r;if(sx<0)sx=0;if(sx>=w)sx=w-1;if(sy<0)sy=0;if(sy>=h)sy=h-1;float weight=kernel[ky*kernelSize+kx];Color p=src[sy*w+sx];red+=p.r*weight;green+=p.g*weight;blue+=p.b*weight;alpha+=p.a*weight;}dst[y*w+x]=(Color){mr_byte(red),mr_byte(green),mr_byte(blue),mr_byte(alpha)};}
    MemFree(image->data);image->data=dst;image->mipmaps=1;
}
void ImageBlurGaussian(Image *image,int blurSize) {
    if(!image||!IsImageValid(*image)||blurSize<2)return;if(!(blurSize&1))blurSize++;if(blurSize>31)blurSize=31;int r=blurSize/2;float*kernel=MemAlloc((unsigned int)(blurSize*blurSize*sizeof(float)));if(!kernel)return;float total=0;
    for(int y=-r;y<=r;y++)for(int x=-r;x<=r;x++){float weight=(float)(r+1-(x<0?-x:x))*(r+1-(y<0?-y:y));kernel[(y+r)*blurSize+x+r]=weight;total+=weight;}for(int i=0;i<blurSize*blurSize;i++)kernel[i]/=total;ImageKernelConvolution(image,kernel,blurSize);MemFree(kernel);
}
void ImageDither(Image *image,int rb,int gb,int bb,int ab) {
    if(!image||!IsImageValid(*image))return;int bits[4]={rb,gb,bb,ab};for(int i=0;i<4;i++){if(bits[i]<0)bits[i]=0;if(bits[i]>8)bits[i]=8;}static const unsigned char matrix[16]={0,8,2,10,12,4,14,6,3,11,1,9,15,7,13,5};Color*p=image->data;
    for(int y=0;y<image->height;y++)for(int x=0;x<image->width;x++){unsigned char*c=(unsigned char*)&p[y*image->width+x];int threshold=(int)matrix[(y&3)*4+(x&3)]-8;for(int k=0;k<4;k++){if(!bits[k]){c[k]=k==3?255:0;continue;}int levels=(1<<bits[k])-1;int value=c[k]+threshold*255/(levels*16);if(value<0)value=0;if(value>255)value=255;c[k]=(unsigned char)(((value*levels+127)/255)*255/levels);}}
}
void ImageToPOT(Image *image,Color fill) { if(!image||!IsImageValid(*image))return;int w=1,h=1;while(w<image->width)w<<=1;while(h<image->height)h<<=1;if(w!=image->width||h!=image->height)ImageResizeCanvas(image,w,h,0,0,fill); }
Image ImageFromChannel(Image image,int channel) { if(!IsImageValid(image)||channel<0||channel>3)return(Image){0};Image out=GenImageColor(image.width,image.height,BLACK);if(!IsImageValid(out))return out;Color*src=image.data,*dst=out.data;for(int i=0;i<image.width*image.height;i++){unsigned char value=((unsigned char*)&src[i])[channel];dst[i]=(Color){value,value,value,255};}return out; }
void ImageAlphaCrop(Image *image,float threshold) {
    if (!image || !IsImageValid(*image)) return; Rectangle border=GetImageAlphaBorder(*image,threshold);
    if (border.width>0 && border.height>0) ImageCrop(image,border);
}
void ImageAlphaClear(Image *image,Color color,float threshold) {
    if (!image || !IsImageValid(*image)) return; Color *p=image->data; unsigned char limit=(unsigned char)(mr_clamp01(threshold)*255);
    for (int i=0;i<image->width*image->height;i++) if (p[i].a<=limit) p[i]=color;
}
void ImageAlphaMask(Image *image,Image mask) {
    if (!image || !IsImageValid(*image) || !IsImageValid(mask) || image->width!=mask.width || image->height!=mask.height) return;
    Color *p=image->data,*m=mask.data; for (int i=0;i<image->width*image->height;i++) p[i].a=m[i].r;
}
void ImageAlphaPremultiply(Image *image) {
    if (!image || !IsImageValid(*image)) return; Color *p=image->data;
    for (int i=0;i<image->width*image->height;i++) { p[i].r=(unsigned char)(p[i].r*p[i].a/255); p[i].g=(unsigned char)(p[i].g*p[i].a/255); p[i].b=(unsigned char)(p[i].b*p[i].a/255); }
}
void ImageColorTint(Image *image,Color tint) { if (!image || !IsImageValid(*image)) return; Color *p=image->data; for(int i=0;i<image->width*image->height;i++) p[i]=ColorTint(p[i],tint); }
void ImageColorInvert(Image *image) { if (!image || !IsImageValid(*image)) return; Color *p=image->data; for(int i=0;i<image->width*image->height;i++){p[i].r=255-p[i].r;p[i].g=255-p[i].g;p[i].b=255-p[i].b;} }
void ImageColorGrayscale(Image *image) { if (!image || !IsImageValid(*image)) return; Color *p=image->data; for(int i=0;i<image->width*image->height;i++){unsigned char g=(unsigned char)((p[i].r*77+p[i].g*150+p[i].b*29)>>8);p[i].r=p[i].g=p[i].b=g;} }
void ImageColorContrast(Image *image,float contrast) { if (!image || !IsImageValid(*image)) return; Color *p=image->data; for(int i=0;i<image->width*image->height;i++) p[i]=ColorContrast(p[i],contrast/100.0f); }
void ImageColorBrightness(Image *image,int brightness) { if (!image || !IsImageValid(*image)) return; if(brightness<-255)brightness=-255;if(brightness>255)brightness=255; Color *p=image->data; for(int i=0;i<image->width*image->height;i++){p[i].r=mr_byte(p[i].r+brightness);p[i].g=mr_byte(p[i].g+brightness);p[i].b=mr_byte(p[i].b+brightness);} }
void ImageColorReplace(Image *image,Color color,Color replace) { if (!image || !IsImageValid(*image)) return; Color *p=image->data; for(int i=0;i<image->width*image->height;i++) if(ColorIsEqual(p[i],color))p[i]=replace; }
Color *LoadImageColors(Image image) {
    if (!IsImageValid(image)) return NULL; size_t count=(size_t)image.width*image.height;
    Color *copy=MemAlloc((unsigned int)(count*sizeof *copy)); if (copy) memcpy(copy,image.data,count*sizeof *copy); return copy;
}
Color *LoadImagePalette(Image image,int maxPaletteSize,int *colorCount) {
    if (colorCount) *colorCount=0; if (!IsImageValid(image) || maxPaletteSize<=0) return NULL;
    Color *palette=MemAlloc((unsigned int)maxPaletteSize*sizeof *palette); if (!palette) return NULL; Color *pixels=image.data; int count=0;
    for (int i=0;i<image.width*image.height && count<maxPaletteSize;i++) { bool found=false; for(int j=0;j<count;j++) if(ColorIsEqual(pixels[i],palette[j])){found=true;break;} if(!found)palette[count++]=pixels[i]; }
    if (colorCount) *colorCount=count; return palette;
}
void UnloadImageColors(Color *colors) { MemFree(colors); }
void UnloadImagePalette(Color *colors) { MemFree(colors); }
Rectangle GetImageAlphaBorder(Image image,float threshold) {
    if (!IsImageValid(image)) return (Rectangle){0}; unsigned char limit=(unsigned char)(mr_clamp01(threshold)*255); Color *p=image.data;
    int minX=image.width,minY=image.height,maxX=-1,maxY=-1;
    for(int y=0;y<image.height;y++)for(int x=0;x<image.width;x++)if(p[y*image.width+x].a>limit){if(x<minX)minX=x;if(y<minY)minY=y;if(x>maxX)maxX=x;if(y>maxY)maxY=y;}
    return maxX>=minX ? (Rectangle){(float)minX,(float)minY,(float)(maxX-minX+1),(float)(maxY-minY+1)} : (Rectangle){0};
}
Color GetImageColor(Image image,int x,int y) {
    if (!IsImageValid(image) || x<0 || y<0 || x>=image.width || y>=image.height) return BLANK;
    return ((Color *)image.data)[y*image.width+x];
}
void ImageClearBackground(Image *dst,Color color) { if(!dst||!IsImageValid(*dst))return;Color*p=dst->data;for(int i=0;i<dst->width*dst->height;i++)p[i]=color; }
void ImageDrawPixel(Image *dst,int x,int y,Color color) { if(!dst||!IsImageValid(*dst)||x<0||y<0||x>=dst->width||y>=dst->height)return;((Color*)dst->data)[y*dst->width+x]=color; }
void ImageDrawPixelV(Image *dst,Vector2 p,Color color) { ImageDrawPixel(dst,(int)p.x,(int)p.y,color); }
void ImageDrawLine(Image *dst,int x0,int y0,int x1,int y1,Color color) {
    int dx=x1>x0?x1-x0:x0-x1,sx=x0<x1?1:-1,dy=-(y1>y0?y1-y0:y0-y1),sy=y0<y1?1:-1,error=dx+dy;
    for(;;){ImageDrawPixel(dst,x0,y0,color);if(x0==x1&&y0==y1)break;int twice=2*error;if(twice>=dy){error+=dy;x0+=sx;}if(twice<=dx){error+=dx;y0+=sy;}}
}
void ImageDrawLineV(Image *dst,Vector2 start,Vector2 end,Color color) { ImageDrawLine(dst,(int)start.x,(int)start.y,(int)end.x,(int)end.y,color); }
void ImageDrawCircle(Image *dst,int cx,int cy,int radius,Color color) { if(radius<0)return;for(int y=-radius;y<=radius;y++){int span=(int)sqrtf((float)(radius*radius-y*y));for(int x=-span;x<=span;x++)ImageDrawPixel(dst,cx+x,cy+y,color);} }
void ImageDrawCircleV(Image *dst,Vector2 center,int radius,Color color) { ImageDrawCircle(dst,(int)center.x,(int)center.y,radius,color); }
void ImageDrawRectangle(Image *dst,int x,int y,int width,int height,Color color) { for(int py=y;py<y+height;py++)for(int px=x;px<x+width;px++)ImageDrawPixel(dst,px,py,color); }
void ImageDrawRectangleV(Image *dst,Vector2 position,Vector2 size,Color color) { ImageDrawRectangle(dst,(int)position.x,(int)position.y,(int)size.x,(int)size.y,color); }
void ImageDrawRectangleRec(Image *dst,Rectangle rec,Color color) { ImageDrawRectangle(dst,(int)rec.x,(int)rec.y,(int)rec.width,(int)rec.height,color); }
void ImageDrawRectangleLines(Image *dst,Rectangle rec,int thick,Color color) { if(thick<1)return;ImageDrawRectangle(dst,(int)rec.x,(int)rec.y,(int)rec.width,thick,color);ImageDrawRectangle(dst,(int)rec.x,(int)(rec.y+rec.height-thick),(int)rec.width,thick,color);ImageDrawRectangle(dst,(int)rec.x,(int)rec.y,thick,(int)rec.height,color);ImageDrawRectangle(dst,(int)(rec.x+rec.width-thick),(int)rec.y,thick,(int)rec.height,color); }
void ImageDraw(Image *dst,Image src,Rectangle srcRec,Rectangle dstRec,Color tint) {
    if(!dst||!IsImageValid(*dst)||!IsImageValid(src)||dstRec.width==0||dstRec.height==0)return;Color *sp=src.data;
    int dw=(int)(dstRec.width<0?-dstRec.width:dstRec.width),dh=(int)(dstRec.height<0?-dstRec.height:dstRec.height);
    for(int y=0;y<dh;y++)for(int x=0;x<dw;x++){int sx=(int)(srcRec.x+x*srcRec.width/dw),sy=(int)(srcRec.y+y*srcRec.height/dh);if(sx>=0&&sy>=0&&sx<src.width&&sy<src.height)ImageDrawPixel(dst,(int)dstRec.x+x,(int)dstRec.y+y,ColorTint(sp[sy*src.width+sx],tint));}
}
Texture2D LoadTextureFromImage(Image image) {
    return IsImageValid(image) ? LoadTextureRGBA((const unsigned char *)image.data,image.width,image.height) : (Texture2D){0};
}
Texture2D LoadTexture(const char *fileName) {
    Image image=LoadImage(fileName); if (!IsImageValid(image)) return (Texture2D){0};
    Texture2D texture=LoadTextureFromImage(image); UnloadImage(image); return texture;
}
bool IsTextureValid(Texture2D texture) {
    return texture.id!=0 && texture.width>0 && texture.height>0 && mr_texture(texture.id)!=NULL;
}
RenderTexture2D LoadRenderTexture(int width,int height) {
    if (!mr.ready || width<=0 || height<=0 || width>8192 || height>8192) return (RenderTexture2D){0};
    MRTexture *entry=NULL; for (int i=0;i<MR_MAX_TEXTURES;i++) if (!mr.textures[i].id) { entry=&mr.textures[i]; break; }
    if (!entry) return (RenderTexture2D){0}; unsigned int id=++mr.nextTexture;
#ifdef _WIN32
    WGPUTextureDescriptor descriptor=WGPU_TEXTURE_DESCRIPTOR_INIT;
    descriptor.size=(WGPUExtent3D){(uint32_t)width,(uint32_t)height,1}; descriptor.dimension=WGPUTextureDimension_2D;
    descriptor.format=mr.config.format; descriptor.usage=WGPUTextureUsage_TextureBinding|WGPUTextureUsage_RenderAttachment|WGPUTextureUsage_CopySrc;
    entry->texture=wgpuDeviceCreateTexture(mr.device,&descriptor); if (!entry->texture) return (RenderTexture2D){0};
    entry->view=wgpuTextureCreateView(entry->texture,NULL);
    descriptor.format=WGPUTextureFormat_Depth24Plus;descriptor.usage=WGPUTextureUsage_RenderAttachment;
    entry->depthTexture=wgpuDeviceCreateTexture(mr.device,&descriptor);
    if(entry->depthTexture)entry->depthView=wgpuTextureCreateView(entry->depthTexture,NULL);
    WGPUBindGroupEntry bindings[2]={WGPU_BIND_GROUP_ENTRY_INIT,WGPU_BIND_GROUP_ENTRY_INIT};
    bindings[0].binding=0; bindings[0].sampler=mr.sampler; bindings[1].binding=1; bindings[1].textureView=entry->view;
    WGPUBindGroupDescriptor group=WGPU_BIND_GROUP_DESCRIPTOR_INIT; group.layout=mr.textureLayout; group.entryCount=2; group.entries=bindings;
    entry->group=wgpuDeviceCreateBindGroup(mr.device,&group);
    if (!entry->view || !entry->group || !entry->depthView) { if(entry->group)wgpuBindGroupRelease(entry->group);if(entry->depthView)wgpuTextureViewRelease(entry->depthView);if(entry->depthTexture)wgpuTextureRelease(entry->depthTexture);if(entry->view)wgpuTextureViewRelease(entry->view);wgpuTextureRelease(entry->texture);memset(entry,0,sizeof *entry);return(RenderTexture2D){0}; }
#else
    mr_web_render_texture(id,width,height);
#endif
    entry->id=id;entry->width=width;entry->height=height;entry->mipmaps=1;entry->renderTarget=true; Texture2D texture={id,width,height,1,7}; return (RenderTexture2D){id,texture,{0}};
}
bool IsRenderTextureValid(RenderTexture2D target) { return target.id!=0 && target.id==target.texture.id && IsTextureValid(target.texture); }
void UnloadRenderTexture(RenderTexture2D target) { if (mr.renderTarget==target.id) EndTextureMode(); UnloadTexture(target.texture); }

void DrawTexturePro(Texture2D texture,Rectangle source,Rectangle dest,Vector2 origin,float rotation,Color tint) {
    if (!mr_texture(texture.id) || texture.width<=0 || texture.height<=0 || dest.width==0 || dest.height==0) return;
    float u0=source.x/texture.width,v0=source.y/texture.height;
    float u1=(source.x+source.width)/texture.width,v1=(source.y+source.height)/texture.height;
    float angle=rotation*MR_DEG2RAD,c=cosf(angle),s=sinf(angle);
    Vector2 local[4]={{-origin.x,-origin.y},{dest.width-origin.x,-origin.y},
        {dest.width-origin.x,dest.height-origin.y},{-origin.x,dest.height-origin.y}};
    Vector2 point[4];
    for(int i=0;i<4;i++) point[i]=(Vector2){dest.x+local[i].x*c-local[i].y*s,dest.y+local[i].x*s+local[i].y*c};
    mr_triangle(point[0],point[1],point[2],(Vector2){u0,v0},(Vector2){u1,v0},(Vector2){u1,v1},tint,texture.id);
    mr_triangle(point[0],point[2],point[3],(Vector2){u0,v0},(Vector2){u1,v1},(Vector2){u0,v1},tint,texture.id);
}
void DrawTextureRec(Texture2D texture,Rectangle source,Vector2 position,Color tint) {
    Rectangle dest={position.x,position.y,source.width<0?-source.width:source.width,source.height<0?-source.height:source.height};
    DrawTexturePro(texture,source,dest,(Vector2){0},0,tint);
}
void DrawTextureEx(Texture2D texture,Vector2 position,float rotation,float scale,Color tint) {
    if (scale<=0) return;
    DrawTexturePro(texture,(Rectangle){0,0,(float)texture.width,(float)texture.height},
        (Rectangle){position.x,position.y,texture.width*scale,texture.height*scale},(Vector2){0},rotation,tint);
}
void DrawTextureV(Texture2D texture,Vector2 position,Color tint) { DrawTextureEx(texture,position,0,1,tint); }
void DrawTexture(Texture2D texture,int x,int y,Color tint) { DrawTextureV(texture,(Vector2){(float)x,(float)y},tint); }
void DrawTextureNPatch(Texture2D texture,NPatchInfo info,Rectangle dest,Vector2 origin,float rotation,Color tint) {
    float sw=info.source.width<0?-info.source.width:info.source.width;
    float sh=info.source.height<0?-info.source.height:info.source.height;
    if (!IsTextureValid(texture) || sw<=0 || sh<=0 || dest.width<=0 || dest.height<=0) return;
    float left=(float)info.left,right=(float)info.right,top=(float)info.top,bottom=(float)info.bottom;
    if (left<0) left=0; if (right<0) right=0; if (top<0) top=0; if (bottom<0) bottom=0;
    if (left+right>sw) { float scale=sw/(left+right); left*=scale; right*=scale; }
    if (top+bottom>sh) { float scale=sh/(top+bottom); top*=scale; bottom*=scale; }
    if (info.layout==NPATCH_THREE_PATCH_VERTICAL) left=right=0;
    if (info.layout==NPATCH_THREE_PATCH_HORIZONTAL) top=bottom=0;
    float dl=left,dr=right,dt=top,db=bottom;
    if (dl+dr>dest.width) { float scale=dest.width/(dl+dr); dl*=scale; dr*=scale; }
    if (dt+db>dest.height) { float scale=dest.height/(dt+db); dt*=scale; db*=scale; }
    float sxSign=info.source.width<0?-1.0f:1.0f,sySign=info.source.height<0?-1.0f:1.0f;
    float sx[4]={info.source.x,info.source.x+sxSign*left,info.source.x+sxSign*(sw-right),info.source.x+info.source.width};
    float sy[4]={info.source.y,info.source.y+sySign*top,info.source.y+sySign*(sh-bottom),info.source.y+info.source.height};
    float dx[4]={0,dl,dest.width-dr,dest.width},dy[4]={0,dt,dest.height-db,dest.height};
    int columnFirst=0,columnLast=3,rowFirst=0,rowLast=3;
    if (info.layout==NPATCH_THREE_PATCH_VERTICAL) { columnLast=1; sx[1]=info.source.x+info.source.width; dx[1]=dest.width; }
    if (info.layout==NPATCH_THREE_PATCH_HORIZONTAL) { rowLast=1; sy[1]=info.source.y+info.source.height; dy[1]=dest.height; }
    float angle=rotation*MR_DEG2RAD,c=cosf(angle),s=sinf(angle);
    for (int row=rowFirst;row<rowLast;row++) for (int column=columnFirst;column<columnLast;column++) {
        float pieceWidth=dx[column+1]-dx[column],pieceHeight=dy[row+1]-dy[row];
        float sourceWidth=sx[column+1]-sx[column],sourceHeight=sy[row+1]-sy[row];
        if (pieceWidth<=0 || pieceHeight<=0 || sourceWidth==0 || sourceHeight==0) continue;
        float localX=dx[column]-origin.x,localY=dy[row]-origin.y;
        Rectangle pieceDest={dest.x+localX*c-localY*s,dest.y+localX*s+localY*c,pieceWidth,pieceHeight};
        Vector2 pieceOrigin={0,0};
        DrawTexturePro(texture,(Rectangle){sx[column],sy[row],sourceWidth,sourceHeight},pieceDest,pieceOrigin,rotation,tint);
    }
}
