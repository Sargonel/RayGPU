/* SarGPU shapes module. Compiled through sargpu.c; do not compile separately. */
static void mr_triangle_colors(Vector2 a,Vector2 b,Vector2 c,Vector2 uvA,Vector2 uvB,Vector2 uvC,
    Color colorA,Color colorB,Color colorC,unsigned int texture) {
    if (!mr.drawing || mr.targetWidth<=0 || mr.targetHeight<=0) return;
    if (mr.vertexCount+3>MR_MAX_VERTICES) {
        if (!mr.overflow) puts("sargpu: frame vertex limit reached; remaining geometry skipped");
        mr.overflow=true; return;
    }
    unsigned int sx=mr.scissorActive?(unsigned int)(mr.scissor.x<0?0:mr.scissor.x):0;
    unsigned int sy=mr.scissorActive?(unsigned int)(mr.scissor.y<0?0:mr.scissor.y):0;
    unsigned int sw=mr.scissorActive?(unsigned int)(mr.scissor.width<0?0:mr.scissor.width):(unsigned int)mr.targetWidth;
    unsigned int sh=mr.scissorActive?(unsigned int)(mr.scissor.height<0?0:mr.scissor.height):(unsigned int)mr.targetHeight;
    MRBatch *last=mr.batchCount?&mr.batches[mr.batchCount-1]:NULL;
    if (!last || last->texture!=texture || last->blend!=(unsigned int)mr.blendMode || last->shader!=mr.currentShader || last->x!=sx || last->y!=sy || last->width!=sw || last->height!=sh)
        mr.batches[mr.batchCount++]=(MRBatch){mr.vertexCount,0,texture,(unsigned int)mr.blendMode,mr.currentShader,sx,sy,sw,sh};
    mr.batches[mr.batchCount-1].count+=3;
    Vector2 p[3]={a,b,c},uv[3]={uvA,uvB,uvC}; Color colors[3]={colorA,colorB,colorC};
    if (mr.camera2dActive) for (int i=0;i<3;i++) p[i]=GetWorldToScreen2D(p[i],mr.camera2d);
    for (int i=0;i<3;i++) mr.vertices[mr.vertexCount++]=(MRVertex){
        2*p[i].x/mr.targetWidth-1,1-2*p[i].y/mr.targetHeight,uv[i].x,uv[i].y,
        0.0f,colors[i].r,colors[i].g,colors[i].b,colors[i].a};
}
static void mr_triangle(Vector2 a,Vector2 b,Vector2 c,Vector2 uvA,Vector2 uvB,Vector2 uvC,Color color,unsigned int texture) {
    mr_triangle_colors(a,b,c,uvA,uvB,uvC,color,color,color,texture);
}
static Vector2 mr_shapes_uv(void) {
    if (mr.shapesTexture.width<=0 || mr.shapesTexture.height<=0) return (Vector2){0};
    return (Vector2){(mr.shapesSource.x+mr.shapesSource.width*0.5f)/mr.shapesTexture.width,
        (mr.shapesSource.y+mr.shapesSource.height*0.5f)/mr.shapesTexture.height};
}
void DrawTriangle(Vector2 a,Vector2 b,Vector2 c,Color color) {
    Vector2 uv=mr_shapes_uv();
    mr_triangle(a,b,c,uv,uv,uv,color,mr.shapesTexture.id);
}
static void mr_quad(float x,float y,float w,float h,Color color,unsigned int texture) {
    Vector2 a={x,y},b={x+w,y},c={x+w,y+h},d={x,y+h};
    mr_triangle(a,b,c,(Vector2){0,0},(Vector2){1,0},(Vector2){1,1},color,texture);
    mr_triangle(a,c,d,(Vector2){0,0},(Vector2){1,1},(Vector2){0,1},color,texture);
}
void DrawRectangle(int x,int y,int width,int height,Color color) {
    if (width>0 && height>0) {
        Vector2 a={(float)x,(float)y},b={(float)(x+width),(float)y},c={(float)(x+width),(float)(y+height)},d={(float)x,(float)(y+height)};
        DrawTriangle(a,b,c,color); DrawTriangle(a,c,d,color);
    }
}
void DrawCircle(int x,int y,float radius,Color color) {
    if (!(radius>0)) return;
    for (int i=0;i<64;i++) {
        float a=i*6.28318530718f/64,b=(i+1)*6.28318530718f/64;
        DrawTriangle((Vector2){(float)x,(float)y},(Vector2){x+cosf(a)*radius,y+sinf(a)*radius},(Vector2){x+cosf(b)*radius,y+sinf(b)*radius},color);
    }
}
void DrawLineEx(Vector2 a,Vector2 b,float thickness,Color color) {
    float dx=b.x-a.x,dy=b.y-a.y,length=sqrtf(dx*dx+dy*dy);
    if (!(length>0) || !(thickness>0)) return;
    float nx=-dy/length*thickness/2,ny=dx/length*thickness/2;
    Vector2 p={a.x+nx,a.y+ny},q={b.x+nx,b.y+ny},r={b.x-nx,b.y-ny},s={a.x-nx,a.y-ny};
    DrawTriangle(p,q,r,color); DrawTriangle(p,r,s,color);
}
void DrawPixel(int x,int y,Color color) { DrawRectangle(x,y,1,1,color); }
void DrawPixelV(Vector2 p,Color color) { DrawPixel((int)p.x,(int)p.y,color); }
void DrawLine(int x1,int y1,int x2,int y2,Color color) {
    DrawLineEx((Vector2){(float)x1,(float)y1},(Vector2){(float)x2,(float)y2},1,color);
}
void DrawLineV(Vector2 a,Vector2 b,Color color) { DrawLineEx(a,b,1,color); }
void DrawLineStrip(const Vector2 *points,int count,Color color) {
    if (!points) return;
    for (int i=1;i<count;i++) DrawLineV(points[i-1],points[i],color);
}
void DrawLineBezier(Vector2 start,Vector2 end,float thick,Color color) {
    Vector2 c1={start.x,start.y+(end.y-start.y)*0.5f};
    Vector2 c2={end.x,end.y-(end.y-start.y)*0.5f};
    DrawSplineSegmentBezierCubic(start,c1,c2,end,thick,color);
}
static int mr_arc_segments(float start,float end,int segments) {
    float arc=end-start; if (arc<0) arc=-arc;
    if (segments<1) segments=(int)(arc/15.0f)+1;
    return segments<1 ? 1 : segments;
}
void DrawCircleSector(Vector2 center,float radius,float startAngle,float endAngle,int segments,Color color) {
    if (!(radius>0)) return;
    segments=mr_arc_segments(startAngle,endAngle,segments);
    float step=(endAngle-startAngle)*MR_DEG2RAD/segments;
    float angle=startAngle*MR_DEG2RAD;
    for (int i=0;i<segments;i++,angle+=step) DrawTriangle(center,
        (Vector2){center.x+cosf(angle)*radius,center.y+sinf(angle)*radius},
        (Vector2){center.x+cosf(angle+step)*radius,center.y+sinf(angle+step)*radius},color);
}
void DrawCircleSectorLines(Vector2 center,float radius,float startAngle,float endAngle,int segments,Color color) {
    if (!(radius>0)) return;
    segments=mr_arc_segments(startAngle,endAngle,segments);
    float step=(endAngle-startAngle)*MR_DEG2RAD/segments;
    Vector2 first={center.x+cosf(startAngle*MR_DEG2RAD)*radius,center.y+sinf(startAngle*MR_DEG2RAD)*radius};
    Vector2 previous=first;
    DrawLineV(center,first,color);
    for (int i=1;i<=segments;i++) {
        float angle=startAngle*MR_DEG2RAD+step*i;
        Vector2 next={center.x+cosf(angle)*radius,center.y+sinf(angle)*radius};
        DrawLineV(previous,next,color); previous=next;
    }
    DrawLineV(previous,center,color);
}
void DrawCircleGradient(int x,int y,float radius,Color inner,Color outer) {
    if (!(radius>0)) return;
    Vector2 center={(float)x,(float)y};
    for (int i=0;i<64;i++) {
        float a=i*2*MR_PI/64,b=(i+1)*2*MR_PI/64;
        Vector2 uv=mr_shapes_uv();
        mr_triangle_colors(center,(Vector2){x+cosf(a)*radius,y+sinf(a)*radius},
            (Vector2){x+cosf(b)*radius,y+sinf(b)*radius},uv,uv,uv,
            inner,outer,outer,mr.shapesTexture.id);
    }
}
void DrawCircleV(Vector2 center,float radius,Color color) { DrawCircle((int)center.x,(int)center.y,radius,color); }
void DrawCircleLinesV(Vector2 center,float radius,Color color) {
    if (!(radius>0)) return;
    Vector2 points[65];
    for (int i=0;i<=64;i++) {
        float angle=i*2*MR_PI/64;
        points[i]=(Vector2){center.x+cosf(angle)*radius,center.y+sinf(angle)*radius};
    }
    DrawLineStrip(points,65,color);
}
void DrawCircleLines(int x,int y,float radius,Color color) { DrawCircleLinesV((Vector2){(float)x,(float)y},radius,color); }
void DrawEllipse(int x,int y,float radiusH,float radiusV,Color color) {
    if (!(radiusH>0) || !(radiusV>0)) return;
    Vector2 center={(float)x,(float)y};
    for (int i=0;i<64;i++) {
        float a=i*2*MR_PI/64,b=(i+1)*2*MR_PI/64;
        DrawTriangle(center,(Vector2){x+cosf(a)*radiusH,y+sinf(a)*radiusV},
            (Vector2){x+cosf(b)*radiusH,y+sinf(b)*radiusV},color);
    }
}
void DrawEllipseLines(int x,int y,float radiusH,float radiusV,Color color) {
    if (!(radiusH>0) || !(radiusV>0)) return;
    Vector2 previous={(float)x+radiusH,(float)y};
    for (int i=1;i<=64;i++) {
        float a=i*2*MR_PI/64;
        Vector2 next={x+cosf(a)*radiusH,y+sinf(a)*radiusV};
        DrawLineV(previous,next,color); previous=next;
    }
}
void DrawRing(Vector2 center,float innerRadius,float outerRadius,float startAngle,float endAngle,int segments,Color color) {
    if (innerRadius<0) innerRadius=0;
    if (outerRadius<innerRadius) { float swap=outerRadius; outerRadius=innerRadius; innerRadius=swap; }
    if (!(outerRadius>0)) return;
    segments=mr_arc_segments(startAngle,endAngle,segments);
    float step=(endAngle-startAngle)*MR_DEG2RAD/segments,angle=startAngle*MR_DEG2RAD;
    for (int i=0;i<segments;i++,angle+=step) {
        Vector2 a={center.x+cosf(angle)*innerRadius,center.y+sinf(angle)*innerRadius};
        Vector2 b={center.x+cosf(angle)*outerRadius,center.y+sinf(angle)*outerRadius};
        Vector2 c={center.x+cosf(angle+step)*outerRadius,center.y+sinf(angle+step)*outerRadius};
        Vector2 d={center.x+cosf(angle+step)*innerRadius,center.y+sinf(angle+step)*innerRadius};
        DrawTriangle(a,b,c,color); DrawTriangle(a,c,d,color);
    }
}
void DrawRingLines(Vector2 center,float innerRadius,float outerRadius,float startAngle,float endAngle,int segments,Color color) {
    if (innerRadius<0) innerRadius=0;
    if (outerRadius<innerRadius) { float swap=outerRadius; outerRadius=innerRadius; innerRadius=swap; }
    segments=mr_arc_segments(startAngle,endAngle,segments);
    float step=(endAngle-startAngle)*MR_DEG2RAD/segments;
    Vector2 innerFirst={center.x+cosf(startAngle*MR_DEG2RAD)*innerRadius,center.y+sinf(startAngle*MR_DEG2RAD)*innerRadius};
    Vector2 outerFirst={center.x+cosf(startAngle*MR_DEG2RAD)*outerRadius,center.y+sinf(startAngle*MR_DEG2RAD)*outerRadius};
    Vector2 inner=innerFirst,outer=outerFirst;
    DrawLineV(inner,outer,color);
    for (int i=1;i<=segments;i++) {
        float angle=startAngle*MR_DEG2RAD+step*i;
        Vector2 nextInner={center.x+cosf(angle)*innerRadius,center.y+sinf(angle)*innerRadius};
        Vector2 nextOuter={center.x+cosf(angle)*outerRadius,center.y+sinf(angle)*outerRadius};
        DrawLineV(inner,nextInner,color); DrawLineV(outer,nextOuter,color);
        inner=nextInner; outer=nextOuter;
    }
    DrawLineV(inner,outer,color);
}
void DrawRectangleV(Vector2 p,Vector2 size,Color color) { DrawRectangle((int)p.x,(int)p.y,(int)size.x,(int)size.y,color); }
void DrawRectangleRec(Rectangle rec,Color color) {
    Vector2 a={rec.x,rec.y},b={rec.x+rec.width,rec.y},c={rec.x+rec.width,rec.y+rec.height},d={rec.x,rec.y+rec.height};
    DrawTriangle(a,b,c,color); DrawTriangle(a,c,d,color);
}
static Vector2 mr_rotate_point(Vector2 p,float c,float s,Vector2 translation) {
    return (Vector2){translation.x+p.x*c-p.y*s,translation.y+p.x*s+p.y*c};
}
void DrawRectanglePro(Rectangle rec,Vector2 origin,float rotation,Color color) {
    float angle=rotation*MR_DEG2RAD,c=cosf(angle),s=sinf(angle);
    Vector2 translation={rec.x,rec.y};
    Vector2 a=mr_rotate_point((Vector2){-origin.x,-origin.y},c,s,translation);
    Vector2 b=mr_rotate_point((Vector2){rec.width-origin.x,-origin.y},c,s,translation);
    Vector2 d=mr_rotate_point((Vector2){-origin.x,rec.height-origin.y},c,s,translation);
    Vector2 e=mr_rotate_point((Vector2){rec.width-origin.x,rec.height-origin.y},c,s,translation);
    DrawTriangle(a,b,e,color); DrawTriangle(a,e,d,color);
}
void DrawRectangleGradientEx(Rectangle rec,Color topLeft,Color bottomLeft,Color topRight,Color bottomRight) {
    Vector2 a={rec.x,rec.y},b={rec.x+rec.width,rec.y},c={rec.x+rec.width,rec.y+rec.height},d={rec.x,rec.y+rec.height};
    Vector2 uv=mr_shapes_uv();
    mr_triangle_colors(a,b,c,uv,uv,uv,topLeft,topRight,bottomRight,mr.shapesTexture.id);
    mr_triangle_colors(a,c,d,uv,uv,uv,topLeft,bottomRight,bottomLeft,mr.shapesTexture.id);
}
void DrawRectangleGradientV(int x,int y,int width,int height,Color top,Color bottom) {
    DrawRectangleGradientEx((Rectangle){(float)x,(float)y,(float)width,(float)height},top,bottom,top,bottom);
}
void DrawRectangleGradientH(int x,int y,int width,int height,Color left,Color right) {
    DrawRectangleGradientEx((Rectangle){(float)x,(float)y,(float)width,(float)height},left,left,right,right);
}
void DrawRectangleLinesEx(Rectangle rec,float thick,Color color) {
    if (!(thick>0) || !(rec.width>0) || !(rec.height>0)) return;
    if (thick*2>rec.width) thick=rec.width/2;
    if (thick*2>rec.height) thick=rec.height/2;
    DrawRectangleRec((Rectangle){rec.x,rec.y,rec.width,thick},color);
    DrawRectangleRec((Rectangle){rec.x,rec.y+rec.height-thick,rec.width,thick},color);
    DrawRectangleRec((Rectangle){rec.x,rec.y+thick,thick,rec.height-2*thick},color);
    DrawRectangleRec((Rectangle){rec.x+rec.width-thick,rec.y+thick,thick,rec.height-2*thick},color);
}
void DrawRectangleLines(int x,int y,int width,int height,Color color) {
    DrawRectangleLinesEx((Rectangle){(float)x,(float)y,(float)width,(float)height},1,color);
}
static float mr_round_radius(Rectangle rec,float roundness) {
    float size=rec.width<rec.height ? rec.width:rec.height;
    if (roundness<0) roundness=0; if (roundness>1) roundness=1;
    return size*roundness*0.5f;
}
void DrawRectangleRounded(Rectangle rec,float roundness,int segments,Color color) {
    float radius=mr_round_radius(rec,roundness);
    if (!(radius>0)) { DrawRectangleRec(rec,color); return; }
    DrawRectangleRec((Rectangle){rec.x+radius,rec.y,rec.width-2*radius,rec.height},color);
    DrawRectangleRec((Rectangle){rec.x,rec.y+radius,radius,rec.height-2*radius},color);
    DrawRectangleRec((Rectangle){rec.x+rec.width-radius,rec.y+radius,radius,rec.height-2*radius},color);
    DrawCircleSector((Vector2){rec.x+radius,rec.y+radius},radius,180,270,segments,color);
    DrawCircleSector((Vector2){rec.x+rec.width-radius,rec.y+radius},radius,270,360,segments,color);
    DrawCircleSector((Vector2){rec.x+rec.width-radius,rec.y+rec.height-radius},radius,0,90,segments,color);
    DrawCircleSector((Vector2){rec.x+radius,rec.y+rec.height-radius},radius,90,180,segments,color);
}
static void mr_arc_line(Vector2 center,float radius,float start,float end,int segments,float thick,Color color) {
    segments=mr_arc_segments(start,end,segments);
    float step=(end-start)*MR_DEG2RAD/segments,angle=start*MR_DEG2RAD;
    Vector2 previous={center.x+cosf(angle)*radius,center.y+sinf(angle)*radius};
    for (int i=1;i<=segments;i++) {
        angle=start*MR_DEG2RAD+step*i;
        Vector2 next={center.x+cosf(angle)*radius,center.y+sinf(angle)*radius};
        DrawLineEx(previous,next,thick,color); previous=next;
    }
}
void DrawRectangleRoundedLinesEx(Rectangle rec,float roundness,int segments,float thick,Color color) {
    float radius=mr_round_radius(rec,roundness);
    if (!(radius>0)) { DrawRectangleLinesEx(rec,thick,color); return; }
    DrawLineEx((Vector2){rec.x+radius,rec.y},(Vector2){rec.x+rec.width-radius,rec.y},thick,color);
    DrawLineEx((Vector2){rec.x+rec.width,rec.y+radius},(Vector2){rec.x+rec.width,rec.y+rec.height-radius},thick,color);
    DrawLineEx((Vector2){rec.x+rec.width-radius,rec.y+rec.height},(Vector2){rec.x+radius,rec.y+rec.height},thick,color);
    DrawLineEx((Vector2){rec.x,rec.y+rec.height-radius},(Vector2){rec.x,rec.y+radius},thick,color);
    mr_arc_line((Vector2){rec.x+radius,rec.y+radius},radius,180,270,segments,thick,color);
    mr_arc_line((Vector2){rec.x+rec.width-radius,rec.y+radius},radius,270,360,segments,thick,color);
    mr_arc_line((Vector2){rec.x+rec.width-radius,rec.y+rec.height-radius},radius,0,90,segments,thick,color);
    mr_arc_line((Vector2){rec.x+radius,rec.y+rec.height-radius},radius,90,180,segments,thick,color);
}
void DrawRectangleRoundedLines(Rectangle rec,float roundness,int segments,Color color) {
    DrawRectangleRoundedLinesEx(rec,roundness,segments,1,color);
}
void DrawTriangleLines(Vector2 a,Vector2 b,Vector2 c,Color color) {
    DrawLineV(a,b,color); DrawLineV(b,c,color); DrawLineV(c,a,color);
}
void DrawTriangleFan(const Vector2 *points,int count,Color color) {
    if (!points) return;
    for (int i=1;i<count-1;i++) DrawTriangle(points[0],points[i],points[i+1],color);
}
void DrawTriangleStrip(const Vector2 *points,int count,Color color) {
    if (!points) return;
    for (int i=0;i<count-2;i++) {
        if (i&1) DrawTriangle(points[i+1],points[i],points[i+2],color);
        else DrawTriangle(points[i],points[i+1],points[i+2],color);
    }
}
void DrawPoly(Vector2 center,int sides,float radius,float rotation,Color color) {
    if (sides<3 || !(radius>0)) return;
    float step=2*MR_PI/sides,angle=rotation*MR_DEG2RAD;
    for (int i=0;i<sides;i++,angle+=step) DrawTriangle(center,
        (Vector2){center.x+cosf(angle)*radius,center.y+sinf(angle)*radius},
        (Vector2){center.x+cosf(angle+step)*radius,center.y+sinf(angle+step)*radius},color);
}
void DrawPolyLinesEx(Vector2 center,int sides,float radius,float rotation,float thick,Color color) {
    if (sides<3 || !(radius>0)) return;
    float step=2*MR_PI/sides,angle=rotation*MR_DEG2RAD;
    Vector2 first={center.x+cosf(angle)*radius,center.y+sinf(angle)*radius},previous=first;
    for (int i=1;i<sides;i++) {
        angle+=step; Vector2 next={center.x+cosf(angle)*radius,center.y+sinf(angle)*radius};
        DrawLineEx(previous,next,thick,color); previous=next;
    }
    DrawLineEx(previous,first,thick,color);
}
void DrawPolyLines(Vector2 center,int sides,float radius,float rotation,Color color) { DrawPolyLinesEx(center,sides,radius,rotation,1,color); }

