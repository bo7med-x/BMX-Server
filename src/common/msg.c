/*
	This file is part of BMX Server.

    BMX Server is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    BMX Server is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with BMX Server.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "shared.h"

MSG_BeginReading_t MSG_BeginReading = (MSG_BeginReading_t)ADDR(0x807F174,0x807F96B);
MSG_ReadLong_t MSG_ReadLong = (MSG_ReadLong_t)ADDR(0x807F2F0,0x80800E8);
MSG_ReadStringLine_t MSG_ReadStringLine = (MSG_ReadStringLine_t)ADDR(0x807F3FC,0x8080212);
MSG_WriteBitsCompress_t MSG_WriteBitsCompress = (MSG_WriteBitsCompress_t)ADDR(0x0807f03c,0x807FC1B);
MSG_WriteData_t MSG_WriteData = (MSG_WriteData_t)ADDR(0x0807eef0,0x807FD10);

MSG_ReadString_t MSG_ReadString = (MSG_ReadString_t)ADDR(0x807F320,0x808013B);

/*
	meh ill just dump it here
*/


/*
============================================================================

					BYTE ORDER FUNCTIONS

============================================================================
*/

#ifdef _SGI_SOURCE
#define __BIG_ENDIAN__
#endif

#ifdef __BIG_ENDIAN__

short   LittleShort( short l ) {
	byte b1,b2;

	b1 = l & 255;
	b2 = ( l >> 8 ) & 255;

	return ( b1 << 8 ) + b2;
}

short   BigShort( short l ) {
	return l;
}


int    LittleLong( int l ) {
	byte b1,b2,b3,b4;

	b1 = l & 255;
	b2 = ( l >> 8 ) & 255;
	b3 = ( l >> 16 ) & 255;
	b4 = ( l >> 24 ) & 255;

	return ( (int)b1 << 24 ) + ( (int)b2 << 16 ) + ( (int)b3 << 8 ) + b4;
}

int    BigLong( int l ) {
	return l;
}


float   LittleFloat( float l ) {
	union {byte b[4]; float f;} in, out;

	in.f = l;
	out.b[0] = in.b[3];
	out.b[1] = in.b[2];
	out.b[2] = in.b[1];
	out.b[3] = in.b[0];

	return out.f;
}

float   BigFloat( float l ) {
	return l;
}

#ifdef SIN
unsigned short   LittleUnsignedShort( unsigned short l ) {
	byte b1,b2;

	b1 = l & 255;
	b2 = ( l >> 8 ) & 255;

	return ( b1 << 8 ) + b2;
}

unsigned short   BigUnsignedShort( unsigned short l ) {
	return l;
}

unsigned    LittleUnsigned( unsigned l ) {
	byte b1,b2,b3,b4;

	b1 = l & 255;
	b2 = ( l >> 8 ) & 255;
	b3 = ( l >> 16 ) & 255;
	b4 = ( l >> 24 ) & 255;

	return ( (unsigned)b1 << 24 ) + ( (unsigned)b2 << 16 ) + ( (unsigned)b3 << 8 ) + b4;
}

unsigned    BigUnsigned( unsigned l ) {
	return l;
}
#endif


#else


short   BigShort( short l ) {
	byte b1,b2;

	b1 = l & 255;
	b2 = ( l >> 8 ) & 255;

	return ( b1 << 8 ) + b2;
}

short   LittleShort( short l ) {
	return l;
}


int    BigLong( int l ) {
	byte b1,b2,b3,b4;

	b1 = l & 255;
	b2 = ( l >> 8 ) & 255;
	b3 = ( l >> 16 ) & 255;
	b4 = ( l >> 24 ) & 255;

	return ( (int)b1 << 24 ) + ( (int)b2 << 16 ) + ( (int)b3 << 8 ) + b4;
}

int    LittleLong( int l ) {
	return l;
}

float   BigFloat( float l ) {
	union {byte b[4]; float f;} in, out;

	in.f = l;
	out.b[0] = in.b[3];
	out.b[1] = in.b[2];
	out.b[2] = in.b[1];
	out.b[3] = in.b[0];

	return out.f;
}

float   LittleFloat( float l ) {
	return l;
}

#ifdef SIN
unsigned short   BigUnsignedShort( unsigned short l ) {
	byte b1,b2;

	b1 = l & 255;
	b2 = ( l >> 8 ) & 255;

	return ( b1 << 8 ) + b2;
}

unsigned short   LittleUnsignedShort( unsigned short l ) {
	return l;
}


unsigned    BigUnsigned( unsigned l ) {
	byte b1,b2,b3,b4;

	b1 = l & 255;
	b2 = ( l >> 8 ) & 255;
	b3 = ( l >> 16 ) & 255;
	b4 = ( l >> 24 ) & 255;

	return ( (unsigned)b1 << 24 ) + ( (unsigned)b2 << 16 ) + ( (unsigned)b3 << 8 ) + b4;
}

unsigned    LittleUnsigned( unsigned l ) {
	return l;
}
#endif


#endif
