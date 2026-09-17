/* RayGPU core module. Compiled through raygpu.c; do not compile separately. */
static unsigned int mr_config_flags;
static void mr_error(const char *s) { puts(s); mr.error=true; mr.close=true; }
static void mr_shader_texture_changed(unsigned int textureId,bool removed);
static void mr_init_3d_defaults(void) {
    mr.ambientColor=WHITE;mr.ambientIntensity=0.22f;mr.pbrEnabled=true;
    mr.fogColor=(Color){128,140,155,255};mr.fogStart=10;mr.fogEnd=100;mr.fogDensity=0.02f;mr.fogMode=FOG_DISABLED;
    mr.lights[0]=(Light3D){true,LIGHT_DIRECTIONAL,{4.5f,8.5f,3.5f},{0,0,0},WHITE,1.0f,0,0.9f,0.8f};
    mr.skyboxTint=WHITE;
}
void SetConfigFlags(unsigned int flags) { mr_config_flags|=flags; }
#ifdef _WIN32
static WGPUStringView mr_string(const char *s) { return (WGPUStringView){s,WGPU_STRLEN}; }
static void mr_message(const char *prefix,WGPUStringView message) {
    fprintf(stderr,"raygpu: %s: ",prefix);
    if (message.data) fwrite(message.data,1,message.length==WGPU_STRLEN ? strlen(message.data) : message.length,stderr);
    fputc('\n',stderr);
}
#endif
static double mr_clock(void) {
#ifdef __wasm__
    return mr_web_now()/1000.0;
#else
    LARGE_INTEGER ticks,frequency; QueryPerformanceCounter(&ticks); QueryPerformanceFrequency(&frequency);
    return (double)ticks.QuadPart/(double)frequency.QuadPart;
#endif
}
#ifdef _WIN32
static void mr_yield(int ms) { Sleep((DWORD)ms); }
static void mr_limit_frame(void) {
    if (!mr.softwareFrameLimit || mr.fps<=0) return;
    const double deadline=mr.frameStart+1.0/mr.fps;
    /* Sleep for the coarse part and spin only for the final fraction. This
     * keeps accurate pacing without burning a CPU core while the game idles. */
    for (;;) {
        double remaining=deadline-mr_clock();if(remaining<=0)break;
        if(remaining>0.002)Sleep((DWORD)((remaining-0.001)*1000.0));
        else YieldProcessor();
    }
}
#endif
static void mr_key(int key,bool down) {
    if (key<=KEY_NULL || key>=512) return;
    if (down && !mr.keys[key]) {
        mr.pressed[key]=true;
        if (mr.keyQueueCount<16) mr.keyQueue[mr.keyQueueCount++]=key;
    }
    else if (down) mr.repeated[key]=true;
    if (!down && mr.keys[key]) mr.released[key]=true;
    mr.keys[key]=down;
}
static void mr_button(int button,bool down) {
    if (button<0 || button>=7) return;
    if (down && !mr.buttons[button]) mr.clicked[button]=true;
    if (!down && mr.buttons[button]) mr.buttonReleased[button]=true;
    mr.buttons[button]=down;
}
static float mr_distance(Vector2 a,Vector2 b) {
    float x=b.x-a.x,y=b.y-a.y;return sqrtf(x*x+y*y);
}
static float mr_gesture_angle(Vector2 from,Vector2 to) {
    float angle=atan2f(from.y-to.y,to.x-from.x)*(180.0f/MR_PI);
    return angle<0?angle+360.0f:angle;
}
static int mr_touch_index(int id) {
    for(int i=0;i<mr.touchCount;i++)if(mr.touches[i].id==id)return i;return -1;
}
static void mr_touch_event(int id,int action,float x,float y) {
    Vector2 position={x,y};int index=mr_touch_index(id);double now=mr_clock();
    if(action==0) {
        if(index<0&&mr.touchCount<MR_MAX_TOUCH_POINTS){index=mr.touchCount++;mr.touches[index]=(MRTouchPoint){id,position};}
        else if(index>=0)mr.touches[index].position=position;
        if(mr.touchCount==1) {
            bool doubleTap=mr.gestureLastTapTime>0&&now-mr.gestureLastTapTime<0.35&&mr_distance(position,mr.gestureLastTapPosition)<30.0f;
            mr.gestureDetected=doubleTap?GESTURE_DOUBLETAP:GESTURE_TAP;
            mr.gestureStart=position;mr.gestureDrag=(Vector2){0};mr.gestureStartTime=now;mr.gestureHoldStart=now;
        } else if(mr.touchCount==2) {
            Vector2 a=mr.touches[0].position,b=mr.touches[1].position;
            mr.gesturePinch=(Vector2){b.x-a.x,b.y-a.y};mr.gesturePinchDistance=mr_distance(a,b);
            mr.gesturePinchAngle=mr_gesture_angle(a,b);mr.gestureDetected=GESTURE_HOLD;mr.gestureHoldStart=now;
        }
        return;
    }
    if(index<0)return;
    mr.touches[index].position=position;
    if(action==1) {
        if(mr.touchCount==1) {
            mr.gestureDrag=(Vector2){position.x-mr.gestureStart.x,position.y-mr.gestureStart.y};
            if(mr_distance(mr.gestureStart,position)>=6.0f)mr.gestureDetected=GESTURE_DRAG;
        } else if(mr.touchCount>=2) {
            Vector2 a=mr.touches[0].position,b=mr.touches[1].position;
            float distance=mr_distance(a,b);
            mr.gesturePinch=(Vector2){b.x-a.x,b.y-a.y};mr.gesturePinchAngle=mr_gesture_angle(a,b);
            if(distance>mr.gesturePinchDistance+2.0f)mr.gestureDetected=GESTURE_PINCH_OUT;
            else if(distance<mr.gesturePinchDistance-2.0f)mr.gestureDetected=GESTURE_PINCH_IN;
            else mr.gestureDetected=GESTURE_HOLD;
            mr.gesturePinchDistance=distance;
        }
        return;
    }
    if(mr.touchCount==1) {
        float distance=mr_distance(mr.gestureStart,position);double elapsed=now-mr.gestureStartTime;
        mr.gestureDrag=(Vector2){position.x-mr.gestureStart.x,position.y-mr.gestureStart.y};
        mr.gestureDragAngle=mr_gesture_angle(mr.gestureStart,position);
        if(distance>=50.0f&&elapsed<=0.75) {
            if(mr.gestureDragAngle<45||mr.gestureDragAngle>=315)mr.gestureDetected=GESTURE_SWIPE_RIGHT;
            else if(mr.gestureDragAngle<135)mr.gestureDetected=GESTURE_SWIPE_UP;
            else if(mr.gestureDragAngle<225)mr.gestureDetected=GESTURE_SWIPE_LEFT;
            else mr.gestureDetected=GESTURE_SWIPE_DOWN;
        } else if(distance<10.0f&&elapsed<=0.35) {
            bool doubleTap=mr.gestureLastTapTime>0&&now-mr.gestureLastTapTime<0.35&&mr_distance(position,mr.gestureLastTapPosition)<30.0f;
            mr.gestureDetected=doubleTap?GESTURE_DOUBLETAP:GESTURE_TAP;
            mr.gestureLastTapTime=now;mr.gestureLastTapPosition=position;
        }
    }
    for(int i=index+1;i<mr.touchCount;i++)mr.touches[i-1]=mr.touches[i];
    mr.touchCount--;
    if(mr.touchCount==1){mr.gestureStart=mr.touches[0].position;mr.gestureStartTime=now;mr.gestureHoldStart=now;mr.gestureDrag=(Vector2){0};}
    else if(mr.touchCount==0){mr.gesturePinch=(Vector2){0};mr.gesturePinchDistance=0;}
}
static void mr_update_gestures(void) {
    if(mr.touchCount==1){float distance=mr_distance(mr.gestureStart,mr.touches[0].position);if(distance>=6.0f)mr.gestureDetected=GESTURE_DRAG;else if(mr_clock()-mr.gestureStartTime>=0.5)mr.gestureDetected=GESTURE_HOLD;}
}
static void mr_gamepad_state(int gamepad,bool available,const char *name,const bool *buttons,const float *axes) {
    if(gamepad<0||gamepad>=MR_MAX_GAMEPADS)return;MRGamepadState *state=&mr.gamepads[gamepad];
    for(int button=1;button<MR_GAMEPAD_BUTTONS;button++){
        bool down=available&&buttons&&buttons[button];
        if(down&&!state->buttons[button]){state->pressed[button]=true;mr.gamepadLastButton=button;}
        if(!down&&state->buttons[button])state->released[button]=true;
        state->buttons[button]=down;
    }
    state->available=available;
    if(available&&axes)memcpy(state->axes,axes,sizeof state->axes);else memset(state->axes,0,sizeof state->axes);
    if(available&&name){size_t length=0;while(name[length]&&length+1<sizeof state->name)length++;memcpy(state->name,name,length);state->name[length]=0;}
    else state->name[0]=0;
}
static void mr_mouse(float x,float y) {
    x=x*mr.mouseScale.x+mr.mouseOffset.x; y=y*mr.mouseScale.y+mr.mouseOffset.y;
    mr.mouseDelta.x+=x-mr.mouse.x; mr.mouseDelta.y+=y-mr.mouse.y;
    mr.mouse=(Vector2){x,y};
}
static void mr_clear_input(void) {
    memset(mr.keys,0,sizeof mr.keys); memset(mr.pressed,0,sizeof mr.pressed);
    memset(mr.repeated,0,sizeof mr.repeated); memset(mr.released,0,sizeof mr.released);
    memset(mr.buttons,0,sizeof mr.buttons); memset(mr.clicked,0,sizeof mr.clicked);
    memset(mr.buttonReleased,0,sizeof mr.buttonReleased);
    mr.touchCount=0;mr.gestureDetected=GESTURE_NONE;
    mr.mouseDelta=(Vector2){0}; mr.wheel=(Vector2){0};
    mr.keyQueueCount=0; mr.charQueueCount=0;
}
static void mr_finish_input_frame(void) {
    memset(mr.pressed,0,sizeof mr.pressed); memset(mr.repeated,0,sizeof mr.repeated);
    memset(mr.released,0,sizeof mr.released); memset(mr.clicked,0,sizeof mr.clicked);
    memset(mr.buttonReleased,0,sizeof mr.buttonReleased);
    for(int gamepad=0;gamepad<MR_MAX_GAMEPADS;gamepad++){memset(mr.gamepads[gamepad].pressed,0,sizeof mr.gamepads[gamepad].pressed);memset(mr.gamepads[gamepad].released,0,sizeof mr.gamepads[gamepad].released);}
    mr.gamepadLastButton=GAMEPAD_BUTTON_UNKNOWN;if(mr.touchCount==0)mr.gestureDetected=GESTURE_NONE;
    mr.mouseDelta=(Vector2){0}; mr.wheel=(Vector2){0};
    mr.keyQueueCount=0; mr.charQueueCount=0; mr.resized=false;
}
#ifdef _WIN32
static int mr_translate_key(WPARAM key,LPARAM detail) {
    if ((key>='0' && key<='9') || (key>='A' && key<='Z')) return (int)key;
    if (key>=VK_F1 && key<=VK_F12) return KEY_F1+(int)(key-VK_F1);
    if (key>=VK_NUMPAD0 && key<=VK_NUMPAD9) return KEY_KP_0+(int)(key-VK_NUMPAD0);
    switch (key) {
    case VK_SPACE: return KEY_SPACE; case VK_ESCAPE: return KEY_ESCAPE;
    case VK_RETURN: return (detail&(1L<<24)) ? KEY_KP_ENTER : KEY_ENTER;
    case VK_TAB: return KEY_TAB; case VK_BACK: return KEY_BACKSPACE;
    case VK_INSERT: return KEY_INSERT; case VK_DELETE: return KEY_DELETE;
    case VK_RIGHT: return KEY_RIGHT; case VK_LEFT: return KEY_LEFT;
    case VK_DOWN: return KEY_DOWN; case VK_UP: return KEY_UP;
    case VK_PRIOR: return KEY_PAGE_UP; case VK_NEXT: return KEY_PAGE_DOWN;
    case VK_HOME: return KEY_HOME; case VK_END: return KEY_END;
    case VK_CAPITAL: return KEY_CAPS_LOCK; case VK_SCROLL: return KEY_SCROLL_LOCK;
    case VK_NUMLOCK: return KEY_NUM_LOCK; case VK_SNAPSHOT: return KEY_PRINT_SCREEN;
    case VK_PAUSE: return KEY_PAUSE;
    case VK_SHIFT: {
        UINT translated=MapVirtualKeyA((UINT)((detail>>16)&0xff),MAPVK_VSC_TO_VK_EX);
        return translated==VK_RSHIFT ? KEY_RIGHT_SHIFT : KEY_LEFT_SHIFT;
    }
    case VK_LSHIFT: return KEY_LEFT_SHIFT; case VK_RSHIFT: return KEY_RIGHT_SHIFT;
    case VK_CONTROL: return (detail&(1L<<24)) ? KEY_RIGHT_CONTROL : KEY_LEFT_CONTROL;
    case VK_LCONTROL: return KEY_LEFT_CONTROL; case VK_RCONTROL: return KEY_RIGHT_CONTROL;
    case VK_MENU: return (detail&(1L<<24)) ? KEY_RIGHT_ALT : KEY_LEFT_ALT;
    case VK_LMENU: return KEY_LEFT_ALT; case VK_RMENU: return KEY_RIGHT_ALT;
    case VK_LWIN: return KEY_LEFT_SUPER; case VK_RWIN: return KEY_RIGHT_SUPER;
    case VK_APPS: return KEY_KB_MENU;
    case VK_DECIMAL: return KEY_KP_DECIMAL; case VK_DIVIDE: return KEY_KP_DIVIDE;
    case VK_MULTIPLY: return KEY_KP_MULTIPLY; case VK_SUBTRACT: return KEY_KP_SUBTRACT;
    case VK_ADD: return KEY_KP_ADD;
    case VK_OEM_7: return KEY_APOSTROPHE; case VK_OEM_COMMA: return KEY_COMMA;
    case VK_OEM_MINUS: return KEY_MINUS; case VK_OEM_PERIOD: return KEY_PERIOD;
    case VK_OEM_2: return KEY_SLASH; case VK_OEM_1: return KEY_SEMICOLON;
    case VK_OEM_PLUS: return KEY_EQUAL; case VK_OEM_4: return KEY_LEFT_BRACKET;
    case VK_OEM_5: return KEY_BACKSLASH; case VK_OEM_6: return KEY_RIGHT_BRACKET;
    case VK_OEM_3: return KEY_GRAVE;
    case VK_VOLUME_UP: return KEY_VOLUME_UP; case VK_VOLUME_DOWN: return KEY_VOLUME_DOWN;
    default: return KEY_NULL;
    }
}
typedef struct MRMonitorList { HMONITOR handles[16]; int count; } MRMonitorList;
static BOOL CALLBACK mr_collect_monitor(HMONITOR monitor,HDC dc,LPRECT rectangle,LPARAM userData) {
    (void)dc; (void)rectangle;
    MRMonitorList *list=(MRMonitorList *)userData;
    if (list->count<16) list->handles[list->count++]=monitor;
    return TRUE;
}
static MRMonitorList mr_get_monitors(void) {
    MRMonitorList list={0}; EnumDisplayMonitors(NULL,NULL,mr_collect_monitor,(LPARAM)&list); return list;
}
static HMONITOR mr_get_monitor(int index) {
    MRMonitorList list=mr_get_monitors(); return index>=0&&index<list.count ? list.handles[index] : NULL;
}
static int mr_monitor_index(HMONITOR monitor) {
    MRMonitorList list=mr_get_monitors(); for(int i=0;i<list.count;i++)if(list.handles[i]==monitor)return i; return 0;
}
static DWORD mr_window_style(unsigned int flags) {
    if(flags&FLAG_WINDOW_UNDECORATED)return WS_POPUP;
    DWORD style=WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX;
    if(flags&FLAG_WINDOW_RESIZABLE)style|=WS_THICKFRAME|WS_MAXIMIZEBOX;
    return style;
}
typedef DWORD (WINAPI *MRXInputGetState)(DWORD,XINPUT_STATE *);
typedef DWORD (WINAPI *MRXInputSetState)(DWORD,XINPUT_VIBRATION *);
static HMODULE mr_xinput_module;
static MRXInputGetState mr_xinput_get_state;
static MRXInputSetState mr_xinput_set_state;
static void mr_xinput_init(void) {
    if(mr_xinput_module)return;
    const char *libraries[]={"xinput1_4.dll","xinput1_3.dll","xinput9_1_0.dll"};
    for(int i=0;i<3&&!mr_xinput_module;i++)mr_xinput_module=LoadLibraryA(libraries[i]);
    if(mr_xinput_module){mr_xinput_get_state=(MRXInputGetState)(void *)GetProcAddress(mr_xinput_module,"XInputGetState");mr_xinput_set_state=(MRXInputSetState)(void *)GetProcAddress(mr_xinput_module,"XInputSetState");}
}
static float mr_xinput_stick(SHORT value) { return value<0?(float)value/32768.0f:(float)value/32767.0f; }
static void mr_poll_gamepads(void) {
    mr_xinput_init();
    for(int gamepad=0;gamepad<MR_MAX_GAMEPADS;gamepad++) {
        XINPUT_STATE input={0};bool down[MR_GAMEPAD_BUTTONS]={0};float axes[MR_GAMEPAD_AXES]={0};char name[32];
        bool available=mr_xinput_get_state&&mr_xinput_get_state((DWORD)gamepad,&input)==ERROR_SUCCESS;
        if(available) {
            WORD b=input.Gamepad.wButtons;
            down[GAMEPAD_BUTTON_LEFT_FACE_UP]=(b&XINPUT_GAMEPAD_DPAD_UP)!=0;down[GAMEPAD_BUTTON_LEFT_FACE_RIGHT]=(b&XINPUT_GAMEPAD_DPAD_RIGHT)!=0;
            down[GAMEPAD_BUTTON_LEFT_FACE_DOWN]=(b&XINPUT_GAMEPAD_DPAD_DOWN)!=0;down[GAMEPAD_BUTTON_LEFT_FACE_LEFT]=(b&XINPUT_GAMEPAD_DPAD_LEFT)!=0;
            down[GAMEPAD_BUTTON_RIGHT_FACE_UP]=(b&XINPUT_GAMEPAD_Y)!=0;down[GAMEPAD_BUTTON_RIGHT_FACE_RIGHT]=(b&XINPUT_GAMEPAD_B)!=0;
            down[GAMEPAD_BUTTON_RIGHT_FACE_DOWN]=(b&XINPUT_GAMEPAD_A)!=0;down[GAMEPAD_BUTTON_RIGHT_FACE_LEFT]=(b&XINPUT_GAMEPAD_X)!=0;
            down[GAMEPAD_BUTTON_LEFT_TRIGGER_1]=(b&XINPUT_GAMEPAD_LEFT_SHOULDER)!=0;down[GAMEPAD_BUTTON_RIGHT_TRIGGER_1]=(b&XINPUT_GAMEPAD_RIGHT_SHOULDER)!=0;
            down[GAMEPAD_BUTTON_LEFT_TRIGGER_2]=input.Gamepad.bLeftTrigger>XINPUT_GAMEPAD_TRIGGER_THRESHOLD;
            down[GAMEPAD_BUTTON_RIGHT_TRIGGER_2]=input.Gamepad.bRightTrigger>XINPUT_GAMEPAD_TRIGGER_THRESHOLD;
            down[GAMEPAD_BUTTON_MIDDLE_LEFT]=(b&XINPUT_GAMEPAD_BACK)!=0;down[GAMEPAD_BUTTON_MIDDLE_RIGHT]=(b&XINPUT_GAMEPAD_START)!=0;
            down[GAMEPAD_BUTTON_LEFT_THUMB]=(b&XINPUT_GAMEPAD_LEFT_THUMB)!=0;down[GAMEPAD_BUTTON_RIGHT_THUMB]=(b&XINPUT_GAMEPAD_RIGHT_THUMB)!=0;
            axes[GAMEPAD_AXIS_LEFT_X]=mr_xinput_stick(input.Gamepad.sThumbLX);axes[GAMEPAD_AXIS_LEFT_Y]=-mr_xinput_stick(input.Gamepad.sThumbLY);
            axes[GAMEPAD_AXIS_RIGHT_X]=mr_xinput_stick(input.Gamepad.sThumbRX);axes[GAMEPAD_AXIS_RIGHT_Y]=-mr_xinput_stick(input.Gamepad.sThumbRY);
            axes[GAMEPAD_AXIS_LEFT_TRIGGER]=(float)input.Gamepad.bLeftTrigger/127.5f-1.0f;axes[GAMEPAD_AXIS_RIGHT_TRIGGER]=(float)input.Gamepad.bRightTrigger/127.5f-1.0f;
            snprintf(name,sizeof name,"XInput Gamepad %d",gamepad+1);
        }
        mr_gamepad_state(gamepad,available,name,down,axes);
        if(mr.gamepads[gamepad].vibrationEnd>0&&mr_clock()>=mr.gamepads[gamepad].vibrationEnd){XINPUT_VIBRATION vibration={0};if(mr_xinput_set_state)mr_xinput_set_state((DWORD)gamepad,&vibration);mr.gamepads[gamepad].vibrationEnd=0;}
    }
}
static HCURSOR mr_cursor_handle(int cursor) {
    const char *id=IDC_ARROW;
    switch(cursor){case MOUSE_CURSOR_IBEAM:id=IDC_IBEAM;break;case MOUSE_CURSOR_CROSSHAIR:id=IDC_CROSS;break;case MOUSE_CURSOR_POINTING_HAND:id=IDC_HAND;break;case MOUSE_CURSOR_RESIZE_EW:id=IDC_SIZEWE;break;case MOUSE_CURSOR_RESIZE_NS:id=IDC_SIZENS;break;case MOUSE_CURSOR_RESIZE_NWSE:id=IDC_SIZENWSE;break;case MOUSE_CURSOR_RESIZE_NESW:id=IDC_SIZENESW;break;case MOUSE_CURSOR_RESIZE_ALL:id=IDC_SIZEALL;break;case MOUSE_CURSOR_NOT_ALLOWED:id=IDC_NO;break;default:break;}
    return LoadCursorA(NULL,id);
}
static void mr_apply_cursor(void) { SetCursor((mr.cursorHidden||mr.cursorDisabled)?NULL:mr_cursor_handle(mr.cursorShape)); }
static void mr_apply_cursor_clip(void) {
    if(!mr.cursorDisabled||!mr.window||!mr.focused){ClipCursor(NULL);return;}
    RECT area;if(GetClientRect(mr.window,&area)){POINT a={area.left,area.top},b={area.right,area.bottom};ClientToScreen(mr.window,&a);ClientToScreen(mr.window,&b);area=(RECT){a.x,a.y,b.x,b.y};ClipCursor(&area);}
}
static bool mr_mouse_message_is_touch(void) { return ((unsigned long)GetMessageExtraInfo()&0xffffff00u)==0xff515700u; }
static void mr_release_path_list(FilePathList files) {
    if(files.paths){for(unsigned int i=0;i<files.count;i++)MemFree(files.paths[i]);MemFree(files.paths);}
}
static void mr_set_fullscreen_mode(bool fullscreen,bool borderless) {
    if(!mr.window)return;
    if(!fullscreen&&!borderless){
        if(!mr.fullscreen&&!mr.borderless)return;
        SetWindowLongPtrA(mr.window,GWL_STYLE,mr.windowedStyle);
        SetWindowLongPtrA(mr.window,GWL_EXSTYLE,mr.windowedExStyle);
        SetWindowPlacement(mr.window,&mr.windowedPlacement);
        SetWindowPos(mr.window,NULL,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE|SWP_FRAMECHANGED);
        mr.fullscreen=mr.borderless=false; return;
    }
    if(!mr.fullscreen&&!mr.borderless){
        mr.windowedStyle=GetWindowLongPtrA(mr.window,GWL_STYLE);
        mr.windowedExStyle=GetWindowLongPtrA(mr.window,GWL_EXSTYLE);
        mr.windowedPlacement=(WINDOWPLACEMENT){0};mr.windowedPlacement.length=sizeof mr.windowedPlacement;
        GetWindowPlacement(mr.window,&mr.windowedPlacement);
    }
    HMONITOR monitor=MonitorFromWindow(mr.window,MONITOR_DEFAULTTONEAREST);
    MONITORINFO info={0};info.cbSize=sizeof info;if(!GetMonitorInfoA(monitor,&info))return;
    LONG_PTR currentStyle=GetWindowLongPtrA(mr.window,GWL_STYLE);
    SetWindowLongPtrA(mr.window,GWL_STYLE,WS_POPUP|(currentStyle&WS_VISIBLE));
    SetWindowLongPtrA(mr.window,GWL_EXSTYLE,mr.windowedExStyle&~(WS_EX_CLIENTEDGE|WS_EX_WINDOWEDGE));
    SetWindowPos(mr.window,HWND_TOP,info.rcMonitor.left,info.rcMonitor.top,
        info.rcMonitor.right-info.rcMonitor.left,info.rcMonitor.bottom-info.rcMonitor.top,
        SWP_NOACTIVATE|SWP_FRAMECHANGED);
    mr.fullscreen=fullscreen;mr.borderless=borderless;
}
static LRESULT CALLBACK mr_window_proc(HWND window,UINT message,WPARAM w,LPARAM l) {
    switch(message) {
    case WM_CLOSE: mr.close=true; return 0;
    case WM_SIZE: mr.width=LOWORD(l); mr.height=HIWORD(l); mr.resized=true;if(mr.cursorDisabled)mr_apply_cursor_clip();return 0;
    case WM_SETFOCUS: mr.focused=true; mr_apply_cursor_clip(); return 0;
    case WM_KILLFOCUS: mr.focused=false; ClipCursor(NULL); mr_clear_input(); return 0;
    case WM_SETCURSOR: if(LOWORD(l)==HTCLIENT){mr_apply_cursor();return TRUE;}break;
    case WM_MOUSELEAVE: mr.cursorOnScreen=false;mr.mouseTracking=false;return 0;
    case WM_CHAR:
        if ((unsigned int)w>=32 && mr.charQueueCount<16) mr.charQueue[mr.charQueueCount++]=(int)w;
        return 0;
    case WM_GETMINMAXINFO: {
        MINMAXINFO *limits=(MINMAXINFO *)l;
        RECT minRect={0,0,mr.minWidth,mr.minHeight},maxRect={0,0,mr.maxWidth,mr.maxHeight};
        DWORD style=(DWORD)(mr.window?GetWindowLongPtrA(mr.window,GWL_STYLE):mr_window_style(mr.flags));
        if (mr.minWidth>0 && mr.minHeight>0) { AdjustWindowRect(&minRect,style,FALSE); limits->ptMinTrackSize=(POINT){minRect.right-minRect.left,minRect.bottom-minRect.top}; }
        if (mr.maxWidth>0 && mr.maxHeight>0) { AdjustWindowRect(&maxRect,style,FALSE); limits->ptMaxTrackSize=(POINT){maxRect.right-maxRect.left,maxRect.bottom-maxRect.top}; }
        return 0;
    }
    case WM_KEYDOWN: case WM_SYSKEYDOWN: case WM_KEYUP: case WM_SYSKEYUP: {
        int key=mr_translate_key(w,l);
        mr_key(key,message==WM_KEYDOWN || message==WM_SYSKEYDOWN); return 0;
    }
    case WM_MOUSEMOVE: {
        int x=GET_X_LPARAM(l),y=GET_Y_LPARAM(l);
        if(mr.cursorDisabled&&mr.focused){int centerX=mr.width/2,centerY=mr.height/2;mr.mouseDelta.x+=(float)(x-centerX);mr.mouseDelta.y+=(float)(y-centerY);mr.mouse=(Vector2){(float)centerX,(float)centerY};if(x!=centerX||y!=centerY){POINT center={centerX,centerY};ClientToScreen(window,&center);SetCursorPos(center.x,center.y);}}
        else mr_mouse((float)x,(float)y);
        mr.cursorOnScreen=true;
        if(!mr.mouseTracking){TRACKMOUSEEVENT tracking={sizeof tracking,TME_LEAVE,window,0};TrackMouseEvent(&tracking);mr.mouseTracking=true;}
        if(mr.buttons[MOUSE_BUTTON_LEFT]&&!mr_mouse_message_is_touch())mr_touch_event(-1,1,(float)x,(float)y);
        return 0;
    }
    case WM_MOUSEWHEEL: mr.wheel.y+=(float)GET_WHEEL_DELTA_WPARAM(w)/(float)WHEEL_DELTA; return 0;
    case WM_MOUSEHWHEEL: mr.wheel.x+=(float)GET_WHEEL_DELTA_WPARAM(w)/(float)WHEEL_DELTA; return 0;
    case WM_LBUTTONDOWN: mr_button(0,true);if(!mr_mouse_message_is_touch())mr_touch_event(-1,0,(float)GET_X_LPARAM(l),(float)GET_Y_LPARAM(l));SetCapture(window); return 0;
    case WM_RBUTTONDOWN: mr_button(1,true); SetCapture(window); return 0;
    case WM_MBUTTONDOWN: mr_button(2,true); SetCapture(window); return 0;
    case WM_LBUTTONUP: if(!mr_mouse_message_is_touch())mr_touch_event(-1,2,(float)GET_X_LPARAM(l),(float)GET_Y_LPARAM(l));mr_button(0,false); if (!mr.buttons[1] && !mr.buttons[2]) ReleaseCapture(); return 0;
    case WM_RBUTTONUP: mr_button(1,false); if (!mr.buttons[0] && !mr.buttons[2]) ReleaseCapture(); return 0;
    case WM_MBUTTONUP: mr_button(2,false); if (!mr.buttons[0] && !mr.buttons[1]) ReleaseCapture(); return 0;
    case WM_XBUTTONDOWN: {int button=GET_XBUTTON_WPARAM(w)==XBUTTON1?MOUSE_BUTTON_SIDE:MOUSE_BUTTON_EXTRA;mr_button(button,true);SetCapture(window);return TRUE;}
    case WM_XBUTTONUP: {int button=GET_XBUTTON_WPARAM(w)==XBUTTON1?MOUSE_BUTTON_SIDE:MOUSE_BUTTON_EXTRA;mr_button(button,false);if(!mr.buttons[0]&&!mr.buttons[1]&&!mr.buttons[2]&&!mr.buttons[3]&&!mr.buttons[4])ReleaseCapture();return TRUE;}
    case WM_POINTERDOWN: case WM_POINTERUPDATE: case WM_POINTERUP: {
        POINTER_INFO info;if(GetPointerInfo(GET_POINTERID_WPARAM(w),&info)&&(info.pointerType==PT_TOUCH||info.pointerType==PT_PEN)){
            POINT point=info.ptPixelLocation;ScreenToClient(window,&point);
            int action=message==WM_POINTERDOWN?0:(message==WM_POINTERUP?2:1);
            mr_touch_event((int)info.pointerId,action,(float)point.x,(float)point.y);return 0;
        }
        break;
    }
    case WM_CAPTURECHANGED: memset(mr.buttons,0,sizeof mr.buttons); return 0;
    case WM_DROPFILES: {
        HDROP drop=(HDROP)w;UINT count=DragQueryFileW(drop,0xffffffffu,NULL,0);
        mr_release_path_list(mr.droppedFiles);mr.droppedFiles=(FilePathList){0};
        if(count){mr.droppedFiles.paths=MemAlloc(count*sizeof *mr.droppedFiles.paths);if(mr.droppedFiles.paths){memset(mr.droppedFiles.paths,0,count*sizeof *mr.droppedFiles.paths);mr.droppedFiles.capacity=count;for(UINT i=0;i<count;i++){UINT length=DragQueryFileW(drop,i,NULL,0);WCHAR *wide=MemAlloc((length+1)*sizeof *wide);if(!wide||!DragQueryFileW(drop,i,wide,length+1)){MemFree(wide);continue;}int bytes=WideCharToMultiByte(CP_UTF8,0,wide,-1,NULL,0,NULL,NULL);char *path=bytes>0?MemAlloc((unsigned int)bytes):NULL;if(path){WideCharToMultiByte(CP_UTF8,0,wide,-1,path,bytes,NULL,NULL);mr.droppedFiles.paths[mr.droppedFiles.count++]=path;}MemFree(wide);}}}
        DragFinish(drop);return 0;
    }
    }
    return DefWindowProcA(window,message,w,l);
}
static void mr_pump(void) {
    MSG message;
    while (PeekMessageA(&message,NULL,0,0,PM_REMOVE)) {
        if (message.message==WM_QUIT) mr.close=true;
        TranslateMessage(&message); DispatchMessageA(&message);
    }
    mr_poll_gamepads();mr_update_gestures();
    if (mr.instance) wgpuInstanceProcessEvents(mr.instance);
}
static void mr_adapter(WGPURequestAdapterStatus status,WGPUAdapter adapter,WGPUStringView message,void *a,void *b) {
    (void)a; (void)b;
    if (status==WGPURequestAdapterStatus_Success) mr.adapter=adapter;
    else { mr_message("adapter request failed",message); mr_error("No WebGPU adapter available"); }
    mr.adapterDone=true;
}
static void mr_device(WGPURequestDeviceStatus status,WGPUDevice device,WGPUStringView message,void *a,void *b) {
    (void)a; (void)b;
    if (status==WGPURequestDeviceStatus_Success) mr.device=device;
    else { mr_message("device request failed",message); mr_error("Could not create device"); }
    mr.deviceDone=true;
}
static void mr_gpu_error(WGPUDevice const *device,WGPUErrorType type,WGPUStringView message,void *a,void *b) {
    (void)device; (void)type; (void)a; (void)b;
    mr_message("WebGPU error",message); mr.error=true; mr.close=true;
}
static void mr_device_lost(WGPUDevice const *device,WGPUDeviceLostReason reason,WGPUStringView message,void *a,void *b) {
    (void)device; (void)a; (void)b;
    if (reason==WGPUDeviceLostReason_Destroyed || reason==WGPUDeviceLostReason_CallbackCancelled) return;
    mr_message("device lost",message); mr.error=true; mr.close=true;
}
#else
static void mr_poll_gamepads(void) {
    for(int gamepad=0;gamepad<MR_MAX_GAMEPADS;gamepad++){
        bool down[MR_GAMEPAD_BUTTONS]={0};float axes[MR_GAMEPAD_AXES]={0};char name[128]={0};bool available=mr_web_gamepad_available(gamepad)!=0;
        if(available){mr_web_gamepad_name(gamepad,name,sizeof name);for(int button=1;button<MR_GAMEPAD_BUTTONS;button++)down[button]=mr_web_gamepad_button(gamepad,button)!=0;for(int axis=0;axis<MR_GAMEPAD_AXES;axis++)axes[axis]=mr_web_gamepad_axis(gamepad,axis);}
        mr_gamepad_state(gamepad,available,name,down,axes);
    }
}
static void mr_pump(void) { mr_poll_gamepads();mr_update_gestures(); }
#endif
static MRTexture *mr_texture(unsigned int id) {
    if (!id) return NULL;
    for (int i=0;i<MR_MAX_TEXTURES;i++) if (mr.textures[i].id==id) return &mr.textures[i];
    return NULL;
}
#ifdef _WIN32
Texture2D LoadTextureRGBA(const unsigned char *pixels,int width,int height) {
    if (!mr.device || !pixels || width<=0 || height<=0 || width>8192 || height>8192) return (Texture2D){0};
    MRTexture *t=NULL;
    for (int i=0;i<MR_MAX_TEXTURES;i++) if (!mr.textures[i].id) { t=&mr.textures[i]; break; }
    if (!t) { fprintf(stderr,"raygpu: texture limit reached\n"); return (Texture2D){0}; }
    WGPUTextureDescriptor desc=WGPU_TEXTURE_DESCRIPTOR_INIT;
    desc.size=(WGPUExtent3D){(uint32_t)width,(uint32_t)height,1};
    desc.dimension=WGPUTextureDimension_2D; desc.format=WGPUTextureFormat_RGBA8Unorm;
    desc.usage=WGPUTextureUsage_TextureBinding|WGPUTextureUsage_CopyDst|WGPUTextureUsage_CopySrc;
    t->texture=wgpuDeviceCreateTexture(mr.device,&desc);
    t->view=wgpuTextureCreateView(t->texture,NULL);
    WGPUTexelCopyTextureInfo destination=WGPU_TEXEL_COPY_TEXTURE_INFO_INIT; destination.texture=t->texture;
    /* Explicit initialization avoids an IntelliSense bug with the Windows
     * SDK's UINT32_MAX literal used by Dawn's initializer macro. */
    WGPUTexelCopyBufferLayout layout={0};
    layout.bytesPerRow=(uint32_t)width*4; layout.rowsPerImage=(uint32_t)height;
    wgpuQueueWriteTexture(mr.queue,&destination,pixels,(size_t)width*height*4,&layout,&desc.size);
    WGPUBindGroupEntry entries[2]={WGPU_BIND_GROUP_ENTRY_INIT,WGPU_BIND_GROUP_ENTRY_INIT};
    entries[0].binding=0; entries[0].sampler=mr.sampler;
    entries[1].binding=1; entries[1].textureView=t->view;
    WGPUBindGroupDescriptor group=WGPU_BIND_GROUP_DESCRIPTOR_INIT;
    group.layout=mr.textureLayout; group.entryCount=2; group.entries=entries;
    t->group=wgpuDeviceCreateBindGroup(mr.device,&group);
    t->pixels=MemAlloc((unsigned int)((size_t)width*height*4));if(!t->pixels){wgpuBindGroupRelease(t->group);wgpuTextureViewRelease(t->view);wgpuTextureRelease(t->texture);memset(t,0,sizeof*t);return(Texture2D){0};}memcpy(t->pixels,pixels,(size_t)width*height*4);t->width=width;t->height=height;t->mipmaps=1;t->id=++mr.nextTexture;
    return (Texture2D){t->id,width,height,1,7};
}
#else
Texture2D LoadTextureRGBA(const unsigned char *pixels,int width,int height) {
    if (!mr.ready || !pixels || width<=0 || height<=0 || width>8192 || height>8192) return (Texture2D){0};
    for (int i=0;i<MR_MAX_TEXTURES;i++) if (!mr.textures[i].id) {
        unsigned int id=++mr.nextTexture;
        mr.textures[i].pixels=MemAlloc((unsigned int)((size_t)width*height*4));if(!mr.textures[i].pixels)return(Texture2D){0};memcpy(mr.textures[i].pixels,pixels,(size_t)width*height*4);mr.textures[i].width=width;mr.textures[i].height=height;mr.textures[i].mipmaps=1;mr.textures[i].id=id; mr_web_texture(id,pixels,width,height);
        return (Texture2D){id,width,height,1,7};
    }
    puts("raygpu: texture limit reached"); return (Texture2D){0};
}
#endif
void UnloadTexture(Texture2D texture) {
    MRTexture *t=mr_texture(texture.id); if (!t || texture.id==mr.white) return;
    if (mr.drawing) { puts("raygpu: unload textures outside BeginDrawing/EndDrawing"); return; }
    if(!mr.close)mr_shader_texture_changed(texture.id,true);
#ifdef _WIN32
    wgpuBindGroupRelease(t->group); if (t->customSampler) wgpuSamplerRelease(t->customSampler);
    if (t->depthView) wgpuTextureViewRelease(t->depthView);
    if (t->depthTexture) wgpuTextureRelease(t->depthTexture);
    wgpuTextureViewRelease(t->view); wgpuTextureRelease(t->texture);
#else
    mr_web_unload(t->id);
#endif
    if (texture.id==mr.shapesTexture.id) {
        mr.shapesTexture=(Texture2D){mr.white,1,1,1,7}; mr.shapesSource=(Rectangle){0,0,1,1};
    }
    MemFree(t->pixels);memset(t,0,sizeof *t);
}
static MRShaderEntry *mr_shader(unsigned int id) {
    if(!id)return NULL;for(int i=0;i<32;i++)if(mr.shaders[i].id==id)return &mr.shaders[i];return NULL;
}
void UpdateTextureRec(Texture2D texture,Rectangle rec,const void *pixels) {
    MRTexture *t=mr_texture(texture.id); if (!t || !pixels) return;
    int x=(int)rec.x,y=(int)rec.y,width=(int)rec.width,height=(int)rec.height;
    if (x<0 || y<0 || width<=0 || height<=0 || x+width>texture.width || y+height>texture.height) return;
    if(t->pixels&&!t->renderTarget)for(int row=0;row<height;row++)memcpy(t->pixels+((size_t)(y+row)*t->width+x)*4,(const unsigned char*)pixels+(size_t)row*width*4,(size_t)width*4);
#ifdef _WIN32
    WGPUTexelCopyTextureInfo destination=WGPU_TEXEL_COPY_TEXTURE_INFO_INIT;
    destination.texture=t->texture; destination.origin=(WGPUOrigin3D){(uint32_t)x,(uint32_t)y,0};
    WGPUTexelCopyBufferLayout layout={0}; layout.bytesPerRow=(uint32_t)width*4; layout.rowsPerImage=(uint32_t)height;
    WGPUExtent3D extent={(uint32_t)width,(uint32_t)height,1};
    wgpuQueueWriteTexture(mr.queue,&destination,pixels,(size_t)width*height*4,&layout,&extent);
#else
    mr_web_texture_update(t->id,x,y,width,height,pixels);
#endif
}
void UpdateTexture(Texture2D texture,const void *pixels) {
    UpdateTextureRec(texture,(Rectangle){0,0,(float)texture.width,(float)texture.height},pixels);
}
static void mr_apply_texture_params(MRTexture *texture) {
#ifdef _WIN32
    WGPUSamplerDescriptor descriptor=WGPU_SAMPLER_DESCRIPTOR_INIT;
    bool linear=texture->filter!=TEXTURE_FILTER_POINT;
    descriptor.magFilter=linear?WGPUFilterMode_Linear:WGPUFilterMode_Nearest;
    descriptor.minFilter=linear?WGPUFilterMode_Linear:WGPUFilterMode_Nearest;
    descriptor.mipmapFilter=linear?WGPUMipmapFilterMode_Linear:WGPUMipmapFilterMode_Nearest;
    WGPUAddressMode address=WGPUAddressMode_Repeat;
    if(texture->wrap==TEXTURE_WRAP_CLAMP || texture->wrap==TEXTURE_WRAP_MIRROR_CLAMP) address=WGPUAddressMode_ClampToEdge;
    else if(texture->wrap==TEXTURE_WRAP_MIRROR_REPEAT) address=WGPUAddressMode_MirrorRepeat;
    descriptor.addressModeU=address; descriptor.addressModeV=address;
    if(texture->filter>=TEXTURE_FILTER_ANISOTROPIC_4X) descriptor.maxAnisotropy=(uint16_t)(4u<<(texture->filter-TEXTURE_FILTER_ANISOTROPIC_4X));
    WGPUSampler sampler=wgpuDeviceCreateSampler(mr.device,&descriptor); if(!sampler)return;
    WGPUBindGroupEntry entries[2]={WGPU_BIND_GROUP_ENTRY_INIT,WGPU_BIND_GROUP_ENTRY_INIT};
    entries[0].binding=0;entries[0].sampler=sampler;entries[1].binding=1;entries[1].textureView=texture->view;
    WGPUBindGroupDescriptor groupDescriptor=WGPU_BIND_GROUP_DESCRIPTOR_INIT;
    groupDescriptor.layout=mr.textureLayout;groupDescriptor.entryCount=2;groupDescriptor.entries=entries;
    WGPUBindGroup group=wgpuDeviceCreateBindGroup(mr.device,&groupDescriptor);if(!group){wgpuSamplerRelease(sampler);return;}
    wgpuBindGroupRelease(texture->group);if(texture->customSampler)wgpuSamplerRelease(texture->customSampler);
    texture->group=group;texture->customSampler=sampler;
#else
    mr_web_texture_params(texture->id,texture->filter,texture->wrap);
#endif
    mr_shader_texture_changed(texture->id,false);
}
void SetTextureFilter(Texture2D texture,int filter) {
    MRTexture *entry=mr_texture(texture.id);if(!entry)return;
    if(filter<TEXTURE_FILTER_POINT)filter=TEXTURE_FILTER_POINT;if(filter>TEXTURE_FILTER_ANISOTROPIC_16X)filter=TEXTURE_FILTER_ANISOTROPIC_16X;
    entry->filter=filter;mr_apply_texture_params(entry);
}
void SetTextureWrap(Texture2D texture,int wrap) {
    MRTexture *entry=mr_texture(texture.id);if(!entry)return;
    if(wrap<TEXTURE_WRAP_REPEAT)wrap=TEXTURE_WRAP_REPEAT;if(wrap>TEXTURE_WRAP_MIRROR_CLAMP)wrap=TEXTURE_WRAP_MIRROR_CLAMP;
    entry->wrap=wrap;mr_apply_texture_params(entry);
}
void SetShapesTexture(Texture2D texture,Rectangle source) {
    if (!mr_texture(texture.id) || texture.width<=0 || texture.height<=0) return;
    mr.shapesTexture=texture; mr.shapesSource=source;
}
Texture2D GetShapesTexture(void) { return mr.shapesTexture; }
Rectangle GetShapesTextureRectangle(void) { return mr.shapesSource; }
static const char *mr_default_vertex_wgsl="struct V{@builtin(position)position:vec4f,@location(0)uv:vec2f,@location(1)color:vec4f};@vertex fn vs(@location(0)p:vec2f,@location(1)uv:vec2f,@location(2)c:vec4f,@location(3)z:f32)->V{var o:V;o.position=vec4f(p,z,1);o.uv=uv;o.color=c;return o;}";
static const char *mr_default_fragment_wgsl="struct V{@builtin(position)position:vec4f,@location(0)uv:vec2f,@location(1)color:vec4f};@group(0)@binding(0)var smp:sampler;@group(0)@binding(1)var tex:texture_2d<f32>;@fragment fn fs(v:V)->@location(0)vec4f{return textureSample(tex,smp,v.uv)*v.color;}";
#ifdef _WIN32
static bool mr_make_shader_pipelines(const char *vsCode,const char *fsCode,WGPURenderPipeline output[6]) {
    WGPUShaderSourceWGSL vsSource=WGPU_SHADER_SOURCE_WGSL_INIT,fsSource=WGPU_SHADER_SOURCE_WGSL_INIT;vsSource.code=mr_string(vsCode);fsSource.code=mr_string(fsCode);
    WGPUShaderModuleDescriptor vsDesc=WGPU_SHADER_MODULE_DESCRIPTOR_INIT,fsDesc=WGPU_SHADER_MODULE_DESCRIPTOR_INIT;vsDesc.nextInChain=&vsSource.chain;fsDesc.nextInChain=&fsSource.chain;
    WGPUShaderModule vs=wgpuDeviceCreateShaderModule(mr.device,&vsDesc),fs=wgpuDeviceCreateShaderModule(mr.device,&fsDesc);if(!vs||!fs)return false;
    WGPUBindGroupLayout layouts[3]={mr.textureLayout,mr.uniformLayout,mr.shaderTextureLayout};WGPUPipelineLayoutDescriptor ld=WGPU_PIPELINE_LAYOUT_DESCRIPTOR_INIT;ld.bindGroupLayoutCount=3;ld.bindGroupLayouts=layouts;WGPUPipelineLayout layout=wgpuDeviceCreatePipelineLayout(mr.device,&ld);
    WGPUVertexAttribute attributes[4]={WGPU_VERTEX_ATTRIBUTE_INIT,WGPU_VERTEX_ATTRIBUTE_INIT,WGPU_VERTEX_ATTRIBUTE_INIT,WGPU_VERTEX_ATTRIBUTE_INIT};attributes[0].format=WGPUVertexFormat_Float32x2;attributes[1].format=WGPUVertexFormat_Float32x2;attributes[1].offset=offsetof(MRVertex,u);attributes[1].shaderLocation=1;attributes[2].format=WGPUVertexFormat_Unorm8x4;attributes[2].offset=offsetof(MRVertex,r);attributes[2].shaderLocation=2;attributes[3].format=WGPUVertexFormat_Float32;attributes[3].offset=offsetof(MRVertex,z);attributes[3].shaderLocation=3;
    WGPUVertexBufferLayout vl=WGPU_VERTEX_BUFFER_LAYOUT_INIT;vl.arrayStride=sizeof(MRVertex);vl.stepMode=WGPUVertexStepMode_Vertex;vl.attributeCount=4;vl.attributes=attributes;
    WGPUBlendState blend=WGPU_BLEND_STATE_INIT;WGPUColorTargetState target=WGPU_COLOR_TARGET_STATE_INIT;target.format=mr.config.format;target.blend=&blend;WGPUFragmentState fragment=WGPU_FRAGMENT_STATE_INIT;fragment.module=fs;fragment.entryPoint=mr_string("fs");fragment.targetCount=1;fragment.targets=&target;
    WGPUDepthStencilState depth=WGPU_DEPTH_STENCIL_STATE_INIT;depth.format=WGPUTextureFormat_Depth24Plus;depth.depthWriteEnabled=WGPUOptionalBool_True;depth.depthCompare=WGPUCompareFunction_LessEqual;
    WGPURenderPipelineDescriptor pd=WGPU_RENDER_PIPELINE_DESCRIPTOR_INIT;pd.layout=layout;pd.vertex.module=vs;pd.vertex.entryPoint=mr_string("vs");pd.vertex.bufferCount=1;pd.vertex.buffers=&vl;pd.primitive.topology=WGPUPrimitiveTopology_TriangleList;pd.fragment=&fragment;pd.depthStencil=&depth;
    bool ok=true;for(int mode=0;mode<6;mode++){blend.color.operation=mode==BLEND_SUBTRACT_COLORS?WGPUBlendOperation_ReverseSubtract:WGPUBlendOperation_Add;blend.alpha.operation=WGPUBlendOperation_Add;blend.color.srcFactor=(mode==BLEND_ALPHA_PREMULTIPLY||mode==BLEND_ADD_COLORS||mode==BLEND_SUBTRACT_COLORS)?WGPUBlendFactor_One:mode==BLEND_MULTIPLIED?WGPUBlendFactor_Dst:WGPUBlendFactor_SrcAlpha;blend.color.dstFactor=mode==BLEND_ADDITIVE||mode==BLEND_ADD_COLORS||mode==BLEND_SUBTRACT_COLORS?WGPUBlendFactor_One:WGPUBlendFactor_OneMinusSrcAlpha;blend.alpha.srcFactor=WGPUBlendFactor_One;blend.alpha.dstFactor=mode==BLEND_ADDITIVE||mode==BLEND_ADD_COLORS||mode==BLEND_SUBTRACT_COLORS?WGPUBlendFactor_One:WGPUBlendFactor_OneMinusSrcAlpha;output[mode]=wgpuDeviceCreateRenderPipeline(mr.device,&pd);if(!output[mode])ok=false;}
    wgpuPipelineLayoutRelease(layout);wgpuShaderModuleRelease(vs);wgpuShaderModuleRelease(fs);return ok;
}
static WGPURenderPipeline mr_make_material_pipeline(const char *vsCode,const char *fsCode){
    WGPUShaderSourceWGSL vsSource=WGPU_SHADER_SOURCE_WGSL_INIT,fsSource=WGPU_SHADER_SOURCE_WGSL_INIT;vsSource.code=mr_string(vsCode);fsSource.code=mr_string(fsCode);WGPUShaderModuleDescriptor vsDesc=WGPU_SHADER_MODULE_DESCRIPTOR_INIT,fsDesc=WGPU_SHADER_MODULE_DESCRIPTOR_INIT;vsDesc.nextInChain=&vsSource.chain;fsDesc.nextInChain=&fsSource.chain;WGPUShaderModule vs=wgpuDeviceCreateShaderModule(mr.device,&vsDesc),fs=wgpuDeviceCreateShaderModule(mr.device,&fsDesc);if(!vs||!fs)return NULL;
    WGPUVertexAttribute ma[8];memset(ma,0,sizeof ma);ma[0].format=WGPUVertexFormat_Float32x3;ma[0].shaderLocation=0;ma[1].format=WGPUVertexFormat_Float32x3;ma[1].offset=offsetof(MRGpuVertex,nx);ma[1].shaderLocation=1;ma[2].format=WGPUVertexFormat_Float32x2;ma[2].offset=offsetof(MRGpuVertex,u);ma[2].shaderLocation=2;ma[3].format=WGPUVertexFormat_Unorm8x4;ma[3].offset=offsetof(MRGpuVertex,r);ma[3].shaderLocation=3;ma[4].format=WGPUVertexFormat_Uint8x4;ma[4].offset=offsetof(MRGpuVertex,boneIds);ma[4].shaderLocation=4;ma[5].format=WGPUVertexFormat_Float32x4;ma[5].offset=offsetof(MRGpuVertex,boneWeights);ma[5].shaderLocation=5;ma[6].format=WGPUVertexFormat_Float32x4;ma[6].offset=offsetof(MRGpuVertex,tangent);ma[6].shaderLocation=6;ma[7].format=WGPUVertexFormat_Float32x2;ma[7].offset=offsetof(MRGpuVertex,u2);ma[7].shaderLocation=7;
    WGPUVertexAttribute ia[8];memset(ia,0,sizeof ia);for(int i=0;i<4;i++){ia[i].format=WGPUVertexFormat_Float32x4;ia[i].offset=(uint64_t)i*16;ia[i].shaderLocation=(uint32_t)i+8;}ia[4].format=WGPUVertexFormat_Unorm8x4;ia[4].offset=offsetof(MRInstance3D,tint);ia[4].shaderLocation=12;ia[5].format=WGPUVertexFormat_Float32x4;ia[5].offset=offsetof(MRInstance3D,material);ia[5].shaderLocation=13;ia[6].format=WGPUVertexFormat_Unorm8x4;ia[6].offset=offsetof(MRInstance3D,emission);ia[6].shaderLocation=14;ia[7].format=WGPUVertexFormat_Uint32x4;ia[7].offset=offsetof(MRInstance3D,skin);ia[7].shaderLocation=15;WGPUVertexBufferLayout layouts[2]={WGPU_VERTEX_BUFFER_LAYOUT_INIT,WGPU_VERTEX_BUFFER_LAYOUT_INIT};layouts[0].arrayStride=sizeof(MRGpuVertex);layouts[0].attributeCount=8;layouts[0].attributes=ma;layouts[1].arrayStride=sizeof(MRInstance3D);layouts[1].stepMode=WGPUVertexStepMode_Instance;layouts[1].attributeCount=8;layouts[1].attributes=ia;
    WGPUBlendState blend=WGPU_BLEND_STATE_INIT;blend.color.srcFactor=WGPUBlendFactor_SrcAlpha;blend.color.dstFactor=WGPUBlendFactor_OneMinusSrcAlpha;blend.alpha.srcFactor=WGPUBlendFactor_One;blend.alpha.dstFactor=WGPUBlendFactor_OneMinusSrcAlpha;WGPUColorTargetState target=WGPU_COLOR_TARGET_STATE_INIT;target.format=mr.config.format;target.blend=&blend;WGPUFragmentState fragment=WGPU_FRAGMENT_STATE_INIT;fragment.module=fs;fragment.entryPoint=mr_string("fs");fragment.targetCount=1;fragment.targets=&target;WGPUDepthStencilState depth=WGPU_DEPTH_STENCIL_STATE_INIT;depth.format=WGPUTextureFormat_Depth24Plus;depth.depthWriteEnabled=WGPUOptionalBool_True;depth.depthCompare=WGPUCompareFunction_LessEqual;WGPURenderPipelineDescriptor pd=WGPU_RENDER_PIPELINE_DESCRIPTOR_INIT;pd.layout=mr.materialPipelineLayout;pd.vertex.module=vs;pd.vertex.entryPoint=mr_string("vs");pd.vertex.bufferCount=2;pd.vertex.buffers=layouts;pd.fragment=&fragment;pd.primitive.topology=WGPUPrimitiveTopology_TriangleList;pd.primitive.cullMode=WGPUCullMode_None;pd.depthStencil=&depth;WGPURenderPipeline result=wgpuDeviceCreateRenderPipeline(mr.device,&pd);wgpuShaderModuleRelease(vs);wgpuShaderModuleRelease(fs);return result;
}
#endif
static unsigned int mr_name_hash(const char *name){unsigned int hash=2166136261u;if(name)while(*name){hash^=(unsigned char)*name++;hash*=16777619u;}return hash?hash:1;}
static bool mr_text_starts(const char *text,const char *prefix){while(*prefix)if(*text++!=*prefix++)return false;return true;}
static void mr_shader_add_location(MRShaderEntry *entry,const char *name,int slot){if(!entry||!name||!*name||slot<0||slot>=32)return;unsigned int hash=mr_name_hash(name);for(int i=0;i<entry->locationCount;i++)if(entry->nameHashes[i]==hash)return;if(entry->locationCount<32){entry->nameHashes[entry->locationCount]=hash;entry->nameSlots[entry->locationCount]=(unsigned char)slot;entry->locationCount++;}}
static void mr_shader_parse_named_locations(MRShaderEntry *entry,const char *code,const char *tag,int tagLength,int maxSlot){if(!code)return;
    for(const char *p=code;*p;p++)if(*p=='@'&&mr_text_starts(p,tag)){
        p+=tagLength;while(*p==' '||*p=='\t')p++;char name[64];int n=0;while((*p=='_'||(*p>='a'&&*p<='z')||(*p>='A'&&*p<='Z')||(*p>='0'&&*p<='9'))&&n<63)name[n++]=*p++;name[n]=0;
        while(*p==' '||*p=='\t')p++;int slot=0,hasDigit=false;while(*p>='0'&&*p<='9'){hasDigit=true;slot=slot*10+(*p++-'0');}
        if(!n||!hasDigit||slot<0||slot>=maxSlot)continue;entry->explicitLocations=true;mr_shader_add_location(entry,name,slot);
    }}