Vector2 GetSplinePointLinear(Vector2 a,Vector2 b,float t) {
    return (Vector2){a.x+(b.x-a.x)*t,a.y+(b.y-a.y)*t};
}
Vector2 GetSplinePointBasis(Vector2 p1,Vector2 p2,Vector2 p3,Vector2 p4,float t) {
    float t2=t*t,t3=t2*t;
    float a=(-t3+3*t2-3*t+1)/6,b=(3*t3-6*t2+4)/6,c=(-3*t3+3*t2+3*t+1)/6,d=t3/6;
    return (Vector2){p1.x*a+p2.x*b+p3.x*c+p4.x*d,p1.y*a+p2.y*b+p3.y*c+p4.y*d};
}
Vector2 GetSplinePointCatmullRom(Vector2 p1,Vector2 p2,Vector2 p3,Vector2 p4,float t) {
    float t2=t*t,t3=t2*t;
    return (Vector2){0.5f*((2*p2.x)+(-p1.x+p3.x)*t+(2*p1.x-5*p2.x+4*p3.x-p4.x)*t2+(-p1.x+3*p2.x-3*p3.x+p4.x)*t3),
        0.5f*((2*p2.y)+(-p1.y+p3.y)*t+(2*p1.y-5*p2.y+4*p3.y-p4.y)*t2+(-p1.y+3*p2.y-3*p3.y+p4.y)*t3)};
}
Vector2 GetSplinePointBezierQuad(Vector2 p1,Vector2 c2,Vector2 p3,float t) {
    float u=1-t; return (Vector2){u*u*p1.x+2*u*t*c2.x+t*t*p3.x,u*u*p1.y+2*u*t*c2.y+t*t*p3.y};
}
Vector2 GetSplinePointBezierCubic(Vector2 p1,Vector2 c2,Vector2 c3,Vector2 p4,float t) {
    float u=1-t,u2=u*u,t2=t*t;
    return (Vector2){u2*u*p1.x+3*u2*t*c2.x+3*u*t2*c3.x+t2*t*p4.x,
        u2*u*p1.y+3*u2*t*c2.y+3*u*t2*c3.y+t2*t*p4.y};
}
typedef Vector2 (*MRSplinePoint)(Vector2,Vector2,Vector2,Vector2,float);
static void mr_draw_spline4(Vector2 p1,Vector2 p2,Vector2 p3,Vector2 p4,float thick,Color color,MRSplinePoint point) {
    Vector2 previous=point(p1,p2,p3,p4,0);
    for (int i=1;i<=24;i++) { Vector2 next=point(p1,p2,p3,p4,i/24.0f); DrawLineEx(previous,next,thick,color); previous=next; }
}
static Vector2 mr_basis_adapter(Vector2 a,Vector2 b,Vector2 c,Vector2 d,float t) { return GetSplinePointBasis(a,b,c,d,t); }
static Vector2 mr_catmull_adapter(Vector2 a,Vector2 b,Vector2 c,Vector2 d,float t) { return GetSplinePointCatmullRom(a,b,c,d,t); }
static Vector2 mr_quad_adapter(Vector2 a,Vector2 b,Vector2 c,Vector2 unused,float t) { (void)unused; return GetSplinePointBezierQuad(a,b,c,t); }
static Vector2 mr_cubic_adapter(Vector2 a,Vector2 b,Vector2 c,Vector2 d,float t) { return GetSplinePointBezierCubic(a,b,c,d,t); }
void DrawSplineSegmentLinear(Vector2 p1,Vector2 p2,float thick,Color color) { DrawLineEx(p1,p2,thick,color); }
void DrawSplineSegmentBasis(Vector2 p1,Vector2 p2,Vector2 p3,Vector2 p4,float thick,Color color) { mr_draw_spline4(p1,p2,p3,p4,thick,color,mr_basis_adapter); }
void DrawSplineSegmentCatmullRom(Vector2 p1,Vector2 p2,Vector2 p3,Vector2 p4,float thick,Color color) { mr_draw_spline4(p1,p2,p3,p4,thick,color,mr_catmull_adapter); }
void DrawSplineSegmentBezierQuadratic(Vector2 p1,Vector2 c2,Vector2 p3,float thick,Color color) { mr_draw_spline4(p1,c2,p3,p3,thick,color,mr_quad_adapter); }
void DrawSplineSegmentBezierCubic(Vector2 p1,Vector2 c2,Vector2 c3,Vector2 p4,float thick,Color color) { mr_draw_spline4(p1,c2,c3,p4,thick,color,mr_cubic_adapter); }
void DrawSplineLinear(const Vector2 *points,int count,float thick,Color color) {
    if (!points) return; for (int i=1;i<count;i++) DrawSplineSegmentLinear(points[i-1],points[i],thick,color);
}
void DrawSplineBasis(const Vector2 *points,int count,float thick,Color color) {
    if (!points) return; for (int i=0;i<count-3;i++) DrawSplineSegmentBasis(points[i],points[i+1],points[i+2],points[i+3],thick,color);
}
void DrawSplineCatmullRom(const Vector2 *points,int count,float thick,Color color) {
    if (!points) return; for (int i=0;i<count-3;i++) DrawSplineSegmentCatmullRom(points[i],points[i+1],points[i+2],points[i+3],thick,color);
}
void DrawSplineBezierQuadratic(const Vector2 *points,int count,float thick,Color color) {
    if (!points) return; for (int i=0;i+2<count;i+=2) DrawSplineSegmentBezierQuadratic(points[i],points[i+1],points[i+2],thick,color);
}
void DrawSplineBezierCubic(const Vector2 *points,int count,float thick,Color color) {
    if (!points) return; for (int i=0;i+3<count;i+=3) DrawSplineSegmentBezierCubic(points[i],points[i+1],points[i+2],points[i+3],thick,color);
}

