
Quake2 release 4

Some options/fixes added to original Hyperion sources.

fixes beta3 release 2:
gl_shadows - fix from old versions (fog now should work with it):
Part of particles/effects dissapeared and shield was wrongly rendered (seen on thirdperson)
when using (default) vertex arrays.

fixes r4:

Workaround for "big gun" level: Didn´t get saved so game didn´t continue further.

Effects:
    
gl_fog
gl_fogred 
gl_foggreen 
gl_fogblue 
gl_fogstart 
gl_fogend 

Debug gl options:
gl_showtris - 1 or 2 values
gl_showbbox - 1 or 2 values 

//
Thirdperson playing:
Needed "players" dir on baseq2 - some updated mod (rogue/xatris) files fix some issues:
If not, a colored polygon appears instead of model.

cl_3dcam
cl_3dcam_alpha - translucency of model when camera gets on back
cl_3dcam_dist  - default 50
cl_3dcam_angle - default 30

Bug: At beginning of level, weapon gets to default model instead of showing the proper one:
hand 1 / hand 0 fixes it (not for some mods - maybe an issue reading weapon textures from dirs)
//

cl_drawfps
cl_item_bob - bobbing items

Widescreen modes: horplus option needs to be enabled for correct gun fov (extra calculations - it gets saved !)

Notes:

Voodoo gfx users:  gl_guardband option needed for correct performance (not for Permedia !!).

Notes: ref_gl.dll Warpos still in unknown state regarding of freeing textures from time to time.
ref_glnolru.dll does it when changing level (faster on slower systems). Should be no problem with 68k versions.


