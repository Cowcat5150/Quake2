extern qboolean mlooking;
extern unsigned sys_msg_time;
extern qboolean mouseactive;    // false when not focus app
extern qboolean mouseinitialized;

extern cvar_t *in_mouse;

void IN_MouseMove (usercmd_t *cmd);
void IN_InitMouse(void);
void IN_ShutdownMouse(void);
void IN_StartupMouse (void);

// end 
