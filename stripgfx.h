#ifndef INCLUDED_stripgfx_h
#define INCLUDED_stripgfx_h

#define NO_GRAPHICS 1
#define DEC_GRAPHICS 2
#define IBM_GRAPHICS 3

void populate_gfx_array (int gfxset);
unsigned char strip_gfx (unsigned char inchar);

#endif /* !INCLUDED_stripgfx_h */
