/* SarGPU models module. Compiled through sargpu.c; do not compile separately. */
static Vector3 mr_v3_add(Vector3 a,Vector3 b){return(Vector3){a.x+b.x,a.y+b.y,a.z+b.z};}
static Vector3 mr_v3_sub(Vector3 a,Vector3 b){return(Vector3){a.x-b.x,a.y-b.y,a.z-b.z};}
static Vector3 mr_v3_scale(Vector3 a,float s){return(Vector3){a.x*s,a.y*s,a.z*s};}
static float mr_v3_dot(Vector3 a,Vector3 b){return a.x*b.x+a.y*b.y+a.z*b.z;}
static Vector3 mr_v3_cross(Vector3 a,Vector3 b){return(Vector3){a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
static float mr_v3_len(Vector3 a){return sqrtf(mr_v3_dot(a,a));}
static Vector3 mr_v3_norm(Vector3 a){float n=mr_v3_len(a);return n>0?mr_v3_scale(a,1.0f/n):(Vector3){0};}
static void mr_camera_basis(Camera3D camera,Vector3 *right,Vector3 *up,Vector3 *forward){
    *forward=mr_v3_norm(mr_v3_sub(camera.target,camera.position));
    *right=mr_v3_norm(mr_v3_cross(*forward,camera.up));
    if(mr_v3_len(*right)<0.0001f)*right=(Vector3){1,0,0};
    *up=mr_v3_norm(mr_v3_cross(*right,*forward));
}
typedef struct MRProjected3D { float x,y,z,viewZ; bool visible; } MRProjected3D;
static MRProjected3D mr_project3d_ex(Vector3 point,Camera3D camera,int width,int height){
    Vector3 right,up,forward;mr_camera_basis(camera,&right,&up,&forward);
    Vector3 relative=mr_v3_sub(point,camera.position);
    float x=mr_v3_dot(relative,right),y=mr_v3_dot(relative,up),z=mr_v3_dot(relative,forward);
    float aspect=height>0?(float)width/(float)height:1.0f,n=0.01f,f=1000.0f,nx=0,ny=0,depth=1;
    if(camera.projection==CAMERA_ORTHOGRAPHIC){float vertical=camera.fovy>0?camera.fovy:1;nx=2*x/(vertical*aspect);ny=2*y/vertical;depth=(z-n)/(f-n);}
    else{float tangent=sinf(camera.fovy*MR_DEG2RAD*0.5f)/cosf(camera.fovy*MR_DEG2RAD*0.5f);if(tangent<=0)tangent=0.0001f;nx=x/(z*tangent*aspect);ny=y/(z*tangent);depth=f/(f-n)-(n*f)/((f-n)*z);}
    return(MRProjected3D){nx,ny,depth,z,z>n};
}
Vector2 GetWorldToScreenEx(Vector3 point,Camera camera,int width,int height){MRProjected3D p=mr_project3d_ex(point,camera,width,height);return(Vector2){(p.x+1)*0.5f*width,(1-p.y)*0.5f*height};}
Vector2 GetWorldToScreen(Vector3 point,Camera camera){return GetWorldToScreenEx(point,camera,mr.width,mr.height);}
Matrix GetCameraMatrix(Camera camera){
    Vector3 r,u,f;mr_camera_basis(camera,&r,&u,&f);Matrix m={0};
    m.m0=r.x;m.m4=r.y;m.m8=r.z;m.m12=-mr_v3_dot(r,camera.position);
    m.m1=u.x;m.m5=u.y;m.m9=u.z;m.m13=-mr_v3_dot(u,camera.position);
    m.m2=-f.x;m.m6=-f.y;m.m10=-f.z;m.m14=mr_v3_dot(f,camera.position);m.m15=1;return m;
}
Ray GetScreenToWorldRayEx(Vector2 position,Camera camera,int width,int height){
    Vector3 r,u,f;mr_camera_basis(camera,&r,&u,&f);float nx=2*position.x/width-1,ny=1-2*position.y/height;
    if(camera.projection==CAMERA_ORTHOGRAPHIC){float vertical=camera.fovy>0?camera.fovy:1;Vector3 origin=mr_v3_add(camera.position,mr_v3_add(mr_v3_scale(r,nx*vertical*width/height*0.5f),mr_v3_scale(u,ny*vertical*0.5f)));return(Ray){origin,f};}
    float tangent=sinf(camera.fovy*MR_DEG2RAD*0.5f)/cosf(camera.fovy*MR_DEG2RAD*0.5f);
    Vector3 direction=mr_v3_norm(mr_v3_add(f,mr_v3_add(mr_v3_scale(r,nx*tangent*width/height),mr_v3_scale(u,ny*tangent))));return(Ray){camera.position,direction};
}
Ray GetScreenToWorldRay(Vector2 position,Camera camera){return GetScreenToWorldRayEx(position,camera,mr.width,mr.height);}
void BeginMode3D(Camera3D camera){mr.camera3d=camera;mr.camera3dActive=true;mr.camera2dActive=false;}
void EndMode3D(void){mr.camera3dActive=false;}
Light3D CreateLight3D(int type,Vector3 position,Vector3 target,Color color,float intensity,float range){
    if(type<LIGHT_DIRECTIONAL||type>LIGHT_SPOT)type=LIGHT_POINT;if(intensity<0)intensity=0;if(range<0)range=0;
    return(Light3D){true,type,position,target,color,intensity,range,0.9f,0.8f};
}
void SetLight3D(int index,Light3D light){if(index<0||index>=SARGPU_MAX_LIGHTS)return;if(light.type<LIGHT_DIRECTIONAL||light.type>LIGHT_SPOT)light.type=LIGHT_POINT;if(light.intensity<0)light.intensity=0;if(light.range<0)light.range=0;mr.lights[index]=light;}
Light3D GetLight3D(int index){return index>=0&&index<SARGPU_MAX_LIGHTS?mr.lights[index]:(Light3D){0};}
void SetAmbientLight(Color color,float intensity){mr.ambientColor=color;mr.ambientIntensity=intensity<0?0:intensity;}
void SetFog(int mode,Color color,float start,float end,float density){if(mode<FOG_DISABLED||mode>FOG_EXPONENTIAL_SQUARED)mode=FOG_DISABLED;if(start<0)start=0;if(end<=start)end=start+0.001f;if(density<0)density=0;mr.fogMode=mode;mr.fogColor=color;mr.fogStart=start;mr.fogEnd=end;mr.fogDensity=density;}
void DisableFog(void){mr.fogMode=FOG_DISABLED;}
void SetPBRMode(bool enabled){mr.pbrEnabled=enabled;}
bool IsPBRModeEnabled(void){return mr.pbrEnabled;}
void DrawSkybox(Texture2D panorama,Color tint){if(mr.drawing&&mr.camera3dActive&&IsTextureValid(panorama)){mr.skyboxTexture=panorama.id;mr.skyboxTint=tint;}}
static bool mr_batch_room(unsigned int texture){
    if(!mr.drawing||mr.vertexCount+3>MR_MAX_VERTICES)return false;
    unsigned int sx=mr.scissorActive?(unsigned int)(mr.scissor.x<0?0:mr.scissor.x):0,sy=mr.scissorActive?(unsigned int)(mr.scissor.y<0?0:mr.scissor.y):0;
    unsigned int sw=mr.scissorActive?(unsigned int)(mr.scissor.width<0?0:mr.scissor.width):(unsigned int)mr.targetWidth,sh=mr.scissorActive?(unsigned int)(mr.scissor.height<0?0:mr.scissor.height):(unsigned int)mr.targetHeight;
    MRBatch *last=mr.batchCount?&mr.batches[mr.batchCount-1]:NULL;
    if(!last||last->texture!=texture||last->blend!=(unsigned int)mr.blendMode||last->shader!=mr.currentShader||last->x!=sx||last->y!=sy||last->width!=sw||last->height!=sh)mr.batches[mr.batchCount++]=(MRBatch){mr.vertexCount,0,texture,(unsigned int)mr.blendMode,mr.currentShader,sx,sy,sw,sh};
    mr.batches[mr.batchCount-1].count+=3;return true;
}
static void mr_projected_triangle_uv(MRProjected3D a,MRProjected3D b,MRProjected3D c,
    Vector2 uvA,Vector2 uvB,Vector2 uvC,Color colorA,Color colorB,Color colorC,unsigned int texture){
    if(!a.visible||!b.visible||!c.visible||!mr_batch_room(texture))return;
    MRProjected3D p[3]={a,b,c};Vector2 uv[3]={uvA,uvB,uvC};Color colors[3]={colorA,colorB,colorC};
    for(int i=0;i<3;i++)mr.vertices[mr.vertexCount++]=(MRVertex){p[i].x,p[i].y,uv[i].x,uv[i].y,p[i].z,colors[i].r,colors[i].g,colors[i].b,colors[i].a};
}
static void mr_projected_triangle(MRProjected3D a,MRProjected3D b,MRProjected3D c,Color color){
    float u=(mr.shapesSource.x+mr.shapesSource.width*0.5f)/mr.shapesTexture.width,v=(mr.shapesSource.y+mr.shapesSource.height*0.5f)/mr.shapesTexture.height;Vector2 uv={u,v};
    mr_projected_triangle_uv(a,b,c,uv,uv,uv,color,color,color,mr.shapesTexture.id);
}
static void mr_triangle3d(Vector3 a,Vector3 b,Vector3 c,Color color){if(!mr.camera3dActive)return;mr_projected_triangle(mr_project3d_ex(a,mr.camera3d,mr.targetWidth,mr.targetHeight),mr_project3d_ex(b,mr.camera3d,mr.targetWidth,mr.targetHeight),mr_project3d_ex(c,mr.camera3d,mr.targetWidth,mr.targetHeight),color);}
void DrawTriangle3D(Vector3 a,Vector3 b,Vector3 c,Color color){mr_triangle3d(a,b,c,color);}
void DrawTriangleStrip3D(const Vector3 *points,int count,Color color){if(!points)return;for(int i=2;i<count;i++)if(i&1)mr_triangle3d(points[i-1],points[i-2],points[i],color);else mr_triangle3d(points[i-2],points[i-1],points[i],color);}
void DrawLine3D(Vector3 a,Vector3 b,Color color){
    if(!mr.camera3dActive)return;MRProjected3D p=mr_project3d_ex(a,mr.camera3d,mr.targetWidth,mr.targetHeight),q=mr_project3d_ex(b,mr.camera3d,mr.targetWidth,mr.targetHeight);if(!p.visible||!q.visible)return;
    float dx=(q.x-p.x)*mr.targetWidth*0.5f,dy=(q.y-p.y)*mr.targetHeight*0.5f,n=sqrtf(dx*dx+dy*dy);if(n<=0)return;
    float ox=-dy/n/mr.targetWidth*2,oy=dx/n/mr.targetHeight*2;
    /* Keep endpoint depth instead of flattening the whole line to its average.
     * Perspective grid lines can span a large depth range; averaging made the
     * near half incorrectly cover solid geometry. */
    MRProjected3D a1={p.x+ox,p.y+oy,p.z,p.viewZ,true},a2={q.x+ox,q.y+oy,q.z,q.viewZ,true};
    MRProjected3D b1={q.x-ox,q.y-oy,q.z,q.viewZ,true},b2={p.x-ox,p.y-oy,p.z,p.viewZ,true};
    mr_projected_triangle(a1,a2,b1,color);mr_projected_triangle(a1,b1,b2,color);
}
void DrawPoint3D(Vector3 p,Color color){Vector3 r,u,f;mr_camera_basis(mr.camera3d,&r,&u,&f);DrawLine3D(mr_v3_sub(p,mr_v3_scale(u,0.025f)),mr_v3_add(p,mr_v3_scale(u,0.025f)),color);}
static Vector3 mr_rotate_axis(Vector3 v,Vector3 axis,float angle){axis=mr_v3_norm(axis);float c=cosf(angle),s=sinf(angle);return mr_v3_add(mr_v3_add(mr_v3_scale(v,c),mr_v3_scale(mr_v3_cross(axis,v),s)),mr_v3_scale(axis,mr_v3_dot(axis,v)*(1-c)));}
void DrawCircle3D(Vector3 center,float radius,Vector3 axis,float angle,Color color){Vector3 previous=mr_v3_add(center,mr_rotate_axis((Vector3){radius,0,0},axis,angle*MR_DEG2RAD));for(int i=1;i<=36;i++){float a=2*MR_PI*i/36;Vector3 local={cosf(a)*radius,sinf(a)*radius,0};Vector3 point=mr_v3_add(center,mr_rotate_axis(local,axis,angle*MR_DEG2RAD));DrawLine3D(previous,point,color);previous=point;}}
void DrawCubeV(Vector3 p,Vector3 s,Color color){
    float x=s.x/2,y=s.y/2,z=s.z/2;Vector3 v[8]={{p.x-x,p.y-y,p.z-z},{p.x+x,p.y-y,p.z-z},{p.x+x,p.y+y,p.z-z},{p.x-x,p.y+y,p.z-z},{p.x-x,p.y-y,p.z+z},{p.x+x,p.y-y,p.z+z},{p.x+x,p.y+y,p.z+z},{p.x-x,p.y+y,p.z+z}};
    int f[12][3]={{0,2,1},{0,3,2},{4,5,6},{4,6,7},{0,1,5},{0,5,4},{3,7,6},{3,6,2},{0,4,7},{0,7,3},{1,2,6},{1,6,5}};for(int i=0;i<12;i++)mr_triangle3d(v[f[i][0]],v[f[i][1]],v[f[i][2]],color);
}
void DrawCube(Vector3 p,float w,float h,float l,Color c){DrawCubeV(p,(Vector3){w,h,l},c);}
void DrawCubeTextureRec(Texture2D texture,Rectangle source,Vector3 p,float w,float h,float l,Color color){
    if(!mr.camera3dActive||!IsTextureValid(texture)||texture.width<=0||texture.height<=0)return;
    float x=w/2,y=h/2,z=l/2;
    Vector3 v[8]={{p.x-x,p.y-y,p.z-z},{p.x+x,p.y-y,p.z-z},{p.x+x,p.y+y,p.z-z},{p.x-x,p.y+y,p.z-z},{p.x-x,p.y-y,p.z+z},{p.x+x,p.y-y,p.z+z},{p.x+x,p.y+y,p.z+z},{p.x-x,p.y+y,p.z+z}};
    int faces[6][4]={{0,3,2,1},{4,5,6,7},{0,1,5,4},{3,7,6,2},{0,4,7,3},{1,2,6,5}};
    float u0=source.x/texture.width,v0=source.y/texture.height,u1=(source.x+source.width)/texture.width,v1=(source.y+source.height)/texture.height;
    Vector2 uv[4]={{u0,v1},{u0,v0},{u1,v0},{u1,v1}};
    for(int face=0;face<6;face++){
        MRProjected3D q[4];for(int i=0;i<4;i++)q[i]=mr_project3d_ex(v[faces[face][i]],mr.camera3d,mr.targetWidth,mr.targetHeight);
        mr_projected_triangle_uv(q[0],q[1],q[2],uv[0],uv[1],uv[2],color,color,color,texture.id);
        mr_projected_triangle_uv(q[0],q[2],q[3],uv[0],uv[2],uv[3],color,color,color,texture.id);
    }
}
void DrawCubeTexture(Texture2D texture,Vector3 position,float width,float height,float length,Color color){DrawCubeTextureRec(texture,(Rectangle){0,0,(float)texture.width,(float)texture.height},position,width,height,length,color);}
void DrawCubeWiresV(Vector3 p,Vector3 s,Color color){float x=s.x/2,y=s.y/2,z=s.z/2;Vector3 v[8]={{p.x-x,p.y-y,p.z-z},{p.x+x,p.y-y,p.z-z},{p.x+x,p.y+y,p.z-z},{p.x-x,p.y+y,p.z-z},{p.x-x,p.y-y,p.z+z},{p.x+x,p.y-y,p.z+z},{p.x+x,p.y+y,p.z+z},{p.x-x,p.y+y,p.z+z}};int e[12][2]={{0,1},{1,2},{2,3},{3,0},{4,5},{5,6},{6,7},{7,4},{0,4},{1,5},{2,6},{3,7}};for(int i=0;i<12;i++)DrawLine3D(v[e[i][0]],v[e[i][1]],color);}
void DrawCubeWires(Vector3 p,float w,float h,float l,Color c){DrawCubeWiresV(p,(Vector3){w,h,l},c);}
void DrawSphereEx(Vector3 center,float radius,int rings,int slices,Color color){if(rings<2)rings=2;if(slices<3)slices=3;for(int y=0;y<rings;y++){float a0=-MR_PI/2+MR_PI*y/rings,a1=-MR_PI/2+MR_PI*(y+1)/rings;for(int x=0;x<slices;x++){float b0=2*MR_PI*x/slices,b1=2*MR_PI*(x+1)/slices;Vector3 p0={center.x+cosf(a0)*cosf(b0)*radius,center.y+sinf(a0)*radius,center.z+cosf(a0)*sinf(b0)*radius},p1={center.x+cosf(a0)*cosf(b1)*radius,center.y+sinf(a0)*radius,center.z+cosf(a0)*sinf(b1)*radius},p2={center.x+cosf(a1)*cosf(b1)*radius,center.y+sinf(a1)*radius,center.z+cosf(a1)*sinf(b1)*radius},p3={center.x+cosf(a1)*cosf(b0)*radius,center.y+sinf(a1)*radius,center.z+cosf(a1)*sinf(b0)*radius};mr_triangle3d(p0,p1,p2,color);mr_triangle3d(p0,p2,p3,color);}}}
void DrawSphere(Vector3 c,float r,Color color){DrawSphereEx(c,r,16,24,color);}
void DrawSphereWires(Vector3 center,float radius,int rings,int slices,Color color){if(rings<2)rings=2;if(slices<3)slices=3;for(int y=1;y<rings;y++){float a=-MR_PI/2+MR_PI*y/rings;Vector3 previous={center.x+cosf(a)*radius,center.y+sinf(a)*radius,center.z};for(int x=1;x<=slices;x++){float b=2*MR_PI*x/slices;Vector3 p={center.x+cosf(a)*cosf(b)*radius,center.y+sinf(a)*radius,center.z+cosf(a)*sinf(b)*radius};DrawLine3D(previous,p,color);previous=p;}}for(int x=0;x<slices;x++){float b=2*MR_PI*x/slices;Vector3 previous={center.x,center.y-radius,center.z};for(int y=1;y<=rings;y++){float a=-MR_PI/2+MR_PI*y/rings;Vector3 p={center.x+cosf(a)*cosf(b)*radius,center.y+sinf(a)*radius,center.z+cosf(a)*sinf(b)*radius};DrawLine3D(previous,p,color);previous=p;}}}
static void mr_cylinder_basis(Vector3 a,Vector3 b,Vector3 *u,Vector3 *v){Vector3 axis=mr_v3_norm(mr_v3_sub(b,a)),helper=(axis.y>-0.9f&&axis.y<0.9f)?(Vector3){0,1,0}:(Vector3){1,0,0};*u=mr_v3_norm(mr_v3_cross(axis,helper));*v=mr_v3_cross(axis,*u);}
void DrawCylinderEx(Vector3 a,Vector3 b,float ra,float rb,int sides,Color color){if(sides<3)sides=3;Vector3 u,v;mr_cylinder_basis(a,b,&u,&v);for(int i=0;i<sides;i++){float x=2*MR_PI*i/sides,y=2*MR_PI*(i+1)/sides;Vector3 a0=mr_v3_add(a,mr_v3_add(mr_v3_scale(u,cosf(x)*ra),mr_v3_scale(v,sinf(x)*ra))),a1=mr_v3_add(a,mr_v3_add(mr_v3_scale(u,cosf(y)*ra),mr_v3_scale(v,sinf(y)*ra))),b0=mr_v3_add(b,mr_v3_add(mr_v3_scale(u,cosf(x)*rb),mr_v3_scale(v,sinf(x)*rb))),b1=mr_v3_add(b,mr_v3_add(mr_v3_scale(u,cosf(y)*rb),mr_v3_scale(v,sinf(y)*rb)));mr_triangle3d(a0,a1,b1,color);mr_triangle3d(a0,b1,b0,color);mr_triangle3d(a,a1,a0,color);mr_triangle3d(b,b0,b1,color);}}
void DrawCylinder(Vector3 p,float top,float bottom,float height,int slices,Color color){DrawCylinderEx(p,(Vector3){p.x,p.y+height,p.z},bottom,top,slices,color);}
void DrawCylinderWiresEx(Vector3 a,Vector3 b,float ra,float rb,int sides,Color color){if(sides<3)sides=3;Vector3 u,v;mr_cylinder_basis(a,b,&u,&v);for(int i=0;i<sides;i++){float x=2*MR_PI*i/sides,y=2*MR_PI*(i+1)/sides;Vector3 a0=mr_v3_add(a,mr_v3_add(mr_v3_scale(u,cosf(x)*ra),mr_v3_scale(v,sinf(x)*ra))),a1=mr_v3_add(a,mr_v3_add(mr_v3_scale(u,cosf(y)*ra),mr_v3_scale(v,sinf(y)*ra))),b0=mr_v3_add(b,mr_v3_add(mr_v3_scale(u,cosf(x)*rb),mr_v3_scale(v,sinf(x)*rb))),b1=mr_v3_add(b,mr_v3_add(mr_v3_scale(u,cosf(y)*rb),mr_v3_scale(v,sinf(y)*rb)));DrawLine3D(a0,a1,color);DrawLine3D(b0,b1,color);DrawLine3D(a0,b0,color);}}
void DrawCylinderWires(Vector3 p,float top,float bottom,float height,int slices,Color color){DrawCylinderWiresEx(p,(Vector3){p.x,p.y+height,p.z},bottom,top,slices,color);}
void DrawCapsule(Vector3 a,Vector3 b,float radius,int slices,int rings,Color color){DrawCylinderEx(a,b,radius,radius,slices,color);DrawSphereEx(a,radius,rings,slices,color);DrawSphereEx(b,radius,rings,slices,color);}
void DrawCapsuleWires(Vector3 a,Vector3 b,float radius,int slices,int rings,Color color){DrawCylinderWiresEx(a,b,radius,radius,slices,color);DrawSphereWires(a,radius,rings,slices,color);DrawSphereWires(b,radius,rings,slices,color);}
void DrawPlane(Vector3 p,Vector2 s,Color c){Vector3 a={p.x-s.x/2,p.y,p.z-s.y/2},b={p.x+s.x/2,p.y,p.z-s.y/2},d={p.x-s.x/2,p.y,p.z+s.y/2},e={p.x+s.x/2,p.y,p.z+s.y/2};mr_triangle3d(a,e,b,c);mr_triangle3d(a,d,e,c);}
void DrawRay(Ray ray,Color color){DrawLine3D(ray.position,mr_v3_add(ray.position,mr_v3_scale(ray.direction,10000)),color);}
void DrawGrid(int slices,float spacing){int half=slices/2;for(int i=-half;i<=half;i++){Color c=i==0?(Color){130,130,130,255}:(Color){80,80,80,255};DrawLine3D((Vector3){i*spacing,0,-half*spacing},(Vector3){i*spacing,0,half*spacing},c);DrawLine3D((Vector3){-half*spacing,0,i*spacing},(Vector3){half*spacing,0,i*spacing},c);}}
bool CheckCollisionSpheres(Vector3 a,float ar,Vector3 b,float br){Vector3 d=mr_v3_sub(a,b);float r=ar+br;return mr_v3_dot(d,d)<=r*r;}
bool CheckCollisionBoxes(BoundingBox a,BoundingBox b){return a.min.x<=b.max.x&&a.max.x>=b.min.x&&a.min.y<=b.max.y&&a.max.y>=b.min.y&&a.min.z<=b.max.z&&a.max.z>=b.min.z;}
bool CheckCollisionBoxSphere(BoundingBox b,Vector3 c,float r){float x=c.x<b.min.x?b.min.x:c.x>b.max.x?b.max.x:c.x,y=c.y<b.min.y?b.min.y:c.y>b.max.y?b.max.y:c.y,z=c.z<b.min.z?b.min.z:c.z>b.max.z?b.max.z:c.z;Vector3 d={c.x-x,c.y-y,c.z-z};return mr_v3_dot(d,d)<=r*r;}
RayCollision GetRayCollisionSphere(Ray ray,Vector3 center,float radius){RayCollision hit={0};Vector3 oc=mr_v3_sub(ray.position,center);float a=mr_v3_dot(ray.direction,ray.direction),b=2*mr_v3_dot(oc,ray.direction),c=mr_v3_dot(oc,oc)-radius*radius,d=b*b-4*a*c;if(d<0||a==0)return hit;float t=(-b-sqrtf(d))/(2*a);if(t<0)t=(-b+sqrtf(d))/(2*a);if(t<0)return hit;hit.hit=true;hit.distance=t;hit.point=mr_v3_add(ray.position,mr_v3_scale(ray.direction,t));hit.normal=mr_v3_norm(mr_v3_sub(hit.point,center));return hit;}
RayCollision GetRayCollisionBox(Ray ray,BoundingBox box){RayCollision hit={0};float tmin=0,tmax=1e30f;Vector3 normal={0};float o[3]={ray.position.x,ray.position.y,ray.position.z},d[3]={ray.direction.x,ray.direction.y,ray.direction.z},mn[3]={box.min.x,box.min.y,box.min.z},mx[3]={box.max.x,box.max.y,box.max.z};for(int i=0;i<3;i++){if(d[i]>-0.000001f&&d[i]<0.000001f){if(o[i]<mn[i]||o[i]>mx[i])return hit;continue;}float a=(mn[i]-o[i])/d[i],b=(mx[i]-o[i])/d[i],sign=-1;if(a>b){float q=a;a=b;b=q;sign=1;}if(a>tmin){tmin=a;normal=(Vector3){0};if(i==0)normal.x=sign;if(i==1)normal.y=sign;if(i==2)normal.z=sign;}if(b<tmax)tmax=b;if(tmin>tmax)return hit;}if(tmax<0)return hit;hit.hit=true;hit.distance=tmin>=0?tmin:tmax;hit.point=mr_v3_add(ray.position,mr_v3_scale(ray.direction,hit.distance));hit.normal=normal;return hit;}
RayCollision GetRayCollisionTriangle(Ray ray,Vector3 a,Vector3 b,Vector3 c){RayCollision hit={0};Vector3 e1=mr_v3_sub(b,a),e2=mr_v3_sub(c,a),p=mr_v3_cross(ray.direction,e2);float det=mr_v3_dot(e1,p);if(det>-0.000001f&&det<0.000001f)return hit;float inv=1/det;Vector3 t=mr_v3_sub(ray.position,a);float u=mr_v3_dot(t,p)*inv;if(u<0||u>1)return hit;Vector3 q=mr_v3_cross(t,e1);float v=mr_v3_dot(ray.direction,q)*inv;if(v<0||u+v>1)return hit;float distance=mr_v3_dot(e2,q)*inv;if(distance<0)return hit;hit.hit=true;hit.distance=distance;hit.point=mr_v3_add(ray.position,mr_v3_scale(ray.direction,distance));hit.normal=mr_v3_norm(mr_v3_cross(e1,e2));return hit;}
RayCollision GetRayCollisionQuad(Ray ray,Vector3 a,Vector3 b,Vector3 c,Vector3 d){RayCollision first=GetRayCollisionTriangle(ray,a,b,c),second=GetRayCollisionTriangle(ray,a,c,d);if(!first.hit)return second;if(!second.hit)return first;return first.distance<=second.distance?first:second;}
static Matrix mr_matrix_identity(void){Matrix m={0};m.m0=m.m5=m.m10=m.m15=1;return m;}
static Vector3 mr_v3_transform(Vector3 v,Matrix m){return(Vector3){m.m0*v.x+m.m4*v.y+m.m8*v.z+m.m12,m.m1*v.x+m.m5*v.y+m.m9*v.z+m.m13,m.m2*v.x+m.m6*v.y+m.m10*v.z+m.m14};}
static Matrix mr_matrix_multiply(Matrix a,Matrix b){
    const float *x=(const float*)&a,*y=(const float*)&b;Matrix result={0};float *r=(float*)&result;
    for(int row=0;row<4;row++)for(int col=0;col<4;col++)for(int k=0;k<4;k++)r[row*4+col]+=x[row*4+k]*y[k*4+col];return result;
}
static Matrix mr_model_matrix(Vector3 position,Vector3 axis,float angle,Vector3 scale){
    axis=mr_v3_norm(axis);if(mr_v3_len(axis)<0.0001f)axis=(Vector3){0,1,0};float c=cosf(angle*MR_DEG2RAD),s=sinf(angle*MR_DEG2RAD),t=1-c,x=axis.x,y=axis.y,z=axis.z;Matrix m=mr_matrix_identity();
    m.m0=(t*x*x+c)*scale.x;m.m4=(t*x*y-s*z)*scale.y;m.m8=(t*x*z+s*y)*scale.z;
    m.m1=(t*x*y+s*z)*scale.x;m.m5=(t*y*y+c)*scale.y;m.m9=(t*y*z-s*x)*scale.z;
    m.m2=(t*x*z-s*y)*scale.x;m.m6=(t*y*z+s*x)*scale.y;m.m10=(t*z*z+c)*scale.z;
    m.m12=position.x;m.m13=position.y;m.m14=position.z;return m;
}
static MRMeshEntry *mr_mesh_entry(unsigned int id){if(!id)return NULL;for(int i=0;i<MR_MAX_MESHES;i++)if(mr.meshes[i].id==id)return &mr.meshes[i];return NULL;}
static MRGpuVertex *mr_pack_mesh(Mesh mesh){
    if(!mesh.vertices||mesh.vertexCount<=0)return NULL;
    MRGpuVertex *packed=MemAlloc((unsigned int)mesh.vertexCount*sizeof(MRGpuVertex));if(!packed)return NULL;
    memset(packed,0,(size_t)mesh.vertexCount*sizeof(MRGpuVertex));
    float *positions=mesh.animVertices?mesh.animVertices:mesh.vertices,*normals=mesh.animNormals?mesh.animNormals:mesh.normals;
    for(int i=0;i<mesh.vertexCount;i++){
        MRGpuVertex *v=&packed[i];v->x=positions[i*3];v->y=positions[i*3+1];v->z=positions[i*3+2];
        v->nx=normals?normals[i*3]:0;v->ny=normals?normals[i*3+1]:1;v->nz=normals?normals[i*3+2]:0;
        v->u=mesh.texcoords?mesh.texcoords[i*2]:0.5f;v->v=mesh.texcoords?mesh.texcoords[i*2+1]:0.5f;
        v->r=mesh.colors?mesh.colors[i*4]:255;v->g=mesh.colors?mesh.colors[i*4+1]:255;v->b=mesh.colors?mesh.colors[i*4+2]:255;v->a=mesh.colors?mesh.colors[i*4+3]:255;
        if(mesh.boneIds)memcpy(v->boneIds,mesh.boneIds+i*4,4);if(mesh.boneWeights)memcpy(v->boneWeights,mesh.boneWeights+i*4,4*sizeof(float));
        if(mesh.tangents)memcpy(v->tangent,mesh.tangents+i*4,4*sizeof(float));else{v->tangent[0]=1;v->tangent[3]=1;}
        v->u2=mesh.texcoords2?mesh.texcoords2[i*2]:v->u;v->v2=mesh.texcoords2?mesh.texcoords2[i*2+1]:v->v;
    }
    return packed;
}
static void mr_upload_mesh_data(Mesh mesh,MRMeshEntry *entry){
    MRGpuVertex *packed=mr_pack_mesh(mesh);if(!packed||!entry)return;int indexCount=mesh.indices?mesh.triangleCount*3:0;
#ifdef _WIN32
    if(entry->vertexBuffer)wgpuBufferRelease(entry->vertexBuffer);if(entry->indexBuffer)wgpuBufferRelease(entry->indexBuffer);entry->vertexBuffer=NULL;entry->indexBuffer=NULL;
    WGPUBufferDescriptor descriptor=WGPU_BUFFER_DESCRIPTOR_INIT;descriptor.size=(uint64_t)mesh.vertexCount*sizeof(MRGpuVertex);descriptor.usage=WGPUBufferUsage_Vertex|WGPUBufferUsage_CopyDst;entry->vertexBuffer=wgpuDeviceCreateBuffer(mr.device,&descriptor);if(entry->vertexBuffer)wgpuQueueWriteBuffer(mr.queue,entry->vertexBuffer,0,packed,(size_t)descriptor.size);
    if(indexCount>0){size_t indexBytes=(size_t)indexCount*sizeof(unsigned short),paddedBytes=(indexBytes+3)&~(size_t)3;descriptor.size=(uint64_t)paddedBytes;descriptor.usage=WGPUBufferUsage_Index|WGPUBufferUsage_CopyDst;entry->indexBuffer=wgpuDeviceCreateBuffer(mr.device,&descriptor);if(entry->indexBuffer){unsigned char *padded=MemAlloc((unsigned int)paddedBytes);if(padded){memset(padded,0,paddedBytes);memcpy(padded,mesh.indices,indexBytes);wgpuQueueWriteBuffer(mr.queue,entry->indexBuffer,0,padded,paddedBytes);MemFree(padded);}}}
#else
    mr_web_mesh_upload(entry->id,packed,mesh.vertexCount,mesh.indices,indexCount);
#endif
    entry->vertexCount=mesh.vertexCount;entry->indexCount=indexCount;entry->indexed=indexCount>0;MemFree(packed);
}
static void mr_update_mesh_vertices(Mesh mesh){MRMeshEntry *entry=mr_mesh_entry(mesh.vaoId);if(!entry||entry->vertexCount!=mesh.vertexCount)return;MRGpuVertex *packed=mr_pack_mesh(mesh);if(!packed)return;
#ifdef _WIN32
    if(entry->vertexBuffer)wgpuQueueWriteBuffer(mr.queue,entry->vertexBuffer,0,packed,(size_t)mesh.vertexCount*sizeof(MRGpuVertex));
#else
    mr_web_mesh_update(entry->id,packed,mesh.vertexCount);
#endif
    MemFree(packed);
}
void UploadMesh(Mesh *mesh,bool dynamic){(void)dynamic;if(!mesh||!mesh->vertices||mesh->vertexCount<=0||!mr.ready)return;MRMeshEntry *entry=mr_mesh_entry(mesh->vaoId);if(!entry)for(int i=0;i<MR_MAX_MESHES;i++)if(!mr.meshes[i].id){entry=&mr.meshes[i];entry->id=++mr.nextMesh;if(!entry->id)entry->id=++mr.nextMesh;mesh->vaoId=entry->id;break;}if(!entry){puts("sargpu: mesh limit reached");return;}if(!mesh->vboId){mesh->vboId=MemAlloc(9*sizeof(unsigned int));if(mesh->vboId)for(int i=0;i<9;i++)mesh->vboId[i]=entry->id;}mr_upload_mesh_data(*mesh,entry);}
void UpdateMeshBuffer(Mesh mesh,int index,const void *data,int dataSize,int offset){if(!data||dataSize<=0||offset<0)return;void *target=NULL;int capacity=0;if(index==0){target=mesh.animVertices?mesh.animVertices:mesh.vertices;capacity=mesh.vertexCount*3*(int)sizeof(float);}else if(index==1){target=mesh.texcoords;capacity=mesh.vertexCount*2*(int)sizeof(float);}else if(index==2){target=mesh.animNormals?mesh.animNormals:mesh.normals;capacity=mesh.vertexCount*3*(int)sizeof(float);}else if(index==3){target=mesh.colors;capacity=mesh.vertexCount*4;}else if(index==4){target=mesh.tangents;capacity=mesh.vertexCount*4*(int)sizeof(float);}else if(index==5){target=mesh.texcoords2;capacity=mesh.vertexCount*2*(int)sizeof(float);}else if(index==6){target=mesh.indices;capacity=mesh.triangleCount*3*(int)sizeof(unsigned short);}if(target&&offset+dataSize<=capacity){memcpy((unsigned char*)target+offset,data,(size_t)dataSize);MRMeshEntry *entry=mr_mesh_entry(mesh.vaoId);if(entry){if(index==6)mr_upload_mesh_data(mesh,entry);else mr_update_mesh_vertices(mesh);}}}
void UnloadMesh(Mesh mesh){MRMeshEntry *entry=mr_mesh_entry(mesh.vaoId);if(entry){
#ifdef _WIN32
    if(entry->vertexBuffer)wgpuBufferRelease(entry->vertexBuffer);if(entry->indexBuffer)wgpuBufferRelease(entry->indexBuffer);
#else
    mr_web_mesh_unload(entry->id);
#endif
    memset(entry,0,sizeof *entry);}MemFree(mesh.vertices);MemFree(mesh.texcoords);MemFree(mesh.texcoords2);MemFree(mesh.normals);MemFree(mesh.tangents);MemFree(mesh.colors);MemFree(mesh.indices);MemFree(mesh.animVertices);MemFree(mesh.animNormals);MemFree(mesh.boneIds);MemFree(mesh.boneWeights);MemFree(mesh.boneMatrices);MemFree(mesh.vboId);if(mesh.morphTargets)for(int i=0;i<mesh.morphTargetCount;i++){MemFree(mesh.morphTargets[i].vertices);MemFree(mesh.morphTargets[i].normals);MemFree(mesh.morphTargets[i].tangents);}MemFree(mesh.morphTargets);MemFree(mesh.morphWeights);MemFree(mesh.morphBaseVertices);MemFree(mesh.morphBaseNormals);MemFree(mesh.morphBaseTangents);}
Material LoadMaterialDefault(void){Material material={0};material.maps=MemAlloc(11*sizeof(MaterialMap));if(material.maps){memset(material.maps,0,11*sizeof(MaterialMap));for(int i=0;i<11;i++)material.maps[i].color=WHITE;material.maps[MATERIAL_MAP_ALBEDO].texture=(Texture2D){mr.white,1,1,1,7};material.maps[MATERIAL_MAP_METALNESS].value=0;material.maps[MATERIAL_MAP_ROUGHNESS].value=1;material.maps[MATERIAL_MAP_NORMAL].value=1;material.maps[MATERIAL_MAP_OCCLUSION].value=1;material.maps[MATERIAL_MAP_EMISSION].color=BLACK;material.params[0]=1;material.params[1]=1;}return material;}
bool IsMaterialValid(Material material){return material.maps!=NULL;}
void UnloadMaterial(Material material){
    if(material.maps)for(int i=0;i<11;i++)if(material.maps[i].texture.id&&material.maps[i].texture.id!=mr.white)UnloadTexture(material.maps[i].texture);
    if(material.shader.id)UnloadShader(material.shader);MemFree(material.maps);
}
void SetMaterialTexture(Material *material,int mapType,Texture2D texture){if(material&&material->maps&&mapType>=0&&mapType<11)material->maps[mapType].texture=texture;}
BoundingBox GetMeshBoundingBox(Mesh mesh){BoundingBox box={0};if(!mesh.vertices||mesh.vertexCount<=0)return box;box.min=box.max=(Vector3){mesh.vertices[0],mesh.vertices[1],mesh.vertices[2]};for(int i=1;i<mesh.vertexCount;i++){Vector3 p={mesh.vertices[i*3],mesh.vertices[i*3+1],mesh.vertices[i*3+2]};if(p.x<box.min.x)box.min.x=p.x;if(p.y<box.min.y)box.min.y=p.y;if(p.z<box.min.z)box.min.z=p.z;if(p.x>box.max.x)box.max.x=p.x;if(p.y>box.max.y)box.max.y=p.y;if(p.z>box.max.z)box.max.z=p.z;}return box;}
static Matrix mr_view_projection(Camera3D camera,int width,int height){Vector3 r,u,f;mr_camera_basis(camera,&r,&u,&f);float aspect=height>0?(float)width/height:1,n=0.01f,farPlane=1000.0f;Matrix m={0};if(camera.projection==CAMERA_ORTHOGRAPHIC){float vertical=camera.fovy>0?camera.fovy:1,sx=2/(vertical*aspect),sy=2/vertical,sz=1/(farPlane-n);m.m0=r.x*sx;m.m4=r.y*sx;m.m8=r.z*sx;m.m12=-mr_v3_dot(r,camera.position)*sx;m.m1=u.x*sy;m.m5=u.y*sy;m.m9=u.z*sy;m.m13=-mr_v3_dot(u,camera.position)*sy;m.m2=f.x*sz;m.m6=f.y*sz;m.m10=f.z*sz;m.m14=(-mr_v3_dot(f,camera.position)-n)*sz;m.m15=1;}else{float tangent=sinf(camera.fovy*MR_DEG2RAD*0.5f)/cosf(camera.fovy*MR_DEG2RAD*0.5f);if(tangent<=0)tangent=0.0001f;float sx=1/(tangent*aspect),sy=1/tangent,sz=farPlane/(farPlane-n),cameraForward=mr_v3_dot(f,camera.position);m.m0=r.x*sx;m.m4=r.y*sx;m.m8=r.z*sx;m.m12=-mr_v3_dot(r,camera.position)*sx;m.m1=u.x*sy;m.m5=u.y*sy;m.m9=u.z*sy;m.m13=-mr_v3_dot(u,camera.position)*sy;m.m2=f.x*sz;m.m6=f.y*sz;m.m10=f.z*sz;m.m14=-cameraForward*sz-n*farPlane/(farPlane-n);m.m3=f.x;m.m7=f.y;m.m11=f.z;m.m15=-cameraForward;}return m;}
static void mr_prepare_scene3d(int width,int height){
    Vector3 right,up,forward;mr_camera_basis(mr.camera3d,&right,&up,&forward);float aspect=height>0?(float)width/height:1.0f;
    float tangent=mr.camera3d.projection==CAMERA_PERSPECTIVE?sinf(mr.camera3d.fovy*MR_DEG2RAD*0.5f)/cosf(mr.camera3d.fovy*MR_DEG2RAD*0.5f):0;
    mr.scene3d=(MRScene3D){0};mr.scene3d.viewProjection=mr_view_projection(mr.camera3d,width,height);
    mr.scene3d.camera=(Vector4){mr.camera3d.position.x,mr.camera3d.position.y,mr.camera3d.position.z,1};
    mr.scene3d.ambient=(Vector4){mr.ambientColor.r/255.0f,mr.ambientColor.g/255.0f,mr.ambientColor.b/255.0f,mr.ambientIntensity};
    mr.scene3d.fogColor=(Vector4){mr.fogColor.r/255.0f,mr.fogColor.g/255.0f,mr.fogColor.b/255.0f,mr.fogColor.a/255.0f};
    mr.scene3d.fogParams=(Vector4){(float)mr.fogMode,mr.fogStart,mr.fogEnd,mr.fogDensity};
    mr.scene3d.skyRight=(Vector4){right.x,right.y,right.z,aspect};mr.scene3d.skyUp=(Vector4){up.x,up.y,up.z,tangent};
    mr.scene3d.skyForward=(Vector4){forward.x,forward.y,forward.z,(float)mr.camera3d.projection};
    int lightCount=0;for(int i=0;i<SARGPU_MAX_LIGHTS;i++){Light3D light=mr.lights[i];if(light.enabled)lightCount++;Vector3 direction=mr_v3_norm(mr_v3_sub(light.target,light.position));MRLightGPU *gpu=&mr.scene3d.lights[i];gpu->positionType=(Vector4){light.position.x,light.position.y,light.position.z,(float)light.type};gpu->directionRange=(Vector4){direction.x,direction.y,direction.z,light.range};gpu->colorIntensity=(Vector4){light.color.r/255.0f,light.color.g/255.0f,light.color.b/255.0f,light.intensity};gpu->spotEnabled=(Vector4){light.innerCutoff,light.outerCutoff,light.enabled?1.0f:0.0f,0};}
    (void)lightCount;mr.scene3d.settings=(Vector4){mr.pbrEnabled?1.0f:0.0f,mr.skyboxTint.r/255.0f,mr.skyboxTint.g/255.0f,mr.skyboxTint.b/255.0f};
}
static void mr_queue_mesh_instances(Mesh mesh,Material material,const Matrix *transforms,int count){
    if(!mr.drawing||!mr.camera3dActive||!transforms||count<=0||!mr_mesh_entry(mesh.vaoId))return;
    if(mr.instanceCount3d>=MR_MAX_3D_INSTANCES||mr.drawCount3d>=MR_MAX_3D_DRAWS)return;
    if(count>MR_MAX_3D_INSTANCES-(int)mr.instanceCount3d)count=MR_MAX_3D_INSTANCES-(int)mr.instanceCount3d;
    MRShaderEntry *shader=mr_shader(material.shader.id);unsigned int shaderId=shader&&shader->materialShader?shader->id:0;
    unsigned int textures[SARGPU_MAX_SHADER_TEXTURES],flags=0;Color tint=WHITE,emission=BLACK;
    for(int slot=0;slot<SARGPU_MAX_SHADER_TEXTURES;slot++){
        unsigned int id=mr.white;if(material.maps&&slot<11&&IsTextureValid(material.maps[slot].texture)){id=material.maps[slot].texture.id;flags|=1u<<slot;}
        else if(shader&&shader->extraTextures[slot]&&mr_texture(shader->extraTextures[slot]))id=shader->extraTextures[slot];textures[slot]=id;
    }
    float metallic=0,roughness=1,normalScale=1,emissionStrength=1;if(material.maps){tint=material.maps[MATERIAL_MAP_ALBEDO].color;emission=material.maps[MATERIAL_MAP_EMISSION].color;metallic=material.maps[MATERIAL_MAP_METALNESS].value;roughness=material.maps[MATERIAL_MAP_ROUGHNESS].value;normalScale=material.maps[MATERIAL_MAP_NORMAL].value;emissionStrength=material.params[1];}
    if(roughness<=0)roughness=0.04f;if(normalScale==0)normalScale=1;if(emissionStrength==0)emissionStrength=1;
    unsigned int boneOffset=mr.boneMatrixCount3d,boneCount=0;if(mesh.boneMatrices&&mesh.boneCount>0){boneCount=(unsigned int)mesh.boneCount;if(boneCount>MR_MAX_BONE_MATRICES_FRAME-mr.boneMatrixCount3d)boneCount=MR_MAX_BONE_MATRICES_FRAME-mr.boneMatrixCount3d;if(boneCount){memcpy(mr.boneMatricesFrame+boneOffset,mesh.boneMatrices,(size_t)boneCount*sizeof(Matrix));mr.boneMatrixCount3d+=boneCount;}}
    unsigned int first=mr.instanceCount3d;
    for(int i=0;i<count;i++){MRInstance3D *instance=&mr.instances3d[mr.instanceCount3d++];memset(instance,0,sizeof *instance);instance->model=transforms[i];instance->tint=tint;instance->material[0]=metallic;instance->material[1]=roughness;instance->material[2]=normalScale;instance->material[3]=emissionStrength;instance->emission=emission;instance->skin[0]=boneOffset;instance->skin[1]=boneCount;instance->skin[2]=flags;}
    MRDraw3D *command=&mr.draws3d[mr.drawCount3d++];memset(command,0,sizeof *command);command->mesh=mesh.vaoId;command->firstInstance=first;command->instanceCount=(unsigned int)count;command->shader=shaderId;memcpy(command->textures,textures,sizeof textures);
}
void DrawMesh(Mesh mesh,Material material,Matrix transform){mr_queue_mesh_instances(mesh,material,&transform,1);}
void DrawMeshInstanced(Mesh mesh,Material material,const Matrix *transforms,int instances){mr_queue_mesh_instances(mesh,material,transforms,instances);}
Model LoadModelFromMesh(Mesh mesh){Model model={0};model.transform=mr_matrix_identity();model.meshCount=1;model.materialCount=1;model.meshes=MemAlloc(sizeof(Mesh));model.materials=MemAlloc(sizeof(Material));model.meshMaterial=MemAlloc(sizeof(int));if(!model.meshes||!model.materials||!model.meshMaterial){MemFree(model.meshes);MemFree(model.materials);MemFree(model.meshMaterial);return(Model){0};}model.meshes[0]=mesh;model.materials[0]=LoadMaterialDefault();model.meshMaterial[0]=0;return model;}
bool IsModelValid(Model model){if(model.meshCount<=0||!model.meshes||model.materialCount<=0||!model.materials||!model.meshMaterial)return false;for(int i=0;i<model.meshCount;i++)if(model.meshes[i].vertices&&model.meshes[i].vertexCount>0&&model.meshes[i].triangleCount>0)return true;return false;}
void SetModelMeshMaterial(Model *model,int meshId,int materialId){if(model&&model->meshMaterial&&meshId>=0&&meshId<model->meshCount&&materialId>=0&&materialId<model->materialCount)model->meshMaterial[meshId]=materialId;}
static void mr_free_morph_data(ModelMorphData *data);
void UnloadModel(Model model){mr_free_morph_data(model.morphData);unsigned int unloaded[128]={0};int unloadedCount=0;if(model.materials)for(int i=0;i<model.materialCount;i++)if(model.materials[i].maps){for(int map=0;map<11;map++){Texture2D texture=model.materials[i].maps[map].texture;bool seen=texture.id==0||texture.id==mr.white;for(int j=0;j<unloadedCount;j++)if(unloaded[j]==texture.id)seen=true;if(!seen&&unloadedCount<128){unloaded[unloadedCount++]=texture.id;UnloadTexture(texture);}}UnloadMaterial(model.materials[i]);}if(model.meshes)for(int i=0;i<model.meshCount;i++)UnloadMesh(model.meshes[i]);MemFree(model.meshes);MemFree(model.materials);MemFree(model.meshMaterial);MemFree(model.bones);MemFree(model.bindPose);}
static void mr_draw_model_internal(Model model,Matrix transform,Color tint,int style){if(!IsModelValid(model))return;Matrix combined=mr_matrix_multiply(transform,model.transform);for(int i=0;i<model.meshCount;i++){int materialId=model.meshMaterial[i];if(materialId<0||materialId>=model.materialCount)materialId=0;Material material=model.materials[materialId];Color old=material.maps?material.maps[0].color:WHITE;if(material.maps)material.maps[0].color=ColorTint(old,tint);if(style==0)DrawMesh(model.meshes[i],material,combined);else{Mesh mesh=model.meshes[i];float *vertices=mesh.animVertices?mesh.animVertices:mesh.vertices;for(int t=0;t<mesh.triangleCount;t++){int a=mesh.indices?mesh.indices[t*3]:t*3,b=mesh.indices?mesh.indices[t*3+1]:t*3+1,c=mesh.indices?mesh.indices[t*3+2]:t*3+2;if(a>=mesh.vertexCount||b>=mesh.vertexCount||c>=mesh.vertexCount)continue;Vector3 p[3];int ids[3]={a,b,c};for(int j=0;j<3;j++)p[j]=mr_v3_transform((Vector3){vertices[ids[j]*3],vertices[ids[j]*3+1],vertices[ids[j]*3+2]},combined);if(style==1){DrawLine3D(p[0],p[1],tint);DrawLine3D(p[1],p[2],tint);DrawLine3D(p[2],p[0],tint);}else{DrawPoint3D(p[0],tint);DrawPoint3D(p[1],tint);DrawPoint3D(p[2],tint);}}}if(material.maps)material.maps[0].color=old;}}
void DrawModelEx(Model model,Vector3 position,Vector3 axis,float angle,Vector3 scale,Color tint){mr_draw_model_internal(model,mr_model_matrix(position,axis,angle,scale),tint,0);}
void DrawModel(Model model,Vector3 position,float scale,Color tint){DrawModelEx(model,position,(Vector3){0,1,0},0,(Vector3){scale,scale,scale},tint);}
void DrawModelWiresEx(Model model,Vector3 position,Vector3 axis,float angle,Vector3 scale,Color tint){mr_draw_model_internal(model,mr_model_matrix(position,axis,angle,scale),tint,1);}
void DrawModelWires(Model model,Vector3 position,float scale,Color tint){DrawModelWiresEx(model,position,(Vector3){0,1,0},0,(Vector3){scale,scale,scale},tint);}
void DrawModelPointsEx(Model model,Vector3 position,Vector3 axis,float angle,Vector3 scale,Color tint){mr_draw_model_internal(model,mr_model_matrix(position,axis,angle,scale),tint,2);}
void DrawModelPoints(Model model,Vector3 position,float scale,Color tint){DrawModelPointsEx(model,position,(Vector3){0,1,0},0,(Vector3){scale,scale,scale},tint);}
void DrawBillboardPro(Camera camera,Texture2D texture,Rectangle source,Vector3 position,Vector3 up,Vector2 size,Vector2 origin,float rotation,Color tint){
    if(!mr.camera3dActive||!IsTextureValid(texture)||texture.width<=0||texture.height<=0)return;
    Vector3 forward=mr_v3_norm(mr_v3_sub(camera.position,position));
    Vector3 upAxis=mr_v3_norm(up);if(mr_v3_len(upAxis)<0.0001f)upAxis=(Vector3){0,1,0};
    Vector3 right=mr_v3_norm(mr_v3_cross(upAxis,forward));
    if(mr_v3_len(right)<0.0001f){Vector3 fallback=(forward.y>-0.9f&&forward.y<0.9f)?(Vector3){0,1,0}:(Vector3){1,0,0};right=mr_v3_norm(mr_v3_cross(fallback,forward));}
    upAxis=mr_v3_norm(mr_v3_cross(forward,right));
    float radians=rotation*MR_DEG2RAD,c=cosf(radians),s=sinf(radians);
    float x[4]={-origin.x,size.x-origin.x,size.x-origin.x,-origin.x};
    float y[4]={size.y-origin.y,size.y-origin.y,-origin.y,-origin.y};
    Vector3 p[4];for(int i=0;i<4;i++){float xr=x[i]*c-y[i]*s,yr=x[i]*s+y[i]*c;p[i]=mr_v3_add(position,mr_v3_add(mr_v3_scale(right,xr),mr_v3_scale(upAxis,yr)));}
    float u0=source.x/texture.width,v0=source.y/texture.height,u1=(source.x+source.width)/texture.width,v1=(source.y+source.height)/texture.height;
    Vector2 uv[4]={{u0,v0},{u1,v0},{u1,v1},{u0,v1}};MRProjected3D projected[4];for(int i=0;i<4;i++)projected[i]=mr_project3d_ex(p[i],camera,mr.targetWidth,mr.targetHeight);
    mr_projected_triangle_uv(projected[0],projected[2],projected[1],uv[0],uv[2],uv[1],tint,tint,tint,texture.id);
    mr_projected_triangle_uv(projected[0],projected[3],projected[2],uv[0],uv[3],uv[2],tint,tint,tint,texture.id);
}
void DrawBillboardRec(Camera camera,Texture2D texture,Rectangle source,Vector3 position,Vector2 size,Color tint){DrawBillboardPro(camera,texture,source,position,camera.up,size,(Vector2){size.x*0.5f,size.y*0.5f},0,tint);}
void DrawBillboard(Camera camera,Texture2D texture,Vector3 position,float scale,Color tint){float aspect=texture.height>0?(float)texture.width/texture.height:1;DrawBillboardRec(camera,texture,(Rectangle){0,0,(float)texture.width,(float)texture.height},position,(Vector2){scale*aspect,scale},tint);}
BoundingBox GetModelBoundingBox(Model model){BoundingBox result={0};bool first=true;if(!IsModelValid(model))return result;for(int i=0;i<model.meshCount;i++){BoundingBox box=GetMeshBoundingBox(model.meshes[i]);Vector3 corners[8]={{box.min.x,box.min.y,box.min.z},{box.max.x,box.min.y,box.min.z},{box.min.x,box.max.y,box.min.z},{box.max.x,box.max.y,box.min.z},{box.min.x,box.min.y,box.max.z},{box.max.x,box.min.y,box.max.z},{box.min.x,box.max.y,box.max.z},{box.max.x,box.max.y,box.max.z}};for(int j=0;j<8;j++){Vector3 p=mr_v3_transform(corners[j],model.transform);if(first){result.min=result.max=p;first=false;}if(p.x<result.min.x)result.min.x=p.x;if(p.y<result.min.y)result.min.y=p.y;if(p.z<result.min.z)result.min.z=p.z;if(p.x>result.max.x)result.max.x=p.x;if(p.y>result.max.y)result.max.y=p.y;if(p.z>result.max.z)result.max.z=p.z;}}return result;}
void DrawBoundingBox(BoundingBox b,Color color){Vector3 size={b.max.x-b.min.x,b.max.y-b.min.y,b.max.z-b.min.z},center={(b.min.x+b.max.x)/2,(b.min.y+b.max.y)/2,(b.min.z+b.max.z)/2};DrawCubeWiresV(center,size,color);}
RayCollision GetRayCollisionMesh(Ray ray,Mesh mesh,Matrix transform){RayCollision closest={0};if(!mesh.vertices)return closest;for(int t=0;t<mesh.triangleCount;t++){int ids[3]={mesh.indices?mesh.indices[t*3]:t*3,mesh.indices?mesh.indices[t*3+1]:t*3+1,mesh.indices?mesh.indices[t*3+2]:t*3+2};if(ids[2]>=mesh.vertexCount)continue;Vector3 p[3];for(int j=0;j<3;j++)p[j]=mr_v3_transform((Vector3){mesh.vertices[ids[j]*3],mesh.vertices[ids[j]*3+1],mesh.vertices[ids[j]*3+2]},transform);RayCollision hit=GetRayCollisionTriangle(ray,p[0],p[1],p[2]);if(hit.hit&&(!closest.hit||hit.distance<closest.distance))closest=hit;}return closest;}
void GenMeshTangents(Mesh *mesh){
    if(!mesh||!mesh->vertices||!mesh->normals||!mesh->texcoords||mesh->vertexCount<=0||mesh->triangleCount<=0)return;
    Vector3 *tan1=MemAlloc((unsigned int)mesh->vertexCount*sizeof(Vector3)),*tan2=MemAlloc((unsigned int)mesh->vertexCount*sizeof(Vector3));
    float *tangents=MemAlloc((unsigned int)mesh->vertexCount*4*sizeof(float));
    if(!tan1||!tan2||!tangents){MemFree(tan1);MemFree(tan2);MemFree(tangents);return;}
    memset(tan1,0,(size_t)mesh->vertexCount*sizeof(Vector3));memset(tan2,0,(size_t)mesh->vertexCount*sizeof(Vector3));
    for(int triangle=0;triangle<mesh->triangleCount;triangle++){
        int id[3]={mesh->indices?mesh->indices[triangle*3]:triangle*3,mesh->indices?mesh->indices[triangle*3+1]:triangle*3+1,mesh->indices?mesh->indices[triangle*3+2]:triangle*3+2};
        if(id[0]<0||id[1]<0||id[2]<0||id[0]>=mesh->vertexCount||id[1]>=mesh->vertexCount||id[2]>=mesh->vertexCount)continue;
        Vector3 p[3];Vector2 uv[3];for(int i=0;i<3;i++){p[i]=(Vector3){mesh->vertices[id[i]*3],mesh->vertices[id[i]*3+1],mesh->vertices[id[i]*3+2]};uv[i]=(Vector2){mesh->texcoords[id[i]*2],mesh->texcoords[id[i]*2+1]};}
        Vector3 e1=mr_v3_sub(p[1],p[0]),e2=mr_v3_sub(p[2],p[0]);float s1=uv[1].x-uv[0].x,s2=uv[2].x-uv[0].x,t1=uv[1].y-uv[0].y,t2=uv[2].y-uv[0].y,den=s1*t2-s2*t1;if(den>-0.000001f&&den<0.000001f)continue;float inv=1/den;
        Vector3 sdir={(t2*e1.x-t1*e2.x)*inv,(t2*e1.y-t1*e2.y)*inv,(t2*e1.z-t1*e2.z)*inv},tdir={(s1*e2.x-s2*e1.x)*inv,(s1*e2.y-s2*e1.y)*inv,(s1*e2.z-s2*e1.z)*inv};
        for(int i=0;i<3;i++){tan1[id[i]]=mr_v3_add(tan1[id[i]],sdir);tan2[id[i]]=mr_v3_add(tan2[id[i]],tdir);}
    }
    for(int i=0;i<mesh->vertexCount;i++){Vector3 n={mesh->normals[i*3],mesh->normals[i*3+1],mesh->normals[i*3+2]},t=mr_v3_sub(tan1[i],mr_v3_scale(n,mr_v3_dot(n,tan1[i])));if(mr_v3_len(t)<0.0001f){Vector3 helper=(n.y>-0.9f&&n.y<0.9f)?(Vector3){0,1,0}:(Vector3){1,0,0};t=mr_v3_cross(helper,n);}t=mr_v3_norm(t);tangents[i*4]=t.x;tangents[i*4+1]=t.y;tangents[i*4+2]=t.z;tangents[i*4+3]=mr_v3_dot(mr_v3_cross(n,t),tan2[i])<0?-1:1;}
    MemFree(mesh->tangents);mesh->tangents=tangents;MemFree(tan1);MemFree(tan2);
}
static Mesh mr_mesh_allocate(int triangleCount){
    Mesh mesh={0};if(triangleCount<=0||triangleCount>1000000)return mesh;mesh.triangleCount=triangleCount;mesh.vertexCount=triangleCount*3;
    mesh.vertices=MemAlloc((unsigned int)mesh.vertexCount*3*sizeof(float));mesh.normals=MemAlloc((unsigned int)mesh.vertexCount*3*sizeof(float));mesh.texcoords=MemAlloc((unsigned int)mesh.vertexCount*2*sizeof(float));
    if(!mesh.vertices||!mesh.normals||!mesh.texcoords){UnloadMesh(mesh);return(Mesh){0};}return mesh;
}
static void mr_mesh_vertex(Mesh *mesh,int index,Vector3 position,Vector3 normal,Vector2 uv){mesh->vertices[index*3]=position.x;mesh->vertices[index*3+1]=position.y;mesh->vertices[index*3+2]=position.z;mesh->normals[index*3]=normal.x;mesh->normals[index*3+1]=normal.y;mesh->normals[index*3+2]=normal.z;mesh->texcoords[index*2]=uv.x;mesh->texcoords[index*2+1]=uv.y;}
static void mr_mesh_triangle(Mesh *mesh,int *vertex,Vector3 a,Vector3 b,Vector3 c,Vector3 normal,Vector2 ua,Vector2 ub,Vector2 uc){mr_mesh_vertex(mesh,(*vertex)++,a,normal,ua);mr_mesh_vertex(mesh,(*vertex)++,b,normal,ub);mr_mesh_vertex(mesh,(*vertex)++,c,normal,uc);}
Mesh GenMeshPoly(int sides,float radius){
    if(sides<3)sides=3;Mesh mesh=mr_mesh_allocate(sides);if(!mesh.vertices)return mesh;int vertex=0;
    for(int i=0;i<sides;i++){float a=2*MR_PI*i/sides,b=2*MR_PI*(i+1)/sides;mr_mesh_triangle(&mesh,&vertex,(Vector3){0,0,0},(Vector3){cosf(b)*radius,0,sinf(b)*radius},(Vector3){cosf(a)*radius,0,sinf(a)*radius},(Vector3){0,1,0},(Vector2){0.5f,0.5f},(Vector2){cosf(b)*0.5f+0.5f,sinf(b)*0.5f+0.5f},(Vector2){cosf(a)*0.5f+0.5f,sinf(a)*0.5f+0.5f});}
    UploadMesh(&mesh,false);return mesh;
}
Mesh GenMeshPlane(float width,float length,int resX,int resZ){if(resX<1)resX=1;if(resZ<1)resZ=1;Mesh mesh={0};mesh.triangleCount=resX*resZ*2;mesh.vertexCount=mesh.triangleCount*3;mesh.vertices=MemAlloc(mesh.vertexCount*3*sizeof(float));mesh.normals=MemAlloc(mesh.vertexCount*3*sizeof(float));mesh.texcoords=MemAlloc(mesh.vertexCount*2*sizeof(float));if(!mesh.vertices||!mesh.normals||!mesh.texcoords){UnloadMesh(mesh);return(Mesh){0};}int v=0;for(int z=0;z<resZ;z++)for(int x=0;x<resX;x++){float x0=-width/2+width*x/resX,x1=-width/2+width*(x+1)/resX,z0=-length/2+length*z/resZ,z1=-length/2+length*(z+1)/resZ;Vector3 p[6]={{x0,0,z0},{x1,0,z1},{x1,0,z0},{x0,0,z0},{x0,0,z1},{x1,0,z1}};Vector2 uv[6]={{(float)x/resX,(float)z/resZ},{(float)(x+1)/resX,(float)(z+1)/resZ},{(float)(x+1)/resX,(float)z/resZ},{(float)x/resX,(float)z/resZ},{(float)x/resX,(float)(z+1)/resZ},{(float)(x+1)/resX,(float)(z+1)/resZ}};for(int i=0;i<6;i++,v++){mesh.vertices[v*3]=p[i].x;mesh.vertices[v*3+1]=p[i].y;mesh.vertices[v*3+2]=p[i].z;mesh.normals[v*3+1]=1;mesh.texcoords[v*2]=uv[i].x;mesh.texcoords[v*2+1]=uv[i].y;}}UploadMesh(&mesh,false);return mesh;}
Mesh GenMeshCube(float width,float height,float length){Mesh mesh={0};mesh.triangleCount=12;mesh.vertexCount=36;mesh.vertices=MemAlloc(108*sizeof(float));mesh.normals=MemAlloc(108*sizeof(float));mesh.texcoords=MemAlloc(72*sizeof(float));if(!mesh.vertices||!mesh.normals||!mesh.texcoords){UnloadMesh(mesh);return(Mesh){0};}Vector3 p[8]={{-width/2,-height/2,-length/2},{width/2,-height/2,-length/2},{width/2,height/2,-length/2},{-width/2,height/2,-length/2},{-width/2,-height/2,length/2},{width/2,-height/2,length/2},{width/2,height/2,length/2},{-width/2,height/2,length/2}};int faces[6][4]={{0,3,2,1},{4,5,6,7},{0,1,5,4},{3,7,6,2},{0,4,7,3},{1,2,6,5}};Vector3 normals[6]={{0,0,-1},{0,0,1},{0,-1,0},{0,1,0},{-1,0,0},{1,0,0}};int v=0;int order[6]={0,1,2,0,2,3};Vector2 uv[4]={{0,1},{1,1},{1,0},{0,0}};for(int f=0;f<6;f++)for(int j=0;j<6;j++,v++){int q=order[j];Vector3 point=p[faces[f][q]];mesh.vertices[v*3]=point.x;mesh.vertices[v*3+1]=point.y;mesh.vertices[v*3+2]=point.z;mesh.normals[v*3]=normals[f].x;mesh.normals[v*3+1]=normals[f].y;mesh.normals[v*3+2]=normals[f].z;mesh.texcoords[v*2]=uv[q].x;mesh.texcoords[v*2+1]=uv[q].y;}UploadMesh(&mesh,false);return mesh;}
Mesh GenMeshSphere(float radius,int rings,int slices){if(rings<2)rings=2;if(slices<3)slices=3;Mesh mesh={0};mesh.triangleCount=rings*slices*2;mesh.vertexCount=mesh.triangleCount*3;mesh.vertices=MemAlloc(mesh.vertexCount*3*sizeof(float));mesh.normals=MemAlloc(mesh.vertexCount*3*sizeof(float));mesh.texcoords=MemAlloc(mesh.vertexCount*2*sizeof(float));if(!mesh.vertices||!mesh.normals||!mesh.texcoords){UnloadMesh(mesh);return(Mesh){0};}int v=0;for(int y=0;y<rings;y++)for(int x=0;x<slices;x++){float a0=-MR_PI/2+MR_PI*y/rings,a1=-MR_PI/2+MR_PI*(y+1)/rings,b0=2*MR_PI*x/slices,b1=2*MR_PI*(x+1)/slices;Vector3 p[6]={{cosf(a0)*cosf(b0),sinf(a0),cosf(a0)*sinf(b0)},{cosf(a0)*cosf(b1),sinf(a0),cosf(a0)*sinf(b1)},{cosf(a1)*cosf(b1),sinf(a1),cosf(a1)*sinf(b1)},{cosf(a0)*cosf(b0),sinf(a0),cosf(a0)*sinf(b0)},{cosf(a1)*cosf(b1),sinf(a1),cosf(a1)*sinf(b1)},{cosf(a1)*cosf(b0),sinf(a1),cosf(a1)*sinf(b0)}};Vector2 uv[6]={{(float)x/slices,(float)y/rings},{(float)(x+1)/slices,(float)y/rings},{(float)(x+1)/slices,(float)(y+1)/rings},{(float)x/slices,(float)y/rings},{(float)(x+1)/slices,(float)(y+1)/rings},{(float)x/slices,(float)(y+1)/rings}};for(int i=0;i<6;i++,v++){mesh.vertices[v*3]=p[i].x*radius;mesh.vertices[v*3+1]=p[i].y*radius;mesh.vertices[v*3+2]=p[i].z*radius;mesh.normals[v*3]=p[i].x;mesh.normals[v*3+1]=p[i].y;mesh.normals[v*3+2]=p[i].z;mesh.texcoords[v*2]=uv[i].x;mesh.texcoords[v*2+1]=uv[i].y;}}UploadMesh(&mesh,false);return mesh;}
Mesh GenMeshHemiSphere(float radius,int rings,int slices){
    if(rings<1)rings=1;if(slices<3)slices=3;Mesh mesh=mr_mesh_allocate(rings*slices*2);if(!mesh.vertices)return mesh;int vertex=0;
    for(int y=0;y<rings;y++)for(int x=0;x<slices;x++){float a0=MR_PI*0.5f*y/rings,a1=MR_PI*0.5f*(y+1)/rings,b0=2*MR_PI*x/slices,b1=2*MR_PI*(x+1)/slices;Vector3 n0={cosf(a0)*cosf(b0),sinf(a0),cosf(a0)*sinf(b0)},n1={cosf(a0)*cosf(b1),sinf(a0),cosf(a0)*sinf(b1)},n2={cosf(a1)*cosf(b1),sinf(a1),cosf(a1)*sinf(b1)},n3={cosf(a1)*cosf(b0),sinf(a1),cosf(a1)*sinf(b0)};Vector3 p[4]={mr_v3_scale(n0,radius),mr_v3_scale(n1,radius),mr_v3_scale(n2,radius),mr_v3_scale(n3,radius)};Vector2 uv0={(float)x/slices,(float)y/rings},uv1={(float)(x+1)/slices,(float)y/rings},uv2={(float)(x+1)/slices,(float)(y+1)/rings},uv3={(float)x/slices,(float)(y+1)/rings};mr_mesh_vertex(&mesh,vertex++,p[0],n0,uv0);mr_mesh_vertex(&mesh,vertex++,p[1],n1,uv1);mr_mesh_vertex(&mesh,vertex++,p[2],n2,uv2);mr_mesh_vertex(&mesh,vertex++,p[0],n0,uv0);mr_mesh_vertex(&mesh,vertex++,p[2],n2,uv2);mr_mesh_vertex(&mesh,vertex++,p[3],n3,uv3);}
    UploadMesh(&mesh,false);return mesh;
}
static Mesh mr_gen_mesh_cone(float bottomRadius,float topRadius,float height,int slices){
    if(slices<3)slices=3;int sideTriangles=topRadius>0?slices*2:slices,capTriangles=slices+(topRadius>0?slices:0);Mesh mesh=mr_mesh_allocate(sideTriangles+capTriangles);if(!mesh.vertices)return mesh;int vertex=0;float slope=height!=0?(bottomRadius-topRadius)/height:0;
    for(int i=0;i<slices;i++){float a=2*MR_PI*i/slices,b=2*MR_PI*(i+1)/slices;Vector3 p0={cosf(a)*bottomRadius,0,sinf(a)*bottomRadius},p1={cosf(b)*bottomRadius,0,sinf(b)*bottomRadius},q0={cosf(a)*topRadius,height,sinf(a)*topRadius},q1={cosf(b)*topRadius,height,sinf(b)*topRadius},n0=mr_v3_norm((Vector3){cosf(a),slope,sinf(a)}),n1=mr_v3_norm((Vector3){cosf(b),slope,sinf(b)});Vector2 uv0={(float)i/slices,1},uv1={(float)(i+1)/slices,1},uv2={(float)(i+1)/slices,0},uv3={(float)i/slices,0};if(topRadius>0){mr_mesh_vertex(&mesh,vertex++,p0,n0,uv0);mr_mesh_vertex(&mesh,vertex++,q1,n1,uv2);mr_mesh_vertex(&mesh,vertex++,p1,n1,uv1);mr_mesh_vertex(&mesh,vertex++,p0,n0,uv0);mr_mesh_vertex(&mesh,vertex++,q0,n0,uv3);mr_mesh_vertex(&mesh,vertex++,q1,n1,uv2);}else{mr_mesh_vertex(&mesh,vertex++,p0,n0,uv0);mr_mesh_vertex(&mesh,vertex++,q0,mr_v3_norm(mr_v3_add(n0,n1)),(Vector2){((float)i+0.5f)/slices,0});mr_mesh_vertex(&mesh,vertex++,p1,n1,uv1);}mr_mesh_triangle(&mesh,&vertex,(Vector3){0,0,0},p1,p0,(Vector3){0,-1,0},(Vector2){0.5f,0.5f},(Vector2){cosf(b)*0.5f+0.5f,sinf(b)*0.5f+0.5f},(Vector2){cosf(a)*0.5f+0.5f,sinf(a)*0.5f+0.5f});if(topRadius>0)mr_mesh_triangle(&mesh,&vertex,(Vector3){0,height,0},q0,q1,(Vector3){0,1,0},(Vector2){0.5f,0.5f},(Vector2){cosf(a)*0.5f+0.5f,sinf(a)*0.5f+0.5f},(Vector2){cosf(b)*0.5f+0.5f,sinf(b)*0.5f+0.5f});}
    UploadMesh(&mesh,false);return mesh;
}
Mesh GenMeshCylinder(float radius,float height,int slices){return mr_gen_mesh_cone(radius,radius,height,slices);}
Mesh GenMeshCone(float radius,float height,int slices){return mr_gen_mesh_cone(radius,0,height,slices);}
Mesh GenMeshTorus(float radius,float size,int radSeg,int sides){
    if(radSeg<3)radSeg=3;if(sides<3)sides=3;Mesh mesh=mr_mesh_allocate(radSeg*sides*2);if(!mesh.vertices)return mesh;int vertex=0;
    for(int r=0;r<radSeg;r++)for(int s=0;s<sides;s++){float a0=2*MR_PI*r/radSeg,a1=2*MR_PI*(r+1)/radSeg,b0=2*MR_PI*s/sides,b1=2*MR_PI*(s+1)/sides;Vector3 p[4],n[4];float aa[4]={a0,a1,a1,a0},bb[4]={b0,b0,b1,b1};for(int i=0;i<4;i++){n[i]=(Vector3){cosf(aa[i])*cosf(bb[i]),sinf(bb[i]),sinf(aa[i])*cosf(bb[i])};p[i]=(Vector3){cosf(aa[i])*(radius+size*cosf(bb[i])),size*sinf(bb[i]),sinf(aa[i])*(radius+size*cosf(bb[i]))};}Vector2 uv[4]={{(float)r/radSeg,(float)s/sides},{(float)(r+1)/radSeg,(float)s/sides},{(float)(r+1)/radSeg,(float)(s+1)/sides},{(float)r/radSeg,(float)(s+1)/sides}};int order[6]={0,2,1,0,3,2};for(int i=0;i<6;i++){int q=order[i];mr_mesh_vertex(&mesh,vertex++,p[q],n[q],uv[q]);}}
    UploadMesh(&mesh,false);return mesh;
}
static Vector3 mr_knot_point(float t,float radius){float r=radius/3.0f;return(Vector3){r*(2+cosf(3*t))*cosf(2*t),r*sinf(3*t),r*(2+cosf(3*t))*sinf(2*t)};}
Mesh GenMeshKnot(float radius,float size,int radSeg,int sides){
    if(radSeg<6)radSeg=6;if(sides<3)sides=3;Mesh mesh=mr_mesh_allocate(radSeg*sides*2);if(!mesh.vertices)return mesh;int vertex=0;
    for(int r=0;r<radSeg;r++){float t0=2*MR_PI*r/radSeg,t1=2*MR_PI*(r+1)/radSeg;Vector3 center[2]={mr_knot_point(t0,radius),mr_knot_point(t1,radius)},right[2],up[2];for(int e=0;e<2;e++){float t=e?t1:t0;Vector3 tangent=mr_v3_norm(mr_v3_sub(mr_knot_point(t+0.001f,radius),mr_knot_point(t-0.001f,radius))),helper=(tangent.y>-0.9f&&tangent.y<0.9f)?(Vector3){0,1,0}:(Vector3){1,0,0};right[e]=mr_v3_norm(mr_v3_cross(helper,tangent));up[e]=mr_v3_norm(mr_v3_cross(tangent,right[e]));}for(int s=0;s<sides;s++){float a=2*MR_PI*s/sides,b=2*MR_PI*(s+1)/sides;Vector3 n[4]={mr_v3_add(mr_v3_scale(right[0],cosf(a)),mr_v3_scale(up[0],sinf(a))),mr_v3_add(mr_v3_scale(right[1],cosf(a)),mr_v3_scale(up[1],sinf(a))),mr_v3_add(mr_v3_scale(right[1],cosf(b)),mr_v3_scale(up[1],sinf(b))),mr_v3_add(mr_v3_scale(right[0],cosf(b)),mr_v3_scale(up[0],sinf(b)))};Vector3 p[4]={mr_v3_add(center[0],mr_v3_scale(n[0],size)),mr_v3_add(center[1],mr_v3_scale(n[1],size)),mr_v3_add(center[1],mr_v3_scale(n[2],size)),mr_v3_add(center[0],mr_v3_scale(n[3],size))};Vector2 uv[4]={{(float)r/radSeg,(float)s/sides},{(float)(r+1)/radSeg,(float)s/sides},{(float)(r+1)/radSeg,(float)(s+1)/sides},{(float)r/radSeg,(float)(s+1)/sides}};int order[6]={0,2,1,0,3,2};for(int i=0;i<6;i++){int q=order[i];mr_mesh_vertex(&mesh,vertex++,p[q],n[q],uv[q]);}}}
    UploadMesh(&mesh,false);return mesh;
}
Mesh GenMeshHeightmap(Image heightmap,Vector3 size){
    if(!IsImageValid(heightmap)||heightmap.width<2||heightmap.height<2)return(Mesh){0};int cells=(heightmap.width-1)*(heightmap.height-1);Mesh mesh=mr_mesh_allocate(cells*2);if(!mesh.vertices)return mesh;int vertex=0;
    for(int z=0;z<heightmap.height-1;z++)for(int x=0;x<heightmap.width-1;x++){Vector3 p[4];Vector2 uv[4];int px[4]={x,x+1,x+1,x},pz[4]={z,z,z+1,z+1};for(int i=0;i<4;i++){Color c=GetImageColor(heightmap,px[i],pz[i]);float h=(c.r+c.g+c.b)/(3.0f*255.0f);p[i]=(Vector3){((float)px[i]/(heightmap.width-1)-0.5f)*size.x,h*size.y,((float)pz[i]/(heightmap.height-1)-0.5f)*size.z};uv[i]=(Vector2){(float)px[i]/(heightmap.width-1),(float)pz[i]/(heightmap.height-1)};}Vector3 n0=mr_v3_norm(mr_v3_cross(mr_v3_sub(p[2],p[0]),mr_v3_sub(p[1],p[0]))),n1=mr_v3_norm(mr_v3_cross(mr_v3_sub(p[3],p[0]),mr_v3_sub(p[2],p[0])));mr_mesh_triangle(&mesh,&vertex,p[0],p[2],p[1],n0,uv[0],uv[2],uv[1]);mr_mesh_triangle(&mesh,&vertex,p[0],p[3],p[2],n1,uv[0],uv[3],uv[2]);}
    UploadMesh(&mesh,false);return mesh;
}
Mesh GenMeshCubicmap(Image cubicmap,Vector3 cubeSize){
    if(!IsImageValid(cubicmap)||cubicmap.width<=0||cubicmap.height<=0)return(Mesh){0};int cubes=0;for(int z=0;z<cubicmap.height;z++)for(int x=0;x<cubicmap.width;x++){Color c=GetImageColor(cubicmap,x,z);if(c.r||c.g||c.b)cubes++;}Mesh mesh=mr_mesh_allocate(cubes*12);if(!mesh.vertices)return mesh;int vertex=0;int faces[6][4]={{0,3,2,1},{4,5,6,7},{0,1,5,4},{3,7,6,2},{0,4,7,3},{1,2,6,5}};Vector3 normals[6]={{0,0,-1},{0,0,1},{0,-1,0},{0,1,0},{-1,0,0},{1,0,0}};int order[6]={0,1,2,0,2,3};Vector2 uv[4]={{0,1},{1,1},{1,0},{0,0}};
    for(int z=0;z<cubicmap.height;z++)for(int x=0;x<cubicmap.width;x++){Color c=GetImageColor(cubicmap,x,z);if(!(c.r||c.g||c.b))continue;Vector3 center={((float)x-(cubicmap.width-1)*0.5f)*cubeSize.x,cubeSize.y*0.5f,((float)z-(cubicmap.height-1)*0.5f)*cubeSize.z},p[8]={{center.x-cubeSize.x/2,0,center.z-cubeSize.z/2},{center.x+cubeSize.x/2,0,center.z-cubeSize.z/2},{center.x+cubeSize.x/2,cubeSize.y,center.z-cubeSize.z/2},{center.x-cubeSize.x/2,cubeSize.y,center.z-cubeSize.z/2},{center.x-cubeSize.x/2,0,center.z+cubeSize.z/2},{center.x+cubeSize.x/2,0,center.z+cubeSize.z/2},{center.x+cubeSize.x/2,cubeSize.y,center.z+cubeSize.z/2},{center.x-cubeSize.x/2,cubeSize.y,center.z+cubeSize.z/2}};for(int f=0;f<6;f++)for(int i=0;i<6;i++){int q=order[i];mr_mesh_vertex(&mesh,vertex++,p[faces[f][q]],normals[f],uv[q]);}}
    UploadMesh(&mesh,false);return mesh;
}
typedef struct MRTextBuilder { char *data; int length,capacity; bool failed; } MRTextBuilder;
static bool mr_text_reserve(MRTextBuilder *builder,int extra){
    if(builder->failed||extra<0||builder->length>0x7fffffff-extra){builder->failed=true;return false;}
    int required=builder->length+extra+1;if(required<=builder->capacity)return true;int capacity=builder->capacity?builder->capacity:1024;
    while(capacity<required){if(capacity>0x3fffffff){capacity=required;break;}capacity*=2;}
    char *grown=MemRealloc(builder->data,(unsigned int)capacity);if(!grown){builder->failed=true;return false;}builder->data=grown;builder->capacity=capacity;return true;
}
static void mr_text_append_n(MRTextBuilder *builder,const char *text,int length){if(!text||length<=0||!mr_text_reserve(builder,length))return;memcpy(builder->data+builder->length,text,(size_t)length);builder->length+=length;builder->data[builder->length]=0;}
static void mr_text_append(MRTextBuilder *builder,const char *text){int length=0;if(!text)return;while(text[length])length++;mr_text_append_n(builder,text,length);}
static void mr_text_integer(MRTextBuilder *builder,int value){char digits[16];int count=0;unsigned int number;if(value<0){mr_text_append(builder,"-");number=(unsigned int)(-(value+1))+1;}else number=(unsigned int)value;do{digits[count++]=(char)('0'+number%10);number/=10;}while(number);while(count--)mr_text_append_n(builder,&digits[count],1);}
static void mr_text_float(MRTextBuilder *builder,float value){
    if(value<0){mr_text_append(builder,"-");value=-value;}if(value>2147483000.0f)value=2147483000.0f;
    int whole=(int)value;float fraction=value-whole;int decimals=(int)(fraction*1000000.0f+0.5f);if(decimals>=1000000){whole++;decimals=0;}mr_text_integer(builder,whole);
    if(decimals){char digits[6];for(int i=5;i>=0;i--){digits[i]=(char)('0'+decimals%10);decimals/=10;}int count=6;while(count>0&&digits[count-1]=='0')count--;mr_text_append(builder,".");mr_text_append_n(builder,digits,count);}else mr_text_append(builder,".0");
}
static bool mr_text_save(MRTextBuilder *builder,const char *fileName){bool result=!builder->failed&&builder->data&&SaveFileData(fileName,builder->data,builder->length);MemFree(builder->data);builder->data=NULL;return result;}
bool ExportMesh(Mesh mesh,const char *fileName){
    if(!fileName||!mesh.vertices||mesh.vertexCount<=0||mesh.triangleCount<=0||!IsFileExtension(fileName,".obj"))return false;MRTextBuilder out={0};mr_text_append(&out,"# Exported by SarGPU\n");
    for(int i=0;i<mesh.vertexCount;i++){mr_text_append(&out,"v ");for(int c=0;c<3;c++){if(c)mr_text_append(&out," ");mr_text_float(&out,mesh.vertices[i*3+c]);}mr_text_append(&out,"\n");}
    if(mesh.texcoords)for(int i=0;i<mesh.vertexCount;i++){mr_text_append(&out,"vt ");mr_text_float(&out,mesh.texcoords[i*2]);mr_text_append(&out," ");mr_text_float(&out,1.0f-mesh.texcoords[i*2+1]);mr_text_append(&out,"\n");}
    if(mesh.normals)for(int i=0;i<mesh.vertexCount;i++){mr_text_append(&out,"vn ");for(int c=0;c<3;c++){if(c)mr_text_append(&out," ");mr_text_float(&out,mesh.normals[i*3+c]);}mr_text_append(&out,"\n");}
    for(int triangle=0;triangle<mesh.triangleCount;triangle++){mr_text_append(&out,"f");for(int corner=0;corner<3;corner++){int index=(mesh.indices?mesh.indices[triangle*3+corner]:triangle*3+corner)+1;if(index<1||index>mesh.vertexCount){out.failed=true;break;}mr_text_append(&out," ");mr_text_integer(&out,index);if(mesh.texcoords){mr_text_append(&out,"/");mr_text_integer(&out,index);if(mesh.normals){mr_text_append(&out,"/");mr_text_integer(&out,index);}}else if(mesh.normals){mr_text_append(&out,"//");mr_text_integer(&out,index);}}mr_text_append(&out,"\n");}
    return mr_text_save(&out,fileName);
}
bool ExportMeshAsCode(Mesh mesh,const char *fileName){
    if(!fileName||!mesh.vertices||mesh.vertexCount<=0)return false;MRTextBuilder out={0};mr_text_append(&out,"/* Mesh exported by SarGPU */\n#pragma once\n\n#define MESH_VERTEX_COUNT ");mr_text_integer(&out,mesh.vertexCount);mr_text_append(&out,"\n#define MESH_TRIANGLE_COUNT ");mr_text_integer(&out,mesh.triangleCount);mr_text_append(&out,"\n\nstatic const float meshVertices[] = {");
    for(int i=0;i<mesh.vertexCount*3;i++){if(i)mr_text_append(&out,",");if(i%9==0)mr_text_append(&out,"\n    ");mr_text_float(&out,mesh.vertices[i]);mr_text_append(&out,"f");}mr_text_append(&out,"\n};\n");
    if(mesh.texcoords){mr_text_append(&out,"\nstatic const float meshTexcoords[] = {");for(int i=0;i<mesh.vertexCount*2;i++){if(i)mr_text_append(&out,",");if(i%8==0)mr_text_append(&out,"\n    ");mr_text_float(&out,mesh.texcoords[i]);mr_text_append(&out,"f");}mr_text_append(&out,"\n};\n");}
    if(mesh.normals){mr_text_append(&out,"\nstatic const float meshNormals[] = {");for(int i=0;i<mesh.vertexCount*3;i++){if(i)mr_text_append(&out,",");if(i%9==0)mr_text_append(&out,"\n    ");mr_text_float(&out,mesh.normals[i]);mr_text_append(&out,"f");}mr_text_append(&out,"\n};\n");}
    if(mesh.colors){mr_text_append(&out,"\nstatic const unsigned char meshColors[] = {");for(int i=0;i<mesh.vertexCount*4;i++){if(i)mr_text_append(&out,",");if(i%16==0)mr_text_append(&out,"\n    ");mr_text_integer(&out,mesh.colors[i]);}mr_text_append(&out,"\n};\n");}
    if(mesh.indices){mr_text_append(&out,"\nstatic const unsigned short meshIndices[] = {");for(int i=0;i<mesh.triangleCount*3;i++){if(i)mr_text_append(&out,",");if(i%12==0)mr_text_append(&out,"\n    ");mr_text_integer(&out,mesh.indices[i]);}mr_text_append(&out,"\n};\n");}
    return mr_text_save(&out,fileName);
}
typedef enum MRJsonType { MR_JSON_OBJECT,MR_JSON_ARRAY,MR_JSON_STRING,MR_JSON_VALUE } MRJsonType;
typedef struct MRJsonToken { int start,end,parent; MRJsonType type; } MRJsonToken;
static int mr_json_tokenize(const char *json,int length,MRJsonToken *tokens,int capacity){int count=0,parent=-1;for(int i=0;i<length;){char c=json[i];if(c=='{'||c=='['){if(count>=capacity)return-1;tokens[count]=(MRJsonToken){i,-1,parent,c=='{'?MR_JSON_OBJECT:MR_JSON_ARRAY};parent=count++;i++;}else if(c=='}'||c==']'){if(parent<0)return-1;tokens[parent].end=i+1;parent=tokens[parent].parent;i++;}else if(c=='\"'){int start=++i;while(i<length&&json[i]!='\"'){if(json[i]=='\\'&&i+1<length)i+=2;else i++;}if(i>=length||count>=capacity)return-1;tokens[count++]=(MRJsonToken){start,i,parent,MR_JSON_STRING};i++;}else if(c==' '||c=='\t'||c=='\r'||c=='\n'||c==':'||c==',')i++;else{int start=i;while(i<length&&json[i]!=','&&json[i]!=']'&&json[i]!='}'&&json[i]!=' '&&json[i]!='\t'&&json[i]!='\r'&&json[i]!='\n')i++;if(count>=capacity)return-1;tokens[count++]=(MRJsonToken){start,i,parent,MR_JSON_VALUE};}}return parent==-1?count:-1;}
static bool mr_json_equal(const char *json,MRJsonToken token,const char *text){int i=0;while(text[i]&&token.start+i<token.end&&json[token.start+i]==text[i])i++;return !text[i]&&token.start+i==token.end;}
static int mr_json_skip(MRJsonToken *tokens,int count,int index){int end=tokens[index].end,indexNext=index+1;while(indexNext<count&&tokens[indexNext].start<end)indexNext++;return indexNext;}
static int mr_json_get(const char *json,MRJsonToken *tokens,int count,int object,const char *key){if(object<0||tokens[object].type!=MR_JSON_OBJECT)return-1;for(int i=object+1;i<count&&tokens[i].start<tokens[object].end;i++){if(tokens[i].parent==object&&tokens[i].type==MR_JSON_STRING){int value=i+1;if(value<count&&mr_json_equal(json,tokens[i],key))return value;if(value<count)i=mr_json_skip(tokens,count,value)-1;}}return-1;}
static int mr_json_at(MRJsonToken *tokens,int count,int array,int wanted){if(array<0||tokens[array].type!=MR_JSON_ARRAY)return-1;int n=0;for(int i=array+1;i<count&&tokens[i].start<tokens[array].end;i=mr_json_skip(tokens,count,i))if(tokens[i].parent==array){if(n++==wanted)return i;}return-1;}
static int mr_json_count(MRJsonToken *tokens,int count,int array){int n=0;while(mr_json_at(tokens,count,array,n)>=0)n++;return n;}
static double mr_json_number(const char *json,MRJsonToken token){int i=token.start,sign=1,exponent=0,expSign=1;double value=0,fraction=0.1;if(i<token.end&&json[i]=='-'){sign=-1;i++;}while(i<token.end&&json[i]>='0'&&json[i]<='9')value=value*10+(json[i++]-'0');if(i<token.end&&json[i]=='.'){i++;while(i<token.end&&json[i]>='0'&&json[i]<='9'){value+=(json[i++]-'0')*fraction;fraction*=0.1;}}if(i<token.end&&(json[i]=='e'||json[i]=='E')){i++;if(i<token.end&&(json[i]=='-'||json[i]=='+')){if(json[i++]=='-')expSign=-1;}while(i<token.end&&json[i]>='0'&&json[i]<='9')exponent=exponent*10+(json[i++]-'0');double power=1;while(exponent--)power*=10;value=expSign<0?value/power:value*power;}return value*sign;}
static int mr_json_int(const char *json,MRJsonToken token){return(int)mr_json_number(json,token);}
static uint32_t mr_u32le(const unsigned char *p){return(uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
typedef struct MRGlbView { const unsigned char *data; int length,stride; } MRGlbView;
typedef struct MRGlbAccessor { const unsigned char *data; int count,stride,componentType,components; } MRGlbAccessor;
static int mr_gltf_components(const char *json,MRJsonToken token){if(mr_json_equal(json,token,"SCALAR"))return 1;if(mr_json_equal(json,token,"VEC2"))return 2;if(mr_json_equal(json,token,"VEC3"))return 3;if(mr_json_equal(json,token,"VEC4"))return 4;if(mr_json_equal(json,token,"MAT4"))return 16;return 0;}
static int mr_gltf_component_size(int type){return type==5120||type==5121?1:type==5122||type==5123?2:type==5125||type==5126?4:0;}
static unsigned int mr_gltf_index(const unsigned char *data,int componentType){if(componentType==5121)return data[0];if(componentType==5123)return(unsigned int)(data[0]|data[1]<<8);if(componentType==5125)return mr_u32le(data);return 0;}
/* glTF 2.0 loader. It deliberately follows raylib's practical contract:
 * triangle primitives, one skin, four joints per vertex, metallic/roughness
 * materials, external/embedded resources, and flattened node instances. */
typedef struct MRGltfBuffer { const unsigned char *data; unsigned char *owned; int length; } MRGltfBuffer;
typedef struct MRGltfDoc {
    unsigned char *file; const char *json; int jsonLength; MRJsonToken *tokens, *rootToken;
    int tokenCount,bufferCount; MRGltfBuffer *buffers; const unsigned char *glbBin; int glbBinLength;
    char directory[1024];
    int accessorCount,viewCount; unsigned char **accessorData,**viewData;
    bool failed;
    unsigned char *sceneNodes; int selectedSkin;
} MRGltfDoc;
typedef struct MRGltfAccessor { const unsigned char *data; int count,stride,componentType,components; bool normalized; } MRGltfAccessor;
#include "external/meshopt_decode.h"
static int mr_gltf_int(MRGltfDoc *doc,int object,const char *name,int fallback){int t=mr_json_get(doc->json,doc->tokens,doc->tokenCount,object,name);return t<0?fallback:mr_json_int(doc->json,doc->tokens[t]);}
static void mr_gltf_free_cache(MRGltfDoc *doc){if(doc->accessorData)for(int i=0;i<doc->accessorCount;i++)MemFree(doc->accessorData[i]);if(doc->viewData)for(int i=0;i<doc->viewCount;i++)MemFree(doc->viewData[i]);MemFree(doc->accessorData);MemFree(doc->viewData);MemFree(doc->sceneNodes);}

static int mr_token_text(const char *json,MRJsonToken token,char *out,int capacity){
    int n=0; if(!out||capacity<=0)return 0;
    for(int i=token.start;i<token.end&&n+1<capacity;i++){
        if(json[i]=='\\'&&i+1<token.end){char c=json[++i];out[n++]=(c=='/')?'/':(c=='\\')?'\\':(c=='n')?'\n':(c=='r')?'\r':(c=='t')?'\t':c;}
        else out[n++]=json[i];
    } out[n]=0; return n;
}
static void mr_join_path(char *out,int capacity,const char *directory,const char *name){
    int n=0;if(!out||capacity<=0)return;
    if(name&&((name[0]=='/'||name[0]=='\\')||(name[0]&&name[1]==':'))){while(name[n]&&n+1<capacity){out[n]=name[n];n++;}out[n]=0;return;}
    while(directory&&directory[n]&&n+1<capacity){out[n]=directory[n];n++;}
    if(n&&out[n-1]!='/'&&out[n-1]!='\\'&&n+1<capacity)out[n++]='/';
    for(int i=0;name&&name[i]&&n+1<capacity;i++)out[n++]=name[i];out[n]=0;
}
static int mr_b64_value(char c){return c>='A'&&c<='Z'?c-'A':c>='a'&&c<='z'?c-'a'+26:c>='0'&&c<='9'?c-'0'+52:c=='+'?62:c=='/'?63:-1;}
static bool mr_text_contains(const char *text,const char *part){if(!text||!part)return false;unsigned int textLength=TextLength(text),length=TextLength(part);if(!length)return true;if(length>textLength)return false;for(unsigned int i=0;i+length<=textLength;i++)if(memcmp(text+i,part,length)==0)return true;return false;}
static unsigned char *mr_decode_data_uri(const char *uri,int *size){
    if(size)*size=0;if(!uri)return NULL;const char *comma=uri;while(*comma&&*comma!=',')comma++;if(!*comma)return NULL;
    bool base64=false;for(const char *p=uri;p<comma;p++)if((p+7<=comma)&&memcmp(p,";base64",7)==0){base64=true;break;}if(!base64)return NULL;
    const char *p=comma+1;int length=(int)TextLength(p),capacity=length/4*3+3,n=0,bits=-8;unsigned int value=0;unsigned char *out=MemAlloc((unsigned int)capacity);if(!out)return NULL;
    for(;*p;p++){if(*p=='=')break;int digit=mr_b64_value(*p);if(digit<0)continue;value=(value<<6)|digit;bits+=6;if(bits>=0){out[n++]=(unsigned char)((value>>bits)&255);bits-=8;}}
    if(size)*size=n;return out;
}
static bool mr_gltf_open(const char *fileName,MRGltfDoc *doc){
    memset(doc,0,sizeof *doc);if(!fileName||!IsFileExtension(fileName,".gltf;.glb"))return false;
    const char *directory=GetDirectoryPath(fileName);int directoryLength=(int)TextLength(directory);if(directoryLength>=(int)sizeof(doc->directory))directoryLength=(int)sizeof(doc->directory)-1;memcpy(doc->directory,directory,(size_t)directoryLength);doc->directory[directoryLength]=0;
    int fileSize=0;doc->file=LoadFileData(fileName,&fileSize);if(!doc->file)return false;
    if(IsFileExtension(fileName,".glb")){
        if(fileSize<20||mr_u32le(doc->file)!=0x46546c67u||mr_u32le(doc->file+4)!=2)goto fail;
        for(int offset=12;offset+8<=fileSize;){int length=(int)mr_u32le(doc->file+offset);uint32_t type=mr_u32le(doc->file+offset+4);offset+=8;if(length<0||offset+length>fileSize)goto fail;if(type==0x4e4f534au){doc->json=(const char*)doc->file+offset;doc->jsonLength=length;}else if(type==0x004e4942u){doc->glbBin=doc->file+offset;doc->glbBinLength=length;}offset+=length;}
    }else{doc->json=(const char*)doc->file;doc->jsonLength=fileSize;}
    if(!doc->json||doc->jsonLength<=0)goto fail;
    int capacity=doc->jsonLength/2+128;doc->tokens=MemAlloc((unsigned int)capacity*sizeof(MRJsonToken));
    doc->tokenCount=doc->tokens?mr_json_tokenize(doc->json,doc->jsonLength,doc->tokens,capacity):-1;
    if(doc->tokenCount<=0||doc->tokens[0].type!=MR_JSON_OBJECT)goto fail;doc->rootToken=&doc->tokens[0];
    int required=mr_json_get(doc->json,doc->tokens,doc->tokenCount,0,"extensionsRequired");for(int i=0;i<mr_json_count(doc->tokens,doc->tokenCount,required);i++){int t=mr_json_at(doc->tokens,doc->tokenCount,required,i);if(t>=0&&mr_json_equal(doc->json,doc->tokens[t],"KHR_draco_mesh_compression")){puts("sargpu: required Draco mesh compression is not supported");goto fail;}}
    int buffers=mr_json_get(doc->json,doc->tokens,doc->tokenCount,0,"buffers");doc->bufferCount=mr_json_count(doc->tokens,doc->tokenCount,buffers);
    if(doc->bufferCount<1&&doc->glbBin)doc->bufferCount=1;
    doc->buffers=MemAlloc((unsigned int)doc->bufferCount*sizeof(MRGltfBuffer));if(doc->bufferCount&&!doc->buffers)goto fail;
    if(doc->buffers)memset(doc->buffers,0,(size_t)doc->bufferCount*sizeof(MRGltfBuffer));
    for(int i=0;i<doc->bufferCount;i++){
        int object=mr_json_at(doc->tokens,doc->tokenCount,buffers,i),uriToken=mr_json_get(doc->json,doc->tokens,doc->tokenCount,object,"uri");
        if(uriToken<0&&i==0&&doc->glbBin){doc->buffers[i].data=doc->glbBin;doc->buffers[i].length=doc->glbBinLength;continue;}
        if(uriToken<0)continue;int uriLength=doc->tokens[uriToken].end-doc->tokens[uriToken].start;char *uri=MemAlloc((unsigned)uriLength+1);if(!uri)goto fail;mr_token_text(doc->json,doc->tokens[uriToken],uri,uriLength+1);int size=0;unsigned char *data=NULL;
        if(uriLength>=5&&memcmp(uri,"data:",5)==0)data=mr_decode_data_uri(uri,&size);else{char path[2048];mr_join_path(path,sizeof path,doc->directory,uri);data=LoadFileData(path,&size);}MemFree(uri);
        doc->buffers[i]=(MRGltfBuffer){data,data,size};
    }
    doc->accessorCount=mr_json_count(doc->tokens,doc->tokenCount,mr_json_get(doc->json,doc->tokens,doc->tokenCount,0,"accessors"));
    doc->viewCount=mr_json_count(doc->tokens,doc->tokenCount,mr_json_get(doc->json,doc->tokens,doc->tokenCount,0,"bufferViews"));
    if(doc->accessorCount){doc->accessorData=MemAlloc((unsigned int)doc->accessorCount*sizeof(void*));if(!doc->accessorData)goto fail;memset(doc->accessorData,0,(size_t)doc->accessorCount*sizeof(void*));}
    if(doc->viewCount){doc->viewData=MemAlloc((unsigned int)doc->viewCount*sizeof(void*));if(!doc->viewData)goto fail;memset(doc->viewData,0,(size_t)doc->viewCount*sizeof(void*));}
    return true;
fail:
    mr_gltf_free_cache(doc);
    if(doc->buffers){for(int i=0;i<doc->bufferCount;i++)MemFree(doc->buffers[i].owned);MemFree(doc->buffers);}MemFree(doc->tokens);UnloadFileData(doc->file);memset(doc,0,sizeof *doc);return false;
}
static void mr_gltf_close(MRGltfDoc *doc){if(!doc)return;mr_gltf_free_cache(doc);if(doc->buffers)for(int i=0;i<doc->bufferCount;i++)MemFree(doc->buffers[i].owned);MemFree(doc->buffers);MemFree(doc->tokens);UnloadFileData(doc->file);memset(doc,0,sizeof *doc);}
static MRGlbView mr_gltf2_view(MRGltfDoc *doc,int viewIndex){
    MRGlbView result={0};int views=mr_json_get(doc->json,doc->tokens,doc->tokenCount,0,"bufferViews"),view=mr_json_at(doc->tokens,doc->tokenCount,views,viewIndex);if(view<0)return result;
    int extensions=mr_json_get(doc->json,doc->tokens,doc->tokenCount,view,"extensions"),compressed=mr_json_get(doc->json,doc->tokens,doc->tokenCount,extensions,"EXT_meshopt_compression");
    if(compressed>=0){
        int buffer=mr_gltf_int(doc,compressed,"buffer",-1),offset=mr_gltf_int(doc,compressed,"byteOffset",0),length=mr_gltf_int(doc,compressed,"byteLength",-1),stride=mr_gltf_int(doc,compressed,"byteStride",0),count=mr_gltf_int(doc,compressed,"count",0);
        int mode=mr_json_get(doc->json,doc->tokens,doc->tokenCount,compressed,"mode"),filter=mr_json_get(doc->json,doc->tokens,doc->tokenCount,compressed,"filter");
        bool attributes=mode>=0&&mr_json_equal(doc->json,doc->tokens[mode],"ATTRIBUTES"),triangles=mode>=0&&mr_json_equal(doc->json,doc->tokens[mode],"TRIANGLES"),sequence=mode>=0&&mr_json_equal(doc->json,doc->tokens[mode],"INDICES");
        if(buffer<0||buffer>=doc->bufferCount||!doc->buffers[buffer].data||offset<0||length<=0||offset>doc->buffers[buffer].length||length>doc->buffers[buffer].length-offset||count<=0||stride<=0||stride>256||(size_t)count>2147483647u/(unsigned)stride||mr_gltf_int(doc,view,"byteLength",-1)!=count*stride||mr_gltf_int(doc,view,"byteStride",stride)!=stride||(!attributes&&!triangles&&!sequence)||(attributes&&stride%4)||(!attributes&&(stride!=2&&stride!=4))||(triangles&&count%3))goto compressed_fail;
        if(!doc->viewData[viewIndex]){
            unsigned char *decoded=MemAlloc((unsigned int)count*stride);if(!decoded)goto compressed_fail;
            const unsigned char *source=doc->buffers[buffer].data+offset;
            int status=attributes?meshopt_decodeVertexBuffer(decoded,count,stride,source,length):triangles?meshopt_decodeIndexBuffer(decoded,count,stride,source,length):meshopt_decodeIndexSequence(decoded,count,stride,source,length);
            if(status){MemFree(decoded);goto compressed_fail;}
            if(filter>=0&&!mr_json_equal(doc->json,doc->tokens[filter],"NONE")){
                if(!attributes){MemFree(decoded);goto compressed_fail;}
                if(mr_json_equal(doc->json,doc->tokens[filter],"OCTAHEDRAL")&&(stride==4||stride==8)){if(stride==4)mr_meshopt_oct8((signed char*)decoded,count);else mr_meshopt_oct16((short*)decoded,count);}
                else if(mr_json_equal(doc->json,doc->tokens[filter],"QUATERNION")&&stride==8)decodeFilterQuat((short*)decoded,count);
                else if(mr_json_equal(doc->json,doc->tokens[filter],"EXPONENTIAL")&&stride%4==0)decodeFilterExp((unsigned int*)decoded,(size_t)count*stride/4);
                else{MemFree(decoded);goto compressed_fail;}
            }
            doc->viewData[viewIndex]=decoded;
        }
        return(MRGlbView){doc->viewData[viewIndex],count*stride,stride};
compressed_fail: doc->failed=true;return result;
    }
    int bufferToken=mr_json_get(doc->json,doc->tokens,doc->tokenCount,view,"buffer"),offsetToken=mr_json_get(doc->json,doc->tokens,doc->tokenCount,view,"byteOffset"),lengthToken=mr_json_get(doc->json,doc->tokens,doc->tokenCount,view,"byteLength"),strideToken=mr_json_get(doc->json,doc->tokens,doc->tokenCount,view,"byteStride");
    int buffer=bufferToken>=0?mr_json_int(doc->json,doc->tokens[bufferToken]):0,offset=offsetToken>=0?mr_json_int(doc->json,doc->tokens[offsetToken]):0,length=lengthToken>=0?mr_json_int(doc->json,doc->tokens[lengthToken]):0;
    if(buffer<0||buffer>=doc->bufferCount||!doc->buffers[buffer].data||offset<0||length<0||offset>doc->buffers[buffer].length||length>doc->buffers[buffer].length-offset)return result;
    result.data=doc->buffers[buffer].data+offset;result.length=length;result.stride=strideToken>=0?mr_json_int(doc->json,doc->tokens[strideToken]):0;return result;
}
static MRGltfAccessor mr_gltf2_accessor(MRGltfDoc *doc,int accessorIndex){
    MRGltfAccessor result={0};int accessors=mr_json_get(doc->json,doc->tokens,doc->tokenCount,0,"accessors"),accessor=mr_json_at(doc->tokens,doc->tokenCount,accessors,accessorIndex);if(accessor<0)return result;
    int viewToken=mr_json_get(doc->json,doc->tokens,doc->tokenCount,accessor,"bufferView"),countToken=mr_json_get(doc->json,doc->tokens,doc->tokenCount,accessor,"count"),componentToken=mr_json_get(doc->json,doc->tokens,doc->tokenCount,accessor,"componentType"),typeToken=mr_json_get(doc->json,doc->tokens,doc->tokenCount,accessor,"type"),offsetToken=mr_json_get(doc->json,doc->tokens,doc->tokenCount,accessor,"byteOffset"),normalizedToken=mr_json_get(doc->json,doc->tokens,doc->tokenCount,accessor,"normalized");
    if(countToken<0||componentToken<0||typeToken<0)return result;MRGlbView view=viewToken>=0?mr_gltf2_view(doc,mr_json_int(doc->json,doc->tokens[viewToken])):(MRGlbView){0};int offset=offsetToken>=0?mr_json_int(doc->json,doc->tokens[offsetToken]):0;
    result.count=mr_json_int(doc->json,doc->tokens[countToken]);result.componentType=mr_json_int(doc->json,doc->tokens[componentToken]);result.components=mr_gltf_components(doc->json,doc->tokens[typeToken]);result.normalized=normalizedToken>=0&&mr_json_equal(doc->json,doc->tokens[normalizedToken],"true");
    int packed=result.components*mr_gltf_component_size(result.componentType);result.stride=view.stride?view.stride:packed;
    if(offset<0||result.count<=0||packed<=0||result.stride<packed||(size_t)result.count>2147483647u/(unsigned)packed)goto accessor_fail;
    if(viewToken>=0&&(!view.data||offset>view.length||(size_t)(result.count-1)*result.stride+packed>(size_t)(view.length-offset)))goto accessor_fail;
    int sparse=mr_json_get(doc->json,doc->tokens,doc->tokenCount,accessor,"sparse");
    if(sparse<0&&viewToken>=0){result.data=view.data+offset;return result;}
    if(!doc->accessorData[accessorIndex]){
        unsigned char *dense=MemAlloc((unsigned int)result.count*packed);if(!dense)goto accessor_fail;
        if(viewToken>=0)for(int i=0;i<result.count;i++)memcpy(dense+(size_t)i*packed,view.data+offset+(size_t)i*result.stride,packed);else memset(dense,0,(size_t)result.count*packed);
        if(sparse>=0){
            int count=mr_gltf_int(doc,sparse,"count",-1),indices=mr_json_get(doc->json,doc->tokens,doc->tokenCount,sparse,"indices"),values=mr_json_get(doc->json,doc->tokens,doc->tokenCount,sparse,"values"),type=mr_gltf_int(doc,indices,"componentType",0),indexSize=mr_gltf_component_size(type),io=mr_gltf_int(doc,indices,"byteOffset",0),vo=mr_gltf_int(doc,values,"byteOffset",0);
            MRGlbView iv=mr_gltf2_view(doc,mr_gltf_int(doc,indices,"bufferView",-1)),vv=mr_gltf2_view(doc,mr_gltf_int(doc,values,"bufferView",-1));
            if(count<=0||count>result.count||(type!=5121&&type!=5123&&type!=5125)||!iv.data||!vv.data||io<0||vo<0||io>iv.length||vo>vv.length||(size_t)count*indexSize>(size_t)(iv.length-io)||(size_t)count*packed>(size_t)(vv.length-vo)){MemFree(dense);goto accessor_fail;}
            unsigned int previous=0;for(int i=0;i<count;i++){unsigned int index=mr_gltf_index(iv.data+io+(size_t)i*indexSize,type);if(index>=(unsigned)result.count||(i&&index<=previous)){MemFree(dense);goto accessor_fail;}previous=index;memcpy(dense+(size_t)index*packed,vv.data+vo+(size_t)i*packed,packed);}
        }
        doc->accessorData[accessorIndex]=dense;
    }
    result.data=doc->accessorData[accessorIndex];result.stride=packed;return result;
accessor_fail: doc->failed=true;return(MRGltfAccessor){0};
}
static float mr_gltf2_value(const unsigned char *p,int type,bool normalized){
    if(type==5126){float v;memcpy(&v,p,4);return v;}if(type==5121)return normalized?(float)p[0]/255.0f:(float)p[0];
    if(type==5120){int v=(signed char)p[0];return normalized?(v<-127?-1.0f:(float)v/127.0f):(float)v;}
    if(type==5123){unsigned int v=(unsigned int)p[0]|((unsigned int)p[1]<<8);return normalized?(float)v/65535.0f:(float)v;}
    if(type==5122){int v=(short)((unsigned int)p[0]|((unsigned int)p[1]<<8));return normalized?(v<-32767?-1.0f:(float)v/32767.0f):(float)v;}
    if(type==5125)return(float)mr_u32le(p);return 0;
}
static void mr_gltf2_read(MRGltfAccessor a,int index,float *out,int count){int size=mr_gltf_component_size(a.componentType);if(!a.data||index<0||index>=a.count)return;const unsigned char *p=a.data+index*a.stride;for(int i=0;i<count;i++)out[i]=i<a.components?mr_gltf2_value(p+i*size,a.componentType,a.normalized):0;}
static Matrix mr_gltf_trs(Vector3 t,Quaternion q,Vector3 s){
    float n=sqrtf(q.x*q.x+q.y*q.y+q.z*q.z+q.w*q.w);if(n<=0)q=(Quaternion){0,0,0,1};else{q.x/=n;q.y/=n;q.z/=n;q.w/=n;}float x=q.x,y=q.y,z=q.z,w=q.w;Matrix m=mr_matrix_identity();
    m.m0=(1-2*y*y-2*z*z)*s.x;m.m4=(2*x*y-2*z*w)*s.y;m.m8=(2*x*z+2*y*w)*s.z;
    m.m1=(2*x*y+2*z*w)*s.x;m.m5=(1-2*x*x-2*z*z)*s.y;m.m9=(2*y*z-2*x*w)*s.z;
    m.m2=(2*x*z-2*y*w)*s.x;m.m6=(2*y*z+2*x*w)*s.y;m.m10=(1-2*x*x-2*y*y)*s.z;m.m12=t.x;m.m13=t.y;m.m14=t.z;return m;
}
static Transform mr_gltf_node_transform(MRGltfDoc *doc,int node){
    Transform tr={{0,0,0},{0,0,0,1},{1,1,1}};if(node<0)return tr;int token;
    token=mr_json_get(doc->json,doc->tokens,doc->tokenCount,node,"translation");for(int i=0;i<3&&token>=0;i++){int v=mr_json_at(doc->tokens,doc->tokenCount,token,i);if(v>=0)((float*)&tr.translation)[i]=(float)mr_json_number(doc->json,doc->tokens[v]);}
    token=mr_json_get(doc->json,doc->tokens,doc->tokenCount,node,"rotation");for(int i=0;i<4&&token>=0;i++){int v=mr_json_at(doc->tokens,doc->tokenCount,token,i);if(v>=0)((float*)&tr.rotation)[i]=(float)mr_json_number(doc->json,doc->tokens[v]);}
    token=mr_json_get(doc->json,doc->tokens,doc->tokenCount,node,"scale");for(int i=0;i<3&&token>=0;i++){int v=mr_json_at(doc->tokens,doc->tokenCount,token,i);if(v>=0)((float*)&tr.scale)[i]=(float)mr_json_number(doc->json,doc->tokens[v]);}return tr;
}
static Matrix mr_gltf_node_local(MRGltfDoc *doc,int node){
    int matrix=mr_json_get(doc->json,doc->tokens,doc->tokenCount,node,"matrix");if(matrix>=0){float a[16]={0};for(int i=0;i<16;i++){int v=mr_json_at(doc->tokens,doc->tokenCount,matrix,i);if(v>=0)a[i]=(float)mr_json_number(doc->json,doc->tokens[v]);}Matrix m={0};m.m0=a[0];m.m1=a[1];m.m2=a[2];m.m3=a[3];m.m4=a[4];m.m5=a[5];m.m6=a[6];m.m7=a[7];m.m8=a[8];m.m9=a[9];m.m10=a[10];m.m11=a[11];m.m12=a[12];m.m13=a[13];m.m14=a[14];m.m15=a[15];return m;}Transform t=mr_gltf_node_transform(doc,node);return mr_gltf_trs(t.translation,t.rotation,t.scale);
}
static int mr_gltf_parent(MRGltfDoc *doc,int nodeIndex){int nodes=mr_json_get(doc->json,doc->tokens,doc->tokenCount,0,"nodes"),count=mr_json_count(doc->tokens,doc->tokenCount,nodes);for(int i=0;i<count;i++){int node=mr_json_at(doc->tokens,doc->tokenCount,nodes,i),children=mr_json_get(doc->json,doc->tokens,doc->tokenCount,node,"children"),n=mr_json_count(doc->tokens,doc->tokenCount,children);for(int j=0;j<n;j++){int child=mr_json_at(doc->tokens,doc->tokenCount,children,j);if(child>=0&&mr_json_int(doc->json,doc->tokens[child])==nodeIndex)return i;}}return-1;}
static Matrix mr_gltf_world(MRGltfDoc *doc,int nodeIndex){int nodes=mr_json_get(doc->json,doc->tokens,doc->tokenCount,0,"nodes"),node=mr_json_at(doc->tokens,doc->tokenCount,nodes,nodeIndex);Matrix world=mr_gltf_node_local(doc,node);for(int depth=0;depth<128;depth++){int parent=mr_gltf_parent(doc,nodeIndex);if(parent<0)break;nodeIndex=parent;node=mr_json_at(doc->tokens,doc->tokenCount,nodes,nodeIndex);world=mr_matrix_multiply(mr_gltf_node_local(doc,node),world);}return world;}
static bool mr_gltf_visit(MRGltfDoc *doc,int nodes,int count,int index,int depth){
    if(index<0||index>=count||depth>=128||doc->sceneNodes[index])return false;
    doc->sceneNodes[index]=1;int node=mr_json_at(doc->tokens,doc->tokenCount,nodes,index),children=mr_json_get(doc->json,doc->tokens,doc->tokenCount,node,"children"),n=mr_json_count(doc->tokens,doc->tokenCount,children);
    int mesh=mr_gltf_int(doc,node,"mesh",-1),meshes=mr_json_get(doc->json,doc->tokens,doc->tokenCount,0,"meshes");if(mesh>=mr_json_count(doc->tokens,doc->tokenCount,meshes))return false;
    int skin=mr_gltf_int(doc,node,"skin",-1),skins=mr_json_get(doc->json,doc->tokens,doc->tokenCount,0,"skins");if(skin>=0){if(skin>=mr_json_count(doc->tokens,doc->tokenCount,skins)||(doc->selectedSkin>=0&&doc->selectedSkin!=skin))return false;doc->selectedSkin=skin;}
    for(int i=0;i<n;i++){int child=mr_json_at(doc->tokens,doc->tokenCount,children,i);if(child<0||!mr_gltf_visit(doc,nodes,count,mr_json_int(doc->json,doc->tokens[child]),depth+1))return false;}
    doc->sceneNodes[index]=2;return true;
}
static bool mr_gltf_select_scene(MRGltfDoc *doc,int requested){
    int nodes=mr_json_get(doc->json,doc->tokens,doc->tokenCount,0,"nodes"),count=mr_json_count(doc->tokens,doc->tokenCount,nodes),scenes=mr_json_get(doc->json,doc->tokens,doc->tokenCount,0,"scenes"),sceneCount=mr_json_count(doc->tokens,doc->tokenCount,scenes);doc->selectedSkin=-1;
    if(count){doc->sceneNodes=MemAlloc((unsigned)count);if(!doc->sceneNodes)return false;memset(doc->sceneNodes,0,count);}
    if(sceneCount){int index=requested<0?mr_gltf_int(doc,0,"scene",0):requested;if(index<0||index>=sceneCount)return false;int scene=mr_json_at(doc->tokens,doc->tokenCount,scenes,index),roots=mr_json_get(doc->json,doc->tokens,doc->tokenCount,scene,"nodes"),n=mr_json_count(doc->tokens,doc->tokenCount,roots);for(int i=0;i<n;i++){int root=mr_json_at(doc->tokens,doc->tokenCount,roots,i);if(root<0||!mr_gltf_visit(doc,nodes,count,mr_json_int(doc->json,doc->tokens[root]),0))return false;}}
    else{if(requested>=0)return false;for(int i=0;i<count;i++)if(mr_gltf_parent(doc,i)<0&&!mr_gltf_visit(doc,nodes,count,i,0))return false;for(int i=0;i<count;i++)if(!doc->sceneNodes[i])return false;}
    return true;
}
static Vector3 mr_transform_direction(Vector3 v,Matrix m){return mr_v3_norm((Vector3){m.m0*v.x+m.m4*v.y+m.m8*v.z,m.m1*v.x+m.m5*v.y+m.m9*v.z,m.m2*v.x+m.m6*v.y+m.m10*v.z});}
static Vector3 mr_transform_normal(Vector3 v,Matrix m){float a=m.m0,b=m.m4,c=m.m8,d=m.m1,e=m.m5,f=m.m9,g=m.m2,h=m.m6,i=m.m10;return mr_v3_norm((Vector3){(e*i-f*h)*v.x+(f*g-d*i)*v.y+(d*h-e*g)*v.z,(c*h-b*i)*v.x+(a*i-c*g)*v.y+(b*g-a*h)*v.z,(b*f-c*e)*v.x+(c*d-a*f)*v.y+(a*e-b*d)*v.z});}
static Texture2D mr_gltf_image_texture(MRGltfDoc *doc,int imageIndex){
    int images=mr_json_get(doc->json,doc->tokens,doc->tokenCount,0,"images"),image=mr_json_at(doc->tokens,doc->tokenCount,images,imageIndex);if(image<0)return(Texture2D){0};int viewToken=mr_json_get(doc->json,doc->tokens,doc->tokenCount,image,"bufferView"),uriToken=mr_json_get(doc->json,doc->tokens,doc->tokenCount,image,"uri"),mimeToken=mr_json_get(doc->json,doc->tokens,doc->tokenCount,image,"mimeType");Image decoded={0};char mime[64]="",uri[2048]="";if(mimeToken>=0)mr_token_text(doc->json,doc->tokens[mimeToken],mime,sizeof mime);
    if(viewToken>=0){MRGlbView view=mr_gltf2_view(doc,mr_json_int(doc->json,doc->tokens[viewToken]));const char *ext=mr_text_contains(mime,"jpeg")?".jpg":mr_text_contains(mime,"gif")?".gif":".png";if(view.data)decoded=LoadImageFromMemory(ext,view.data,view.length);}
    else if(uriToken>=0){mr_token_text(doc->json,doc->tokens[uriToken],uri,sizeof uri);if(memcmp(uri,"data:",5)==0){int size=0;unsigned char *bytes=mr_decode_data_uri(uri,&size);const char *ext=mr_text_contains(uri,"image/jpeg")?".jpg":mr_text_contains(uri,"image/gif")?".gif":".png";if(bytes)decoded=LoadImageFromMemory(ext,bytes,size);MemFree(bytes);}else{char path[2048];mr_join_path(path,sizeof path,doc->directory,uri);decoded=LoadImage(path);}}
    Texture2D texture=LoadTextureFromImage(decoded);UnloadImage(decoded);return texture;
}
static Texture2D mr_gltf_info_texture(MRGltfDoc *doc,int info){int indexToken=mr_json_get(doc->json,doc->tokens,doc->tokenCount,info,"index");if(indexToken<0)return(Texture2D){0};int textures=mr_json_get(doc->json,doc->tokens,doc->tokenCount,0,"textures"),texture=mr_json_at(doc->tokens,doc->tokenCount,textures,mr_json_int(doc->json,doc->tokens[indexToken])),source=mr_json_get(doc->json,doc->tokens,doc->tokenCount,texture,"source");return source>=0?mr_gltf_image_texture(doc,mr_json_int(doc->json,doc->tokens[source])):(Texture2D){0};}
static void mr_gltf_material(MRGltfDoc *doc,int materialToken,Material *material){
    int pbr=mr_json_get(doc->json,doc->tokens,doc->tokenCount,materialToken,"pbrMetallicRoughness"),factor=mr_json_get(doc->json,doc->tokens,doc->tokenCount,pbr,"baseColorFactor");
    if(factor>=0)for(int i=0;i<4;i++){int v=mr_json_at(doc->tokens,doc->tokenCount,factor,i);if(v>=0){int c=(int)(mr_json_number(doc->json,doc->tokens[v])*255+0.5);if(c<0)c=0;if(c>255)c=255;((unsigned char*)&material->maps[MATERIAL_MAP_ALBEDO].color)[i]=(unsigned char)c;}}
    int info=mr_json_get(doc->json,doc->tokens,doc->tokenCount,pbr,"baseColorTexture");Texture2D texture=mr_gltf_info_texture(doc,info);if(IsTextureValid(texture))material->maps[MATERIAL_MAP_ALBEDO].texture=texture;
    int value=mr_json_get(doc->json,doc->tokens,doc->tokenCount,pbr,"metallicFactor");if(value>=0)material->maps[MATERIAL_MAP_METALNESS].value=(float)mr_json_number(doc->json,doc->tokens[value]);
    value=mr_json_get(doc->json,doc->tokens,doc->tokenCount,pbr,"roughnessFactor");if(value>=0)material->maps[MATERIAL_MAP_ROUGHNESS].value=(float)mr_json_number(doc->json,doc->tokens[value]);
    info=mr_json_get(doc->json,doc->tokens,doc->tokenCount,pbr,"metallicRoughnessTexture");texture=mr_gltf_info_texture(doc,info);if(IsTextureValid(texture))material->maps[MATERIAL_MAP_ROUGHNESS].texture=texture;
    const char *keys[3]={"normalTexture","occlusionTexture","emissiveTexture"};int maps[3]={MATERIAL_MAP_NORMAL,MATERIAL_MAP_OCCLUSION,MATERIAL_MAP_EMISSION};for(int i=0;i<3;i++){info=mr_json_get(doc->json,doc->tokens,doc->tokenCount,materialToken,keys[i]);texture=mr_gltf_info_texture(doc,info);if(IsTextureValid(texture))material->maps[maps[i]].texture=texture;}
    factor=mr_json_get(doc->json,doc->tokens,doc->tokenCount,materialToken,"emissiveFactor");if(factor>=0)for(int i=0;i<3;i++){int v=mr_json_at(doc->tokens,doc->tokenCount,factor,i);if(v>=0){int c=(int)(mr_json_number(doc->json,doc->tokens[v])*255+0.5);if(c<0)c=0;if(c>255)c=255;((unsigned char*)&material->maps[MATERIAL_MAP_EMISSION].color)[i]=(unsigned char)c;}}material->maps[MATERIAL_MAP_EMISSION].color.a=255;
}
static Transform mr_matrix_decompose(Matrix m){
    Transform t={{m.m12,m.m13,m.m14},{0,0,0,1},{sqrtf(m.m0*m.m0+m.m1*m.m1+m.m2*m.m2),sqrtf(m.m4*m.m4+m.m5*m.m5+m.m6*m.m6),sqrtf(m.m8*m.m8+m.m9*m.m9+m.m10*m.m10)}};
    if(t.scale.x==0||t.scale.y==0||t.scale.z==0)return t;float a00=m.m0/t.scale.x,a01=m.m4/t.scale.y,a02=m.m8/t.scale.z,a10=m.m1/t.scale.x,a11=m.m5/t.scale.y,a12=m.m9/t.scale.z,a20=m.m2/t.scale.x,a21=m.m6/t.scale.y,a22=m.m10/t.scale.z,trace=a00+a11+a22;
    if(trace>0){float s=sqrtf(trace+1)*2;t.rotation.w=0.25f*s;t.rotation.x=(a21-a12)/s;t.rotation.y=(a02-a20)/s;t.rotation.z=(a10-a01)/s;}
    else if(a00>a11&&a00>a22){float s=sqrtf(1+a00-a11-a22)*2;t.rotation.w=(a21-a12)/s;t.rotation.x=0.25f*s;t.rotation.y=(a01+a10)/s;t.rotation.z=(a02+a20)/s;}
    else if(a11>a22){float s=sqrtf(1+a11-a00-a22)*2;t.rotation.w=(a02-a20)/s;t.rotation.x=(a01+a10)/s;t.rotation.y=0.25f*s;t.rotation.z=(a12+a21)/s;}
    else{float s=sqrtf(1+a22-a00-a11)*2;t.rotation.w=(a10-a01)/s;t.rotation.x=(a02+a20)/s;t.rotation.y=(a12+a21)/s;t.rotation.z=0.25f*s;}return t;
}
static int mr_gltf_attribute(MRGltfDoc *doc,int attributes,const char *name){int token=mr_json_get(doc->json,doc->tokens,doc->tokenCount,attributes,name);return token>=0?mr_json_int(doc->json,doc->tokens[token]):-1;}
typedef struct MRMorphChannel { int animation,node,count,targets,interpolation; float *times,*values; } MRMorphChannel;
struct ModelMorphData { int count; MRMorphChannel *channels; };
static void mr_free_morph_data(ModelMorphData *data){if(!data)return;for(int i=0;i<data->count;i++){MemFree(data->channels[i].times);MemFree(data->channels[i].values);}MemFree(data->channels);MemFree(data);}
static bool mr_gltf_morph_animations(MRGltfDoc *doc,Model *model){
    int animations=mr_json_get(doc->json,doc->tokens,doc->tokenCount,0,"animations"),count=mr_json_count(doc->tokens,doc->tokenCount,animations),total=0;
    for(int a=0;a<count;a++){int anim=mr_json_at(doc->tokens,doc->tokenCount,animations,a),channels=mr_json_get(doc->json,doc->tokens,doc->tokenCount,anim,"channels");total+=mr_json_count(doc->tokens,doc->tokenCount,channels);}
    if(!total)return true;ModelMorphData *data=MemAlloc(sizeof *data);if(!data)return false;*data=(ModelMorphData){0};data->channels=MemAlloc((unsigned)total*sizeof(MRMorphChannel));if(!data->channels){MemFree(data);return false;}memset(data->channels,0,(size_t)total*sizeof(MRMorphChannel));model->morphData=data;
    for(int a=0;a<count;a++){
        int anim=mr_json_at(doc->tokens,doc->tokenCount,animations,a),channels=mr_json_get(doc->json,doc->tokens,doc->tokenCount,anim,"channels"),n=mr_json_count(doc->tokens,doc->tokenCount,channels),samplers=mr_json_get(doc->json,doc->tokens,doc->tokenCount,anim,"samplers");
        for(int c=0;c<n;c++){
            int channel=mr_json_at(doc->tokens,doc->tokenCount,channels,c),target=mr_json_get(doc->json,doc->tokens,doc->tokenCount,channel,"target"),path=mr_json_get(doc->json,doc->tokens,doc->tokenCount,target,"path");if(path<0||!mr_json_equal(doc->json,doc->tokens[path],"weights"))continue;
            int node=mr_gltf_int(doc,target,"node",-1),targets=0;for(int m=0;m<model->meshCount;m++)if(model->meshes[m].sourceNode==node&&model->meshes[m].morphTargetCount){if(targets&&targets!=model->meshes[m].morphTargetCount)return false;targets=model->meshes[m].morphTargetCount;}if(!targets)continue;
            int sampler=mr_json_at(doc->tokens,doc->tokenCount,samplers,mr_gltf_int(doc,channel,"sampler",-1)),interpolation=mr_json_get(doc->json,doc->tokens,doc->tokenCount,sampler,"interpolation"),kind=0;
            if(interpolation>=0){if(mr_json_equal(doc->json,doc->tokens[interpolation],"STEP"))kind=1;else if(mr_json_equal(doc->json,doc->tokens[interpolation],"CUBICSPLINE"))kind=2;else if(!mr_json_equal(doc->json,doc->tokens[interpolation],"LINEAR"))return false;}
            MRGltfAccessor input=mr_gltf2_accessor(doc,mr_gltf_int(doc,sampler,"input",-1)),output=mr_gltf2_accessor(doc,mr_gltf_int(doc,sampler,"output",-1));size_t values=(size_t)input.count*targets*(kind==2?3:1);
            if(!input.data||!output.data||input.componentType!=5126||input.components!=1||output.componentType!=5126||output.components!=1||input.count<=0||values>2147483647u/sizeof(float)||values!=(size_t)output.count)return false;
            MRMorphChannel *ch=&data->channels[data->count++];*ch=(MRMorphChannel){a,node,input.count,targets,kind,NULL,NULL};ch->times=MemAlloc((unsigned)input.count*sizeof(float));ch->values=MemAlloc((unsigned)values*sizeof(float));if(!ch->times||!ch->values)return false;
            for(int i=0;i<input.count;i++){mr_gltf2_read(input,i,&ch->times[i],1);if(!__builtin_isfinite(ch->times[i])||ch->times[i]<0||(i&&ch->times[i]<=ch->times[i-1]))return false;}
            for(size_t i=0;i<values;i++){mr_gltf2_read(output,(int)i,&ch->values[i],1);if(!__builtin_isfinite(ch->values[i]))return false;}
        }
    }
    return !doc->failed;
}
void UpdateModelMorphAnimation(Model model,int animationIndex,float time){
    if(!model.morphData||!__builtin_isfinite(time))return;
    for(int c=0;c<model.morphData->count;c++){
        MRMorphChannel *ch=&model.morphData->channels[c];if(ch->animation!=animationIndex)continue;int key=0;while(key+1<ch->count&&time>=ch->times[key+1])key++;
        float w[256]={0};int next=key+1<ch->count?key+1:key;float dt=ch->times[next]-ch->times[key],f=dt>0?(time-ch->times[key])/dt:0;if(f<0)f=0;if(f>1)f=1;
        for(int t=0;t<ch->targets;t++){
            if(ch->interpolation==2){float a=ch->values[(key*3+1)*ch->targets+t],b=ch->values[(next*3+1)*ch->targets+t],ta=ch->values[(key*3+2)*ch->targets+t],tb=ch->values[next*3*ch->targets+t],f2=f*f,f3=f2*f;w[t]=(2*f3-3*f2+1)*a+(f3-2*f2+f)*dt*ta+(-2*f3+3*f2)*b+(f3-f2)*dt*tb;}
            else{float a=ch->values[key*ch->targets+t],b=ch->values[next*ch->targets+t];w[t]=ch->interpolation==1?a:a+(b-a)*f;}
        }
        for(int m=0;m<model.meshCount;m++)if(model.meshes[m].sourceNode==ch->node)SetMeshMorphWeights(model.meshes[m],w,ch->targets);
    }
}
static Vector3 mr_gltf_linear(Vector3 v,Matrix m){return(Vector3){m.m0*v.x+m.m4*v.y+m.m8*v.z,m.m1*v.x+m.m5*v.y+m.m9*v.z,m.m2*v.x+m.m6*v.y+m.m10*v.z};}
static Vector3 mr_gltf_normal_delta(Vector3 v,Matrix m){float a=m.m0,b=m.m4,c=m.m8,d=m.m1,e=m.m5,f=m.m9,g=m.m2,h=m.m6,i=m.m10;return(Vector3){(e*i-f*h)*v.x+(f*g-d*i)*v.y+(d*h-e*g)*v.z,(c*h-b*i)*v.x+(a*i-c*g)*v.y+(b*g-a*h)*v.z,(b*f-c*e)*v.x+(c*d-a*f)*v.y+(a*e-b*d)*v.z};}
void SetMeshMorphWeights(Mesh mesh,const float *weights,int count){
    if(!weights||count<=0||count!=mesh.morphTargetCount||!mesh.morphBaseVertices)return;
    for(int t=0;t<count;t++)if(!__builtin_isfinite(weights[t]))return;
    if(mesh.morphWeights!=weights)memcpy(mesh.morphWeights,weights,(size_t)count*sizeof(float));
    for(int i=0;i<mesh.vertexCount;i++){
        Vector3 p=*(Vector3*)(mesh.morphBaseVertices+i*3),n=mesh.morphBaseNormals?*(Vector3*)(mesh.morphBaseNormals+i*3):(Vector3){0},tan=mesh.morphBaseTangents?*(Vector3*)(mesh.morphBaseTangents+i*4):(Vector3){0};
        for(int t=0;t<count;t++){MeshMorphTarget target=mesh.morphTargets[t];float w=weights[t];if(target.vertices)p=mr_v3_add(p,mr_v3_scale(*(Vector3*)(target.vertices+i*3),w));if(target.normals)n=mr_v3_add(n,mr_v3_scale(*(Vector3*)(target.normals+i*3),w));if(target.tangents)tan=mr_v3_add(tan,mr_v3_scale(*(Vector3*)(target.tangents+i*3),w));}
        memcpy(mesh.vertices+i*3,&p,sizeof p);if(mesh.normals){n=mr_v3_norm(n);memcpy(mesh.normals+i*3,&n,sizeof n);}if(mesh.tangents){tan=mr_v3_norm(tan);memcpy(mesh.tangents+i*4,&tan,sizeof tan);}
    }
    if(mesh.animVertices)memcpy(mesh.animVertices,mesh.vertices,(size_t)mesh.vertexCount*3*sizeof(float));if(mesh.animNormals&&mesh.normals)memcpy(mesh.animNormals,mesh.normals,(size_t)mesh.vertexCount*3*sizeof(float));mr_update_mesh_vertices(mesh);
}
static bool mr_gltf_load_morphs(MRGltfDoc *doc,int primitive,int sourceMesh,int node,Matrix world,Mesh *mesh,MRGltfAccessor normals,MRGltfAccessor tangents){
    int targets=mr_json_get(doc->json,doc->tokens,doc->tokenCount,primitive,"targets"),count=mr_json_count(doc->tokens,doc->tokenCount,targets);mesh->sourceNode=node<0?-1:node;
    if(!count)return true;if(count>256)return false;mesh->morphTargetCount=count;
    mesh->morphTargets=MemAlloc((unsigned)count*sizeof(MeshMorphTarget));if(!mesh->morphTargets)return false;memset(mesh->morphTargets,0,(size_t)count*sizeof(MeshMorphTarget));
    mesh->morphWeights=MemAlloc((unsigned)count*sizeof(float));mesh->morphBaseVertices=MemAlloc((unsigned)mesh->vertexCount*3*sizeof(float));if(!mesh->morphWeights||!mesh->morphBaseVertices)return false;
    memcpy(mesh->morphBaseVertices,mesh->vertices,(size_t)mesh->vertexCount*3*sizeof(float));
    if(mesh->normals){mesh->morphBaseNormals=MemAlloc((unsigned)mesh->vertexCount*3*sizeof(float));if(!mesh->morphBaseNormals)return false;for(int i=0;i<mesh->vertexCount;i++){float v[3]={0};mr_gltf2_read(normals,i,v,3);Vector3 n=mr_gltf_normal_delta(*(Vector3*)v,world);memcpy(mesh->morphBaseNormals+i*3,&n,sizeof n);}}
    if(mesh->tangents){mesh->morphBaseTangents=MemAlloc((unsigned)mesh->vertexCount*4*sizeof(float));if(!mesh->morphBaseTangents)return false;for(int i=0;i<mesh->vertexCount;i++){float v[4]={0};mr_gltf2_read(tangents,i,v,4);Vector3 t=mr_gltf_linear(*(Vector3*)v,world);memcpy(mesh->morphBaseTangents+i*4,&t,sizeof t);mesh->morphBaseTangents[i*4+3]=v[3];}}
    int weights=mr_json_get(doc->json,doc->tokens,doc->tokenCount,sourceMesh,"weights"),nodes=mr_json_get(doc->json,doc->tokens,doc->tokenCount,0,"nodes"),nt=mr_json_at(doc->tokens,doc->tokenCount,nodes,node),nw=mr_json_get(doc->json,doc->tokens,doc->tokenCount,nt,"weights");if(nw>=0)weights=nw;
    if(weights>=0&&mr_json_count(doc->tokens,doc->tokenCount,weights)!=count)return false;
    for(int t=0;t<count;t++){
        int w=mr_json_at(doc->tokens,doc->tokenCount,weights,t);mesh->morphWeights[t]=w>=0?(float)mr_json_number(doc->json,doc->tokens[w]):0;if(!__builtin_isfinite(mesh->morphWeights[t]))return false;
        int target=mr_json_at(doc->tokens,doc->tokenCount,targets,t);const char *keys[3]={"POSITION","NORMAL","TANGENT"};float **dest[3]={&mesh->morphTargets[t].vertices,&mesh->morphTargets[t].normals,&mesh->morphTargets[t].tangents};
        for(int k=0;k<3;k++){int index=mr_gltf_attribute(doc,target,keys[k]);if(index<0)continue;MRGltfAccessor a=mr_gltf2_accessor(doc,index);if(!a.data||a.count!=mesh->vertexCount||a.components!=3||a.componentType!=5126||(k==1&&!mesh->normals)||(k==2&&!mesh->tangents))return false;
            *dest[k]=MemAlloc((unsigned)mesh->vertexCount*3*sizeof(float));if(!*dest[k])return false;for(int i=0;i<mesh->vertexCount;i++){float v[3]={0};mr_gltf2_read(a,i,v,3);Vector3 d=k==1?mr_gltf_normal_delta(*(Vector3*)v,world):mr_gltf_linear(*(Vector3*)v,world);memcpy(*dest[k]+i*3,&d,sizeof d);}
        }
    }
    SetMeshMorphWeights(*mesh,mesh->morphWeights,count);return !doc->failed;
}
static bool mr_gltf_load_primitive(MRGltfDoc *doc,int primitive,int sourceMesh,int node,Matrix world,Mesh *mesh,int *materialId){
    int mode=mr_json_get(doc->json,doc->tokens,doc->tokenCount,primitive,"mode");if(mode>=0&&mr_json_int(doc->json,doc->tokens[mode])!=4)return false;int attributes=mr_json_get(doc->json,doc->tokens,doc->tokenCount,primitive,"attributes");
    int posIndex=mr_gltf_attribute(doc,attributes,"POSITION"),normalIndex=mr_gltf_attribute(doc,attributes,"NORMAL"),uvIndex=mr_gltf_attribute(doc,attributes,"TEXCOORD_0"),uv2Index=mr_gltf_attribute(doc,attributes,"TEXCOORD_1"),tangentIndex=mr_gltf_attribute(doc,attributes,"TANGENT"),colorIndex=mr_gltf_attribute(doc,attributes,"COLOR_0"),jointsIndex=mr_gltf_attribute(doc,attributes,"JOINTS_0"),weightsIndex=mr_gltf_attribute(doc,attributes,"WEIGHTS_0");if(posIndex<0)return false;
    MRGltfAccessor pos=mr_gltf2_accessor(doc,posIndex),normal=mr_gltf2_accessor(doc,normalIndex),uv=mr_gltf2_accessor(doc,uvIndex),uv2=mr_gltf2_accessor(doc,uv2Index),tangent=mr_gltf2_accessor(doc,tangentIndex),color=mr_gltf2_accessor(doc,colorIndex),joints=mr_gltf2_accessor(doc,jointsIndex),weights=mr_gltf2_accessor(doc,weightsIndex);if(!pos.data||(pos.componentType!=5126&&pos.componentType!=5120&&pos.componentType!=5121&&pos.componentType!=5122&&pos.componentType!=5123)||pos.components!=3)return false;
    int indexToken=mr_json_get(doc->json,doc->tokens,doc->tokenCount,primitive,"indices");MRGltfAccessor indices=indexToken>=0?mr_gltf2_accessor(doc,mr_json_int(doc->json,doc->tokens[indexToken])):(MRGltfAccessor){0};int elementCount=indices.data?indices.count:pos.count;if(elementCount<3)return false;
    mesh->vertexCount=pos.count;mesh->triangleCount=elementCount/3;mesh->vertices=MemAlloc((unsigned int)mesh->vertexCount*3*sizeof(float));
    if(normal.data&&normal.components>=3)mesh->normals=MemAlloc((unsigned int)mesh->vertexCount*3*sizeof(float));if(uv.data&&uv.components>=2)mesh->texcoords=MemAlloc((unsigned int)mesh->vertexCount*2*sizeof(float));if(uv2.data&&uv2.components>=2)mesh->texcoords2=MemAlloc((unsigned int)mesh->vertexCount*2*sizeof(float));if(tangent.data&&tangent.components>=4)mesh->tangents=MemAlloc((unsigned int)mesh->vertexCount*4*sizeof(float));if(color.data&&(color.components==3||color.components==4))mesh->colors=MemAlloc((unsigned int)mesh->vertexCount*4);if(joints.data&&joints.components==4)mesh->boneIds=MemAlloc((unsigned int)mesh->vertexCount*4);if(weights.data&&weights.components==4)mesh->boneWeights=MemAlloc((unsigned int)mesh->vertexCount*4*sizeof(float));if(indices.data)mesh->indices=MemAlloc((unsigned int)mesh->triangleCount*3*sizeof(unsigned short));
    if(!mesh->vertices||(normal.data&&!mesh->normals)||(uv.data&&!mesh->texcoords)||(indices.data&&!mesh->indices)){UnloadMesh(*mesh);*mesh=(Mesh){0};return false;}
    for(int i=0;i<mesh->vertexCount;i++){float v[4]={0,0,0,1};mr_gltf2_read(pos,i,v,3);Vector3 p=mr_v3_transform((Vector3){v[0],v[1],v[2]},world);mesh->vertices[i*3]=p.x;mesh->vertices[i*3+1]=p.y;mesh->vertices[i*3+2]=p.z;
        if(mesh->normals){mr_gltf2_read(normal,i,v,3);Vector3 n=mr_transform_normal((Vector3){v[0],v[1],v[2]},world);mesh->normals[i*3]=n.x;mesh->normals[i*3+1]=n.y;mesh->normals[i*3+2]=n.z;}
        if(mesh->texcoords){mr_gltf2_read(uv,i,v,2);mesh->texcoords[i*2]=v[0];mesh->texcoords[i*2+1]=v[1];}if(mesh->texcoords2){mr_gltf2_read(uv2,i,v,2);mesh->texcoords2[i*2]=v[0];mesh->texcoords2[i*2+1]=v[1];}
        if(mesh->tangents){mr_gltf2_read(tangent,i,v,4);Vector3 n=mr_transform_direction((Vector3){v[0],v[1],v[2]},world);mesh->tangents[i*4]=n.x;mesh->tangents[i*4+1]=n.y;mesh->tangents[i*4+2]=n.z;mesh->tangents[i*4+3]=v[3];}
        if(mesh->colors){mr_gltf2_read(color,i,v,color.components);for(int c=0;c<4;c++){float x=c<color.components?v[c]:1;if(!color.normalized&&color.componentType!=5126)x/=color.componentType==5121?255.0f:65535.0f;if(x<0)x=0;if(x>1)x=1;mesh->colors[i*4+c]=(unsigned char)(x*255+0.5f);}}
        if(mesh->boneIds){mr_gltf2_read(joints,i,v,4);for(int c=0;c<4;c++){int id=(int)v[c];mesh->boneIds[i*4+c]=(unsigned char)(id>255?255:id<0?0:id);}}if(mesh->boneWeights){mr_gltf2_read(weights,i,v,4);for(int c=0;c<4;c++)mesh->boneWeights[i*4+c]=v[c];}
    }
    if(mesh->indices)for(int i=0;i<mesh->triangleCount*3;i++){unsigned int index=mr_gltf_index(indices.data+i*indices.stride,indices.componentType);if(index>65535||index>=(unsigned int)mesh->vertexCount){UnloadMesh(*mesh);*mesh=(Mesh){0};return false;}mesh->indices[i]=(unsigned short)index;}
    if(mesh->boneIds&&mesh->boneWeights){mesh->animVertices=MemAlloc((unsigned int)mesh->vertexCount*3*sizeof(float));if(mesh->animVertices)memcpy(mesh->animVertices,mesh->vertices,(size_t)mesh->vertexCount*3*sizeof(float));if(mesh->normals){mesh->animNormals=MemAlloc((unsigned int)mesh->vertexCount*3*sizeof(float));if(mesh->animNormals)memcpy(mesh->animNormals,mesh->normals,(size_t)mesh->vertexCount*3*sizeof(float));}}
    if(!mr_gltf_load_morphs(doc,primitive,sourceMesh,node,world,mesh,normal,tangent)){doc->failed=true;UnloadMesh(*mesh);*mesh=(Mesh){0};return false;}
    int material=mr_json_get(doc->json,doc->tokens,doc->tokenCount,primitive,"material");*materialId=material>=0?mr_json_int(doc->json,doc->tokens[material])+1:0;UploadMesh(mesh,false);return true;
}
static void mr_gltf_load_skin(MRGltfDoc *doc,Model *model){
    int skins=mr_json_get(doc->json,doc->tokens,doc->tokenCount,0,"skins"),skin=mr_json_at(doc->tokens,doc->tokenCount,skins,doc->selectedSkin),joints=mr_json_get(doc->json,doc->tokens,doc->tokenCount,skin,"joints");int count=mr_json_count(doc->tokens,doc->tokenCount,joints);if(count<=0)return;
    model->boneCount=count;model->bones=MemAlloc((unsigned int)count*sizeof(BoneInfo));model->bindPose=MemAlloc((unsigned int)count*sizeof(Transform));if(!model->bones||!model->bindPose){MemFree(model->bones);MemFree(model->bindPose);model->bones=NULL;model->bindPose=NULL;model->boneCount=0;return;}memset(model->bones,0,(size_t)count*sizeof(BoneInfo));
    int nodes=mr_json_get(doc->json,doc->tokens,doc->tokenCount,0,"nodes");for(int i=0;i<count;i++){int jt=mr_json_at(doc->tokens,doc->tokenCount,joints,i),nodeIndex=mr_json_int(doc->json,doc->tokens[jt]),node=mr_json_at(doc->tokens,doc->tokenCount,nodes,nodeIndex),name=mr_json_get(doc->json,doc->tokens,doc->tokenCount,node,"name");if(name>=0)mr_token_text(doc->json,doc->tokens[name],model->bones[i].name,sizeof model->bones[i].name);model->bones[i].parent=-1;int parent=mr_gltf_parent(doc,nodeIndex);for(int j=0;j<count;j++){int other=mr_json_at(doc->tokens,doc->tokenCount,joints,j);if(other>=0&&mr_json_int(doc->json,doc->tokens[other])==parent){model->bones[i].parent=j;break;}}model->bindPose[i]=mr_matrix_decompose(mr_gltf_world(doc,nodeIndex));}
    for(int i=0;i<model->meshCount;i++)if(model->meshes[i].boneIds&&model->meshes[i].boneWeights){model->meshes[i].boneCount=count;model->meshes[i].boneMatrices=MemAlloc((unsigned int)count*sizeof(Matrix));if(model->meshes[i].boneMatrices)for(int j=0;j<count;j++)model->meshes[i].boneMatrices[j]=mr_matrix_identity();}
}
static Model mr_load_obj(const char *fileName);
static Model mr_load_iqm(const char *fileName);
static Model mr_load_vox(const char *fileName);
static Model mr_load_m3d(const char *fileName);
static ModelAnimation *mr_load_iqm_animations(const char *fileName,int *animCount);
static ModelAnimation *mr_load_m3d_animations(const char *fileName,int *animCount);
Model LoadModel(const char *fileName){return LoadModelFromScene(fileName,-1);}
Model LoadModelFromScene(const char *fileName,int sceneIndex){
    if(!fileName||sceneIndex < -1 || (sceneIndex>=0&&!IsFileExtension(fileName,".gltf;.glb")))return(Model){0};
    if(IsFileExtension(fileName,".obj"))return mr_load_obj(fileName);
    if(IsFileExtension(fileName,".iqm"))return mr_load_iqm(fileName);
    if(IsFileExtension(fileName,".vox"))return mr_load_vox(fileName);
    if(IsFileExtension(fileName,".m3d"))return mr_load_m3d(fileName);
    Model model={0};MRGltfDoc doc;if(!mr_gltf_open(fileName,&doc)){puts("sargpu: could not load glTF 2.0 model");return model;}if(!mr_gltf_select_scene(&doc,sceneIndex)){mr_gltf_close(&doc);return model;}int nodes=mr_json_get(doc.json,doc.tokens,doc.tokenCount,0,"nodes"),nodeCount=mr_json_count(doc.tokens,doc.tokenCount,nodes),meshes=mr_json_get(doc.json,doc.tokens,doc.tokenCount,0,"meshes"),meshCount=mr_json_count(doc.tokens,doc.tokenCount,meshes),primitiveCount=0;
    if(nodeCount>0){for(int i=0;i<nodeCount;i++){if(!doc.sceneNodes[i])continue;int node=mr_json_at(doc.tokens,doc.tokenCount,nodes,i),meshToken=mr_json_get(doc.json,doc.tokens,doc.tokenCount,node,"mesh");if(meshToken<0)continue;int mesh=mr_json_at(doc.tokens,doc.tokenCount,meshes,mr_json_int(doc.json,doc.tokens[meshToken])),primitives=mr_json_get(doc.json,doc.tokens,doc.tokenCount,mesh,"primitives"),n=mr_json_count(doc.tokens,doc.tokenCount,primitives);for(int p=0;p<n;p++){int primitive=mr_json_at(doc.tokens,doc.tokenCount,primitives,p),mode=mr_json_get(doc.json,doc.tokens,doc.tokenCount,primitive,"mode");if(mode<0||mr_json_int(doc.json,doc.tokens[mode])==4)primitiveCount++;}}}
    else for(int i=0;i<meshCount;i++){int mesh=mr_json_at(doc.tokens,doc.tokenCount,meshes,i),primitives=mr_json_get(doc.json,doc.tokens,doc.tokenCount,mesh,"primitives");primitiveCount+=mr_json_count(doc.tokens,doc.tokenCount,primitives);}if(primitiveCount<=0){mr_gltf_close(&doc);return model;}
    int materials=mr_json_get(doc.json,doc.tokens,doc.tokenCount,0,"materials"),sourceMaterials=mr_json_count(doc.tokens,doc.tokenCount,materials);model.transform=mr_matrix_identity();model.meshCount=primitiveCount;model.materialCount=sourceMaterials+1;model.meshes=MemAlloc((unsigned int)primitiveCount*sizeof(Mesh));model.meshMaterial=MemAlloc((unsigned int)primitiveCount*sizeof(int));model.materials=MemAlloc((unsigned int)model.materialCount*sizeof(Material));if(model.meshes)memset(model.meshes,0,(size_t)primitiveCount*sizeof(Mesh));if(model.materials)memset(model.materials,0,(size_t)model.materialCount*sizeof(Material));if(!model.meshes||!model.meshMaterial||!model.materials){mr_gltf_close(&doc);UnloadModel(model);return(Model){0};}memset(model.meshes,0,(size_t)primitiveCount*sizeof(Mesh));memset(model.meshMaterial,0,(size_t)primitiveCount*sizeof(int));for(int i=0;i<model.materialCount;i++)model.materials[i]=LoadMaterialDefault();for(int i=0;i<sourceMaterials;i++)mr_gltf_material(&doc,mr_json_at(doc.tokens,doc.tokenCount,materials,i),&model.materials[i+1]);
    int output=0;if(nodeCount>0){for(int i=0;i<nodeCount;i++){if(!doc.sceneNodes[i])continue;int node=mr_json_at(doc.tokens,doc.tokenCount,nodes,i),meshToken=mr_json_get(doc.json,doc.tokens,doc.tokenCount,node,"mesh");if(meshToken<0)continue;int mesh=mr_json_at(doc.tokens,doc.tokenCount,meshes,mr_json_int(doc.json,doc.tokens[meshToken])),primitives=mr_json_get(doc.json,doc.tokens,doc.tokenCount,mesh,"primitives"),n=mr_json_count(doc.tokens,doc.tokenCount,primitives);Matrix world=mr_gltf_world(&doc,i);for(int p=0;p<n;p++){int primitive=mr_json_at(doc.tokens,doc.tokenCount,primitives,p),mode=mr_json_get(doc.json,doc.tokens,doc.tokenCount,primitive,"mode");if(mode>=0&&mr_json_int(doc.json,doc.tokens[mode])!=4)continue;if(output<primitiveCount){if(!mr_gltf_load_primitive(&doc,primitive,mesh,i,world,&model.meshes[output],&model.meshMaterial[output]))doc.failed=true;output++;}}}}
    else for(int i=0;i<meshCount;i++){int mesh=mr_json_at(doc.tokens,doc.tokenCount,meshes,i),primitives=mr_json_get(doc.json,doc.tokens,doc.tokenCount,mesh,"primitives"),n=mr_json_count(doc.tokens,doc.tokenCount,primitives);for(int p=0;p<n&&output<primitiveCount;p++){if(!mr_gltf_load_primitive(&doc,mr_json_at(doc.tokens,doc.tokenCount,primitives,p),mesh,-1,mr_matrix_identity(),&model.meshes[output],&model.meshMaterial[output]))doc.failed=true;output++;}}
    mr_gltf_load_skin(&doc,&model);bool failed=!mr_gltf_morph_animations(&doc,&model)||doc.failed;mr_gltf_close(&doc);if(failed||!IsModelValid(model)){UnloadModel(model);return(Model){0};}return model;
}
static Quaternion mr_quaternion_normalize(Quaternion q){float n=sqrtf(q.x*q.x+q.y*q.y+q.z*q.z+q.w*q.w);return n>0?(Quaternion){q.x/n,q.y/n,q.z/n,q.w/n}:(Quaternion){0,0,0,1};}
static Quaternion mr_quaternion_slerp(Quaternion a,Quaternion b,float t){a=mr_quaternion_normalize(a);b=mr_quaternion_normalize(b);float dot=a.x*b.x+a.y*b.y+a.z*b.z+a.w*b.w;if(dot<0)b=(Quaternion){-b.x,-b.y,-b.z,-b.w};return mr_quaternion_normalize((Quaternion){a.x+(b.x-a.x)*t,a.y+(b.y-a.y)*t,a.z+(b.z-a.z)*t,a.w+(b.w-a.w)*t});}
static void mr_gltf_sample(MRGltfDoc *doc,int sampler,float time,float *result,int components,bool rotation){
    int inputToken=mr_json_get(doc->json,doc->tokens,doc->tokenCount,sampler,"input"),outputToken=mr_json_get(doc->json,doc->tokens,doc->tokenCount,sampler,"output"),interpolation=mr_json_get(doc->json,doc->tokens,doc->tokenCount,sampler,"interpolation");if(inputToken<0||outputToken<0)return;MRGltfAccessor input=mr_gltf2_accessor(doc,mr_json_int(doc->json,doc->tokens[inputToken])),output=mr_gltf2_accessor(doc,mr_json_int(doc->json,doc->tokens[outputToken]));if(!input.data||!output.data||input.count<=0)return;
    int key=0;float t0=0,t1=0;mr_gltf2_read(input,0,&t0,1);if(time<=t0){mr_gltf2_read(output,interpolation>=0&&mr_json_equal(doc->json,doc->tokens[interpolation],"CUBICSPLINE")?1:0,result,components);return;}for(int i=0;i<input.count-1;i++){mr_gltf2_read(input,i,&t0,1);mr_gltf2_read(input,i+1,&t1,1);key=i;if(time<t1)break;key=i+1;}if(key>=input.count-1){int index=input.count-1;if(interpolation>=0&&mr_json_equal(doc->json,doc->tokens[interpolation],"CUBICSPLINE"))index=index*3+1;mr_gltf2_read(output,index,result,components);return;}
    mr_gltf2_read(input,key,&t0,1);mr_gltf2_read(input,key+1,&t1,1);float f=t1>t0?(time-t0)/(t1-t0):0;if(f<0)f=0;if(f>1)f=1;bool step=interpolation>=0&&mr_json_equal(doc->json,doc->tokens[interpolation],"STEP"),cubic=interpolation>=0&&mr_json_equal(doc->json,doc->tokens[interpolation],"CUBICSPLINE");float a[4]={0},b[4]={0};
    if(step){mr_gltf2_read(output,key,result,components);return;}if(cubic){float outTangent[4]={0},inTangent[4]={0};mr_gltf2_read(output,key*3+1,a,components);mr_gltf2_read(output,key*3+2,outTangent,components);mr_gltf2_read(output,(key+1)*3,b,components);mr_gltf2_read(output,(key+1)*3+1,inTangent,components);float f2=f*f,f3=f2*f,duration=t1-t0;for(int c=0;c<components;c++)result[c]=(2*f3-3*f2+1)*a[c]+(f3-2*f2+f)*outTangent[c]*duration+(-2*f3+3*f2)*b[c]+(f3-f2)*inTangent[c]*duration;if(rotation){Quaternion q=mr_quaternion_normalize(*(Quaternion*)result);memcpy(result,&q,sizeof q);}return;}
    mr_gltf2_read(output,key,a,components);mr_gltf2_read(output,key+1,b,components);if(rotation){Quaternion q=mr_quaternion_slerp(*(Quaternion*)a,*(Quaternion*)b,f);memcpy(result,&q,sizeof q);}else for(int c=0;c<components;c++)result[c]=a[c]+(b[c]-a[c])*f;
}
static void mr_build_pose_world(BoneInfo *bones,int count,Transform *pose){for(int i=0;i<count;i++)if(bones[i].parent>=0&&bones[i].parent<i){Matrix parent=mr_gltf_trs(pose[bones[i].parent].translation,pose[bones[i].parent].rotation,pose[bones[i].parent].scale),local=mr_gltf_trs(pose[i].translation,pose[i].rotation,pose[i].scale);pose[i]=mr_matrix_decompose(mr_matrix_multiply(parent,local));}}
ModelAnimation *LoadModelAnimations(const char *fileName,int *animCount){return LoadModelAnimationsFromScene(fileName,-1,animCount);}
ModelAnimation *LoadModelAnimationsFromScene(const char *fileName,int sceneIndex,int *animCount){
    if(animCount)*animCount=0;
    if(!fileName||sceneIndex < -1 || (sceneIndex>=0&&!IsFileExtension(fileName,".gltf;.glb")))return NULL;
    if(IsFileExtension(fileName,".iqm"))return mr_load_iqm_animations(fileName,animCount);
    if(IsFileExtension(fileName,".m3d"))return mr_load_m3d_animations(fileName,animCount);
    if(animCount)*animCount=0;MRGltfDoc doc;if(!mr_gltf_open(fileName,&doc))return NULL;if(!mr_gltf_select_scene(&doc,sceneIndex)){mr_gltf_close(&doc);return NULL;}int skins=mr_json_get(doc.json,doc.tokens,doc.tokenCount,0,"skins"),skin=mr_json_at(doc.tokens,doc.tokenCount,skins,doc.selectedSkin),joints=mr_json_get(doc.json,doc.tokens,doc.tokenCount,skin,"joints"),boneCount=mr_json_count(doc.tokens,doc.tokenCount,joints),animationsToken=mr_json_get(doc.json,doc.tokens,doc.tokenCount,0,"animations"),count=mr_json_count(doc.tokens,doc.tokenCount,animationsToken);if(boneCount>256||count<=0){mr_gltf_close(&doc);return NULL;}ModelAnimation *animations=MemAlloc((unsigned int)count*sizeof(ModelAnimation));if(!animations){mr_gltf_close(&doc);return NULL;}memset(animations,0,(size_t)count*sizeof(ModelAnimation));int nodes=mr_json_get(doc.json,doc.tokens,doc.tokenCount,0,"nodes");
    for(int a=0;a<count;a++){ModelAnimation *anim=&animations[a];anim->sourceAnimation=a;anim->boneCount=boneCount;anim->bones=MemAlloc((unsigned int)boneCount*sizeof(BoneInfo));if(boneCount&&!anim->bones)continue;if(boneCount)memset(anim->bones,0,(size_t)boneCount*sizeof(BoneInfo));int animation=mr_json_at(doc.tokens,doc.tokenCount,animationsToken,a),name=mr_json_get(doc.json,doc.tokens,doc.tokenCount,animation,"name"),samplers=mr_json_get(doc.json,doc.tokens,doc.tokenCount,animation,"samplers"),channels=mr_json_get(doc.json,doc.tokens,doc.tokenCount,animation,"channels"),channelCount=mr_json_count(doc.tokens,doc.tokenCount,channels);if(name>=0)mr_token_text(doc.json,doc.tokens[name],anim->name,sizeof anim->name);
        int *jointNodes=MemAlloc((unsigned int)boneCount*sizeof(int)),*translation=MemAlloc((unsigned int)boneCount*sizeof(int)),*rotation=MemAlloc((unsigned int)boneCount*sizeof(int)),*scale=MemAlloc((unsigned int)boneCount*sizeof(int));if(boneCount&&(!jointNodes||!translation||!rotation||!scale)){MemFree(jointNodes);MemFree(translation);MemFree(rotation);MemFree(scale);continue;}for(int i=0;i<boneCount;i++){int jt=mr_json_at(doc.tokens,doc.tokenCount,joints,i);jointNodes[i]=mr_json_int(doc.json,doc.tokens[jt]);translation[i]=rotation[i]=scale[i]=-1;int node=mr_json_at(doc.tokens,doc.tokenCount,nodes,jointNodes[i]),boneName=mr_json_get(doc.json,doc.tokens,doc.tokenCount,node,"name");if(boneName>=0)mr_token_text(doc.json,doc.tokens[boneName],anim->bones[i].name,sizeof anim->bones[i].name);anim->bones[i].parent=-1;int parent=mr_gltf_parent(&doc,jointNodes[i]);for(int j=0;j<boneCount;j++){int other=mr_json_at(doc.tokens,doc.tokenCount,joints,j);if(other>=0&&mr_json_int(doc.json,doc.tokens[other])==parent){anim->bones[i].parent=j;break;}}}
        float duration=0;for(int c=0;c<channelCount;c++){int channel=mr_json_at(doc.tokens,doc.tokenCount,channels,c),samplerIndex=mr_json_get(doc.json,doc.tokens,doc.tokenCount,channel,"sampler"),target=mr_json_get(doc.json,doc.tokens,doc.tokenCount,channel,"target"),nodeToken=mr_json_get(doc.json,doc.tokens,doc.tokenCount,target,"node"),path=mr_json_get(doc.json,doc.tokens,doc.tokenCount,target,"path");if(samplerIndex<0||nodeToken<0||path<0)continue;int bone=-1,nodeIndex=mr_json_int(doc.json,doc.tokens[nodeToken]);for(int i=0;i<boneCount;i++)if(jointNodes[i]==nodeIndex){bone=i;break;}bool morph=mr_json_equal(doc.json,doc.tokens[path],"weights");if(bone<0&&!morph)continue;if(morph&&(nodeIndex<0||!doc.sceneNodes||nodeIndex>=mr_json_count(doc.tokens,doc.tokenCount,nodes)||!doc.sceneNodes[nodeIndex]))continue;int sampler=mr_json_at(doc.tokens,doc.tokenCount,samplers,mr_json_int(doc.json,doc.tokens[samplerIndex]));if(bone>=0&&mr_json_equal(doc.json,doc.tokens[path],"translation"))translation[bone]=sampler;else if(bone>=0&&mr_json_equal(doc.json,doc.tokens[path],"rotation"))rotation[bone]=sampler;else if(bone>=0&&mr_json_equal(doc.json,doc.tokens[path],"scale"))scale[bone]=sampler;int inputToken=mr_json_get(doc.json,doc.tokens,doc.tokenCount,sampler,"input");if(inputToken>=0){MRGltfAccessor input=mr_gltf2_accessor(&doc,mr_json_int(doc.json,doc.tokens[inputToken]));float end=0;if(input.count>0)mr_gltf2_read(input,input.count-1,&end,1);if(end>duration)duration=end;}}
        anim->frameCount=(int)(duration*1000.0f/17.0f)+1;if(anim->frameCount<1)anim->frameCount=1;anim->framePoses=MemAlloc((unsigned int)anim->frameCount*sizeof(Transform*));if(!anim->framePoses){anim->frameCount=0;MemFree(jointNodes);MemFree(translation);MemFree(rotation);MemFree(scale);continue;}memset(anim->framePoses,0,(size_t)anim->frameCount*sizeof(Transform*));
        for(int frame=0;frame<anim->frameCount;frame++){if(!boneCount)continue;anim->framePoses[frame]=MemAlloc((unsigned int)boneCount*sizeof(Transform));if(!anim->framePoses[frame])continue;float time=frame*0.017f;for(int b=0;b<boneCount;b++){int node=mr_json_at(doc.tokens,doc.tokenCount,nodes,jointNodes[b]);Transform pose=mr_gltf_node_transform(&doc,node);if(translation[b]>=0)mr_gltf_sample(&doc,translation[b],time,(float*)&pose.translation,3,false);if(rotation[b]>=0)mr_gltf_sample(&doc,rotation[b],time,(float*)&pose.rotation,4,true);if(scale[b]>=0)mr_gltf_sample(&doc,scale[b],time,(float*)&pose.scale,3,false);anim->framePoses[frame][b]=pose;}mr_build_pose_world(anim->bones,boneCount,anim->framePoses[frame]);}
        MemFree(jointNodes);MemFree(translation);MemFree(rotation);MemFree(scale);
    }mr_gltf_close(&doc);if(animCount)*animCount=count;return animations;
}
static Matrix mr_matrix_affine_inverse(Matrix m){
    float a=m.m0,b=m.m4,c=m.m8,d=m.m1,e=m.m5,f=m.m9,g=m.m2,h=m.m6,i=m.m10,det=a*(e*i-f*h)-b*(d*i-f*g)+c*(d*h-e*g);if(det>-0.000001f&&det<0.000001f)return mr_matrix_identity();float q=1/det;Matrix r=mr_matrix_identity();r.m0=(e*i-f*h)*q;r.m4=(c*h-b*i)*q;r.m8=(b*f-c*e)*q;r.m1=(f*g-d*i)*q;r.m5=(a*i-c*g)*q;r.m9=(c*d-a*f)*q;r.m2=(d*h-e*g)*q;r.m6=(b*g-a*h)*q;r.m10=(a*e-b*d)*q;r.m12=-(r.m0*m.m12+r.m4*m.m13+r.m8*m.m14);r.m13=-(r.m1*m.m12+r.m5*m.m13+r.m9*m.m14);r.m14=-(r.m2*m.m12+r.m6*m.m13+r.m10*m.m14);return r;
}
void UpdateModelAnimationBones(Model model,ModelAnimation anim,int frame){if(!IsModelAnimationValid(model,anim)||anim.frameCount<=0||!anim.framePoses)return;if(frame<0)frame=0;frame%=anim.frameCount;for(int m=0;m<model.meshCount;m++)if(model.meshes[m].boneMatrices)for(int b=0;b<model.boneCount;b++){Matrix bind=mr_gltf_trs(model.bindPose[b].translation,model.bindPose[b].rotation,model.bindPose[b].scale),pose=mr_gltf_trs(anim.framePoses[frame][b].translation,anim.framePoses[frame][b].rotation,anim.framePoses[frame][b].scale);model.meshes[m].boneMatrices[b]=mr_matrix_multiply(pose,mr_matrix_affine_inverse(bind));}}
void UpdateModelAnimation(Model model,ModelAnimation anim,int frame){if(!IsModelAnimationValid(model,anim)||anim.frameCount<=0)return;if(frame<0)frame=0;frame%=anim.frameCount;UpdateModelAnimationBones(model,anim,frame);UpdateModelMorphAnimation(model,anim.sourceAnimation,frame*0.017f);}
void UnloadModelAnimation(ModelAnimation anim){if(anim.framePoses)for(int i=0;i<anim.frameCount;i++)MemFree(anim.framePoses[i]);MemFree(anim.framePoses);MemFree(anim.bones);}
void UnloadModelAnimations(ModelAnimation *animations,int animCount){if(animations)for(int i=0;i<animCount;i++)UnloadModelAnimation(animations[i]);MemFree(animations);}
bool IsModelAnimationValid(Model model,ModelAnimation anim){if(model.boneCount==0&&anim.boneCount==0&&model.morphData){for(int i=0;i<model.morphData->count;i++)if(model.morphData->channels[i].animation==anim.sourceAnimation)return true;return false;}if(model.boneCount<=0||model.boneCount!=anim.boneCount||!model.bones||!anim.bones)return false;for(int i=0;i<model.boneCount;i++)if(model.bones[i].parent!=anim.bones[i].parent)return false;return true;}
