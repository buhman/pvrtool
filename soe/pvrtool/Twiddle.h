#ifndef _TWIDDLE_H
#define _TWIDDLE_H

extern void BuildTwiddleTable();

extern void ComputeMaskShift( unsigned long int w, unsigned long int h, unsigned long int& mask, unsigned long int& shift);
extern unsigned long int CalcUntwiddledPos( unsigned long int x, unsigned long int y, unsigned long int mask, unsigned long int shift );




#endif //_TWIDDLE_H
