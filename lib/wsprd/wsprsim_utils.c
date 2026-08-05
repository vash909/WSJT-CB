/*
 Functions used by wsprsim
 */
#include "wsprsim_utils.h"
#include "wsprd_utils.h"
#include "nhash.h"
#include "fano.h"

static char get_locator_character_code(char ch);
static char get_callsign_character_code(char ch);
static long unsigned int pack_grid4_power(char const *grid4, int power);
static long unsigned int pack_call(char const *callsign);
static void pack_prefix(char *callsign, int32_t *n, int32_t *m, int32_t *nadd );
static void interleave(unsigned char *sym);

char get_locator_character_code(char ch) {
    if( ch >=48 && ch <=57 ) { //0-9
        return ch-48;
    }
    if( ch == 32 ) {  //space
        return 36;
    }
    if( ch >= 65 && ch <= 82 ) { //A-Z
        return ch-65;
    }
    return -1;
}

char get_callsign_character_code(char ch) {
    if( ch >=48 && ch <=57 ) { //0-9
        return ch-48;
    }
    if( ch == 32 ) {  //space
        return 36;
    }
    if( ch >= 65 && ch <= 90 ) { //A-Z
        return ch-55;
    }
    return -1;
}

long unsigned int pack_grid4_power(char const *grid4, int power) {
    long unsigned int m;
    
    m=(179-10*grid4[0]-grid4[2])*180+10*grid4[1]+grid4[3];
    m=m*128+power+64;
    return m;
}

long unsigned int pack_call(char const *callsign) {
    unsigned int i;
    long unsigned int n;
    char call6[6];
    memset(call6,' ',sizeof(call6));
    // callsign is 6 characters in length. Exactly.
    size_t call_len = strlen(callsign);
    if( call_len > 6 ) {
        return 0;
    }
    if( isdigit(callsign[2]) ) {
        for (i=0; i<call_len; i++) {
            call6[i]=callsign[i];
        }
    } else if( isdigit(callsign[1]) ) {
        for (i=1; i<call_len+1; i++) {
            call6[i]=callsign[i-1];
        }
    }
    for (i=0; i<6; i++) {
        call6[i]=get_callsign_character_code(call6[i]);
    }
    n = call6[0];
    n = n*36+call6[1];
    n = n*10+call6[2];
    n = n*27+call6[3]-10;
    n = n*27+call6[4]-10;
    n = n*27+call6[5]-10;
    return n;
}

void pack_prefix(char *callsign, int32_t *n, int32_t *m, int32_t *nadd ) {
    size_t i;
    char * call6 = calloc(7,sizeof (char));
    size_t i1=strcspn(callsign,"/");
    
    if( callsign[i1+2] == 0 ) { 
        //single char suffix
        for (i=0; i<i1; i++) {
            call6[i]=callsign[i];
        }
        call6[i] = '\0';
        *n=pack_call(call6);
        *nadd=1;
        int nc = callsign[i1+1];
        if( nc >= 48 && nc <= 57 ) {
            *m=nc-48;
        } else if ( nc >= 65 && nc <= 90 ) {
            *m=nc-65+10;
        } else {
            *m=38;
        }
        *m=60000-32768+*m;
    } else if( callsign[i1+3]==0 ) {
        //two char suffix
        for (i=0; i<i1; i++) {
            call6[i]=callsign[i];
        }
        *n=pack_call(call6);
        *nadd=1;
        *m=10*(callsign[i1+1]-48)+(callsign[i1+2]-48);
        *m=60000 + 26 + *m;
    } else {
        char const * pfx = strtok (callsign,"/");
        char const * call = strtok(NULL," ");
        *n = pack_call (call);
        size_t plen=strlen (pfx);
        if( plen ==1 ) {
            *m=36;
            *m=37*(*m)+36;
        } else if( plen == 2 ) {
            *m=36;
        } else {
            *m=0;
        }
        for (i=0; i<plen; i++) {
            int nc = callsign[i];
            if( nc >= 48 && nc <= 57 ) {
                nc=nc-48;
            } else if ( nc >= 65 && nc <= 90 ) {
                nc=nc-65+10;
            } else {
                nc=36;
            }
            *m=37*(*m)+nc;
        }
        *nadd=0;
        if( *m > 32768 ) {
            *m=*m-32768;
            *nadd=1;
        }
    }
    free (call6);
}

void interleave(unsigned char *sym)
{
    unsigned char tmp[162];
    unsigned char p, i, j;
    
    p=0;
    i=0;
    while (p<162) {
        j=((i * 0x80200802ULL) & 0x0884422110ULL) * 0x0101010101ULL >> 32;
        if (j < 162 ) {
            tmp[j]=sym[p];
            p=p+1;
        }
        i=i+1;
    }
    for (i=0; i<162; i++) {
        sym[i]=tmp[i];
    }
}

