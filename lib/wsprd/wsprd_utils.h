#ifndef WSPRD_UTILS_H
#define WSPRD_UTILS_H

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include "nhash.h"

void unpack50( signed char *dat, int32_t *n1, int32_t *n2 );

int unpackcall( int32_t ncall, char *call );

int unpackgrid( int32_t ngrid, char *grid);

int unpackpfx( int32_t nprefix, char *call);

void deinterleave(unsigned char *sym);

// used by qsort
int doublecomp(const void* elem1, const void* elem2);
int floatcomp(const void* elem1, const void* elem2);

// WSJT-CB (11 m) CB callsign packing (mirrors packcb/unpackcb in
// lib/packjt.f90 and the C encoder in lib/wsprd/wsprsim_utils.c).
void cb_offset(int tnp, int tnl, int tns, int64_t *offset, int64_t *base);
int  packcb(const char *callsign, int32_t *ncb);
void unpackcb(int32_t ncb, char *callsign);

int unpk_( signed char *message, char* hashtab, char* loctab, char *call_loc_pow, char *callsign);

#endif
