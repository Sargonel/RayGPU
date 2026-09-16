/* RayGPU model importers. Compiled through raygpu.c.
 * Copyright (c) 2026 Sargonel. Distributed under the MIT license. */
/* Portable C17 limits also avoid Windows SDK integer suffixes in IntelliSense. */
#define MR_FMT_UINT32_MAX 4294967295u
#define MR_FMT_INT32_MAX 2147483647

static void *mr_fmt_alloc(size_t count,size_t size) {
    if(!count||!size||count>MR_FMT_UINT32_MAX/size)return NULL;
    void *p=MemAlloc((unsigned int)(count*size));if(p)memset(p,0,count*size);return p;
}
static bool mr_fmt_grow(void **data,int *capacity,int count,size_t size) {
    if(count<0)return false;if(count<=*capacity)return true;
    size_t next=*capacity?(size_t)*capacity*2:64;if(next<(size_t)count)next=count;
    if(next>MR_FMT_INT32_MAX||next>MR_FMT_UINT32_MAX/size)return false;
    void *p=MemRealloc(*data,(unsigned int)(next*size));if(!p)return false;
    *data=p;*capacity=(int)next;return true;
}
static bool mr_fmt_range(size_t size,uint32_t offset,uint32_t count,size_t stride) {
    return offset<=size&&stride&&count<=(size-offset)/stride;
}
static float mr_fmt_float(const unsigned char *p){float f;memcpy(&f,p,4);return f;}
static void mr_fmt_copy(char *dst,size_t size,const char *src) {
    if(!size)return;size_t i=0;while(src&&src[i]&&i+1<size){dst[i]=src[i];i++;}dst[i]=0;
}
static double mr_fmt_number(const char *s,char **end) {
    while(*s==' '||*s=='\t')s++;double sign=1,value=0,scale=1;
    if(*s=='-'||*s=='+'){if(*s=='-')sign=-1;s++;}
    while(*s>='0'&&*s<='9')value=value*10+*s++-'0';
    if(*s=='.'){s++;while(*s>='0'&&*s<='9'){scale*=.1;value+=(*s++-'0')*scale;}}
    if(*s=='e'||*s=='E'){const char *e=s+1;int esign=1,exponent=0;if(*e=='+'||*e=='-'){if(*e=='-')esign=-1;e++;}
        if(*e>='0'&&*e<='9'){while(*e>='0'&&*e<='9'){if(exponent<400)exponent=exponent*10+*e-'0';e++;}if(exponent>400)exponent=400;while(exponent--)value*=esign>0?10:.1;s=e;}}
    if(end)*end=(char*)s;return value*sign;
}
static char *mr_fmt_space(char *s){while(*s==' '||*s=='\t')s++;return s;}
static char *mr_fmt_trim(char *s){s=mr_fmt_space(s);size_t n=TextLength(s);while(n&&(s[n-1]==' '||s[n-1]=='\t'))s[--n]=0;return s;}
static char *mr_fmt_line(char **cursor) {
    if(!**cursor)return NULL;char *line=*cursor,*s=line;
    while(*s&&*s!='\n'&&*s!='\r')s++;
    if(*s){char c=*s;*s++=0;if(c=='\r'&&*s=='\n')s++;}*cursor=s;
    char *comment=line;while(*comment&&*comment!='#')comment++;if(*comment)*comment=0;
    return mr_fmt_space(line);
}
static char *mr_fmt_token(char **cursor) {
    char *s=mr_fmt_space(*cursor);if(!*s){*cursor=s;return NULL;}
    char quote=0;if(*s=='"'||*s=='\'')quote=*s++;
    char *result=s;while(*s&&(quote?*s!=quote:*s!=' '&&*s!='\t'))s++;
    if(*s)*s++=0;*cursor=s;return result;
}
typedef struct MRFormatVertex {Vector3 p,n;Vector2 uv;Color color;unsigned char ids[4];float weights[4];} MRFormatVertex;
typedef struct MRFormatTriangle {MRFormatVertex v[3];int material,group;} MRFormatTriangle;
typedef struct MRFormatGeometry {MRFormatTriangle *triangles;int count,capacity,group;} MRFormatGeometry;
static bool mr_fmt_triangle(MRFormatGeometry *g,MRFormatVertex a,MRFormatVertex b,MRFormatVertex c,int material) {
    if(g->count==MR_FMT_INT32_MAX||!mr_fmt_grow((void**)&g->triangles,&g->capacity,g->count+1,sizeof *g->triangles))return false;
    Vector3 normal=mr_v3_norm(mr_v3_cross(mr_v3_sub(b.p,a.p),mr_v3_sub(c.p,a.p)));
    if(mr_v3_dot(a.n,a.n)==0)a.n=normal;if(mr_v3_dot(b.n,b.n)==0)b.n=normal;if(mr_v3_dot(c.n,c.n)==0)c.n=normal;
    g->triangles[g->count++]=(MRFormatTriangle){{a,b,c},material,g->group};return true;
}
/* Split large batches into at most 65535 vertices; no 16-bit index truncation. */
static Model mr_fmt_model(MRFormatGeometry *g,Material *materials,int materialCount,int boneCount) {
    Model model={0};model.transform=mr_matrix_identity();model.materials=materials;model.materialCount=materialCount;
    if(!g->count||!materials||materialCount<=0)goto fail;
    for(int m=0;m<materialCount;m++)if(!materials[m].maps)goto fail;
    for(int i=0;i<g->count;){int material=g->triangles[i].material,group=g->triangles[i].group,start=i;if(material<0||material>=materialCount)goto fail;
        while(i<g->count&&g->triangles[i].material==material&&g->triangles[i].group==group)i++;
        model.meshCount+=(i-start+21844)/21845;}
    model.meshes=mr_fmt_alloc(model.meshCount,sizeof(Mesh));model.meshMaterial=mr_fmt_alloc(model.meshCount,sizeof(int));
    if(!model.meshes||!model.meshMaterial)goto fail;
    int slot=0;
    for(int start=0;start<g->count;){int m=g->triangles[start].material,group=g->triangles[start].group,end=start;
        while(end<g->count&&g->triangles[end].material==m&&g->triangles[end].group==group)end++;
        int left=end-start,source=start;
        while(left){Mesh *mesh=&model.meshes[slot];int triangles=left>21845?21845:left;mesh->vertexCount=triangles*3;mesh->triangleCount=triangles;
            mesh->vertices=mr_fmt_alloc(mesh->vertexCount,3*sizeof(float));mesh->normals=mr_fmt_alloc(mesh->vertexCount,3*sizeof(float));
            mesh->texcoords=mr_fmt_alloc(mesh->vertexCount,2*sizeof(float));mesh->colors=mr_fmt_alloc(mesh->vertexCount,4);
            if(!mesh->vertices||!mesh->normals||!mesh->texcoords||!mesh->colors)goto fail;
            if(boneCount){mesh->boneCount=boneCount;mesh->boneIds=mr_fmt_alloc(mesh->vertexCount,4);mesh->boneWeights=mr_fmt_alloc(mesh->vertexCount,4*sizeof(float));
                mesh->boneMatrices=mr_fmt_alloc(boneCount,sizeof(Matrix));mesh->animVertices=mr_fmt_alloc(mesh->vertexCount,3*sizeof(float));mesh->animNormals=mr_fmt_alloc(mesh->vertexCount,3*sizeof(float));
                if(!mesh->boneIds||!mesh->boneWeights||!mesh->boneMatrices||!mesh->animVertices||!mesh->animNormals)goto fail;
                for(int b=0;b<boneCount;b++)mesh->boneMatrices[b]=mr_matrix_identity();}
            for(int t=0;t<triangles;t++){
                MRFormatTriangle *tri=&g->triangles[source++];for(int j=0;j<3;j++){int v=t*3+j;MRFormatVertex *p=&tri->v[j];
                    memcpy(mesh->vertices+v*3,&p->p,3*sizeof(float));memcpy(mesh->normals+v*3,&p->n,3*sizeof(float));
                    memcpy(mesh->texcoords+v*2,&p->uv,2*sizeof(float));memcpy(mesh->colors+v*4,&p->color,4);
                    if(boneCount){memcpy(mesh->boneIds+v*4,p->ids,4);memcpy(mesh->boneWeights+v*4,p->weights,4*sizeof(float));}}
            }if(boneCount){memcpy(mesh->animVertices,mesh->vertices,(size_t)mesh->vertexCount*3*sizeof(float));memcpy(mesh->animNormals,mesh->normals,(size_t)mesh->vertexCount*3*sizeof(float));}
            model.meshMaterial[slot++]=m;left-=triangles;UploadMesh(mesh,false);
        }start=end;
    }return model;
fail:UnloadModel(model);return(Model){0};
}
typedef struct MRFormatMaterial {char name[256];Material material;} MRFormatMaterial;
typedef struct MRFormatMaterials {MRFormatMaterial *items;int count,capacity;} MRFormatMaterials;
static void mr_fmt_materials_free(MRFormatMaterials *m){for(int i=0;i<m->count;i++){Material material=m->items[i].material;if(material.maps)for(int j=0;j<11;j++)if(material.maps[j].texture.id&&material.maps[j].texture.id!=mr.white)UnloadTexture(material.maps[j].texture);UnloadMaterial(material);}MemFree(m->items);memset(m,0,sizeof *m);}
static int mr_fmt_material_find(MRFormatMaterials *m,const char *name){for(int i=0;i<m->count;i++)if(TextIsEqual(m->items[i].name,name))return i;return -1;}
static int mr_fmt_material_add(MRFormatMaterials *m,const char *name){int existing=mr_fmt_material_find(m,name);if(existing>=0)return existing;
    if(!mr_fmt_grow((void**)&m->items,&m->capacity,m->count+1,sizeof *m->items))return -1;
    MRFormatMaterial *entry=&m->items[m->count];memset(entry,0,sizeof *entry);mr_fmt_copy(entry->name,sizeof entry->name,name);entry->material=LoadMaterialDefault();if(!entry->material.maps)return -1;return m->count++;
}
static char *mr_fmt_map_path(char *s){
    s=mr_fmt_space(s);
    while(*s=='-'){char *option=mr_fmt_token(&s);int values=1;
        if(TextIsEqual(option,"-o")||TextIsEqual(option,"-s")||TextIsEqual(option,"-t"))values=3;
        if(TextIsEqual(option,"-mm"))values=2;
        for(int i=0;i<values;i++){s=mr_fmt_space(s);if(values==3&& !(*s=='+'||*s=='-'||*s=='.'||(*s>='0'&&*s<='9')))break;if(!mr_fmt_token(&s))return s;}
        s=mr_fmt_space(s);
    }size_t n=TextLength(s);while(n&&(s[n-1]==' '||s[n-1]=='\t'))s[--n]=0;
    if(n>=2&&((s[0]=='"'&&s[n-1]=='"')||(s[0]=='\''&&s[n-1]=='\''))){s[n-1]=0;s++;}return s;
}
static bool mr_fmt_mtl(const char *fileName,MRFormatMaterials *materials) {
    char *text=LoadFileText(fileName);if(!text)return false;char directory[1024];mr_fmt_copy(directory,sizeof directory,GetDirectoryPath(fileName));
    char *cursor=text,*line;int current=-1;bool ok=true;
    while((line=mr_fmt_line(&cursor))){char *args=line,*key=mr_fmt_token(&args);if(!key)continue;
        if(TextIsEqual(key,"newmtl")){char *name=mr_fmt_trim(args);if(!*name){ok=false;break;}current=mr_fmt_material_add(materials,name);if(current<0){ok=false;break;}continue;}
        if(current<0)continue;MaterialMap *maps=materials->items[current].material.maps;
        int colorMap=TextIsEqual(key,"Kd")?MATERIAL_MAP_ALBEDO:TextIsEqual(key,"Ks")?MATERIAL_MAP_METALNESS:TextIsEqual(key,"Ke")?MATERIAL_MAP_EMISSION:TextIsEqual(key,"Ka")?MATERIAL_MAP_OCCLUSION:-1;
        if(colorMap>=0){char *end=args;float r=(float)mr_fmt_number(end,&end),g=(float)mr_fmt_number(end,&end),b=(float)mr_fmt_number(end,&end);unsigned char alpha=maps[colorMap].color.a;if(colorMap!=MATERIAL_MAP_ALBEDO&&!alpha)alpha=255;maps[colorMap].color=(Color){mr_byte(r*255),mr_byte(g*255),mr_byte(b*255),alpha};}
        else if(TextIsEqual(key,"d")||TextIsEqual(key,"Tr")){float alpha=(float)mr_fmt_number(args,NULL);if(TextIsEqual(key,"Tr"))alpha=1-alpha;maps[MATERIAL_MAP_ALBEDO].color.a=mr_byte(alpha*255);}
        else if(TextIsEqual(key,"Ns"))maps[MATERIAL_MAP_METALNESS].value=(float)mr_fmt_number(args,NULL);
        else if(TextIsEqual(key,"Pr"))maps[MATERIAL_MAP_ROUGHNESS].value=(float)mr_fmt_number(args,NULL);
        else if(TextIsEqual(key,"Pm"))maps[MATERIAL_MAP_METALNESS].value=(float)mr_fmt_number(args,NULL);
        else {int map=TextIsEqual(key,"map_Kd")?MATERIAL_MAP_ALBEDO:TextIsEqual(key,"map_Ks")||TextIsEqual(key,"map_Pm")?MATERIAL_MAP_METALNESS:TextIsEqual(key,"map_Ke")?MATERIAL_MAP_EMISSION:TextIsEqual(key,"map_Ka")?MATERIAL_MAP_OCCLUSION:TextIsEqual(key,"map_Ns")||TextIsEqual(key,"map_Pr")?MATERIAL_MAP_ROUGHNESS:TextIsEqual(key,"bump")||TextIsEqual(key,"map_Bump")||TextIsEqual(key,"map_bump")||TextIsEqual(key,"norm")?MATERIAL_MAP_NORMAL:-1;
            if(map>=0){char path[2048];mr_join_path(path,sizeof path,directory,mr_fmt_map_path(args));Texture2D texture=LoadTexture(path);if(IsTextureValid(texture)){if(maps[map].texture.id&&maps[map].texture.id!=mr.white)UnloadTexture(maps[map].texture);maps[map].texture=texture;}}}
    }UnloadFileText(text);return ok;
}
Material *LoadMaterials(const char *fileName,int *materialCount) {
    if(materialCount)*materialCount=0;if(!fileName||!IsFileExtension(fileName,".mtl"))return NULL;
    MRFormatMaterials named={0};if(!mr_fmt_mtl(fileName,&named)||!named.count){mr_fmt_materials_free(&named);return NULL;}
    Material *out=mr_fmt_alloc(named.count,sizeof(Material));if(!out){mr_fmt_materials_free(&named);return NULL;}
    for(int i=0;i<named.count;i++)out[i]=named.items[i].material;if(materialCount)*materialCount=named.count;MemFree(named.items);return out;
}
typedef struct MRFormatObjIndex {int p,uv,n;} MRFormatObjIndex;
static bool mr_fmt_obj_index(char *token,int positions,int texcoords,int normals,MRFormatObjIndex *out){
    out->p=out->uv=out->n=-1;char *s=token;int counts[3]={positions,texcoords,normals},*indices[3]={&out->p,&out->uv,&out->n};
    for(int i=0;i<3;i++){if(*s!='/'&&*s){int sign=1,index=0;if(*s=='-'){sign=-1;s++;}else if(*s=='+')s++;if(*s<'0'||*s>'9')return false;
        while(*s>='0'&&*s<='9'){if(index>MR_FMT_INT32_MAX/10-10)return false;index=index*10+*s++-'0';}if(!index)return false;index=sign>0?index-1:counts[i]-index;
        if(index<0||index>=counts[i])return false;*indices[i]=index;}
        if(*s=='/'&&i<2)s++;else if(*s)return false;else break;
    }return out->p>=0;
}
static float mr_fmt_cross2(Vector2 a,Vector2 b,Vector2 c){return(b.x-a.x)*(c.y-a.y)-(b.y-a.y)*(c.x-a.x);}
/* Ear clipping preserves concave OBJ polygons, unlike fan triangulation. */
static bool mr_fmt_polygon(MRFormatGeometry *g,MRFormatVertex *vertices,int count,int material){
    if(count<3)return false;if(count==3)return mr_fmt_triangle(g,vertices[0],vertices[1],vertices[2],material);
    Vector3 normal={0};for(int i=0;i<count;i++){Vector3 a=vertices[i].p,b=vertices[(i+1)%count].p;normal.x+=(a.y-b.y)*(a.z+b.z);normal.y+=(a.z-b.z)*(a.x+b.x);normal.z+=(a.x-b.x)*(a.y+b.y);}
    float ax=normal.x<0?-normal.x:normal.x,ay=normal.y<0?-normal.y:normal.y,az=normal.z<0?-normal.z:normal.z;int drop=ax>ay&&ax>az?0:ay>az?1:2;
    Vector2 *points=mr_fmt_alloc(count,sizeof(Vector2));int *remaining=mr_fmt_alloc(count,sizeof(int));if(!points||!remaining){MemFree(points);MemFree(remaining);return false;}
    float area=0;for(int i=0;i<count;i++){Vector3 p=vertices[i].p;points[i]=drop==0?(Vector2){p.y,p.z}:drop==1?(Vector2){p.x,p.z}:(Vector2){p.x,p.y};remaining[i]=i;}
    for(int i=0;i<count;i++){Vector2 a=points[i],b=points[(i+1)%count];area+=a.x*b.y-a.y*b.x;}float sign=area>=0?1:-1;bool ok=true;int left=count;
    while(left>3){bool clipped=false;for(int i=0;i<left;i++){int ia=remaining[(i+left-1)%left],ib=remaining[i],ic=remaining[(i+1)%left];float cross=mr_fmt_cross2(points[ia],points[ib],points[ic])*sign;if(cross<=.0000001f)continue;
            bool inside=false;for(int j=0;j<left;j++){int q=remaining[j];if(q==ia||q==ib||q==ic)continue;if(mr_fmt_cross2(points[ia],points[ib],points[q])*sign>=0&&mr_fmt_cross2(points[ib],points[ic],points[q])*sign>=0&&mr_fmt_cross2(points[ic],points[ia],points[q])*sign>=0){inside=true;break;}}
            if(inside)continue;if(!mr_fmt_triangle(g,vertices[ia],vertices[ib],vertices[ic],material)){ok=false;break;}for(int j=i;j+1<left;j++)remaining[j]=remaining[j+1];left--;clipped=true;break;
        }if(!ok||!clipped){ok=false;break;}}
    if(ok)ok=mr_fmt_triangle(g,vertices[remaining[0]],vertices[remaining[1]],vertices[remaining[2]],material);MemFree(points);MemFree(remaining);return ok;
}
static Model mr_load_obj(const char *fileName){
    char *text=LoadFileText(fileName);if(!text)return(Model){0};for(char *s=text;*s;s++)if(*s=='\\'&&(s[1]=='\n'||s[1]=='\r')){*s++=' ';*s=' ';if(s[1]=='\n')*++s=' ';}
    Vector3 *positions=NULL,*normals=NULL;Vector2 *uvs=NULL;Color *colors=NULL;int np=0,nn=0,nu=0,cp=0,cn=0,cu=0,cc=0;
    MRFormatGeometry geometry={0};MRFormatMaterials materials={0};MRFormatVertex *polygon=NULL;int polygonCap=0;bool ok=mr_fmt_material_add(&materials,"__default")>=0;int current=0;
    char directory[1024];mr_fmt_copy(directory,sizeof directory,GetDirectoryPath(fileName));char *cursor=text,*line;
    while(ok&&(line=mr_fmt_line(&cursor))){char *args=line,*key=mr_fmt_token(&args);if(!key)continue;
        if(TextIsEqual(key,"v")){if(!mr_fmt_grow((void**)&positions,&cp,np+1,sizeof(Vector3))||!mr_fmt_grow((void**)&colors,&cc,np+1,sizeof(Color))){ok=false;break;}
            char *end=args;float x=(float)mr_fmt_number(end,&end),y=(float)mr_fmt_number(end,&end),z=(float)mr_fmt_number(end,&end);positions[np]=(Vector3){x,y,z};colors[np]=WHITE;
            char *tokens[4]={0};int n=0;while(n<4&&(tokens[n]=mr_fmt_token(&end)))n++;if(n>=3)colors[np]=(Color){mr_byte((float)mr_fmt_number(tokens[0],NULL)*255),mr_byte((float)mr_fmt_number(tokens[1],NULL)*255),mr_byte((float)mr_fmt_number(tokens[2],NULL)*255),255};np++;
        }else if(TextIsEqual(key,"vn")){if(!mr_fmt_grow((void**)&normals,&cn,nn+1,sizeof(Vector3))){ok=false;break;}char *end=args;float x=(float)mr_fmt_number(end,&end),y=(float)mr_fmt_number(end,&end),z=(float)mr_fmt_number(end,&end);normals[nn++]=(Vector3){x,y,z};}
        else if(TextIsEqual(key,"vt")){if(!mr_fmt_grow((void**)&uvs,&cu,nu+1,sizeof(Vector2))){ok=false;break;}char *end=args;float u=(float)mr_fmt_number(end,&end),v=(float)mr_fmt_number(end,&end);uvs[nu++]=(Vector2){u,1-v};}
        else if(TextIsEqual(key,"mtllib")){char *name;while((name=mr_fmt_token(&args))){char path[2048];mr_join_path(path,sizeof path,directory,name);if(!mr_fmt_mtl(path,&materials))puts("raygpu: OBJ material library missing or invalid; using defaults");}}
        else if(TextIsEqual(key,"usemtl")){char *name=mr_fmt_trim(args);current=mr_fmt_material_find(&materials,name);if(current<0)current=0;}
        else if(TextIsEqual(key,"o")||TextIsEqual(key,"g")){if(geometry.group==MR_FMT_INT32_MAX){ok=false;break;}geometry.group++;}
        else if(TextIsEqual(key,"f")){int count=0;char *token;while((token=mr_fmt_token(&args))){MRFormatObjIndex index;if(!mr_fmt_obj_index(token,np,nu,nn,&index)||!mr_fmt_grow((void**)&polygon,&polygonCap,count+1,sizeof *polygon)){ok=false;break;}
                MRFormatVertex v={0};v.p=positions[index.p];v.color=colors[index.p];if(index.n>=0)v.n=normals[index.n];if(index.uv>=0)v.uv=uvs[index.uv];polygon[count++]=v;}
            if(ok)ok=mr_fmt_polygon(&geometry,polygon,count,current);}
    }Model model={0};if(ok&&geometry.count){Material *out=mr_fmt_alloc(materials.count,sizeof(Material));if(out){for(int i=0;i<materials.count;i++){out[i]=materials.items[i].material;materials.items[i].material.maps=NULL;}model=mr_fmt_model(&geometry,out,materials.count,0);}}
    mr_fmt_materials_free(&materials);MemFree(geometry.triangles);MemFree(polygon);MemFree(positions);MemFree(normals);MemFree(uvs);MemFree(colors);UnloadFileText(text);return model;
}
typedef struct MRIqmHeader {
    char magic[16];uint32_t version,filesize,flags,num_text,ofs_text,num_meshes,ofs_meshes;
    uint32_t num_vertexarrays,num_vertexes,ofs_vertexarrays,num_triangles,ofs_triangles,ofs_adjacency;
    uint32_t num_joints,ofs_joints,num_poses,ofs_poses,num_anims,ofs_anims,num_frames,num_framechannels,ofs_frames,ofs_bounds;
    uint32_t num_comment,ofs_comment,num_extensions,ofs_extensions;
} MRIqmHeader;
typedef struct MRIqmDoc {unsigned char *data;size_t size;MRIqmHeader h;} MRIqmDoc;
static bool mr_iqm_open(const char *fileName,MRIqmDoc *doc){
    memset(doc,0,sizeof *doc);int size=0;doc->data=LoadFileData(fileName,&size);doc->size=size;
    if(!doc->data||size<(int)sizeof(MRIqmHeader))goto fail;memcpy(&doc->h,doc->data,sizeof doc->h);
    MRIqmHeader *h=&doc->h;if(memcmp(h->magic,"INTERQUAKEMODEL\0",16)||h->version!=2||h->filesize>doc->size||h->filesize<sizeof *h)goto fail;doc->size=h->filesize;
    if(h->num_joints>256||!mr_fmt_range(doc->size,h->ofs_text,h->num_text,1)||!mr_fmt_range(doc->size,h->ofs_meshes,h->num_meshes,24)||!mr_fmt_range(doc->size,h->ofs_vertexarrays,h->num_vertexarrays,20)||!mr_fmt_range(doc->size,h->ofs_triangles,h->num_triangles,12)||!mr_fmt_range(doc->size,h->ofs_joints,h->num_joints,48))goto fail;
    return true;
fail:UnloadFileData(doc->data);memset(doc,0,sizeof *doc);return false;
}
static const char *mr_iqm_text(MRIqmDoc *doc,uint32_t index){
    if(index>=doc->h.num_text)return NULL;const char *s=(const char*)doc->data+doc->h.ofs_text+index;
    for(uint32_t i=index;i<doc->h.num_text;i++)if(!s[i-index])return s;return NULL;
}
static bool mr_iqm_bones(MRIqmDoc *doc,BoneInfo *bones,Transform *pose){
    for(uint32_t i=0;i<doc->h.num_joints;i++){const unsigned char *p=doc->data+doc->h.ofs_joints+i*48;int parent=(int32_t)mr_u32(p+4);const char *name=mr_iqm_text(doc,mr_u32(p));
        if(!name||parent< -1||parent>=(int)i)return false;mr_fmt_copy(bones[i].name,sizeof bones[i].name,name);bones[i].parent=parent;
        pose[i].translation=(Vector3){mr_fmt_float(p+8),mr_fmt_float(p+12),mr_fmt_float(p+16)};
        pose[i].rotation=mr_quaternion_normalize((Quaternion){mr_fmt_float(p+20),mr_fmt_float(p+24),mr_fmt_float(p+28),mr_fmt_float(p+32)});
        pose[i].scale=(Vector3){mr_fmt_float(p+36),mr_fmt_float(p+40),mr_fmt_float(p+44)};
    }mr_build_pose_world(bones,doc->h.num_joints,pose);return true;
}
static Model mr_load_iqm(const char *fileName){
    MRIqmDoc doc;if(!mr_iqm_open(fileName,&doc))return(Model){0};MRIqmHeader *h=&doc.h;
    MRFormatVertex *vertices=mr_fmt_alloc(h->num_vertexes,sizeof(MRFormatVertex));Material *materials=mr_fmt_alloc(h->num_meshes,sizeof(Material));MRFormatGeometry geometry={0};Model model={0};
    BoneInfo *bones=NULL;Transform *bind=NULL;bool ok=vertices&&materials&&h->num_meshes<=MR_FMT_INT32_MAX;bool positions=false;
    for(uint32_t i=0;vertices&&i<h->num_vertexes;i++)vertices[i].color=WHITE;
    for(uint32_t i=0;ok&&i<h->num_vertexarrays;i++){const unsigned char *array=doc.data+h->ofs_vertexarrays+i*20;uint32_t type=mr_u32(array),format=mr_u32(array+8),components=mr_u32(array+12),offset=mr_u32(array+16);
        if(type>6||type==3)continue;
        size_t unit=format==7?4:format==1?1:0;if(!unit||components>16||!components||!mr_fmt_range(doc.size,offset,h->num_vertexes,components*unit)){ok=false;break;}
        int required=type==0||type==2?3:type==1?2:type==4||type==5||type==6?4:0;
        if(!required)continue;if(components<(uint32_t)required||((type==0||type==1||type==2)&&format!=7)||((type==4||type==5||type==6)&&format!=1)){ok=false;break;}
        if(type==0)positions=true;
        for(uint32_t v=0;v<h->num_vertexes;v++){const unsigned char *p=doc.data+offset+v*components*unit;MRFormatVertex *out=&vertices[v];
            if(type==0)out->p=(Vector3){mr_fmt_float(p),mr_fmt_float(p+4),mr_fmt_float(p+8)};
            if(type==1)out->uv=(Vector2){mr_fmt_float(p),mr_fmt_float(p+4)};
            if(type==2)out->n=(Vector3){mr_fmt_float(p),mr_fmt_float(p+4),mr_fmt_float(p+8)};
            if(type==6)memcpy(&out->color,p,4);
            if(type==4){for(int j=0;j<4;j++){if(p[j]>=h->num_joints&&h->num_joints){ok=false;break;}out->ids[j]=p[j];}}
            if(type==5){for(int j=0;j<4;j++)out->weights[j]=p[j]/255.0f;}
        }
    }ok=ok&&positions;char directory[1024];mr_fmt_copy(directory,sizeof directory,GetDirectoryPath(fileName));
    for(uint32_t m=0;ok&&m<h->num_meshes;m++){const unsigned char *mesh=doc.data+h->ofs_meshes+m*24;uint32_t firstVertex=mr_u32(mesh+8),vertexCount=mr_u32(mesh+12),firstTriangle=mr_u32(mesh+16),triangles=mr_u32(mesh+20);
        if(firstVertex>h->num_vertexes||vertexCount>h->num_vertexes-firstVertex||firstTriangle>h->num_triangles||triangles>h->num_triangles-firstTriangle){ok=false;break;}
        materials[m]=LoadMaterialDefault();if(!materials[m].maps){ok=false;break;}const char *name=mr_iqm_text(&doc,mr_u32(mesh+4));if(name&&*name){char path[2048];mr_join_path(path,sizeof path,directory,name);Texture2D texture=LoadTexture(path);if(IsTextureValid(texture))materials[m].maps[MATERIAL_MAP_ALBEDO].texture=texture;}
        for(uint32_t t=0;t<triangles;t++){const unsigned char *p=doc.data+h->ofs_triangles+(firstTriangle+t)*12;uint32_t a=mr_u32(p),b=mr_u32(p+4),c=mr_u32(p+8);
            if(a<firstVertex||b<firstVertex||c<firstVertex||a-firstVertex>=vertexCount||b-firstVertex>=vertexCount||c-firstVertex>=vertexCount){ok=false;break;}
            if(!mr_fmt_triangle(&geometry,vertices[c],vertices[b],vertices[a],m)){ok=false;break;}}
    }
    if(ok&&h->num_joints){bones=mr_fmt_alloc(h->num_joints,sizeof(BoneInfo));bind=mr_fmt_alloc(h->num_joints,sizeof(Transform));ok=bones&&bind&&mr_iqm_bones(&doc,bones,bind);}
    if(ok){model=mr_fmt_model(&geometry,materials,h->num_meshes,h->num_joints);materials=NULL;if(IsModelValid(model)){model.bones=bones;model.bindPose=bind;model.boneCount=h->num_joints;bones=NULL;bind=NULL;}}
    if(materials){Model unused={0};unused.materials=materials;unused.materialCount=h->num_meshes;UnloadModel(unused);}MemFree(bones);MemFree(bind);MemFree(vertices);MemFree(geometry.triangles);UnloadFileData(doc.data);return model;
}
static ModelAnimation *mr_load_iqm_animations(const char *fileName,int *animCount){
    MRIqmDoc doc;if(!mr_iqm_open(fileName,&doc))return NULL;MRIqmHeader *h=&doc.h;ModelAnimation *animations=NULL;bool ok=false;
    if(!h->num_anims||!h->num_poses||h->num_poses>256||(h->num_joints&&h->num_poses!=h->num_joints)||h->num_anims>MR_FMT_INT32_MAX||!mr_fmt_range(doc.size,h->ofs_poses,h->num_poses,88)||!mr_fmt_range(doc.size,h->ofs_anims,h->num_anims,20)||h->num_framechannels>MR_FMT_UINT32_MAX/2)goto done;
    if(h->num_framechannels?!mr_fmt_range(doc.size,h->ofs_frames,h->num_frames,(size_t)h->num_framechannels*2):h->ofs_frames>doc.size)goto done;
    uint32_t channels=0;for(uint32_t b=0;b<h->num_poses;b++){const unsigned char *p=doc.data+h->ofs_poses+b*88;int parent=(int32_t)mr_u32(p);uint32_t mask=mr_u32(p+4);if(parent< -1||parent>=(int)b||mask>1023)goto done;for(int c=0;c<10;c++)if(mask&(1u<<c))channels++;}
    if(channels!=h->num_framechannels)goto done;
    animations=mr_fmt_alloc(h->num_anims,sizeof(ModelAnimation));if(!animations)goto done;
    for(uint32_t a=0;a<h->num_anims;a++){const unsigned char *p=doc.data+h->ofs_anims+a*20;uint32_t first=mr_u32(p+4),count=mr_u32(p+8);const char *name=mr_iqm_text(&doc,mr_u32(p));ModelAnimation *anim=&animations[a];
        if(!name||!count||count>MR_FMT_INT32_MAX||first>h->num_frames||count>h->num_frames-first)goto done;
        anim->boneCount=h->num_poses;anim->frameCount=count;mr_fmt_copy(anim->name,sizeof anim->name,name);anim->bones=mr_fmt_alloc(anim->boneCount,sizeof(BoneInfo));anim->framePoses=mr_fmt_alloc(count,sizeof(Transform*));
        if(!anim->bones||!anim->framePoses)goto done;
        for(uint32_t b=0;b<h->num_poses;b++){const char *boneName="ANIMJOINTNAME";if(h->num_joints){const unsigned char *joint=doc.data+h->ofs_joints+b*48;boneName=mr_iqm_text(&doc,mr_u32(joint));}if(!boneName)goto done;mr_fmt_copy(anim->bones[b].name,sizeof anim->bones[b].name,boneName);anim->bones[b].parent=(int32_t)mr_u32(doc.data+h->ofs_poses+b*88);}
        for(uint32_t frame=0;frame<count;frame++){Transform *pose=mr_fmt_alloc(h->num_poses,sizeof(Transform));anim->framePoses[frame]=pose;if(!pose)goto done;uint32_t channel=0;
            for(uint32_t b=0;b<h->num_poses;b++){const unsigned char *q=doc.data+h->ofs_poses+b*88;float value[10];uint32_t mask=mr_u32(q+4);
                for(int c=0;c<10;c++){value[c]=mr_fmt_float(q+8+c*4);if(mask&(1u<<c)){const unsigned char *sample=doc.data+h->ofs_frames+((size_t)(first+frame)*channels+channel++)*2;value[c]+=mr_u16(sample)*mr_fmt_float(q+48+c*4);}}
                pose[b].translation=(Vector3){value[0],value[1],value[2]};pose[b].rotation=mr_quaternion_normalize((Quaternion){value[3],value[4],value[5],value[6]});pose[b].scale=(Vector3){value[7],value[8],value[9]};}
            mr_build_pose_world(anim->bones,anim->boneCount,pose);
        }
    }ok=true;
done:UnloadFileData(doc.data);if(!ok){UnloadModelAnimations(animations,h->num_anims);return NULL;}if(animCount)*animCount=h->num_anims;return animations;
}
static Color mr_vox_palette(int index){
    if(!index)return(Color){0};if(index<=215){int i=index-1;return(Color){(unsigned char)(255-(i/36)*51),(unsigned char)(255-((i/6)%6)*51),(unsigned char)(255-(i%6)*51),255};}
    static const unsigned char shades[10]={238,221,187,170,136,119,85,68,34,17};int i=index-216;unsigned char v=shades[i%10];return i<10?(Color){v,0,0,255}:i<20?(Color){0,v,0,255}:i<30?(Color){0,0,v,255}:(Color){v,v,v,255};
}
static unsigned char mr_vox_at(const unsigned char *grid,int sx,int sy,int sz,int x,int y,int z){if(x<0||y<0||z<0||x>=sx||y>=sy||z>=sz)return 0;return grid[((size_t)z*sy+y)*sx+x];}
static Model mr_load_vox(const char *fileName){
    int size=0;unsigned char *data=LoadFileData(fileName,&size),*grid=NULL;int sx=0,sy=0,sz=0;Model model={0};MRFormatGeometry geometry={0};Color palette[256];bool ok=false;
    for(int i=0;i<256;i++)palette[i]=mr_vox_palette(i);
    if(!data||size<20||memcmp(data,"VOX ",4)||(mr_u32(data+4)!=150&&mr_u32(data+4)!=200)||memcmp(data+8,"MAIN",4))goto done;
    size_t end=20;if(mr_u32(data+12)!=0||mr_u32(data+16)>(uint32_t)size-20)goto done;end+=mr_u32(data+16);
    for(size_t offset=20;offset<end;){if(end-offset<12)goto done;const unsigned char *chunk=data+offset;uint32_t length=mr_u32(chunk+4),children=mr_u32(chunk+8);offset+=12;
        if(length>end-offset||children>end-offset-length)goto done;const unsigned char *p=data+offset;
        if(!memcmp(chunk,"SIZE",4)){if(length<12)goto done;uint32_t x=mr_u32(p),y=mr_u32(p+4),z=mr_u32(p+8);if(!x||!y||!z||x>256||y>256||z>256)goto done;MemFree(grid);sx=x;sy=z;sz=y;grid=mr_fmt_alloc((size_t)sx*sy*sz,1);if(!grid)goto done;}
        else if(!memcmp(chunk,"XYZI",4)){if(!grid||length<4||mr_u32(p)>(length-4)/4)goto done;uint32_t count=mr_u32(p);for(uint32_t i=0;i<count;i++){const unsigned char *v=p+4+i*4;if(v[0]>=sx||v[2]>=sy||v[1]>=sz)goto done;grid[((size_t)(sz-v[1]-1)*sy+v[2])*sx+v[0]]=v[3];}}
        else if(!memcmp(chunk,"RGBA",4)){if(length<1024)goto done;for(int i=1;i<256;i++)memcpy(&palette[i],p+(i-1)*4,4);}
        offset+=length; /* Children are parsed in the same bounded traversal; scene nodes are ignored like raylib. */
    }if(!grid)goto done;
    static const int neighbor[6][3]={{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}};
    static const unsigned char corners[6][4][3]={{{1,0,0},{1,1,0},{1,1,1},{1,0,1}},{{0,0,1},{0,1,1},{0,1,0},{0,0,0}},{{0,1,1},{1,1,1},{1,1,0},{0,1,0}},{{0,0,0},{1,0,0},{1,0,1},{0,0,1}},{{1,0,1},{1,1,1},{0,1,1},{0,0,1}},{{0,0,0},{0,1,0},{1,1,0},{1,0,0}}};
    for(int z=0;z<sz;z++)for(int y=0;y<sy;y++)for(int x=0;x<sx;x++){unsigned char index=mr_vox_at(grid,sx,sy,sz,x,y,z);if(!index)continue;
        for(int f=0;f<6;f++){if(mr_vox_at(grid,sx,sy,sz,x+neighbor[f][0],y+neighbor[f][1],z+neighbor[f][2]))continue;MRFormatVertex v[4]={0};for(int j=0;j<4;j++){v[j].p=(Vector3){(x+corners[f][j][0])*.25f,(y+corners[f][j][1])*.25f,(z+corners[f][j][2])*.25f};v[j].n=(Vector3){neighbor[f][0],neighbor[f][1],neighbor[f][2]};v[j].color=palette[index];}
            if(!mr_fmt_triangle(&geometry,v[0],v[1],v[2],0)||!mr_fmt_triangle(&geometry,v[0],v[2],v[3],0))goto done;}}
    if(geometry.count){Material *materials=mr_fmt_alloc(1,sizeof(Material));if(!materials)goto done;materials[0]=LoadMaterialDefault();model=mr_fmt_model(&geometry,materials,1,0);ok=true;}
done:MemFree(grid);MemFree(geometry.triangles);UnloadFileData(data);if(!ok)UnloadModel(model);return ok?model:(Model){0};
}
/* Model3D SDK integration. Keep all allocation and file access in RayGPU. */
static size_t mr_fmt_strlen(const char *s){size_t n=0;while(s[n])n++;return n;}
static int mr_fmt_strcmp(const char *a,const char *b){while(*a&&*a==*b){a++;b++;}return(unsigned char)*a-(unsigned char)*b;}
static char *mr_fmt_strrchr(const char *s,int c){const char *last=NULL;do{if(*s==c)last=s;}while(*s++);return(char*)last;}
static int mr_fmt_atoi(const char *s){return(int)mr_fmt_number(s,NULL);}
#if defined(_WIN32)
#include <locale.h>
#endif
static char *mr_fmt_locale(int category,const char *locale){(void)category;(void)locale;return(char*)"C";}
#define M3D_MALLOC(n) mr_fmt_alloc((n),1)
#define M3D_REALLOC(p,n) MemRealloc((p),(unsigned int)(n))
#define M3D_FREE(p) MemFree(p)
#define M3D_ASCII
#define M3D_IMPLEMENTATION
/* The SDK uses stb_image's internal PNG/zlib helpers, already compiled above. */
#define STB_IMAGE_IMPLEMENTATION
#define strlen mr_fmt_strlen
#define strcmp mr_fmt_strcmp
#define strrchr mr_fmt_strrchr
#define strtod mr_fmt_number
#define atoi mr_fmt_atoi
#define free MemFree
#define setlocale mr_fmt_locale
#if defined(__wasm__)
#define LC_NUMERIC 1
#endif
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-function"
#pragma clang diagnostic ignored "-Wsign-compare"
#pragma clang diagnostic ignored "-Wmisleading-indentation"
#pragma clang diagnostic ignored "-Wparentheses"
#endif
#include "external/m3d.h"
#ifdef __clang__
#pragma clang diagnostic pop
#endif
#undef strlen
#undef strcmp
#undef strrchr
#undef strtod
#undef atoi
#undef free
#undef setlocale
#if defined(__wasm__)
#undef LC_NUMERIC
#endif
#undef STB_IMAGE_IMPLEMENTATION
#undef M3D_IMPLEMENTATION
#undef M3D_MALLOC
#undef M3D_REALLOC
#undef M3D_FREE
static char mr_m3d_directory[1024];
static unsigned char *mr_m3d_read(char *name,unsigned int *size){char path[2048];mr_join_path(path,sizeof path,mr_m3d_directory,name);int length=0;unsigned char *data=LoadFileData(path,&length);if(size)*size=length;return data;}
static unsigned char *mr_m3d_binary(const unsigned char *file,int size){
    const unsigned char *payload=file+8;int length=size-8;unsigned char *decoded=NULL,*result=NULL;
    if(length>=4&&!memcmp(payload,"PRVW",4)){if(length<8)return NULL;uint32_t preview=mr_u32(payload+4);if(preview<8||preview>(uint32_t)length)return NULL;payload+=preview;length-=preview;}
    if(length<4)return NULL;
    if(memcmp(payload,"HEAD",4)){int output=0;decoded=(unsigned char*)stbi_zlib_decode_malloc_guesssize_headerflag((const char*)payload,length,4096,&output,1);if(!decoded)return NULL;payload=decoded;length=output;}
    if(length<(int)sizeof(m3dhdr_t)+8||memcmp(payload,"HEAD",4)||memcmp(payload+length-4,"OMD3",4))goto done;
    uint32_t header=mr_u32(payload+4);if(header<sizeof(m3dhdr_t)+4||header>(uint32_t)length-4)goto done;
    /* Four zero-terminated header strings must stay inside HEAD. */
    size_t string=sizeof(m3dhdr_t);for(int i=0;i<4;i++){while(string<header&&payload[string])string++;if(string==header)goto done;string++;}
    for(size_t offset=header;offset<(size_t)length-4;){if((size_t)length-4-offset<8)goto done;uint32_t chunk=mr_u32(payload+offset+4);if(chunk<8||chunk>(size_t)length-4-offset)goto done;offset+=chunk;}
    if(length>MR_FMT_INT32_MAX-8)goto done;result=mr_fmt_alloc((size_t)length+8,1);if(!result)goto done;
    memcpy(result,"3DMO",4);uint32_t total=length+8;memcpy(result+4,&total,4);memcpy(result+8,payload,length);
done:MemFree(decoded);return result;
}
static m3d_t *mr_m3d_open(const char *fileName,unsigned char **source){
    *source=NULL;int size=0;unsigned char *file=LoadFileData(fileName,&size);if(!file||size<8){UnloadFileData(file);return NULL;}
    bool binary=memcmp(file,"3DMO",4)==0,ascii=memcmp(file,"3dmo",4)==0;
    if((!binary&&!ascii)||(binary&&(mr_u32(file+4)!=(uint32_t)size||size<16))){UnloadFileData(file);return NULL;}
    unsigned char *data=binary?mr_m3d_binary(file,size):mr_fmt_alloc((size_t)size+16,1);if(!data){UnloadFileData(file);return NULL;}
    if(ascii){memcpy(data,file,size);data[size]='\n';data[size+1]='\n';}UnloadFileData(file);
    mr_fmt_copy(mr_m3d_directory,sizeof mr_m3d_directory,GetDirectoryPath(fileName));m3d_t *doc=m3d_load(data,mr_m3d_read,MemFree,NULL);
    if(!doc||M3D_ERR_ISFATAL(doc->errcode)){if(doc)m3d_free(doc);MemFree(data);return NULL;}*source=data;return doc;
}
static Transform mr_m3d_transform(m3d_t *doc,m3db_t bone){
    Transform pose={{0},{0,0,0,1},{1,1,1}};
    /* m3d_pose() appends interpolation vertices without changing numvertex. */
    uint32_t limit=doc->numvertex+2*doc->numbone;
    if(bone.pos<limit){m3dv_t p=doc->vertex[bone.pos];pose.translation=(Vector3){p.x*doc->scale,p.y*doc->scale,p.z*doc->scale};}
    if(bone.ori<limit){m3dv_t q=doc->vertex[bone.ori];pose.rotation=mr_quaternion_normalize((Quaternion){q.x,q.y,q.z,q.w});}return pose;
}
static bool mr_m3d_bones(m3d_t *doc,BoneInfo *bones,Transform *bind){
    for(uint32_t i=0;i<doc->numbone;i++){int parent=doc->bone[i].parent==M3D_UNDEF?-1:(int)doc->bone[i].parent;if(parent< -1||parent>=(int)i||doc->bone[i].pos>=doc->numvertex||doc->bone[i].ori>=doc->numvertex)return false;
        bones[i].parent=parent;mr_fmt_copy(bones[i].name,sizeof bones[i].name,doc->bone[i].name);bind[i]=mr_m3d_transform(doc,doc->bone[i]);}
    bones[doc->numbone].parent=-1;mr_fmt_copy(bones[doc->numbone].name,sizeof bones[doc->numbone].name,"NO BONE");bind[doc->numbone]=(Transform){{0},{0,0,0,1},{1,1,1}};
    mr_build_pose_world(bones,doc->numbone+1,bind);return true;
}
static Color mr_m3d_color(uint32_t rgba){return(Color){rgba&255,(rgba>>8)&255,(rgba>>16)&255,(rgba>>24)&255};}
static void mr_m3d_material(m3d_t *doc,m3dm_t *source,Material *out){
    for(uint32_t i=0;i<source->numprop;i++){m3dp_t p=source->prop[i];int map=p.type==m3dp_Kd||p.type==m3dp_map_Kd?MATERIAL_MAP_ALBEDO:p.type==m3dp_Ks||p.type==m3dp_map_Ks||p.type==m3dp_Pm||p.type==m3dp_map_Pm?MATERIAL_MAP_METALNESS:p.type==m3dp_Ke||p.type==m3dp_map_Ke?MATERIAL_MAP_EMISSION:p.type==m3dp_Ka||p.type==m3dp_map_Ka?MATERIAL_MAP_OCCLUSION:p.type==m3dp_Pr||p.type==m3dp_map_Pr||p.type==m3dp_Ns||p.type==m3dp_map_Ns?MATERIAL_MAP_ROUGHNESS:p.type==m3dp_map_N||p.type==m3dp_map_Km?MATERIAL_MAP_NORMAL:-1;
        if(p.type==m3dp_d){out->maps[MATERIAL_MAP_ALBEDO].color.a=mr_byte(p.value.fnum*255);continue;}if(map<0)continue;
        if(p.type>=128&&p.value.textureid<doc->numtexture){m3dtx_t *t=&doc->texture[p.value.textureid];if(!t->d||!t->w||!t->h||t->f<1||t->f>4)continue;Image image=GenImageColor(t->w,t->h,WHITE);if(!image.data)continue;Color *pixels=image.data;
            for(unsigned int v=0;v<t->w*t->h;v++){unsigned char *q=t->d+v*t->f;pixels[v]=t->f==1?(Color){q[0],q[0],q[0],255}:t->f==2?(Color){q[0],q[0],q[0],q[1]}:t->f==3?(Color){q[0],q[1],q[2],255}:(Color){q[0],q[1],q[2],q[3]};}
            Texture2D texture=LoadTextureFromImage(image);UnloadImage(image);if(IsTextureValid(texture))out->maps[map].texture=texture;
        }else if(p.type==m3dp_Kd||p.type==m3dp_Ks||p.type==m3dp_Ke||p.type==m3dp_Ka)out->maps[map].color=mr_m3d_color(p.value.color);
        else out->maps[map].value=p.value.fnum;
    }
}
static Model mr_load_m3d(const char *fileName){
    unsigned char *data=NULL;m3d_t *doc=mr_m3d_open(fileName,&data);if(!doc)return(Model){0};Model model={0};MRFormatGeometry geometry={0};Material *materials=NULL;BoneInfo *bones=NULL;Transform *bind=NULL;
    int boneCount=doc->numbone&&doc->numskin?(int)doc->numbone+1:0;
    if(!doc->numface||doc->nummaterial>=MR_FMT_INT32_MAX||doc->numbone>255||boneCount>256||doc->numface>MR_FMT_INT32_MAX)goto done;
    int materialCount=doc->nummaterial+1;materials=mr_fmt_alloc(materialCount,sizeof(Material));if(!materials)goto done;
    for(int i=0;i<materialCount;i++){materials[i]=LoadMaterialDefault();if(!materials[i].maps)goto done;if(i)mr_m3d_material(doc,&doc->material[i-1],&materials[i]);}
    for(uint32_t f=0;f<doc->numface;f++){m3df_t face=doc->face[f];int material=face.materialid==M3D_UNDEF?0:(int)face.materialid+1;if(material<0||material>=materialCount)goto done;MRFormatVertex vertices[3]={0};
        for(int j=0;j<3;j++){if(face.vertex[j]>=doc->numvertex)goto done;m3dv_t p=doc->vertex[face.vertex[j]];MRFormatVertex *v=&vertices[j];v->p=(Vector3){p.x*doc->scale,p.y*doc->scale,p.z*doc->scale};v->color=p.color?mr_m3d_color(p.color):WHITE;
            if(face.normal[j]!=M3D_UNDEF){if(face.normal[j]>=doc->numvertex)goto done;m3dv_t n=doc->vertex[face.normal[j]];v->n=(Vector3){n.x,n.y,n.z};}
            if(face.texcoord[j]!=M3D_UNDEF){if(face.texcoord[j]>=doc->numtmap)goto done;v->uv=(Vector2){doc->tmap[face.texcoord[j]].u,doc->tmap[face.texcoord[j]].v};}
            if(boneCount){if(p.skinid==M3D_UNDEF){v->ids[0]=doc->numbone;v->weights[0]=1;}else{if(p.skinid>=doc->numskin)goto done;m3ds_t skin=doc->skin[p.skinid];float total=0;for(int k=0;k<4;k++){if(skin.boneid[k]==M3D_UNDEF)continue;if(skin.boneid[k]>=doc->numbone)goto done;v->ids[k]=skin.boneid[k];v->weights[k]=skin.weight[k];total+=v->weights[k];}if(total>0)for(int k=0;k<4;k++)v->weights[k]/=total;else{v->ids[0]=doc->numbone;v->weights[0]=1;}}}}
        if(!mr_fmt_triangle(&geometry,vertices[0],vertices[1],vertices[2],material))goto done;
    }
    if(boneCount){bones=mr_fmt_alloc(boneCount,sizeof(BoneInfo));bind=mr_fmt_alloc(boneCount,sizeof(Transform));if(!bones||!bind||!mr_m3d_bones(doc,bones,bind))goto done;}
    model=mr_fmt_model(&geometry,materials,materialCount,boneCount);materials=NULL;if(IsModelValid(model)){model.bones=bones;model.bindPose=bind;model.boneCount=boneCount;bones=NULL;bind=NULL;}
done:if(materials){Model unused={0};unused.materials=materials;unused.materialCount=doc->nummaterial+1;UnloadModel(unused);}MemFree(bones);MemFree(bind);MemFree(geometry.triangles);m3d_free(doc);MemFree(data);return model;
}
static ModelAnimation *mr_load_m3d_animations(const char *fileName,int *animCount){
    unsigned char *data=NULL;m3d_t *doc=mr_m3d_open(fileName,&data);if(!doc)return NULL;ModelAnimation *animations=NULL;bool ok=false;
    if(!doc->numaction||!doc->numbone||!doc->numskin||doc->numbone>=256||doc->numaction>MR_FMT_INT32_MAX)goto done;
    animations=mr_fmt_alloc(doc->numaction,sizeof(ModelAnimation));if(!animations)goto done;
    for(uint32_t a=0;a<doc->numaction;a++){ModelAnimation *anim=&animations[a];uint32_t count=doc->action[a].durationmsec/17;if(!count)count=1;if(count>MR_FMT_INT32_MAX)goto done;
        if(!doc->action[a].durationmsec||!doc->action[a].numframe)goto done;
        for(uint32_t f=0;f<doc->action[a].numframe;f++){m3dfr_t frame=doc->action[a].frame[f];for(uint32_t t=0;t<frame.numtransform;t++){m3dtr_t tr=frame.transform[t];if(tr.boneid>=doc->numbone||tr.pos>=doc->numvertex||tr.ori>=doc->numvertex)goto done;}}
        anim->boneCount=doc->numbone+1;anim->frameCount=count;mr_fmt_copy(anim->name,sizeof anim->name,doc->action[a].name);anim->bones=mr_fmt_alloc(anim->boneCount,sizeof(BoneInfo));anim->framePoses=mr_fmt_alloc(count,sizeof(Transform*));Transform *bind=mr_fmt_alloc(anim->boneCount,sizeof(Transform));
        if(!anim->bones||!anim->framePoses||!bind){MemFree(bind);goto done;}bool valid=mr_m3d_bones(doc,anim->bones,bind);MemFree(bind);if(!valid)goto done;
        for(uint32_t f=0;f<count;f++){Transform *transforms=mr_fmt_alloc(anim->boneCount,sizeof(Transform));anim->framePoses[f]=transforms;if(!transforms)goto done;m3db_t *pose=m3d_pose(doc,a,f*17);if(!pose)goto done;
            for(uint32_t b=0;b<doc->numbone;b++){uint32_t limit=doc->numvertex+2*doc->numbone;if(pose[b].pos>=limit||pose[b].ori>=limit){MemFree(pose);goto done;}transforms[b]=mr_m3d_transform(doc,pose[b]);}
            transforms[doc->numbone]=(Transform){{0},{0,0,0,1},{1,1,1}};MemFree(pose);mr_build_pose_world(anim->bones,anim->boneCount,transforms);
        }
    }ok=true;
done:if(!ok){UnloadModelAnimations(animations,doc->numaction);animations=NULL;}else if(animCount)*animCount=doc->numaction;m3d_free(doc);MemFree(data);return animations;
}
