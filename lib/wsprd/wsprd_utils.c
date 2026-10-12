/*
 This file is part of program wsprd, a detector/demodulator/decoder
 for the Weak Signal Propagation Reporter (WSPR) mode.
 
 File name: wsprd_utils.c
 
 Copyright 2001-2015, Joe Taylor, K1JT
 
 Most of the code is based on work by Steven Franke, K9AN, which
 in turn was based on earlier work by K1JT.
 
 Copyright 2014-2015, Steven Franke, K9AN
 
 License: GNU GPL v3
 
 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.
 
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.
 
 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "wsprd_utils.h"

#ifndef int32_t
#define int32_t int
#endif

void unpack50( signed char *dat, int32_t *n1, int32_t *n2 )
{
    int32_t i,i4;
    
    i=dat[0];
    i4=i&255;
    *n1=i4<<20;
    
    i=dat[1];
    i4=i&255;
    *n1=*n1+(i4<<12);
    
    i=dat[2];
    i4=i&255;
    *n1=*n1+(i4<<4);
    
    i=dat[3];
    i4=i&255;
    *n1=*n1+((i4>>4)&15);
    *n2=(i4&15)<<18;
    
    i=dat[4];
    i4=i&255;
    *n2=*n2+(i4<<10);
    
    i=dat[5];
    i4=i&255;
    *n2=*n2+(i4<<2);
    
    i=dat[6];
    i4=i&255;
    *n2=*n2+((i4>>6)&3);
}

int unpackcall( int32_t ncall, char *call )
{
    char c[]={'0','1','2','3','4','5','6','7','8','9','A','B','C','D','E',
        'F','G','H','I','J','K','L','M','N','O','P','Q','R','S','T',
        'U','V','W','X','Y','Z',' '};
    int32_t n;
    int i;
    char tmp[7];

    n=ncall;
    strcpy(call,"......");
    if (n < 262177560 ) {
        i=n%27+10;
        tmp[5]=c[i];
        n=n/27;
        i=n%27+10;
        tmp[4]=c[i];
        n=n/27;
        i=n%27+10;
        tmp[3]=c[i];
        n=n/27;
        i=n%10;
        tmp[2]=c[i];
        n=n/10;
        i=n%36;
        tmp[1]=c[i];
        n=n/36;
        i=n;
        tmp[0]=c[i];
        tmp[6]='\0';
        // remove leading whitespace
        for(i=0; i<5; i++) {
            if( tmp[i] != c[36] )
                break;
        }
        sprintf(call,"%-6s",&tmp[i]);
        // remove trailing whitespace
        for(i=0; i<6; i++) {
            if( call[i] == c[36] ) {
                call[i]='\0';
            }
        }
    } else {
        return 0;
    }
    return 1;
}

int unpackgrid( int32_t ngrid, char *grid)
{
    char c[]={'0','1','2','3','4','5','6','7','8','9','A','B','C','D','E',
        'F','G','H','I','J','K','L','M','N','O','P','Q','R','S','T',
        'U','V','W','X','Y','Z',' '};
    int dlat, dlong;
    
    ngrid=ngrid>>7;
    if( ngrid < 32400 ) {
        dlat=(ngrid%180)-90;
        dlong=(ngrid/180)*2 - 180 + 2;
        if( dlong < -180 )
            dlong=dlong+360;
        if( dlong > 180 )
            dlong=dlong+360;
        int nlong = 60.0*(180.0-dlong)/5.0;
        int n1 = nlong/240;
        int n2 = (nlong - 240*n1)/24;
        grid[0] = c[10+n1];
        grid[2]=  c[n2];
        
        int nlat = 60.0*(dlat+90)/2.5;
        n1 = nlat/240;
        n2 = (nlat-240*n1)/24;
        grid[1]=c[10+n1];
        grid[3]=c[n2];
    } else {
        strcpy(grid,"XXXX");
        return 0;
    }
    return 1;
}

int unpackpfx( int32_t nprefix, char *call)
{
    char nc, pfx[4]={'\0'}, tmpcall[7];
    int i;
    int32_t n;
    
    strcpy(tmpcall,call);
    if( nprefix < 60000 ) {
        // add a prefix of 1 to 3 characters
        n=nprefix;
        for (i=2; i>=0; i--) {
            nc=n%37;
            if( (nc >= 0) & (nc <= 9) ) {
                pfx[i]=nc+48;
            }
            else if( (nc >= 10) & (nc <= 35) ) {
                pfx[i]=nc+55;
            }
            else {
                pfx[i]=' ';
            }
            n=n/37;
        }

        char * p = strrchr(pfx,' ');
        strcpy(call, p ? p + 1 : pfx);
        strncat(call,"/",1);
        strncat(call,tmpcall,strlen(tmpcall));
        
    } else {
        // add a suffix of 1 or 2 characters
        nc=nprefix-60000;
        if( (nc >= 0) & (nc <= 9) ) {
            pfx[0]=nc+48;
            strcpy(call,tmpcall);
            strncat(call,"/",1);
            strncat(call,pfx,1);
        }
        else if( (nc >= 10) & (nc <= 35) ) {
            pfx[0]=nc+55;
            strcpy(call,tmpcall);
            strncat(call,"/",1);
            strncat(call,pfx,1);
        }
        else if( (nc >= 36) & (nc <= 125) ) {
            pfx[0]=(nc-26)/10+48;
            pfx[1]=(nc-26)%10+48;
            strcpy(call,tmpcall);
            strncat(call,"/",1);
            strncat(call,pfx,2);
        }
        else {
            return 0;
        }
    }
    return 1;
}

void deinterleave(unsigned char *sym)
{
    unsigned char tmp[162];
    unsigned char p, i, j;
    
    p=0;
    i=0;
    while (p<162) {
        j=((i * 0x80200802ULL) & 0x0884422110ULL) * 0x0101010101ULL >> 32;
        if (j < 162 ) {
            tmp[p]=sym[j];
            p=p+1;
        }
        i=i+1;
    }
    for (i=0; i<162; i++) {
        sym[i]=tmp[i];
    }
}

// used by qsort
int doublecomp(const void* elem1, const void* elem2)
{
    if(*(const double*)elem1 < *(const double*)elem2)
        return -1;
    return *(const double*)elem1 > *(const double*)elem2;
}

int floatcomp(const void* elem1, const void* elem2)
{
    if(*(const float*)elem1 < *(const float*)elem2)
        return -1;
    return *(const float*)elem1 > *(const float*)elem2;
}

// ---------------------------------------------------------------------------
// WSJT-CB (11 m) CB callsign source coding.
// CB callsign = <prefix 1-3 digits><letters 1-2><suffix 0-4 digits>
//   prefix 1 digit    -> suffix 0-4 digits
//   prefix 2-3 digits -> suffix 0-3 digits
// Bijective mixed-radix enumeration into a 30-bit integer. The shape
// ordering here MUST match packcb/unpackcb in lib/packjt.f90 exactly.
// ---------------------------------------------------------------------------
static const int64_t CB_P10[5] = {1,10,100,1000,10000};
static const int64_t CB_P26[3] = {1,26,676};

void cb_offset(int tnp, int tnl, int tns, int64_t *offset, int64_t *base)
{
    int np,nl,ns,nsmax;
    int64_t sz,p26,p10p,p10s;
    *offset=0; *base=-1;
    for(np=1; np<=3; np++) {
        nsmax = (np==1) ? 4 : 3;
        p10p = CB_P10[np];
        for(nl=1; nl<=2; nl++) {
            p26 = CB_P26[nl];
            for(ns=0; ns<=nsmax; ns++) {
                p10s = CB_P10[ns];
                sz = p10p*p26*p10s;
                if(np==tnp && nl==tnl && ns==tns) { *base = p26*p10s; return; }
                *offset += sz;
            }
        }
    }
}

int packcb(const char *call_in, int32_t *ncb)
{
    char s[16];
    int i,k,np,nl,ns,ip,il,is;
    int64_t offset,base;
    const char *p = call_in;
    while(*p==' ') p++;
    k=0;
    while(p[k] && k<15) {
        char ch=p[k];
        if(ch>='a' && ch<='z') ch-=32;
        s[k]=ch; k++;
    }
    s[k]='\0';
    while(k>0 && s[k-1]==' ') { k--; s[k]='\0'; }
    if(k<2 || k>8) return 0;
    i=0; np=0;
    while(i<k && s[i]>='0' && s[i]<='9') { np++; i++; }
    nl=0;
    while(i<k && s[i]>='A' && s[i]<='Z') { nl++; i++; }
    ns=0;
    while(i<k && s[i]>='0' && s[i]<='9') { ns++; i++; }
    if(i<k) return 0;
    if(np<1 || np>3) return 0;
    if(nl<1 || nl>2) return 0;
    if(np==1) { if(ns>4) return 0; } else { if(ns>3) return 0; }
    ip=0; for(i=0; i<np; i++) ip=10*ip+(s[i]-'0');
    il=0; for(i=np; i<np+nl; i++) il=26*il+(s[i]-'A');
    is=0; for(i=np+nl; i<np+nl+ns; i++) is=10*is+(s[i]-'0');
    cb_offset(np,nl,ns,&offset,&base);
    if(base<0) return 0;
    *ncb = (int32_t)(offset + (int64_t)ip*base + (int64_t)il*CB_P10[ns] + is);
    return 1;
}

void unpackcb(int32_t ncb, char *callsign)
{
    int np,nl,ns,nsmax,ip,il,is,j,d,pos;
    int64_t idx,offset,sz,p26,p10p,p10s,base,r;
    idx=ncb; offset=0; pos=0;
    callsign[0]='\0';
    for(np=1; np<=3; np++) {
        nsmax = (np==1) ? 4 : 3;
        p10p = CB_P10[np];
        for(nl=1; nl<=2; nl++) {
            p26 = CB_P26[nl];
            for(ns=0; ns<=nsmax; ns++) {
                p10s = CB_P10[ns];
                sz = p10p*p26*p10s;
                if(idx < offset+sz) {
                    r = idx-offset; base = p26*p10s;
                    ip = (int)(r/base); r %= base;
                    il = (int)(r/p10s); is = (int)(r%p10s);
                    for(j=np; j>=1; j--) { d=(int)((ip/CB_P10[j-1])%10); callsign[pos++]=(char)('0'+d); }
                    for(j=nl; j>=1; j--) { d=(int)((il/CB_P26[j-1])%26); callsign[pos++]=(char)('A'+d); }
                    for(j=ns; j>=1; j--) { d=(int)((is/CB_P10[j-1])%10); callsign[pos++]=(char)('0'+d); }
                    callsign[pos]='\0';
                    return;
                }
                offset += sz;
            }
        }
    }
}

int unpk_(signed char *message, char *hashtab, char *loctab, char *call_loc_pow, char *callsign)
{
    int n1,n2,ndbm,noprint=0;
    char grid[5],cdbm[4];
    
    unpack50(message,&n1,&n2);

    /* WSJT-CB exclusive mode: the 50 source bits carry a CB (11 m) callsign
       (30 bits) + 15-bit Maidenhead grid + 5-bit power index. The standard
       WSPR type-1/2/3 message formats are not used. */
    (void)hashtab; (void)loctab;
    {
        int64_t n50 = (((int64_t)(n1 & 0x0FFFFFFF)) << 22) | ((int64_t)(n2 & 0x3FFFFF));
        int idbm5    = (int)(n50 & 0x1F);
        int32_t ng15 = (int32_t)((n50 >> 5) & 0x7FFF);
        int32_t ncb  = (int32_t)((n50 >> 20) & 0x3FFFFFFF);
        int dec, rem;

        if( ncb >= 935913420 || idbm5 > 18 ) noprint=1;

        unpackcb(ncb, callsign);
        if( !unpackgrid(ng15<<7, grid) ) return 1;
        grid[4]=0;

        dec = idbm5/3; rem = idbm5%3;
        ndbm = dec*10 + (rem==0 ? 0 : (rem==1 ? 3 : 7));

        memset(call_loc_pow,0,sizeof(char)*23);
        sprintf(cdbm,"%2d",ndbm);
        strncat(call_loc_pow,callsign,strlen(callsign));
        strncat(call_loc_pow," ",1);
        strncat(call_loc_pow,grid,4);
        strncat(call_loc_pow," ",1);
        strncat(call_loc_pow,cdbm,2);
        strncat(call_loc_pow,"\0",1);
    }
    return noprint;
}