static void mr_shader_parse_texture_locations(MRShaderEntry *entry,const char *code){if(!code)return;mr_shader_parse_named_locations(entry,code,"@raygpu_sampler",15,RAYGPU_MAX_SHADER_TEXTURES);for(const char *p=code;*p;p++)if(*p=='@'&&mr_text_starts(p,"@group(2)")){const char *end=p;while(*end&&*end!=';')end++;const char *binding=p;while(binding<end&&!(*binding=='@'&&mr_text_starts(binding,"@binding(")))binding++;if(binding>=end)continue;binding+=9;int value=0,hasDigit=false;while(*binding>='0'&&*binding<='9'){hasDigit=true;value=value*10+(*binding++-'0');}if(!hasDigit||(value&1)==0||value/2>=RAYGPU_MAX_SHADER_TEXTURES)continue;const char *var=binding;while(var<end&&!mr_text_starts(var,"var"))var++;if(var>=end)continue;var+=3;while(var<end&&(*var==' '||*var=='\t'||*var=='\n'||*var=='\r'))var++;char name[64];int n=0;while(var<end&&(*var=='_'||(*var>='a'&&*var<='z')||(*var>='A'&&*var<='Z')||(*var>='0'&&*var<='9'))&&n<63)name[n++]=*var++;name[n]=0;if(n){entry->explicitLocations=true;mr_shader_add_location(entry,name,value/2);}}}
static void mr_shader_add_attribute(MRShaderEntry *entry,const char *name,int slot){if(!entry||!name||!*name||slot<0||slot>15)return;unsigned int hash=mr_name_hash(name);for(int i=0;i<entry->attributeCount;i++)if(entry->attributeHashes[i]==hash)return;if(entry->attributeCount<16){entry->attributeHashes[entry->attributeCount]=hash;entry->attributeSlots[entry->attributeCount]=(unsigned char)slot;entry->attributeCount++;}}
static void mr_shader_parse_attributes(MRShaderEntry *entry,const char *code){if(!code)return;for(const char *p=code;*p;p++)if(*p=='@'&&mr_text_starts(p,"@raygpu_attribute")){p+=17;while(*p==' '||*p=='\t')p++;char name[64];int n=0;while((*p=='_'||(*p>='a'&&*p<='z')||(*p>='A'&&*p<='Z')||(*p>='0'&&*p<='9'))&&n<63)name[n++]=*p++;name[n]=0;while(*p==' '||*p=='\t')p++;int slot=0,hasDigit=false;while(*p>='0'&&*p<='9'){hasDigit=true;slot=slot*10+(*p++-'0');}if(n&&hasDigit)mr_shader_add_attribute(entry,name,slot);}const char *vertex=code;while(*vertex&&!(*vertex=='@'&&mr_text_starts(vertex,"@vertex")))vertex++;while(*vertex&&*vertex!='(')vertex++;if(!*vertex)return;int depth=1;for(const char *p=vertex+1;*p&&depth>0;p++){if(*p=='(')depth++;else if(*p==')')depth--;else if(depth==1&&*p=='@'&&mr_text_starts(p,"@location(")){const char *at=p+10;int slot=0,hasDigit=false;while(*at>='0'&&*at<='9'){hasDigit=true;slot=slot*10+(*at++-'0');}if(!hasDigit||*at!=')')continue;at++;while(*at==' '||*at=='\t'||*at=='\n'||*at=='\r')at++;char name[64];int n=0;while((*at=='_'||(*at>='a'&&*at<='z')||(*at>='A'&&*at<='Z')||(*at>='0'&&*at<='9'))&&n<63)name[n++]=*at++;name[n]=0;if(n)mr_shader_add_attribute(entry,name,slot);}}}
#ifdef _WIN32
static bool mr_rebuild_shader_texture_group(MRShaderEntry *entry){MRTexture *fallback=mr_texture(mr.white);if(!entry||!fallback||!mr.shaderTextureLayout)return false;WGPUBindGroupEntry bindings[RAYGPU_MAX_SHADER_TEXTURES*2];memset(bindings,0,sizeof bindings);for(int i=0;i<RAYGPU_MAX_SHADER_TEXTURES;i++){MRTexture *texture=mr_texture(entry->extraTextures[i]);if(!texture)texture=fallback;bindings[i*2].binding=(uint32_t)i*2;bindings[i*2].sampler=texture->customSampler?texture->customSampler:mr.sampler;bindings[i*2+1].binding=(uint32_t)i*2+1;bindings[i*2+1].textureView=texture->view;}WGPUBindGroupDescriptor descriptor=WGPU_BIND_GROUP_DESCRIPTOR_INIT;descriptor.layout=mr.shaderTextureLayout;descriptor.entryCount=RAYGPU_MAX_SHADER_TEXTURES*2;descriptor.entries=bindings;WGPUBindGroup group=wgpuDeviceCreateBindGroup(mr.device,&descriptor);if(!group)return false;if(entry->textureGroup)wgpuBindGroupRelease(entry->textureGroup);entry->textureGroup=group;return true;}
#endif
Shader LoadShaderFromMemory(const char *vsCode,const char *fsCode){if(!mr.ready)return(Shader){0};if(!vsCode)vsCode=mr_default_vertex_wgsl;if(!fsCode)fsCode=mr_default_fragment_wgsl;MRShaderEntry*entry=NULL;for(int i=0;i<32;i++)if(!mr.shaders[i].id){entry=&mr.shaders[i];break;}if(!entry)return(Shader){0};memset(entry,0,sizeof *entry);mr_shader_parse_named_locations(entry,vsCode,"@raygpu_uniform",15,32);mr_shader_parse_named_locations(entry,fsCode,"@raygpu_uniform",15,32);mr_shader_parse_texture_locations(entry,vsCode);mr_shader_parse_texture_locations(entry,fsCode);mr_shader_parse_attributes(entry,vsCode);unsigned int id=++mr.nextShader;
#ifdef _WIN32
    if(!mr_make_shader_pipelines(vsCode,fsCode,entry->pipelines)){for(int i=0;i<6;i++)if(entry->pipelines[i])wgpuRenderPipelineRelease(entry->pipelines[i]);memset(entry,0,sizeof *entry);return(Shader){0};}
    WGPUBufferDescriptor bd=WGPU_BUFFER_DESCRIPTOR_INIT;bd.size=sizeof entry->uniforms;bd.usage=WGPUBufferUsage_Uniform|WGPUBufferUsage_CopyDst;entry->uniformBuffer=wgpuDeviceCreateBuffer(mr.device,&bd);
    WGPUBindGroupEntry be=WGPU_BIND_GROUP_ENTRY_INIT;be.binding=0;be.buffer=entry->uniformBuffer;be.size=sizeof entry->uniforms;WGPUBindGroupDescriptor gd=WGPU_BIND_GROUP_DESCRIPTOR_INIT;gd.layout=mr.uniformLayout;gd.entryCount=1;gd.entries=&be;entry->uniformGroup=wgpuDeviceCreateBindGroup(mr.device,&gd);
    if(!entry->uniformBuffer||!entry->uniformGroup||!mr_rebuild_shader_texture_group(entry)){if(entry->textureGroup)wgpuBindGroupRelease(entry->textureGroup);if(entry->uniformGroup)wgpuBindGroupRelease(entry->uniformGroup);if(entry->uniformBuffer)wgpuBufferRelease(entry->uniformBuffer);for(int i=0;i<6;i++)if(entry->pipelines[i])wgpuRenderPipelineRelease(entry->pipelines[i]);memset(entry,0,sizeof *entry);return(Shader){0};}
#else
    if(!mr_web_shader_load(id,vsCode,fsCode))return(Shader){0};
#endif
    entry->id=id;return(Shader){id,NULL};}