static float mr_min(float a,float b) { return a<b ? a:b; }
static float mr_max(float a,float b) { return a>b ? a:b; }
static float mr_cross(Vector2 a,Vector2 b,Vector2 c) { return (b.x-a.x)*(c.y-a.y)-(b.y-a.y)*(c.x-a.x); }
bool CheckCollisionRecs(Rectangle a,Rectangle b) {
    return a.x<b.x+b.width && a.x+a.width>b.x && a.y<b.y+b.height && a.y+a.height>b.y;
}
bool CheckCollisionCircles(Vector2 a,float ar,Vector2 b,float br) {
    float x=a.x-b.x,y=a.y-b.y,r=ar+br; return x*x+y*y<=r*r;
}
bool CheckCollisionCircleRec(Vector2 center,float radius,Rectangle rec) {
    float x=mr_max(rec.x,mr_min(center.x,rec.x+rec.width));
    float y=mr_max(rec.y,mr_min(center.y,rec.y+rec.height));
    float dx=center.x-x,dy=center.y-y; return dx*dx+dy*dy<=radius*radius;
}
bool CheckCollisionCircleLine(Vector2 center,float radius,Vector2 p1,Vector2 p2) {
    float dx=p2.x-p1.x,dy=p2.y-p1.y,length=dx*dx+dy*dy;
    float t=length>0 ? ((center.x-p1.x)*dx+(center.y-p1.y)*dy)/length : 0;
    if (t<0) t=0; if (t>1) t=1;
    float x=p1.x+t*dx-center.x,y=p1.y+t*dy-center.y;
    return x*x+y*y<=radius*radius;
}
bool CheckCollisionPointRec(Vector2 p,Rectangle r) { return p.x>=r.x && p.x<=r.x+r.width && p.y>=r.y && p.y<=r.y+r.height; }
bool CheckCollisionPointCircle(Vector2 p,Vector2 c,float radius) { return CheckCollisionCircles(p,0,c,radius); }
bool CheckCollisionPointTriangle(Vector2 p,Vector2 a,Vector2 b,Vector2 c) {
    float d1=mr_cross(a,b,p),d2=mr_cross(b,c,p),d3=mr_cross(c,a,p);
    bool negative=d1<0 || d2<0 || d3<0,positive=d1>0 || d2>0 || d3>0;
    return !(negative && positive);
}
bool CheckCollisionPointLine(Vector2 point,Vector2 p1,Vector2 p2,int threshold) {
    return CheckCollisionCircleLine(point,(float)(threshold<0 ? 0:threshold),p1,p2);
}
bool CheckCollisionPointPoly(Vector2 p,const Vector2 *points,int count) {
    if (!points || count<3) return false;
    bool inside=false;
    for (int i=0,j=count-1;i<count;j=i++) if (((points[i].y>p.y)!=(points[j].y>p.y)) &&
        p.x<(points[j].x-points[i].x)*(p.y-points[i].y)/(points[j].y-points[i].y)+points[i].x) inside=!inside;
    return inside;
}
bool CheckCollisionLines(Vector2 a,Vector2 b,Vector2 c,Vector2 d,Vector2 *point) {
    float denominator=(b.x-a.x)*(d.y-c.y)-(b.y-a.y)*(d.x-c.x);
    if (denominator>-0.000001f && denominator<0.000001f) return false;
    float t=((c.x-a.x)*(d.y-c.y)-(c.y-a.y)*(d.x-c.x))/denominator;
    float u=((c.x-a.x)*(b.y-a.y)-(c.y-a.y)*(b.x-a.x))/denominator;
    if (t<0 || t>1 || u<0 || u>1) return false;
    if (point) *point=(Vector2){a.x+t*(b.x-a.x),a.y+t*(b.y-a.y)};
    return true;
}
Rectangle GetCollisionRec(Rectangle a,Rectangle b) {
    float x=mr_max(a.x,b.x),y=mr_max(a.y,b.y);
    float right=mr_min(a.x+a.width,b.x+b.width),bottom=mr_min(a.y+a.height,b.y+b.height);
    if (right<=x || bottom<=y) return (Rectangle){0};
    return (Rectangle){x,y,right-x,bottom-y};
}