int get_wspr_channel_symbols(char* rawmessage, char* hashtab, char* loctab, unsigned char* symbols) {
    int m=0, ntype=0;
    long unsigned int n=0;
    int i, j, ihash;
    unsigned char pr3[162]=
    {1,1,0,0,0,0,0,0,1,0,0,0,1,1,1,0,0,0,1,0,
        0,1,0,1,1,1,1,0,0,0,0,0,0,0,1,0,0,1,0,1,
        0,0,0,0,0,0,1,0,1,1,0,0,1,1,0,1,0,0,0,1,
        1,0,1,0,0,0,0,1,1,0,1,0,1,0,1,0,1,0,0,1,
        0,0,1,0,1,1,0,0,0,1,1,0,1,0,1,0,0,0,1,0,
        0,0,0,0,1,0,0,1,0,0,1,1,1,0,1,1,0,0,1,1,
        0,1,0,0,0,1,1,1,0,0,0,0,0,1,0,1,0,0,1,1,
        0,0,0,0,0,0,0,1,1,0,1,0,1,1,0,0,0,1,1,0,
        0,0};
    int nu[10]={0,-1,1,0,-1,2,1,0,-1,1};
    char *callsign, *grid, *powstr;
    char grid4[5], message[23];
    
    memset(message,0,sizeof(char)*23);
    i=0;
    while ( rawmessage[i] != 0 && i<23 ) {
        message[i]=rawmessage[i];
        i++;
    }
    
    // WSJT-CB exclusive mode: message is "CALL GRID4 dBm".
    // Encode the CB callsign (30 bits) + 15-bit grid + 5-bit power index
    // into the 50 source bits (split n=top 28, m=bottom 22).
    {
        char *powstr2;
        int power, idbm;
        int32_t ncb;
        int64_t ng15, n50;
        char gc[4];
        callsign = strtok(message," ");
        grid     = strtok(NULL," ");
        powstr2  = strtok(NULL," ");
        if( callsign==NULL || grid==NULL || powstr2==NULL ) return 0;
        if( !packcb(callsign,&ncb) ) return 0;
        power = atoi(powstr2);
        if( power < 0 ) power=0;
        if( power > 60 ) power=60;
        power = power + nu[power%10];
        idbm = (power/10)*3;
        if( power%10==3 ) idbm+=1;
        if( power%10==7 ) idbm+=2;
        for(i=0; i<4; i++) gc[i]=get_locator_character_code(grid[i]);
        ng15 = (int64_t)(179 - 10*gc[0] - gc[2])*180 + 10*gc[1] + gc[3];
        n50 = ((int64_t)ncb<<20) | ((ng15 & 0x7FFF)<<5) | ((int64_t)(idbm & 0x1F));
        n = (long unsigned int)((n50>>22) & 0x0FFFFFFF);
        m = (long unsigned int)(n50 & 0x3FFFFF);
    }

    // pack 50 bits + 31 (0) tail bits into 11 bytes
    unsigned char it, data[11];
    memset(data,0,sizeof(char)*11);
    it=0xFF & (n>>20);
    data[0]=it;
    it=0xFF & (n>>12);
    data[1]=it;
    it=0xFF & (n>>4);
    data[2]=it;
    it= ((n&(0x0F))<<4) + ((m>>18)&(0x0F));
    data[3]=it;
    it=0xFF & (m>>10);
    data[4]=it;
    it=0xFF & (m>>2);
    data[5]=it;
    it=(m & 0x03)<<6 ;
    data[6]=it;
    data[7]=0;
    data[8]=0;
    data[9]=0;
    data[10]=0;
    
    if( printdata ) {
        printf("Data is :");
        for (i=0; i<11; i++) {
            printf("%02X ",data[i]);
        }
        printf("\n");
    }
    
    // make sure that the 11-byte data vector is unpackable
    // unpack it with the routine that the decoder will use and display
    // the result. let the operator decide whether it worked.
    
    char *check_call_loc_pow, *check_callsign;
    check_call_loc_pow=malloc(sizeof(char)*23);
    check_callsign=malloc(sizeof(char)*13);
    signed char check_data[11];
    memcpy(check_data,data,sizeof(char)*11);

    unpk_(check_data,hashtab,loctab,check_call_loc_pow,check_callsign);
//    printf("Will decode as: %s\n",check_call_loc_pow);
 
    unsigned int nbytes=11; // The message with tail is packed into almost 11 bytes.
    unsigned char channelbits[nbytes*8*2]; /* 162 rounded up */
    memset(channelbits,0,sizeof(char)*nbytes*8*2);
    
    encode(channelbits,data,nbytes);
    
    interleave(channelbits);

    for (i=0; i<162; i++) {
        symbols[i]=2*channelbits[i]+pr3[i];
    }
    free(check_call_loc_pow);
    free(check_callsign); 
    return 1;
}