Shader LoadShader(const char*vsFile,const char*fsFile){char*vs=vsFile?LoadFileText(vsFile):NULL,*fs=fsFile?LoadFileText(fsFile):NULL;if((vsFile&&!vs)||(fsFile&&!fs)){UnloadFileText(vs);UnloadFileText(fs);return(Shader){0};}Shader shader=LoadShaderFromMemory(vs,fs);UnloadFileText(vs);UnloadFileText(fs);return shader;}
Shader LoadMaterialShaderFromMemory(const char *vsCode,const char *fsCode){if(!mr.ready||!vsCode||!fsCode)return(Shader){0};MRShaderEntry *entry=NULL;for(int i=0;i<32;i++)if(!mr.shaders[i].id){entry=&mr.shaders[i];break;}if(!entry)return(Shader){0};memset(entry,0,sizeof *entry);mr_shader_parse_named_locations(entry,vsCode,"@raygpu_uniform",15,32);mr_shader_parse_named_locations(entry,fsCode,"@raygpu_uniform",15,32);mr_shader_parse_texture_locations(entry,vsCode);mr_shader_parse_texture_locations(entry,fsCode);mr_shader_parse_attributes(entry,vsCode);unsigned int id=++mr.nextShader;
#ifdef _WIN32
    entry->pipeline3d=mr_make_material_pipeline(vsCode,fsCode);WGPUBufferDescriptor bd=WGPU_BUFFER_DESCRIPTOR_INIT;bd.size=sizeof entry->uniforms;bd.usage=WGPUBufferUsage_Uniform|WGPUBufferUsage_CopyDst;entry->uniformBuffer=wgpuDeviceCreateBuffer(mr.device,&bd);WGPUBindGroupEntry be=WGPU_BIND_GROUP_ENTRY_INIT;be.binding=0;be.buffer=entry->uniformBuffer;be.size=sizeof entry->uniforms;WGPUBindGroupDescriptor gd=WGPU_BIND_GROUP_DESCRIPTOR_INIT;gd.layout=mr.uniformLayout;gd.entryCount=1;gd.entries=&be;entry->uniformGroup=wgpuDeviceCreateBindGroup(mr.device,&gd);if(!entry->pipeline3d||!entry->uniformBuffer||!entry->uniformGroup||!mr_rebuild_shader_texture_group(entry)){if(entry->pipeline3d)wgpuRenderPipelineRelease(entry->pipeline3d);if(entry->textureGroup)wgpuBindGroupRelease(entry->textureGroup);if(entry->uniformGroup)wgpuBindGroupRelease(entry->uniformGroup);if(entry->uniformBuffer)wgpuBufferRelease(entry->uniformBuffer);memset(entry,0,sizeof *entry);return(Shader){0};}
#else
    if(!mr_web_material_shader_load(id,vsCode,fsCode))return(Shader){0};
#endif
    entry->id=id;entry->materialShader=true;return(Shader){id,NULL};}
