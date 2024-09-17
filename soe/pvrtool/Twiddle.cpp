/*************************************************
 Twiddling routines
 
   These functions calculate and return the
   linear offset into a twiddled buffer for
   the given non-twiddled x & y coordinates
  
    Don't forget to call BuildTwiddleTable!

**************************************************/

#include <stdlib.h>

#define TWIDDLE_TABLE_SIZE 1024
unsigned long int g_nTwiddleTable[TWIDDLE_TABLE_SIZE];


//////////////////////////////////////////////////////////////////////
// Bit merging macro used by BuildTwiddleTable
//////////////////////////////////////////////////////////////////////
#define MergeBits(a,b,shft,msk) tmp = ((b>>shft)^a)&msk; a ^= tmp; b ^= tmp << shft;


//////////////////////////////////////////////////////////////////////
// Calculates the twiddled value for the given index
//////////////////////////////////////////////////////////////////////
inline unsigned long int GetTwiddleValue( unsigned long int i )
{
    unsigned long int zero = 0, index = i,tmp;
	MergeBits( zero, index, 1, 0x55555555);
	MergeBits( zero, index, 2, 0x33333333);
	MergeBits( zero, index, 4, 0x0F0F0F0F);
	MergeBits( zero, index, 8, 0x00FF00FF);

	return ( zero<<16 ) | ( index&0xFFFF );
}


//////////////////////////////////////////////////////////////////////
// Generates the twiddle lookup table
//////////////////////////////////////////////////////////////////////
void BuildTwiddleTable()
{
    for( unsigned long int i = 0; i < TWIDDLE_TABLE_SIZE; i++ ) g_nTwiddleTable[i] = GetTwiddleValue( i );
}

//////////////////////////////////////////////////////////////////////
// Calculates the mask and shift for passing to CalcUntwiddledPos
//////////////////////////////////////////////////////////////////////
void ComputeMaskShift( unsigned long int w, unsigned long int h, unsigned long int& mask, unsigned long int& shift)
{
	mask = __min(w,h)-1;
    unsigned long int bit;
	for( bit = 1, shift = 0; mask & bit;  bit <<= 1, shift++ );
}


//////////////////////////////////////////////////////////////////////
// Converts the given x and y values into a linear index into the
// twiddled data. Use ComputeMaskShift to calculate the correct
// mask and shift value for a particular width & height
//////////////////////////////////////////////////////////////////////
unsigned long int CalcUntwiddledPos( unsigned long int x, unsigned long int y, unsigned long int mask, unsigned long int shift )
{
    if( x > 1024 || y > 1024 )
    {
        //special case - larger-than-normal address lookup - compute it on the fly
        return GetTwiddleValue( y & mask )  |  GetTwiddleValue( x & mask ) << 1  |  (( (y|x) & ~mask ) << shift );
    }
    else
        return g_nTwiddleTable[ y & mask ]  |  g_nTwiddleTable[ x & mask ] << 1  |  (( (y|x) & ~mask ) << shift );
}