Shader LoadMaterialShader(const char *vsFile,const char *fsFile){char *vs=vsFile?LoadFileText(vsFile):NULL,*fs=fsFile?LoadFileText(fsFile):NULL;if(!vs||!fs){UnloadFileText(vs);UnloadFileText(fs);return(Shader){0};}Shader shader=LoadMaterialShaderFromMemory(vs,fs);UnloadFileText(vs);UnloadFileText(fs);return shader;}
bool IsShaderValid(Shader shader){return shader.id&&mr_shader(shader.id)!=NULL;}
void UnloadShader(Shader shader){MRShaderEntry*entry=mr_shader(shader.id);if(!entry)return;if(mr.currentShader==shader.id)mr.currentShader=0;
#ifdef _WIN32
    for(int i=0;i<6;i++)if(entry->pipelines[i])wgpuRenderPipelineRelease(entry->pipelines[i]);if(entry->pipeline3d)wgpuRenderPipelineRelease(entry->pipeline3d);if(entry->textureGroup)wgpuBindGroupRelease(entry->textureGroup);if(entry->uniformGroup)wgpuBindGroupRelease(entry->uniformGroup);if(entry->uniformBuffer)wgpuBufferRelease(entry->uniformBuffer);
#else
    mr_web_shader_unload(shader.id);
#endif
    memset(entry,0,sizeof *entry);}
void BeginShaderMode(Shader shader){MRShaderEntry *entry=mr_shader(shader.id);mr.currentShader=entry&&!entry->materialShader?shader.id:0;}
void EndShaderMode(void){mr.currentShader=0;}
int GetShaderLocation(Shader shader,const char *name){MRShaderEntry*entry=mr_shader(shader.id);if(!entry||!name)return-1;unsigned int hash=mr_name_hash(name);for(int i=0;i<entry->locationCount;i++)if(entry->nameHashes[i]==hash)return entry->nameSlots[i];if(entry->explicitLocations||entry->locationCount>=32)return-1;int slot=entry->locationCount;entry->nameHashes[entry->locationCount]=hash;entry->nameSlots[entry->locationCount]=(unsigned char)slot;entry->locationCount++;return slot;}
int GetShaderLocationAttrib(Shader shader,const char *name){MRShaderEntry*entry=mr_shader(shader.id);if(!entry||!name)return-1;unsigned int hash=mr_name_hash(name);for(int i=0;i<entry->attributeCount;i++)if(entry->attributeHashes[i]==hash)return entry->attributeSlots[i];return-1;}
static int mr_uniform_components(int type){switch(type){case SHADER_UNIFORM_VEC2:case SHADER_UNIFORM_IVEC2:return 2;case SHADER_UNIFORM_VEC3:case SHADER_UNIFORM_IVEC3:return 3;case SHADER_UNIFORM_VEC4:case SHADER_UNIFORM_IVEC4:return 4;default:return 1;}}
void SetShaderValueV(Shader shader,int location,const void*value,int type,int count){MRShaderEntry*entry=mr_shader(shader.id);if(!entry||!value||location<0||location>=32||count<=0)return;int itemBytes=mr_uniform_components(type)*4;if(count>4)count=4;memset(entry->uniforms[location],0,64);for(int i=0;i<count;i++)memcpy(entry->uniforms[location]+i*16,(const unsigned char*)value+i*itemBytes,(size_t)itemBytes);
#ifdef _WIN32
    wgpuQueueWriteBuffer(mr.queue,entry->uniformBuffer,(uint64_t)location*64,entry->uniforms[location],64);
#else
    mr_web_shader_uniform(shader.id,location,entry->uniforms[location],64);
#endif
}
void SetShaderValue(Shader shader,int location,const void*value,int type){SetShaderValueV(shader,location,value,type,1);}
void SetShaderValueMatrix(Shader shader,int location,Matrix matrix){SetShaderValueV(shader,location,&matrix,SHADER_UNIFORM_VEC4,4);}
void SetShaderValueTexture(Shader shader,int location,Texture2D texture){MRShaderEntry *entry=mr_shader(shader.id);if(!entry||location<0||location>=RAYGPU_MAX_SHADER_TEXTURES||!mr_texture(texture.id))return;entry->extraTextures[location]=texture.id;
#ifdef _WIN32
    mr_rebuild_shader_texture_group(entry);
#else
    mr_web_shader_texture(shader.id,location,texture.id);
#endif
}
static void mr_shader_texture_changed(unsigned int textureId,bool removed){if(!textureId)return;for(int i=0;i<32;i++){MRShaderEntry *entry=&mr.shaders[i];if(!entry->id)continue;bool changed=false;for(int slot=0;slot<RAYGPU_MAX_SHADER_TEXTURES;slot++)if(entry->extraTextures[slot]==textureId){if(removed)entry->extraTextures[slot]=0;changed=true;}if(changed){
#ifdef _WIN32
        mr_rebuild_shader_texture_group(entry);
#else
        for(int slot=0;slot<RAYGPU_MAX_SHADER_TEXTURES;slot++)mr_web_shader_texture(entry->id,slot,entry->extraTextures[slot]);
#endif
    }}}
#ifdef _WIN32
static bool mr_renderer(void) {
    WGPUSurfaceCapabilities caps=WGPU_SURFACE_CAPABILITIES_INIT;
    if (wgpuSurfaceGetCapabilities(mr.surface,mr.adapter,&caps)!=WGPUStatus_Success || !caps.formatCount || !caps.alphaModeCount) {
        wgpuSurfaceCapabilitiesFreeMembers(caps); mr_error("Cannot query surface capabilities"); return false;
    }
    WGPUSurfaceConfiguration config=WGPU_SURFACE_CONFIGURATION_INIT;
    mr.config=config;
    mr.config.device=mr.device; mr.config.format=caps.formats[0];
    for (size_t i=0;i<caps.formatCount;i++) if (caps.formats[i]==WGPUTextureFormat_BGRA8Unorm || caps.formats[i]==WGPUTextureFormat_RGBA8Unorm) { mr.config.format=caps.formats[i]; break; }
    mr.config.alphaMode=caps.alphaModes[0];
    mr.config.presentMode=WGPUPresentMode_Fifo;
    /* SetTargetFPS is a software cap, like raylib's default behavior. FIFO
     * already waits for vertical sync; combining both waits can halve 60 to
     * 30 FPS. Prefer Immediate and apply exactly one precise frame limit. */
    for (size_t i=0;i<caps.presentModeCount;i++) {
        if (caps.presentModes[i]==WGPUPresentMode_Immediate) {
            mr.config.presentMode=WGPUPresentMode_Immediate;
            mr.softwareFrameLimit=true;
            break;
        }
    }
    wgpuSurfaceCapabilitiesFreeMembers(caps);
    WGPUBindGroupLayoutEntry entries[2]={WGPU_BIND_GROUP_LAYOUT_ENTRY_INIT,WGPU_BIND_GROUP_LAYOUT_ENTRY_INIT};
    entries[0].binding=0; entries[0].visibility=WGPUShaderStage_Fragment; entries[0].sampler.type=WGPUSamplerBindingType_Filtering;
    entries[1].binding=1; entries[1].visibility=WGPUShaderStage_Fragment;
    entries[1].texture.sampleType=WGPUTextureSampleType_Float; entries[1].texture.viewDimension=WGPUTextureViewDimension_2D;
    WGPUBindGroupLayoutDescriptor bindLayout=WGPU_BIND_GROUP_LAYOUT_DESCRIPTOR_INIT;
    bindLayout.entryCount=2; bindLayout.entries=entries;
    mr.textureLayout=wgpuDeviceCreateBindGroupLayout(mr.device,&bindLayout);
    WGPUBindGroupLayoutEntry uniformEntry=WGPU_BIND_GROUP_LAYOUT_ENTRY_INIT;uniformEntry.binding=0;uniformEntry.visibility=WGPUShaderStage_Vertex|WGPUShaderStage_Fragment;uniformEntry.buffer.type=WGPUBufferBindingType_Uniform;uniformEntry.buffer.minBindingSize=32*64;
    WGPUBindGroupLayoutDescriptor uniformDesc=WGPU_BIND_GROUP_LAYOUT_DESCRIPTOR_INIT;uniformDesc.entryCount=1;uniformDesc.entries=&uniformEntry;mr.uniformLayout=wgpuDeviceCreateBindGroupLayout(mr.device,&uniformDesc);
    WGPUBindGroupLayoutEntry shaderTextures[RAYGPU_MAX_SHADER_TEXTURES*2];memset(shaderTextures,0,sizeof shaderTextures);for(int i=0;i<RAYGPU_MAX_SHADER_TEXTURES;i++){shaderTextures[i*2].binding=(uint32_t)i*2;shaderTextures[i*2].visibility=WGPUShaderStage_Fragment;shaderTextures[i*2].sampler.type=WGPUSamplerBindingType_Filtering;shaderTextures[i*2+1].binding=(uint32_t)i*2+1;shaderTextures[i*2+1].visibility=WGPUShaderStage_Fragment;shaderTextures[i*2+1].texture.sampleType=WGPUTextureSampleType_Float;shaderTextures[i*2+1].texture.viewDimension=WGPUTextureViewDimension_2D;}WGPUBindGroupLayoutDescriptor shaderTextureDesc=WGPU_BIND_GROUP_LAYOUT_DESCRIPTOR_INIT;shaderTextureDesc.entryCount=RAYGPU_MAX_SHADER_TEXTURES*2;shaderTextureDesc.entries=shaderTextures;mr.shaderTextureLayout=wgpuDeviceCreateBindGroupLayout(mr.device,&shaderTextureDesc);
    WGPUSamplerDescriptor sampler=WGPU_SAMPLER_DESCRIPTOR_INIT;
    sampler.magFilter=WGPUFilterMode_Nearest; sampler.minFilter=WGPUFilterMode_Nearest;
    sampler.addressModeU=WGPUAddressMode_Repeat; sampler.addressModeV=WGPUAddressMode_Repeat;
    mr.sampler=wgpuDeviceCreateSampler(mr.device,&sampler);
    WGPUPipelineLayoutDescriptor layout=WGPU_PIPELINE_LAYOUT_DESCRIPTOR_INIT;
    layout.bindGroupLayoutCount=1; layout.bindGroupLayouts=&mr.textureLayout;
    WGPUPipelineLayout pipelineLayout=wgpuDeviceCreatePipelineLayout(mr.device,&layout);
    const char *wgsl=
        "struct V { @builtin(position) position: vec4f, @location(0) uv: vec2f, @location(1) color: vec4f };\n"
        "@group(0) @binding(0) var smp: sampler;\n"
        "@group(0) @binding(1) var tex: texture_2d<f32>;\n"
        "@vertex fn vs(@location(0) p: vec2f, @location(1) uv: vec2f, @location(2) c: vec4f, @location(3) z: f32) -> V {\n"
        " var o: V; o.position=vec4f(p,z,1); o.uv=uv; o.color=c; return o; }\n"
        "@fragment fn fs(v: V) -> @location(0) vec4f { return textureSample(tex,smp,v.uv)*v.color; }\n";
    WGPUShaderSourceWGSL source=WGPU_SHADER_SOURCE_WGSL_INIT; source.code=mr_string(wgsl);
    WGPUShaderModuleDescriptor shaderDesc=WGPU_SHADER_MODULE_DESCRIPTOR_INIT; shaderDesc.nextInChain=&source.chain;
    WGPUShaderModule shader=wgpuDeviceCreateShaderModule(mr.device,&shaderDesc);
    WGPUVertexAttribute attributes[4]={WGPU_VERTEX_ATTRIBUTE_INIT,WGPU_VERTEX_ATTRIBUTE_INIT,WGPU_VERTEX_ATTRIBUTE_INIT,WGPU_VERTEX_ATTRIBUTE_INIT};
    attributes[0].format=WGPUVertexFormat_Float32x2;
    attributes[1].format=WGPUVertexFormat_Float32x2; attributes[1].offset=offsetof(MRVertex,u); attributes[1].shaderLocation=1;
    attributes[2].format=WGPUVertexFormat_Unorm8x4; attributes[2].offset=offsetof(MRVertex,r); attributes[2].shaderLocation=2;
    attributes[3].format=WGPUVertexFormat_Float32; attributes[3].offset=offsetof(MRVertex,z); attributes[3].shaderLocation=3;
    WGPUVertexBufferLayout vertexLayout=WGPU_VERTEX_BUFFER_LAYOUT_INIT;
    vertexLayout.arrayStride=sizeof(MRVertex); vertexLayout.stepMode=WGPUVertexStepMode_Vertex;
    vertexLayout.attributeCount=4; vertexLayout.attributes=attributes;
    WGPUBlendState blend=WGPU_BLEND_STATE_INIT;
    WGPUColorTargetState target=WGPU_COLOR_TARGET_STATE_INIT; target.format=mr.config.format; target.blend=&blend;
    WGPUFragmentState fragment=WGPU_FRAGMENT_STATE_INIT;
    fragment.module=shader; fragment.entryPoint=mr_string("fs"); fragment.targetCount=1; fragment.targets=&target;
    WGPURenderPipelineDescriptor pipeline=WGPU_RENDER_PIPELINE_DESCRIPTOR_INIT;
    pipeline.layout=pipelineLayout; pipeline.vertex.module=shader; pipeline.vertex.entryPoint=mr_string("vs");
    pipeline.vertex.bufferCount=1; pipeline.vertex.buffers=&vertexLayout;
    WGPUDepthStencilState depth=WGPU_DEPTH_STENCIL_STATE_INIT; depth.format=WGPUTextureFormat_Depth24Plus;
    depth.depthWriteEnabled=WGPUOptionalBool_True; depth.depthCompare=WGPUCompareFunction_LessEqual;
    pipeline.primitive.topology=WGPUPrimitiveTopology_TriangleList; pipeline.fragment=&fragment; pipeline.depthStencil=&depth;
    for (int mode=0;mode<6;mode++) {
        blend.color.operation=mode==BLEND_SUBTRACT_COLORS?WGPUBlendOperation_ReverseSubtract:WGPUBlendOperation_Add;
        blend.alpha.operation=WGPUBlendOperation_Add;
        blend.color.srcFactor=(mode==BLEND_ALPHA_PREMULTIPLY||mode==BLEND_ADD_COLORS||mode==BLEND_SUBTRACT_COLORS)?WGPUBlendFactor_One:
            mode==BLEND_MULTIPLIED?WGPUBlendFactor_Dst:WGPUBlendFactor_SrcAlpha;
        blend.color.dstFactor=mode==BLEND_ADDITIVE||mode==BLEND_ADD_COLORS||mode==BLEND_SUBTRACT_COLORS?WGPUBlendFactor_One:WGPUBlendFactor_OneMinusSrcAlpha;
        blend.alpha.srcFactor=WGPUBlendFactor_One;
        blend.alpha.dstFactor=mode==BLEND_ADDITIVE||mode==BLEND_ADD_COLORS||mode==BLEND_SUBTRACT_COLORS?WGPUBlendFactor_One:WGPUBlendFactor_OneMinusSrcAlpha;
        mr.pipelines[mode]=wgpuDeviceCreateRenderPipeline(mr.device,&pipeline);
    }
    WGPUBindGroupLayoutEntry sceneEntries[2]={WGPU_BIND_GROUP_LAYOUT_ENTRY_INIT,WGPU_BIND_GROUP_LAYOUT_ENTRY_INIT};
    sceneEntries[0].binding=0;sceneEntries[0].visibility=WGPUShaderStage_Vertex|WGPUShaderStage_Fragment;sceneEntries[0].buffer.type=WGPUBufferBindingType_Uniform;sceneEntries[0].buffer.minBindingSize=sizeof(MRScene3D);
    sceneEntries[1].binding=1;sceneEntries[1].visibility=WGPUShaderStage_Vertex;sceneEntries[1].buffer.type=WGPUBufferBindingType_ReadOnlyStorage;sceneEntries[1].buffer.minBindingSize=sizeof(Matrix);
    WGPUBindGroupLayoutDescriptor sceneLayoutDesc=WGPU_BIND_GROUP_LAYOUT_DESCRIPTOR_INIT;sceneLayoutDesc.entryCount=2;sceneLayoutDesc.entries=sceneEntries;mr.sceneLayout3d=wgpuDeviceCreateBindGroupLayout(mr.device,&sceneLayoutDesc);
    WGPUBindGroupLayout materialLayouts[4]={mr.textureLayout,mr.uniformLayout,mr.shaderTextureLayout,mr.sceneLayout3d};WGPUPipelineLayoutDescriptor materialLayoutDesc=WGPU_PIPELINE_LAYOUT_DESCRIPTOR_INIT;materialLayoutDesc.bindGroupLayoutCount=4;materialLayoutDesc.bindGroupLayouts=materialLayouts;mr.materialPipelineLayout=wgpuDeviceCreatePipelineLayout(mr.device,&materialLayoutDesc);
    const char *wgsl3d=
        "struct Light{positionType:vec4f,directionRange:vec4f,colorIntensity:vec4f,spotEnabled:vec4f};"
        "struct Scene{vp:mat4x4f,camera:vec4f,ambient:vec4f,fogColor:vec4f,fog:vec4f,skyRight:vec4f,skyUp:vec4f,skyForward:vec4f,settings:vec4f,lights:array<Light,8>};"
        "struct V{@builtin(position)position:vec4f,@location(0)uv:vec2f,@location(1)uv2:vec2f,@location(2)color:vec4f,@location(3)normal:vec3f,@location(4)tangent:vec4f,@location(5)world:vec3f,@location(6)material:vec4f,@location(7)emission:vec4f,@location(8)@interpolate(flat)flags:u32};"
        "@group(2)@binding(0)var s0:sampler;@group(2)@binding(1)var t0:texture_2d<f32>;@group(2)@binding(2)var s1:sampler;@group(2)@binding(3)var t1:texture_2d<f32>;@group(2)@binding(4)var s2:sampler;@group(2)@binding(5)var t2:texture_2d<f32>;@group(2)@binding(6)var s3:sampler;@group(2)@binding(7)var t3:texture_2d<f32>;@group(2)@binding(8)var s4:sampler;@group(2)@binding(9)var t4:texture_2d<f32>;@group(2)@binding(10)var s5:sampler;@group(2)@binding(11)var t5:texture_2d<f32>;@group(2)@binding(12)var s6:sampler;@group(2)@binding(13)var t6:texture_2d<f32>;@group(2)@binding(14)var s7:sampler;@group(2)@binding(15)var t7:texture_2d<f32>;"
        "@group(3)@binding(0)var<uniform>scene:Scene;@group(3)@binding(1)var<storage,read>bones:array<mat4x4f>;"
        "@vertex fn vs(@location(0)p0:vec3f,@location(1)n0:vec3f,@location(2)uv:vec2f,@location(3)c:vec4f,@location(4)ids:vec4u,@location(5)weights:vec4f,@location(6)tan:vec4f,@location(7)uv2:vec2f,@location(8)m0:vec4f,@location(9)m1:vec4f,@location(10)m2:vec4f,@location(11)m3:vec4f,@location(12)tint:vec4f,@location(13)material:vec4f,@location(14)emission:vec4f,@location(15)skin:vec4u)->V{var p=vec4f(p0,1);var n=n0;var tangent=tan.xyz;if(skin.y>0u&&dot(weights,vec4f(1))>0){let sm=bones[skin.x+ids.x]*weights.x+bones[skin.x+ids.y]*weights.y+bones[skin.x+ids.z]*weights.z+bones[skin.x+ids.w]*weights.w;p=p*sm;n=(vec4f(n,0)*sm).xyz;tangent=(vec4f(tangent,0)*sm).xyz;}let model=mat4x4f(m0,m1,m2,m3);let world=p*model;var o:V;o.position=world*scene.vp;o.uv=uv;o.uv2=uv2;o.color=c*tint;o.normal=normalize((vec4f(n,0)*model).xyz);o.tangent=vec4f(normalize((vec4f(tangent,0)*model).xyz),tan.w);o.world=world.xyz;o.material=material;o.emission=emission;o.flags=skin.z;return o;}"
        "fn fresnel(c:f32,f0:vec3f)->vec3f{return f0+(vec3f(1)-f0)*pow(1-c,5);}fn distribution(nh:f32,r:f32)->f32{let a=r*r;let a2=a*a;let d=nh*nh*(a2-1)+1;return a2/(3.14159265*d*d+0.0001);}fn geometry(nv:f32,nl:f32,r:f32)->f32{let k=(r+1)*(r+1)/8;return nv/(nv*(1-k)+k)*nl/(nl*(1-k)+k);}"
        "@fragment fn fs(v:V,@builtin(front_facing)front:bool)->@location(0)vec4f{let albedoSample=textureSample(t0,s0,v.uv);let metallicSample=textureSample(t1,s1,v.uv);let normalSample=textureSample(t2,s2,v.uv);let mrSample=textureSample(t3,s3,v.uv);let aoSample=textureSample(t4,s4,v.uv2);let emissionSample=textureSample(t5,s5,v.uv);let albedo=albedoSample*v.color;var normal=normalize(v.normal)*select(-1.0,1.0,front);if((v.flags&4u)!=0u){let tn=normalSample.xyz*2-1;let T=normalize(v.tangent.xyz);let B=normalize(cross(normal,T)*v.tangent.w);normal=normalize(mat3x3f(T,B,normal)*vec3f(tn.xy*v.material.z,tn.z));}var metallic=clamp(v.material.x,0,1);var rough=clamp(v.material.y,0.04,1);if((v.flags&2u)!=0u){metallic*=metallicSample.r;}if((v.flags&8u)!=0u){metallic*=mrSample.b;rough*=mrSample.g;}let ao=select(1.0,aoSample.r,(v.flags&16u)!=0u);let view=normalize(scene.camera.xyz-v.world);let nv=max(dot(normal,view),0.001);let f0=mix(vec3f(0.04),albedo.rgb,metallic);var color=scene.ambient.rgb*scene.ambient.a*albedo.rgb*ao;for(var i=0u;i<8u;i++){let light=scene.lights[i];if(light.spotEnabled.z<0.5){continue;}var L=-light.directionRange.xyz;var attenuation=1.0;if(light.positionType.w>0.5){let delta=light.positionType.xyz-v.world;let distance=length(delta);L=delta/max(distance,0.0001);if(light.directionRange.w>0){attenuation*=clamp(1-distance/light.directionRange.w,0,1);attenuation*=attenuation;}if(light.positionType.w>1.5){let cone=dot(-L,normalize(light.directionRange.xyz));attenuation*=smoothstep(light.spotEnabled.y,light.spotEnabled.x,cone);}}let nl=max(dot(normal,L),0);if(nl<=0){continue;}let radiance=light.colorIntensity.rgb*light.colorIntensity.a*attenuation;if(scene.settings.x>0.5){let H=normalize(view+L);let nh=max(dot(normal,H),0);let vh=max(dot(view,H),0);let F=fresnel(vh,f0);let spec=distribution(nh,rough)*geometry(nv,nl,rough)*F/max(4*nv*nl,0.001);let kd=(vec3f(1)-F)*(1-metallic);color+=(kd*albedo.rgb/3.14159265+spec)*radiance*nl;}else{color+=albedo.rgb*radiance*nl;}}color+=emissionSample.rgb*v.emission.rgb*v.material.w;let distance=length(scene.camera.xyz-v.world);var fogFactor=0.0;if(scene.fog.x==1){fogFactor=clamp((distance-scene.fog.y)/max(scene.fog.z-scene.fog.y,0.001),0,1);}else if(scene.fog.x==2){fogFactor=1-exp(-scene.fog.w*distance);}else if(scene.fog.x==3){let d=scene.fog.w*distance;fogFactor=1-exp(-d*d);}return vec4f(mix(color,scene.fogColor.rgb,clamp(fogFactor,0,1)),albedo.a);}";
    WGPUShaderSourceWGSL source3d=WGPU_SHADER_SOURCE_WGSL_INIT;source3d.code=mr_string(wgsl3d);WGPUShaderModuleDescriptor shaderDesc3d=WGPU_SHADER_MODULE_DESCRIPTOR_INIT;shaderDesc3d.nextInChain=&source3d.chain;WGPUShaderModule shader3d=wgpuDeviceCreateShaderModule(mr.device,&shaderDesc3d);
    WGPUVertexAttribute meshAttributes[8];memset(meshAttributes,0,sizeof meshAttributes);
    meshAttributes[0].format=WGPUVertexFormat_Float32x3;meshAttributes[0].offset=offsetof(MRGpuVertex,x);meshAttributes[0].shaderLocation=0;
    meshAttributes[1].format=WGPUVertexFormat_Float32x3;meshAttributes[1].offset=offsetof(MRGpuVertex,nx);meshAttributes[1].shaderLocation=1;
    meshAttributes[2].format=WGPUVertexFormat_Float32x2;meshAttributes[2].offset=offsetof(MRGpuVertex,u);meshAttributes[2].shaderLocation=2;
    meshAttributes[3].format=WGPUVertexFormat_Unorm8x4;meshAttributes[3].offset=offsetof(MRGpuVertex,r);meshAttributes[3].shaderLocation=3;
    meshAttributes[4].format=WGPUVertexFormat_Uint8x4;meshAttributes[4].offset=offsetof(MRGpuVertex,boneIds);meshAttributes[4].shaderLocation=4;meshAttributes[5].format=WGPUVertexFormat_Float32x4;meshAttributes[5].offset=offsetof(MRGpuVertex,boneWeights);meshAttributes[5].shaderLocation=5;meshAttributes[6].format=WGPUVertexFormat_Float32x4;meshAttributes[6].offset=offsetof(MRGpuVertex,tangent);meshAttributes[6].shaderLocation=6;meshAttributes[7].format=WGPUVertexFormat_Float32x2;meshAttributes[7].offset=offsetof(MRGpuVertex,u2);meshAttributes[7].shaderLocation=7;
    WGPUVertexAttribute instanceAttributes[8];memset(instanceAttributes,0,sizeof instanceAttributes);for(int i=0;i<4;i++){instanceAttributes[i].format=WGPUVertexFormat_Float32x4;instanceAttributes[i].offset=(uint64_t)i*16;instanceAttributes[i].shaderLocation=(uint32_t)i+8;}instanceAttributes[4].format=WGPUVertexFormat_Unorm8x4;instanceAttributes[4].offset=offsetof(MRInstance3D,tint);instanceAttributes[4].shaderLocation=12;instanceAttributes[5].format=WGPUVertexFormat_Float32x4;instanceAttributes[5].offset=offsetof(MRInstance3D,material);instanceAttributes[5].shaderLocation=13;instanceAttributes[6].format=WGPUVertexFormat_Unorm8x4;instanceAttributes[6].offset=offsetof(MRInstance3D,emission);instanceAttributes[6].shaderLocation=14;instanceAttributes[7].format=WGPUVertexFormat_Uint32x4;instanceAttributes[7].offset=offsetof(MRInstance3D,skin);instanceAttributes[7].shaderLocation=15;
    WGPUVertexBufferLayout layouts3d[2]={WGPU_VERTEX_BUFFER_LAYOUT_INIT,WGPU_VERTEX_BUFFER_LAYOUT_INIT};layouts3d[0].arrayStride=sizeof(MRGpuVertex);layouts3d[0].stepMode=WGPUVertexStepMode_Vertex;layouts3d[0].attributeCount=8;layouts3d[0].attributes=meshAttributes;layouts3d[1].arrayStride=sizeof(MRInstance3D);layouts3d[1].stepMode=WGPUVertexStepMode_Instance;layouts3d[1].attributeCount=8;layouts3d[1].attributes=instanceAttributes;
    WGPUBlendState blend3d=WGPU_BLEND_STATE_INIT;blend3d.color.srcFactor=WGPUBlendFactor_SrcAlpha;blend3d.color.dstFactor=WGPUBlendFactor_OneMinusSrcAlpha;blend3d.color.operation=WGPUBlendOperation_Add;blend3d.alpha.srcFactor=WGPUBlendFactor_One;blend3d.alpha.dstFactor=WGPUBlendFactor_OneMinusSrcAlpha;blend3d.alpha.operation=WGPUBlendOperation_Add;
    WGPUColorTargetState target3d=WGPU_COLOR_TARGET_STATE_INIT;target3d.format=mr.config.format;target3d.blend=&blend3d;WGPUFragmentState fragment3d=WGPU_FRAGMENT_STATE_INIT;fragment3d.module=shader3d;fragment3d.entryPoint=mr_string("fs");fragment3d.targetCount=1;fragment3d.targets=&target3d;
    WGPURenderPipelineDescriptor pipeline3d=WGPU_RENDER_PIPELINE_DESCRIPTOR_INIT;pipeline3d.layout=mr.materialPipelineLayout;pipeline3d.vertex.module=shader3d;pipeline3d.vertex.entryPoint=mr_string("vs");pipeline3d.vertex.bufferCount=2;pipeline3d.vertex.buffers=layouts3d;pipeline3d.fragment=&fragment3d;pipeline3d.primitive.topology=WGPUPrimitiveTopology_TriangleList;pipeline3d.primitive.cullMode=WGPUCullMode_None;pipeline3d.depthStencil=&depth;mr.pipeline3d=wgpuDeviceCreateRenderPipeline(mr.device,&pipeline3d);
    const char *skyWgsl="struct Scene{vp:mat4x4f,camera:vec4f,ambient:vec4f,fogColor:vec4f,fog:vec4f,skyRight:vec4f,skyUp:vec4f,skyForward:vec4f,settings:vec4f};struct O{@builtin(position)p:vec4f,@location(0)ray:vec3f};@group(0)@binding(0)var smp:sampler;@group(0)@binding(1)var tex:texture_2d<f32>;@group(1)@binding(0)var<uniform>scene:Scene;@vertex fn vs(@builtin(vertex_index)i:u32)->O{let x=f32((i<<1u)&2u);let y=f32(i&2u);let q=vec2f(x*2-1,1-y*2);var o:O;o.p=vec4f(q,1,1);o.ray=normalize(scene.skyForward.xyz+q.x*scene.skyRight.xyz*scene.skyRight.w*scene.skyUp.w+q.y*scene.skyUp.xyz*scene.skyUp.w);return o;}@fragment fn fs(o:O)->@location(0)vec4f{let d=normalize(o.ray);let uv=vec2f(atan2(d.z,d.x)/(6.2831853)+0.5,acos(clamp(d.y,-1,1))/3.14159265);return textureSample(tex,smp,uv)*vec4f(scene.settings.yzw,1);}";WGPUShaderSourceWGSL skySource=WGPU_SHADER_SOURCE_WGSL_INIT;skySource.code=mr_string(skyWgsl);WGPUShaderModuleDescriptor skyDesc=WGPU_SHADER_MODULE_DESCRIPTOR_INIT;skyDesc.nextInChain=&skySource.chain;WGPUShaderModule skyShader=wgpuDeviceCreateShaderModule(mr.device,&skyDesc);WGPUBindGroupLayout skyLayouts[2]={mr.textureLayout,mr.sceneLayout3d};WGPUPipelineLayoutDescriptor skyLayoutDesc=WGPU_PIPELINE_LAYOUT_DESCRIPTOR_INIT;skyLayoutDesc.bindGroupLayoutCount=2;skyLayoutDesc.bindGroupLayouts=skyLayouts;WGPUPipelineLayout skyLayout=wgpuDeviceCreatePipelineLayout(mr.device,&skyLayoutDesc);WGPUFragmentState skyFragment=WGPU_FRAGMENT_STATE_INIT;skyFragment.module=skyShader;skyFragment.entryPoint=mr_string("fs");skyFragment.targetCount=1;skyFragment.targets=&target3d;WGPUDepthStencilState skyDepth=depth;skyDepth.depthWriteEnabled=WGPUOptionalBool_False;WGPURenderPipelineDescriptor skyPipeline=WGPU_RENDER_PIPELINE_DESCRIPTOR_INIT;skyPipeline.layout=skyLayout;skyPipeline.vertex.module=skyShader;skyPipeline.vertex.entryPoint=mr_string("vs");skyPipeline.fragment=&skyFragment;skyPipeline.primitive.topology=WGPUPrimitiveTopology_TriangleList;skyPipeline.depthStencil=&skyDepth;mr.skyboxPipeline=wgpuDeviceCreateRenderPipeline(mr.device,&skyPipeline);wgpuPipelineLayoutRelease(skyLayout);wgpuShaderModuleRelease(skyShader);
    wgpuShaderModuleRelease(shader3d);
    wgpuShaderModuleRelease(shader); wgpuPipelineLayoutRelease(pipelineLayout);
    WGPUBufferDescriptor buffer=WGPU_BUFFER_DESCRIPTOR_INIT;
    buffer.size=sizeof mr.vertices; buffer.usage=WGPUBufferUsage_Vertex|WGPUBufferUsage_CopyDst;
    mr.buffer=wgpuDeviceCreateBuffer(mr.device,&buffer);
    buffer.size=sizeof mr.instances3d;mr.instanceBuffer3d=wgpuDeviceCreateBuffer(mr.device,&buffer);
    WGPUBufferDescriptor sceneBuffer=WGPU_BUFFER_DESCRIPTOR_INIT;sceneBuffer.size=sizeof(MRScene3D);sceneBuffer.usage=WGPUBufferUsage_Uniform|WGPUBufferUsage_CopyDst;mr.sceneBuffer3d=wgpuDeviceCreateBuffer(mr.device,&sceneBuffer);sceneBuffer.size=sizeof mr.boneMatricesFrame;sceneBuffer.usage=WGPUBufferUsage_Storage|WGPUBufferUsage_CopyDst;mr.boneBuffer3d=wgpuDeviceCreateBuffer(mr.device,&sceneBuffer);
    WGPUBindGroupEntry sceneGroupEntries[2]={WGPU_BIND_GROUP_ENTRY_INIT,WGPU_BIND_GROUP_ENTRY_INIT};sceneGroupEntries[0].binding=0;sceneGroupEntries[0].buffer=mr.sceneBuffer3d;sceneGroupEntries[0].size=sizeof(MRScene3D);sceneGroupEntries[1].binding=1;sceneGroupEntries[1].buffer=mr.boneBuffer3d;sceneGroupEntries[1].size=sizeof mr.boneMatricesFrame;WGPUBindGroupDescriptor sceneGroupDesc=WGPU_BIND_GROUP_DESCRIPTOR_INIT;sceneGroupDesc.layout=mr.sceneLayout3d;sceneGroupDesc.entryCount=2;sceneGroupDesc.entries=sceneGroupEntries;mr.sceneGroup3d=wgpuDeviceCreateBindGroup(mr.device,&sceneGroupDesc);
    WGPUBufferDescriptor defaultUniformDesc=WGPU_BUFFER_DESCRIPTOR_INIT;defaultUniformDesc.size=2048;defaultUniformDesc.usage=WGPUBufferUsage_Uniform|WGPUBufferUsage_CopyDst;mr.defaultUniformBuffer3d=wgpuDeviceCreateBuffer(mr.device,&defaultUniformDesc);WGPUBindGroupEntry defaultUniformEntry=WGPU_BIND_GROUP_ENTRY_INIT;defaultUniformEntry.binding=0;defaultUniformEntry.buffer=mr.defaultUniformBuffer3d;defaultUniformEntry.size=2048;WGPUBindGroupDescriptor defaultUniformGroupDesc=WGPU_BIND_GROUP_DESCRIPTOR_INIT;defaultUniformGroupDesc.layout=mr.uniformLayout;defaultUniformGroupDesc.entryCount=1;defaultUniformGroupDesc.entries=&defaultUniformEntry;mr.defaultUniformGroup3d=wgpuDeviceCreateBindGroup(mr.device,&defaultUniformGroupDesc);
    const unsigned char white[4]={255,255,255,255}; mr.white=LoadTextureRGBA(white,1,1).id;
    mr.shapesTexture=(Texture2D){mr.white,1,1,1,7}; mr.shapesSource=(Rectangle){0,0,1,1};
    return mr.pipelines[0] && mr.pipeline3d && mr.buffer && mr.instanceBuffer3d && mr.sceneGroup3d && mr.white && !mr.error;
}
static bool mr_resize_depth(int width,int height) {
    if(width<=0||height<=0)return false;
    if(mr.depthView&&mr.depthWidth==width&&mr.depthHeight==height)return true;
    if(mr.depthView)wgpuTextureViewRelease(mr.depthView);
    if(mr.depthTexture)wgpuTextureRelease(mr.depthTexture);
    mr.depthView=NULL;mr.depthTexture=NULL;mr.depthWidth=mr.depthHeight=0;
    WGPUTextureDescriptor descriptor=WGPU_TEXTURE_DESCRIPTOR_INIT;
    descriptor.size=(WGPUExtent3D){(uint32_t)width,(uint32_t)height,1};
    descriptor.dimension=WGPUTextureDimension_2D;descriptor.format=WGPUTextureFormat_Depth24Plus;
    descriptor.usage=WGPUTextureUsage_RenderAttachment;
    mr.depthTexture=wgpuDeviceCreateTexture(mr.device,&descriptor);
    if(!mr.depthTexture)return false;
    mr.depthView=wgpuTextureCreateView(mr.depthTexture,NULL);
    if(!mr.depthView){wgpuTextureRelease(mr.depthTexture);mr.depthTexture=NULL;return false;}
    mr.depthWidth=width;mr.depthHeight=height;return true;
}
void InitWindow(int width,int height,const char *title) {
    if (mr.instance) { fprintf(stderr,"raygpu: only one window is supported\n"); return; }
    memset(&mr,0,sizeof mr); mr.flags=mr_config_flags; mr.fps=60; mr.dt=1.0f/60; mr.clear=BLACK; mr.exitKey=KEY_ESCAPE; mr.focused=true; mr.mouseScale=(Vector2){1,1};mr.gesturesEnabled=0x3ffu;mr_init_3d_defaults();
    mr.start=mr.previous=mr_clock();
    if (width<=0 || height<=0 || width>8192 || height>8192) { mr_error("Invalid window size"); return; }
    mr.width=width; mr.height=height;
    /* Keep window, framebuffer and mouse coordinates in physical pixels.
     * Without DPI awareness Windows turns 1920x1080 into 3840x2160 when the
     * desktop uses 200% display scaling. This must happen before any HWND is
     * created. */
    SetProcessDPIAware();
    HINSTANCE module=GetModuleHandleA(NULL);
    WNDCLASSA wc={0}; wc.lpfnWndProc=mr_window_proc; wc.hInstance=module;
    wc.lpszClassName="RaygpuWebGPU"; wc.hCursor=LoadCursor(NULL,IDC_ARROW);
    if (!RegisterClassA(&wc) && GetLastError()!=ERROR_CLASS_ALREADY_EXISTS) { mr_error("Cannot register window"); return; }
    DWORD windowStyle=mr_window_style(mr.flags),windowExStyle=0;
    if(mr.flags&FLAG_WINDOW_TOPMOST)windowExStyle|=WS_EX_TOPMOST;
    if((mr.flags&FLAG_WINDOW_MOUSE_PASSTHROUGH)&&(mr.flags&FLAG_WINDOW_UNDECORATED))windowExStyle|=WS_EX_TRANSPARENT;
    RECT rect={0,0,width,height}; AdjustWindowRect(&rect,windowStyle,FALSE);
    int windowWidth=rect.right-rect.left,windowHeight=rect.bottom-rect.top,windowX=CW_USEDEFAULT,windowY=CW_USEDEFAULT;
    POINT origin={0,0};HMONITOR monitor=MonitorFromPoint(origin,MONITOR_DEFAULTTOPRIMARY);MONITORINFO monitorInfo={0};monitorInfo.cbSize=sizeof monitorInfo;
    if(monitor&&GetMonitorInfoA(monitor,&monitorInfo)){int workWidth=monitorInfo.rcWork.right-monitorInfo.rcWork.left,workHeight=monitorInfo.rcWork.bottom-monitorInfo.rcWork.top;windowX=monitorInfo.rcWork.left+(workWidth-windowWidth)/2;windowY=monitorInfo.rcWork.top+(workHeight-windowHeight)/2;}
    mr.window=CreateWindowExA(windowExStyle,wc.lpszClassName,title,windowStyle,windowX,windowY,
        windowWidth,windowHeight,NULL,NULL,module,NULL);
    if (!mr.window) { mr_error("Cannot create window"); return; }
    DragAcceptFiles(mr.window,TRUE);
    if(!(mr.flags&FLAG_WINDOW_HIDDEN)) {
        int show=SW_SHOW;
        if(mr.flags&FLAG_WINDOW_MINIMIZED)show=SW_SHOWMINIMIZED;
        else if(mr.flags&FLAG_WINDOW_MAXIMIZED)show=SW_SHOWMAXIMIZED;
        else if(mr.flags&FLAG_WINDOW_UNFOCUSED)show=SW_SHOWNOACTIVATE;
        ShowWindow(mr.window,show);
    }
    if(mr.flags&FLAG_FULLSCREEN_MODE)mr_set_fullscreen_mode(true,false);
    else if(mr.flags&FLAG_BORDERLESS_WINDOWED_MODE)mr_set_fullscreen_mode(false,true);
    mr.instance=wgpuCreateInstance(NULL);
    if (!mr.instance) { mr_error("Cannot create WebGPU instance"); CloseWindow(); return; }
    WGPUSurfaceDescriptor surface=WGPU_SURFACE_DESCRIPTOR_INIT;
    WGPUSurfaceSourceWindowsHWND hwnd=WGPU_SURFACE_SOURCE_WINDOWS_HWND_INIT;
    hwnd.hinstance=module; hwnd.hwnd=mr.window; surface.nextInChain=&hwnd.chain;
    mr.surface=wgpuInstanceCreateSurface(mr.instance,&surface);
    if (!mr.surface) { mr_error("Cannot create WebGPU surface"); CloseWindow(); return; }
    WGPURequestAdapterOptions options=WGPU_REQUEST_ADAPTER_OPTIONS_INIT;
    options.backendType=WGPUBackendType_D3D12;
    options.compatibleSurface=mr.surface; options.powerPreference=WGPUPowerPreference_HighPerformance;
    WGPURequestAdapterCallbackInfo adapterInfo=WGPU_REQUEST_ADAPTER_CALLBACK_INFO_INIT;
    adapterInfo.mode=MR_CALLBACK_MODE; adapterInfo.callback=mr_adapter;
    wgpuInstanceRequestAdapter(mr.instance,&options,adapterInfo);
    while (!mr.adapterDone) { mr_pump(); mr_yield(1); }
    if (!mr.adapter || mr.close) { CloseWindow(); return; }
    WGPUDeviceDescriptor device=WGPU_DEVICE_DESCRIPTOR_INIT;
    device.uncapturedErrorCallbackInfo.callback=mr_gpu_error;
    device.deviceLostCallbackInfo.mode=MR_CALLBACK_MODE; device.deviceLostCallbackInfo.callback=mr_device_lost;
    WGPURequestDeviceCallbackInfo deviceInfo=WGPU_REQUEST_DEVICE_CALLBACK_INFO_INIT;
    deviceInfo.mode=MR_CALLBACK_MODE; deviceInfo.callback=mr_device;
    wgpuAdapterRequestDevice(mr.adapter,&device,deviceInfo);
    while (!mr.deviceDone) { mr_pump(); mr_yield(1); }
    if (!mr.device || mr.close) { CloseWindow(); return; }
    mr.queue=wgpuDeviceGetQueue(mr.device); mr.ready=mr_renderer();
    if (!mr.ready) { CloseWindow(); return; }
    mr.previous=mr_clock(); puts("raygpu: WebGPU renderer ready");
}
#else
void InitWindow(int width,int height,const char *title) {
    if (mr.ready) return;
    memset(&mr,0,sizeof mr); mr.flags=mr_config_flags; mr.fps=60; mr.dt=1.0f/60; mr.clear=BLACK; mr.exitKey=KEY_ESCAPE; mr.focused=true; mr.mouseScale=(Vector2){1,1};mr.gesturesEnabled=0x3ffu;mr_init_3d_defaults();
    if (width<=0 || height<=0 || width>8192 || height>8192) { mr_error("Invalid window size"); return; }
    mr.width=width; mr.height=height; mr.start=mr.previous=mr_clock();
    mr_web_init(width,height,title); mr_web_window_command(7,(int)mr.flags,0,NULL); mr.ready=true;
    const unsigned char white[4]={255,255,255,255}; mr.white=LoadTextureRGBA(white,1,1).id;
    mr.shapesTexture=(Texture2D){mr.white,1,1,1,7}; mr.shapesSource=(Rectangle){0,0,1,1};
    puts("raygpu: WebGPU renderer ready");
}
#endif
bool IsWindowReady(void) { return mr.ready; }
bool RayGPUHadError(void) { return mr.error; }
bool WindowShouldClose(void) {
    mr_pump();
    mr_dispatch_readbacks();
    if (mr.exitKey>KEY_NULL && IsKeyPressed(mr.exitKey)) mr.close=true;
    double now=mr_clock(); mr.dt=(float)(now-mr.previous); mr.previous=now; mr.frameStart=now;
    if (mr.dt>0.1f) mr.dt=0.1f;
    return mr.close || !mr.ready;
}
int GetScreenWidth(void) { return mr.width; }
int GetScreenHeight(void) { return mr.height; }
int GetRenderWidth(void) { return mr.width; }
int GetRenderHeight(void) { return mr.height; }
bool IsWindowResized(void) { return mr.resized; }
bool IsWindowFocused(void) { return mr.focused; }
bool IsWindowMinimized(void) {
#ifdef _WIN32
    return mr.window && IsIconic(mr.window);
#else
    return false;
#endif
}
bool IsWindowMaximized(void) {
#ifdef _WIN32
    return mr.window && IsZoomed(mr.window);
#else
    return false;
#endif
}
bool IsWindowFullscreen(void) {
#ifdef _WIN32
    return mr.fullscreen;
#else
    return mr_web_window_query(0,0)!=0;
#endif
}
void ToggleFullscreen(void) {
#ifdef _WIN32
    mr_set_fullscreen_mode(!mr.fullscreen,false);
#else
    mr_web_window_command(2,0,0,NULL);
#endif
}
void ToggleBorderlessWindowed(void) {
#ifdef _WIN32
    mr_set_fullscreen_mode(false,!mr.borderless);
#else
    mr_web_window_command(3,0,0,NULL);
#endif
}
Vector2 GetWindowPosition(void) {
#ifdef _WIN32
    RECT rect={0}; if (mr.window && GetWindowRect(mr.window,&rect)) return (Vector2){(float)rect.left,(float)rect.top};
#endif
    return (Vector2){0};
}
Vector2 GetWindowScaleDPI(void) {
#ifdef _WIN32
    if (mr.window) { float scale=GetDpiForWindow(mr.window)/96.0f; if (scale>0) return (Vector2){scale,scale}; }
#else
    float scale=mr_web_window_query(10,0)/1000.0f; if(scale>0)return(Vector2){scale,scale};
#endif
    return (Vector2){1,1};
}
int GetMonitorCount(void) {
#ifdef _WIN32
    return mr_get_monitors().count;
#else
    return mr_web_window_query(1,0);
#endif
}
int GetCurrentMonitor(void) {
#ifdef _WIN32
    return mr.window ? mr_monitor_index(MonitorFromWindow(mr.window,MONITOR_DEFAULTTONEAREST)) : 0;
#else
    return mr_web_window_query(2,0);
#endif
}
Vector2 GetMonitorPosition(int monitor) {
#ifdef _WIN32
    HMONITOR handle=mr_get_monitor(monitor);MONITORINFO info={0};info.cbSize=sizeof info;
    if(handle&&GetMonitorInfoA(handle,&info))return(Vector2){(float)info.rcMonitor.left,(float)info.rcMonitor.top};
    return(Vector2){0};
#else
    return(Vector2){(float)mr_web_window_query(3,monitor),(float)mr_web_window_query(4,monitor)};
#endif
}
int GetMonitorWidth(int monitor) {
#ifdef _WIN32
    HMONITOR handle=mr_get_monitor(monitor);MONITORINFO info={0};info.cbSize=sizeof info;return handle&&GetMonitorInfoA(handle,&info)?info.rcMonitor.right-info.rcMonitor.left:0;
#else
    return mr_web_window_query(5,monitor);
#endif
}
int GetMonitorHeight(int monitor) {
#ifdef _WIN32
    HMONITOR handle=mr_get_monitor(monitor);MONITORINFO info={0};info.cbSize=sizeof info;return handle&&GetMonitorInfoA(handle,&info)?info.rcMonitor.bottom-info.rcMonitor.top:0;
#else
    return mr_web_window_query(6,monitor);
#endif
}
#ifdef _WIN32
static int mr_monitor_device_cap(int monitor,int capability) {
    HMONITOR handle=mr_get_monitor(monitor);MONITORINFOEXA info={0};info.cbSize=sizeof info;
    if(!handle||!GetMonitorInfoA(handle,(MONITORINFO *)&info))return 0;
    typedef HDC (WINAPI *MRCreateDC)(LPCSTR,LPCSTR,LPCSTR,const void *);
    typedef int (WINAPI *MRGetCaps)(HDC,int);typedef BOOL (WINAPI *MRDeleteDC)(HDC);
    HMODULE library=GetModuleHandleA("gdi32.dll");if(!library)return 0;
    MRCreateDC createDC=(MRCreateDC)(void *)GetProcAddress(library,"CreateDCA");
    MRGetCaps getCaps=(MRGetCaps)(void *)GetProcAddress(library,"GetDeviceCaps");
    MRDeleteDC deleteDC=(MRDeleteDC)(void *)GetProcAddress(library,"DeleteDC");
    if(!createDC||!getCaps||!deleteDC)return 0;HDC dc=createDC("DISPLAY",info.szDevice,NULL,NULL);if(!dc)return 0;
    int value=getCaps(dc,capability);deleteDC(dc);return value;
}
#endif
int GetMonitorPhysicalWidth(int monitor) {
#ifdef _WIN32
    return mr_monitor_device_cap(monitor,4);
#else
    return mr_web_window_query(7,monitor);
#endif
}
int GetMonitorPhysicalHeight(int monitor) {
#ifdef _WIN32
    return mr_monitor_device_cap(monitor,6);
#else
    return mr_web_window_query(8,monitor);
#endif
}
int GetMonitorRefreshRate(int monitor) {
#ifdef _WIN32
    return mr_monitor_device_cap(monitor,116);
#else
    return mr_web_window_query(9,monitor);
#endif
}
const char *GetMonitorName(int monitor) {
    static char name[128];name[0]=0;
#ifdef _WIN32
    HMONITOR handle=mr_get_monitor(monitor);MONITORINFOEXA info={0};info.cbSize=sizeof info;
    if(handle&&GetMonitorInfoA(handle,(MONITORINFO *)&info)){
        typedef struct MRDisplayDevice { DWORD cb;CHAR deviceName[32],deviceString[128];DWORD stateFlags;CHAR deviceId[128],deviceKey[128]; } MRDisplayDevice;
        typedef BOOL (WINAPI *MREnumDisplayDevices)(LPCSTR,DWORD,MRDisplayDevice *,DWORD);
        MREnumDisplayDevices enumerate=(MREnumDisplayDevices)(void *)GetProcAddress(GetModuleHandleA("user32.dll"),"EnumDisplayDevicesA");
        MRDisplayDevice display={0};display.cb=sizeof display;
        if(enumerate&&enumerate(info.szDevice,0,&display,0))snprintf(name,sizeof name,"%s",display.deviceString);else snprintf(name,sizeof name,"%s",info.szDevice);
    }
#else
    mr_web_window_text(0,monitor,name,(int)sizeof name);
#endif
    return name;
}
void SetWindowTitle(const char *title) {
    if (!title) return;
#ifdef _WIN32
    if (mr.window) SetWindowTextA(mr.window,title);
#else
    mr_web_window_command(0,0,0,title);
#endif
}
#ifdef _WIN32
static HICON mr_create_window_icon(Image image) {
    if(!IsImageValid(image)||image.width<=0||image.height<=0)return NULL;
    Color *colors=LoadImageColors(image);if(!colors)return NULL;
    size_t colorSize=(size_t)image.width*image.height*4,maskStride=(size_t)((image.width+31)/32)*4,maskSize=maskStride*image.height;
    unsigned char *colorBits=MemAlloc((unsigned int)colorSize),*maskBits=MemAlloc((unsigned int)maskSize);HICON icon=NULL;
    if(colorBits&&maskBits){memset(maskBits,0,maskSize);for(int y=0;y<image.height;y++)for(int x=0;x<image.width;x++){Color c=colors[y*image.width+x];size_t at=((size_t)(image.height-1-y)*image.width+x)*4;colorBits[at]=c.b;colorBits[at+1]=c.g;colorBits[at+2]=c.r;colorBits[at+3]=c.a;}icon=CreateIcon(GetModuleHandleA(NULL),image.width,image.height,1,32,maskBits,colorBits);}
    MemFree(maskBits);MemFree(colorBits);UnloadImageColors(colors);return icon;
}
#endif
void SetWindowIcons(Image *images,int count) {
    if(!images||count<=0)return;int smallest=0,largest=0;for(int i=1;i<count;i++){int area=images[i].width*images[i].height;if(area<images[smallest].width*images[smallest].height)smallest=i;if(area>images[largest].width*images[largest].height)largest=i;}
#ifdef _WIN32
    HICON big=mr_create_window_icon(images[largest]),small=mr_create_window_icon(images[smallest]);
    if(big){SendMessageA(mr.window,WM_SETICON,ICON_BIG,(LPARAM)big);if(mr.bigIcon)DestroyIcon(mr.bigIcon);mr.bigIcon=big;}
    if(small){SendMessageA(mr.window,WM_SETICON,ICON_SMALL,(LPARAM)small);if(mr.smallIcon)DestroyIcon(mr.smallIcon);mr.smallIcon=small;}
#else
    Color *colors=LoadImageColors(images[largest]);if(colors){mr_web_window_icon(colors,images[largest].width,images[largest].height);UnloadImageColors(colors);}
#endif
}
void SetWindowIcon(Image image) { SetWindowIcons(&image,1); }
void SetWindowPosition(int x,int y) {
#ifdef _WIN32
    if (mr.window) SetWindowPos(mr.window,NULL,x,y,0,0,SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE);
#else
    (void)x; (void)y;
#endif
}
void SetWindowMonitor(int monitor) {
#ifdef _WIN32
    HMONITOR handle=mr_get_monitor(monitor);MONITORINFO info={0};info.cbSize=sizeof info;if(!handle||!GetMonitorInfoA(handle,&info)||!mr.window)return;
    if(mr.fullscreen||mr.borderless)SetWindowPos(mr.window,HWND_TOP,info.rcMonitor.left,info.rcMonitor.top,info.rcMonitor.right-info.rcMonitor.left,info.rcMonitor.bottom-info.rcMonitor.top,SWP_NOACTIVATE);
    else SetWindowPos(mr.window,NULL,info.rcWork.left,info.rcWork.top,0,0,SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE);
#else
    mr_web_window_command(4,monitor,0,NULL);
#endif
}
void SetWindowSize(int width,int height) {
    if (width<=0 || height<=0 || width>8192 || height>8192) return;
#ifdef _WIN32
    if (mr.window) { DWORD style=(DWORD)GetWindowLongPtrA(mr.window,GWL_STYLE); RECT rect={0,0,width,height}; AdjustWindowRect(&rect,style,FALSE); SetWindowPos(mr.window,NULL,0,0,rect.right-rect.left,rect.bottom-rect.top,SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE); }
#else
    mr.width=width; mr.height=height; mr.resized=true; mr_web_window_command(1,width,height,NULL);
#endif
}
void SetWindowMinSize(int width,int height) { mr.minWidth=width>0?width:0; mr.minHeight=height>0?height:0; }
void SetWindowMaxSize(int width,int height) { mr.maxWidth=width>0?width:0; mr.maxHeight=height>0?height:0; }
void SetWindowOpacity(float opacity) {
    opacity=mr_clamp01(opacity);
#ifdef _WIN32
    if(!mr.window)return;LONG_PTR style=GetWindowLongPtrA(mr.window,GWL_EXSTYLE);SetWindowLongPtrA(mr.window,GWL_EXSTYLE,style|WS_EX_LAYERED);SetLayeredWindowAttributes(mr.window,0,(BYTE)(opacity*255.0f+0.5f),LWA_ALPHA);
#else
    mr_web_window_command(5,(int)(opacity*255.0f+0.5f),0,NULL);
#endif
}
void SetWindowFocused(void) {
#ifdef _WIN32
    if(mr.window){ShowWindow(mr.window,SW_RESTORE);SetForegroundWindow(mr.window);SetFocus(mr.window);}
#else
    mr_web_window_command(6,0,0,NULL);
#endif
}
void MinimizeWindow(void) {
#ifdef _WIN32
    if (mr.window) ShowWindow(mr.window,SW_MINIMIZE);
#endif
}
void MaximizeWindow(void) {
#ifdef _WIN32
    if (mr.window) ShowWindow(mr.window,SW_MAXIMIZE);
#endif
}
void RestoreWindow(void) {
#ifdef _WIN32
    if (mr.window) ShowWindow(mr.window,SW_RESTORE);
#endif
}
static void mr_cache_clipboard(const char *text) {
    if(!text)text="";size_t length=TextLength(text);char *copy=MemRealloc(mr.clipboardText,(unsigned int)length+1);if(copy){memcpy(copy,text,length+1);mr.clipboardText=copy;}
}
void SetClipboardText(const char *text) {
    if(!text)return;mr_cache_clipboard(text);
#ifdef _WIN32
    int count=MultiByteToWideChar(CP_UTF8,0,text,-1,NULL,0);if(count<=0||!OpenClipboard(mr.window))return;
    EmptyClipboard();HGLOBAL memory=GlobalAlloc(GMEM_MOVEABLE,(SIZE_T)count*sizeof(WCHAR));
    if(memory){WCHAR *wide=GlobalLock(memory);if(wide){MultiByteToWideChar(CP_UTF8,0,text,-1,wide,count);GlobalUnlock(memory);if(!SetClipboardData(CF_UNICODETEXT,memory))GlobalFree(memory);}else GlobalFree(memory);}
    CloseClipboard();
#else
    mr_web_clipboard_set(text);
#endif
}
const char *GetClipboardText(void) {
#ifdef _WIN32
    if(OpenClipboard(mr.window)){HANDLE memory=GetClipboardData(CF_UNICODETEXT);if(memory){const WCHAR *wide=GlobalLock(memory);if(wide){int count=WideCharToMultiByte(CP_UTF8,0,wide,-1,NULL,0,NULL,NULL);if(count>0){char *text=MemAlloc((unsigned int)count);if(text){WideCharToMultiByte(CP_UTF8,0,wide,-1,text,count,NULL,NULL);mr_cache_clipboard(text);MemFree(text);}}GlobalUnlock(memory);}}CloseClipboard();}
#else
    int size=mr_web_clipboard_size();if(size>0){char *text=MemAlloc((unsigned int)size);if(text){if(mr_web_clipboard_get(text,size))mr_cache_clipboard(text);MemFree(text);}}
#endif
    return mr.clipboardText?mr.clipboardText:"";
}
bool IsFileDropped(void) {
#ifdef _WIN32
    return mr.droppedFiles.count>0;
#else
    return mr_web_drop_count()>0;
#endif
}
FilePathList LoadDroppedFiles(void) {
#ifdef _WIN32
    FilePathList files=mr.droppedFiles;mr.droppedFiles=(FilePathList){0};return files;
#else
    int count=mr_web_drop_count();if(count<=0)return(FilePathList){0};FilePathList files={0};files.paths=MemAlloc((unsigned int)count*sizeof *files.paths);if(!files.paths)return files;memset(files.paths,0,(size_t)count*sizeof *files.paths);files.capacity=(unsigned int)count;
    for(int i=0;i<count;i++){int size=mr_web_drop_name_size(i);if(size<=0)continue;char *name=MemAlloc((unsigned int)size);if(name&&mr_web_drop_name(i,name,size))files.paths[files.count++]=name;else MemFree(name);}mr_web_drop_clear();return files;
#endif
}
void UnloadDroppedFiles(FilePathList files) { if(files.paths){for(unsigned int i=0;i<files.count;i++)MemFree(files.paths[i]);MemFree(files.paths);} }
void SetExitKey(int key) { mr.exitKey=(key>=KEY_NULL && key<512)?key:KEY_NULL; }
void SetTargetFPS(int fps) {
    mr.fps=fps>0 ? fps : 0;
#ifdef __wasm__
    mr_web_fps(mr.fps);
#endif
}
float GetFrameTime(void) { return mr.dt; }
double GetTime(void) { return mr_clock()-mr.start; }
int GetFPS(void) { return mr.dt>0.000001f ? (int)(1.0f/mr.dt+0.5f) : 0; }
void WaitTime(double seconds) {
    if (seconds<=0) return;
    double end=mr_clock()+seconds;
#ifdef _WIN32
    while (mr_clock()+0.002<end) Sleep(1);
#endif
    while (mr_clock()<end) { }
}
#ifdef __wasm__
typedef struct MRMemoryBlock { size_t size; struct MRMemoryBlock *next; bool free; } MRMemoryBlock;
extern unsigned char __heap_base;
static MRMemoryBlock *mr_memory_head;
static size_t mr_align_size(size_t size) {
    const size_t alignment=_Alignof(max_align_t);
    return (size+alignment-1)&~(alignment-1);
}
static bool mr_grow_memory(size_t required) {
    const size_t pageSize=65536;
    size_t pages=(required+sizeof(MRMemoryBlock)+pageSize-1)/pageSize;
    size_t oldPages=__builtin_wasm_memory_size(0);
    size_t result=__builtin_wasm_memory_grow(0,pages);
    if (result==(size_t)-1) return false;
    MRMemoryBlock *tail=mr_memory_head;
    while (tail && tail->next) tail=tail->next;
    unsigned char *oldEnd=(unsigned char *)(oldPages*pageSize);
    if (tail && tail->free && (unsigned char *)(tail+1)+tail->size==oldEnd) tail->size+=pages*pageSize;
    else {
        MRMemoryBlock *block=(MRMemoryBlock *)oldEnd;
        block->size=pages*pageSize-sizeof(MRMemoryBlock); block->next=NULL; block->free=true;
        if (tail) tail->next=block; else mr_memory_head=block;
    }
    return true;
}
void *MemAlloc(unsigned int requested) {
    if (!requested) return NULL;
    if (!mr_memory_head) {
        uintptr_t start=((uintptr_t)&__heap_base+_Alignof(max_align_t)-1)&~(uintptr_t)(_Alignof(max_align_t)-1);
        size_t end=__builtin_wasm_memory_size(0)*65536u;
        if (end<=start+sizeof(MRMemoryBlock)) return NULL;
        mr_memory_head=(MRMemoryBlock *)start;
        mr_memory_head->size=end-start-sizeof(MRMemoryBlock);
        mr_memory_head->next=NULL; mr_memory_head->free=true;
    }
    size_t size=mr_align_size(requested);
    for (MRMemoryBlock *block=mr_memory_head;block;block=block->next) if (block->free && block->size>=size) {
        if (block->size>=size+sizeof(MRMemoryBlock)+_Alignof(max_align_t)) {
            MRMemoryBlock *next=(MRMemoryBlock *)((unsigned char *)(block+1)+size);
            next->size=block->size-size-sizeof(MRMemoryBlock); next->next=block->next; next->free=true;
            block->next=next; block->size=size;
        }
        block->free=false; return block+1;
    }
    return mr_grow_memory(size) ? MemAlloc(requested) : NULL;
}
void MemFree(void *pointer) {
    if (!pointer) return;
    MRMemoryBlock *block=(MRMemoryBlock *)pointer-1; block->free=true;
    for (MRMemoryBlock *it=mr_memory_head;it && it->next;) {
        if (it->free && it->next->free) { it->size+=sizeof(MRMemoryBlock)+it->next->size; it->next=it->next->next; }
        else it=it->next;
    }
}
void *MemRealloc(void *pointer,unsigned int size) {
    if (!pointer) return MemAlloc(size);
    if (!size) { MemFree(pointer); return NULL; }
    MRMemoryBlock *old=(MRMemoryBlock *)pointer-1;
    if (old->size>=size) return pointer;
    void *replacement=MemAlloc(size);
    if (replacement) { memcpy(replacement,pointer,old->size); MemFree(pointer); }
    return replacement;
}
#else
void *MemAlloc(unsigned int size) { return size ? malloc(size) : NULL; }
void *MemRealloc(void *pointer,unsigned int size) { return realloc(pointer,size); }
void MemFree(void *pointer) { free(pointer); }
#endif
unsigned char *LoadFileData(const char *fileName,int *dataSize) {
    if (dataSize) *dataSize=0; if (!fileName) return NULL;
#ifdef __wasm__
    int size=mr_web_file_size(fileName); if (size<=0) return NULL;
    unsigned char *data=MemAlloc((unsigned int)size); if (!data) return NULL;
    int read=mr_web_file_read(fileName,data,size); if (read!=size) { MemFree(data); return NULL; }
#else
    FILE *file=NULL; if (fopen_s(&file,fileName,"rb")!=0 || !file) return NULL;
    if (fseek(file,0,SEEK_END)!=0) { fclose(file); return NULL; }
    long length=ftell(file); if (length<=0 || length>0x7fffffffL || fseek(file,0,SEEK_SET)!=0) { fclose(file); return NULL; }
    int size=(int)length; unsigned char *data=MemAlloc((unsigned int)size);
    if (!data || fread(data,1,(size_t)size,file)!=(size_t)size) { fclose(file); MemFree(data); return NULL; }
    fclose(file);
#endif
    if (dataSize) *dataSize=size; return data;
}
void UnloadFileData(unsigned char *data) { MemFree(data); }
bool SaveFileData(const char *fileName,void *data,int dataSize) {
    if (!fileName || !data || dataSize<0) return false;
#ifdef __wasm__
    return mr_web_file_write(fileName,data,dataSize)!=0;
#else
    FILE *file=NULL; if (fopen_s(&file,fileName,"wb")!=0 || !file) return false;
    bool saved=fwrite(data,1,(size_t)dataSize,file)==(size_t)dataSize;
    if (fclose(file)!=0) saved=false;
    return saved;
#endif
}
int GetFileLength(const char *fileName) {
#ifdef __wasm__
    return fileName ? mr_web_file_size(fileName) : 0;
#else
    if (!fileName) return 0; FILE *file=NULL; if (fopen_s(&file,fileName,"rb")!=0 || !file) return 0;
    if (fseek(file,0,SEEK_END)!=0) { fclose(file); return 0; } long length=ftell(file); fclose(file);
    return length>0 && length<=0x7fffffffL ? (int)length : 0;
#endif
}
bool FileExists(const char *fileName) { return GetFileLength(fileName)>0; }
char *LoadFileText(const char *fileName) {
    int size=0; unsigned char *data=LoadFileData(fileName,&size); if (!data) return NULL;
    char *text=MemAlloc((unsigned int)size+1); if (text) { memcpy(text,data,(size_t)size); text[size]='\0'; }
    UnloadFileData(data); return text;
}
void UnloadFileText(char *text) { MemFree(text); }
bool SaveFileText(const char *fileName,char *text) {
    if (!text) return false;
    int length=0; while (text[length]) length++;
    return SaveFileData(fileName,text,length);
}
char *EncodeDataBase64(const unsigned char *data,int dataSize,int *outputSize) {
    static const char table[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    if(outputSize)*outputSize=0;if(dataSize<0||(!data&&dataSize>0))return NULL;
    if(dataSize>0x5ffffffd)return NULL;int length=((dataSize+2)/3)*4;
    char *encoded=MemAlloc((unsigned int)length+1);if(!encoded)return NULL;
    int input=0,output=0;while(input<dataSize){unsigned int a=data[input++],b=input<dataSize?data[input++]:0,c=input<dataSize?data[input++]:0,triple=(a<<16)|(b<<8)|c;encoded[output++]=table[(triple>>18)&63];encoded[output++]=table[(triple>>12)&63];encoded[output++]=table[(triple>>6)&63];encoded[output++]=table[triple&63];}
    int remainder=dataSize%3;if(remainder==1){encoded[length-2]='=';encoded[length-1]='=';}else if(remainder==2)encoded[length-1]='=';encoded[length]=0;if(outputSize)*outputSize=length;return encoded;
}
static int mr_base64_value(unsigned char c){if(c>='A'&&c<='Z')return c-'A';if(c>='a'&&c<='z')return c-'a'+26;if(c>='0'&&c<='9')return c-'0'+52;if(c=='+')return 62;if(c=='/')return 63;return-1;}
unsigned char *DecodeDataBase64(const unsigned char *data,int *outputSize) {
    if(outputSize)*outputSize=0;if(!data)return NULL;int symbols=0,padding=0;
    for(int i=0;data[i];i++){unsigned char c=data[i];if(c==' '||c=='\t'||c=='\r'||c=='\n')continue;if(c=='='){padding++;symbols++;}else{if(mr_base64_value(c)<0||padding)return NULL;symbols++;}}
    if(symbols%4||padding>2)return NULL;int length=(symbols/4)*3-padding;unsigned char *decoded=MemAlloc((unsigned int)(length>0?length:1));if(!decoded)return NULL;
    int values[4],count=0,output=0;for(int i=0;data[i];i++){unsigned char c=data[i];if(c==' '||c=='\t'||c=='\r'||c=='\n')continue;values[count++]=c=='='?0:mr_base64_value(c);if(count==4){unsigned int triple=((unsigned int)values[0]<<18)|((unsigned int)values[1]<<12)|((unsigned int)values[2]<<6)|(unsigned int)values[3];if(output<length)decoded[output++]=(unsigned char)(triple>>16);if(output<length)decoded[output++]=(unsigned char)(triple>>8);if(output<length)decoded[output++]=(unsigned char)triple;count=0;}}
    if(outputSize)*outputSize=length;return decoded;
}
unsigned int ComputeCRC32(unsigned char *data,int dataSize) {
    if(!data||dataSize<=0)return 0;unsigned int crc=0xffffffffu;
    for(int i=0;i<dataSize;i++){crc^=data[i];for(int bit=0;bit<8;bit++)crc=(crc>>1)^(0xedb88320u&((unsigned int)-(int)(crc&1u)));}
    return~crc;
}
static uint32_t mr_rotate_left32(uint32_t value,unsigned int shift){return(value<<shift)|(value>>(32-shift));}
unsigned int *ComputeMD5(unsigned char *data,int dataSize) {
    static unsigned int hash[4];
    static const uint32_t shifts[64]={7,12,17,22,7,12,17,22,7,12,17,22,7,12,17,22,5,9,14,20,5,9,14,20,5,9,14,20,5,9,14,20,4,11,16,23,4,11,16,23,4,11,16,23,4,11,16,23,6,10,15,21,6,10,15,21,6,10,15,21,6,10,15,21};
    static const uint32_t constants[64]={0xd76aa478,0xe8c7b756,0x242070db,0xc1bdceee,0xf57c0faf,0x4787c62a,0xa8304613,0xfd469501,0x698098d8,0x8b44f7af,0xffff5bb1,0x895cd7be,0x6b901122,0xfd987193,0xa679438e,0x49b40821,0xf61e2562,0xc040b340,0x265e5a51,0xe9b6c7aa,0xd62f105d,0x02441453,0xd8a1e681,0xe7d3fbc8,0x21e1cde6,0xc33707d6,0xf4d50d87,0x455a14ed,0xa9e3e905,0xfcefa3f8,0x676f02d9,0x8d2a4c8a,0xfffa3942,0x8771f681,0x6d9d6122,0xfde5380c,0xa4beea44,0x4bdecfa9,0xf6bb4b60,0xbebfbc70,0x289b7ec6,0xeaa127fa,0xd4ef3085,0x04881d05,0xd9d4d039,0xe6db99e5,0x1fa27cf8,0xc4ac5665,0xf4292244,0x432aff97,0xab9423a7,0xfc93a039,0x655b59c3,0x8f0ccc92,0xffeff47d,0x85845dd1,0x6fa87e4f,0xfe2ce6e0,0xa3014314,0x4e0811a1,0xf7537e82,0xbd3af235,0x2ad7d2bb,0xeb86d391};
    hash[0]=0x67452301u;hash[1]=0xefcdab89u;hash[2]=0x98badcfeu;hash[3]=0x10325476u;if(dataSize<0||(!data&&dataSize>0)){memset(hash,0,sizeof hash);return hash;}
    uint64_t bitLength=(uint64_t)(unsigned int)dataSize*8u;uint32_t blocks=(uint32_t)(dataSize/64)+((dataSize%64)<=55?1u:2u);
    for(uint32_t blockIndex=0;blockIndex<blocks;blockIndex++){unsigned char block[64]={0};uint64_t start=(uint64_t)blockIndex*64u;for(int i=0;i<64;i++){uint64_t position=start+(unsigned int)i;if(position<(uint64_t)dataSize)block[i]=data[(int)position];else if(position==(uint64_t)dataSize)block[i]=0x80;}if(blockIndex==blocks-1)for(int i=0;i<8;i++)block[56+i]=(unsigned char)(bitLength>>(i*8));uint32_t words[16];for(int i=0;i<16;i++)words[i]=(uint32_t)block[i*4]|((uint32_t)block[i*4+1]<<8)|((uint32_t)block[i*4+2]<<16)|((uint32_t)block[i*4+3]<<24);uint32_t a=hash[0],b=hash[1],c=hash[2],d=hash[3];for(int i=0;i<64;i++){uint32_t f,g;if(i<16){f=(b&c)|(~b&d);g=(uint32_t)i;}else if(i<32){f=(d&b)|(~d&c);g=(uint32_t)(5*i+1)%16;}else if(i<48){f=b^c^d;g=(uint32_t)(3*i+5)%16;}else{f=c^(b|~d);g=(uint32_t)(7*i)%16;}uint32_t next=d;d=c;c=b;b+=mr_rotate_left32(a+f+constants[i]+words[g],shifts[i]);a=next;}hash[0]+=a;hash[1]+=b;hash[2]+=c;hash[3]+=d;}
    return hash;
}
unsigned int *ComputeSHA1(unsigned char *data,int dataSize) {
    static unsigned int hash[5];hash[0]=0x67452301u;hash[1]=0xefcdab89u;hash[2]=0x98badcfeu;hash[3]=0x10325476u;hash[4]=0xc3d2e1f0u;if(dataSize<0||(!data&&dataSize>0)){memset(hash,0,sizeof hash);return hash;}
    uint64_t bitLength=(uint64_t)(unsigned int)dataSize*8u;uint32_t blocks=(uint32_t)(dataSize/64)+((dataSize%64)<=55?1u:2u);
    for(uint32_t blockIndex=0;blockIndex<blocks;blockIndex++){unsigned char block[64]={0};uint64_t start=(uint64_t)blockIndex*64u;for(int i=0;i<64;i++){uint64_t position=start+(unsigned int)i;if(position<(uint64_t)dataSize)block[i]=data[(int)position];else if(position==(uint64_t)dataSize)block[i]=0x80;}if(blockIndex==blocks-1)for(int i=0;i<8;i++)block[63-i]=(unsigned char)(bitLength>>(i*8));uint32_t words[80]={0};for(int i=0;i<16;i++)words[i]=((uint32_t)block[i*4]<<24)|((uint32_t)block[i*4+1]<<16)|((uint32_t)block[i*4+2]<<8)|block[i*4+3];for(int i=16;i<80;i++)words[i]=mr_rotate_left32(words[i-3]^words[i-8]^words[i-14]^words[i-16],1);uint32_t a=hash[0],b=hash[1],c=hash[2],d=hash[3],e=hash[4];for(int i=0;i<80;i++){uint32_t f,k;if(i<20){f=(b&c)|(~b&d);k=0x5a827999u;}else if(i<40){f=b^c^d;k=0x6ed9eba1u;}else if(i<60){f=(b&c)|(b&d)|(c&d);k=0x8f1bbcdcu;}else{f=b^c^d;k=0xca62c1d6u;}uint32_t next=mr_rotate_left32(a,5)+f+e+k+words[i];e=d;d=c;c=mr_rotate_left32(b,30);b=a;a=next;}hash[0]+=a;hash[1]+=b;hash[2]+=c;hash[3]+=d;hash[4]+=e;}
    return hash;
}
bool DirectoryExists(const char *path) {
#ifdef _WIN32
    DWORD attributes=path?GetFileAttributesA(path):INVALID_FILE_ATTRIBUTES;
    return attributes!=INVALID_FILE_ATTRIBUTES && (attributes&FILE_ATTRIBUTE_DIRECTORY)!=0;
#else
    (void)path; return false;
#endif
}
bool IsPathFile(const char *path) {
#ifdef _WIN32
    DWORD attributes=path?GetFileAttributesA(path):INVALID_FILE_ATTRIBUTES;
    return attributes!=INVALID_FILE_ATTRIBUTES && (attributes&FILE_ATTRIBUTE_DIRECTORY)==0;
#else
    return FileExists(path);
#endif
}
const char *GetFileExtension(const char *fileName) {
    if (!fileName) return ""; const char *extension="";
    for (const char *p=fileName;*p;p++) { if (*p=='.') extension=p; else if (*p=='/' || *p=='\\') extension=""; }
    return extension;
}
static int mr_path_lower(int c) { return c>='A'&&c<='Z'?c+('a'-'A'):c; }
bool IsFileExtension(const char *fileName,const char *extensions) {
    if (!fileName || !extensions) return false; const char *actual=GetFileExtension(fileName);
    for (const char *start=extensions;*start;) {
        while (*start==';' || *start==' ' || *start==',') start++;
        const char *end=start; while (*end && *end!=';' && *end!=',' && *end!=' ') end++;
        const char *a=actual,*b=start; while (b<end && *a && mr_path_lower(*a)==mr_path_lower(*b)) { a++; b++; }
        if (b==end && !*a) return true; start=end;
    }
    return false;
}
const char *GetFileName(const char *path) {
    if (!path) return ""; const char *name=path;
    for (const char *p=path;*p;p++) if (*p=='/' || *p=='\\') name=p+1;
    return name;
}
const char *GetFileNameWithoutExt(const char *path) {
    static char result[1024]; const char *name=GetFileName(path),*extension=GetFileExtension(name);
    unsigned int length=*extension?(unsigned int)(extension-name):TextLength(name); if (length>=sizeof result) length=sizeof result-1;
    memcpy(result,name,length); result[length]='\0'; return result;
}
const char *GetDirectoryPath(const char *path) {
    static char result[1024]; if (!path || !*path) return "."; const char *last=NULL;
    for (const char *p=path;*p;p++) if (*p=='/' || *p=='\\') last=p;
    if (!last) return "."; unsigned int length=(unsigned int)(last-path); if (length==0) length=1;
    else if (length==2 && path[1]==':') length=3;
    if (length>=sizeof result) length=sizeof result-1; memcpy(result,path,length); result[length]='\0'; return result;
}
const char *GetPrevDirectoryPath(const char *path) {
    static char result[1024]; if (!path || !*path) return "."; unsigned int length=TextLength(path);
    while (length>1 && (path[length-1]=='/' || path[length-1]=='\\') && !(length==3 && path[1]==':')) length--;
    while (length>0 && path[length-1]!='/' && path[length-1]!='\\') length--;
    while (length>1 && (path[length-1]=='/' || path[length-1]=='\\') && !(length==3 && path[1]==':')) length--;
    if (!length) return "."; if (length>=sizeof result) length=sizeof result-1;
    memcpy(result,path,length); result[length]='\0'; return result;
}
const char *GetWorkingDirectory(void) {
    static char result[1024];
#ifdef _WIN32
    DWORD length=GetCurrentDirectoryA((DWORD)sizeof result,result); if (length>0 && length<sizeof result) return result;
#endif
    result[0]='.'; result[1]='\0'; return result;
}
const char *GetApplicationDirectory(void) {
    static char result[1024];
#ifdef _WIN32
    DWORD length=GetModuleFileNameA(NULL,result,(DWORD)sizeof result); if (length>0 && length<sizeof result) {
        while (length>0 && result[length-1]!='/' && result[length-1]!='\\') length--;
        if (length>0) result[length]='\0'; return result;
    }
#endif
    result[0]='.'; result[1]='\0'; return result;
}
static unsigned int mr_random_state=0x12345678u;
void SetRandomSeed(unsigned int seed) { mr_random_state=seed ? seed : 0x12345678u; }
static unsigned int mr_random(void) {
    unsigned int x=mr_random_state; x^=x<<13; x^=x>>17; x^=x<<5; return mr_random_state=x;
}
int GetRandomValue(int min,int max) {
    if (min>max) { int swap=min; min=max; max=swap; }
    unsigned int range=(unsigned int)((long long)max-(long long)min)+1u;
    return range ? min+(int)(mr_random()%range) : (int)mr_random();
}
int *LoadRandomSequence(unsigned int count,int min,int max) {
    if (min>max) { int swap=min; min=max; max=swap; }
    unsigned int available=(unsigned int)((long long)max-(long long)min)+1u;
    if (!count || count>available || !available) return NULL;
    int *pool=MemAlloc(available*sizeof *pool); if (!pool) return NULL;
    for (unsigned int i=0;i<available;i++) pool[i]=min+(int)i;
    for (unsigned int i=0;i<count;i++) {
        unsigned int pick=i+mr_random()%(available-i);
        int swap=pool[i]; pool[i]=pool[pick]; pool[pick]=swap;
    }
    if (count<available) {
        int *result=MemAlloc(count*sizeof *result);
        if (result) memcpy(result,pool,count*sizeof *result);
        MemFree(pool); return result;
    }
    return pool;
}
void UnloadRandomSequence(int *sequence) { MemFree(sequence); }
bool IsKeyDown(int key) { return key>=0 && key<512 && mr.keys[key]; }
bool IsKeyPressed(int key) { return key>=0 && key<512 && mr.pressed[key]; }
bool IsKeyPressedRepeat(int key) { return key>=0 && key<512 && mr.repeated[key]; }
bool IsKeyReleased(int key) { return key>=0 && key<512 && mr.released[key]; }
bool IsKeyUp(int key) { return !IsKeyDown(key); }
int GetKeyPressed(void) {
    if (mr.keyQueueCount<=0) return 0;
    int key=mr.keyQueue[0];
    for (int i=1;i<mr.keyQueueCount;i++) mr.keyQueue[i-1]=mr.keyQueue[i];
    mr.keyQueueCount--; return key;
}
int GetCharPressed(void) {
    if (mr.charQueueCount<=0) return 0;
    int codepoint=mr.charQueue[0];
    for (int i=1;i<mr.charQueueCount;i++) mr.charQueue[i-1]=mr.charQueue[i];
    mr.charQueueCount--; return codepoint;
}
bool IsGamepadAvailable(int gamepad) { return gamepad>=0&&gamepad<MR_MAX_GAMEPADS&&mr.gamepads[gamepad].available; }
const char *GetGamepadName(int gamepad) { return IsGamepadAvailable(gamepad)?mr.gamepads[gamepad].name:NULL; }
bool IsGamepadButtonPressed(int gamepad,int button) { return IsGamepadAvailable(gamepad)&&button>0&&button<MR_GAMEPAD_BUTTONS&&mr.gamepads[gamepad].pressed[button]; }
bool IsGamepadButtonDown(int gamepad,int button) { return IsGamepadAvailable(gamepad)&&button>0&&button<MR_GAMEPAD_BUTTONS&&mr.gamepads[gamepad].buttons[button]; }
bool IsGamepadButtonReleased(int gamepad,int button) { return gamepad>=0&&gamepad<MR_MAX_GAMEPADS&&button>0&&button<MR_GAMEPAD_BUTTONS&&mr.gamepads[gamepad].released[button]; }
bool IsGamepadButtonUp(int gamepad,int button) { return !IsGamepadButtonDown(gamepad,button); }
int GetGamepadButtonPressed(void) { int button=mr.gamepadLastButton;mr.gamepadLastButton=GAMEPAD_BUTTON_UNKNOWN;return button; }
int GetGamepadAxisCount(int gamepad) { return IsGamepadAvailable(gamepad)?MR_GAMEPAD_AXES:0; }
float GetGamepadAxisMovement(int gamepad,int axis) { return IsGamepadAvailable(gamepad)&&axis>=0&&axis<MR_GAMEPAD_AXES?mr.gamepads[gamepad].axes[axis]:0.0f; }
int SetGamepadMappings(const char *mappings) { (void)mappings;return 0; }
void SetGamepadVibration(int gamepad,float leftMotor,float rightMotor,float duration) {
    if(gamepad<0||gamepad>=MR_MAX_GAMEPADS)return;leftMotor=mr_clamp01(leftMotor);rightMotor=mr_clamp01(rightMotor);if(duration<=0){duration=0;leftMotor=rightMotor=0;}
#ifdef _WIN32
    mr_xinput_init();if(!mr_xinput_set_state)return;XINPUT_VIBRATION vibration={(WORD)(leftMotor*65535.0f+0.5f),(WORD)(rightMotor*65535.0f+0.5f)};
    mr_xinput_set_state((DWORD)gamepad,&vibration);mr.gamepads[gamepad].vibrationEnd=(leftMotor>0||rightMotor>0)&&duration>0?mr_clock()+duration:0;
#else
    mr_web_gamepad_vibrate(gamepad,leftMotor,rightMotor,(int)(duration*1000.0f+0.5f));
#endif
}
bool IsMouseButtonDown(int b) { return b>=0 && b<7 && mr.buttons[b]; }
bool IsMouseButtonPressed(int b) { return b>=0 && b<7 && mr.clicked[b]; }
bool IsMouseButtonReleased(int b) { return b>=0 && b<7 && mr.buttonReleased[b]; }
bool IsMouseButtonUp(int b) { return !IsMouseButtonDown(b); }
int GetMouseX(void) { return (int)mr.mouse.x; }
int GetMouseY(void) { return (int)mr.mouse.y; }
Vector2 GetMousePosition(void) { return mr.mouse; }
Vector2 GetMouseDelta(void) { return mr.mouseDelta; }
Vector2 GetMouseWheelMoveV(void) { return mr.wheel; }
float GetMouseWheelMove(void) {
    float x=mr.wheel.x<0 ? -mr.wheel.x : mr.wheel.x;
    float y=mr.wheel.y<0 ? -mr.wheel.y : mr.wheel.y;
    return x>y ? mr.wheel.x : mr.wheel.y;
}
void SetMousePosition(int x,int y) {
#ifdef _WIN32
    if (mr.window) {
        float sx=mr.mouseScale.x!=0?mr.mouseScale.x:1,sy=mr.mouseScale.y!=0?mr.mouseScale.y:1;
        POINT point={(LONG)((x-mr.mouseOffset.x)/sx),(LONG)((y-mr.mouseOffset.y)/sy)};
        ClientToScreen(mr.window,&point); SetCursorPos(point.x,point.y);
    }
#endif
    mr.mouseDelta.x+=(float)x-mr.mouse.x; mr.mouseDelta.y+=(float)y-mr.mouse.y; mr.mouse=(Vector2){(float)x,(float)y};
}
void SetMouseOffset(int x,int y) { mr.mouseOffset=(Vector2){(float)x,(float)y}; }
void SetMouseScale(float x,float y) { mr.mouseScale=(Vector2){x,y}; }
void SetMouseCursor(int cursor) {
    if(cursor<MOUSE_CURSOR_DEFAULT||cursor>MOUSE_CURSOR_NOT_ALLOWED)return;mr.cursorShape=cursor;
#ifdef _WIN32
    mr_apply_cursor();
#else
    mr_web_window_command(8,cursor,0,NULL);
#endif
}
int GetTouchX(void) { return (int)GetTouchPosition(0).x; }
int GetTouchY(void) { return (int)GetTouchPosition(0).y; }
Vector2 GetTouchPosition(int index) { return index>=0&&index<mr.touchCount?mr.touches[index].position:(Vector2){-1,-1}; }
int GetTouchPointId(int index) { return index>=0&&index<mr.touchCount?mr.touches[index].id:-1; }
int GetTouchPointCount(void) { return mr.touchCount; }
void SetGesturesEnabled(unsigned int flags) { mr.gesturesEnabled=flags; }
bool IsGestureDetected(unsigned int gesture) { return gesture!=GESTURE_NONE&&(mr.gestureDetected&mr.gesturesEnabled&gesture)==gesture; }
int GetGestureDetected(void) { return (int)(mr.gestureDetected&mr.gesturesEnabled); }
float GetGestureHoldDuration(void) { return mr.touchCount>0&&mr.gestureDetected==GESTURE_HOLD?(float)(mr_clock()-mr.gestureHoldStart):0.0f; }
Vector2 GetGestureDragVector(void) { return mr.gestureDrag; }
float GetGestureDragAngle(void) { return mr.gestureDragAngle; }
Vector2 GetGesturePinchVector(void) { return mr.gesturePinch; }
float GetGesturePinchAngle(void) { return mr.gesturePinchAngle; }
void ShowCursor(void) { mr.cursorHidden=false;
#ifdef _WIN32
    mr_apply_cursor();
#else
    mr_web_window_command(9,0,0,NULL);
#endif
}
void HideCursor(void) { mr.cursorHidden=true;
#ifdef _WIN32
    mr_apply_cursor();
#else
    mr_web_window_command(9,1,0,NULL);
#endif
}
bool IsCursorHidden(void) { return mr.cursorHidden||mr.cursorDisabled; }
void EnableCursor(void) { mr.cursorDisabled=false;
#ifdef _WIN32
    ClipCursor(NULL);if(!mr.buttons[0]&&!mr.buttons[1]&&!mr.buttons[2])ReleaseCapture();mr_apply_cursor();
#else
    mr_web_window_command(10,0,0,NULL);
#endif
}
void DisableCursor(void) { mr.cursorDisabled=true;
#ifdef _WIN32
    if(mr.window){SetCapture(mr.window);POINT center={mr.width/2,mr.height/2};mr.mouse=(Vector2){(float)center.x,(float)center.y};ClientToScreen(mr.window,&center);SetCursorPos(center.x,center.y);}mr_apply_cursor_clip();mr_apply_cursor();
#else
    mr_web_window_command(10,1,0,NULL);
#endif
}
bool IsCursorOnScreen(void) {
#ifdef _WIN32
    return mr.cursorOnScreen;
#else
    return mr_web_window_query(11,0)!=0;
#endif
}
void BeginDrawing(void) { mr_dispatch_readbacks();mr.vertexCount=mr.batchCount=mr.drawCount3d=mr.instanceCount3d=mr.boneMatrixCount3d=0;mr.skyboxTexture=0; mr.overflow=false; mr.renderTarget=0; mr.targetWidth=mr.width; mr.targetHeight=mr.height; mr.drawing=mr.ready; }
void ClearBackground(Color color) { mr.clear=color; mr.vertexCount=mr.batchCount=mr.drawCount3d=mr.instanceCount3d=0; }
void BeginBlendMode(int mode) { mr.blendMode=(mode>=0 && mode<6)?mode:BLEND_ALPHA; }
void EndBlendMode(void) { mr.blendMode=BLEND_ALPHA; }
void BeginScissorMode(int x,int y,int width,int height) {
    if(x<0){width+=x;x=0;}if(y<0){height+=y;y=0;}
    mr.scissorActive=width>0 && height>0; mr.scissor=(Rectangle){(float)x,(float)y,(float)width,(float)height};
}
void EndScissorMode(void) { mr.scissorActive=false; }
Vector2 GetWorldToScreen2D(Vector2 position,Camera2D camera) {
    float angle=camera.rotation*MR_DEG2RAD,c=cosf(angle),s=sinf(angle);
    float x=position.x-camera.target.x,y=position.y-camera.target.y;
    return (Vector2){camera.offset.x+(x*c-y*s)*camera.zoom,camera.offset.y+(x*s+y*c)*camera.zoom};
}
Vector2 GetScreenToWorld2D(Vector2 position,Camera2D camera) {
    if (camera.zoom==0) return camera.target;
    float angle=-camera.rotation*MR_DEG2RAD,c=cosf(angle),s=sinf(angle);
    float x=(position.x-camera.offset.x)/camera.zoom,y=(position.y-camera.offset.y)/camera.zoom;
    return (Vector2){camera.target.x+x*c-y*s,camera.target.y+x*s+y*c};
}
Matrix GetCameraMatrix2D(Camera2D camera) {
    float angle=camera.rotation*MR_DEG2RAD,c=cosf(angle)*camera.zoom,s=sinf(angle)*camera.zoom;
    Matrix result={0}; result.m0=c; result.m4=-s; result.m1=s; result.m5=c; result.m10=1; result.m15=1;
    result.m12=c*(-camera.target.x)-s*(-camera.target.y)+camera.offset.x;
    result.m13=s*(-camera.target.x)+c*(-camera.target.y)+camera.offset.y; return result;
}
void BeginMode2D(Camera2D camera) { mr.camera2d=camera; mr.camera2dActive=true; }
void EndMode2D(void) { mr.camera2dActive=false; }
