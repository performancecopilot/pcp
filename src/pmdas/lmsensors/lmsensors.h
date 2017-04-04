/*
 * Original implementation by Troy Dawson (dawson@fnal.gov)
 *  
 * Copyright (c) 2001,2004 Silicon Graphics, Inc.  All Rights Reserved.
 * 
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */ 

typedef struct {
	int total;
	int n_lm75;
	int n_lm79;
	int n_lm87;
	int n_w83781d;
	int n_mtp008;
	char s_lm75[2][1024] ;
	char s_lm79[2][1024] ;
	char s_lm87[2][1024] ;
	char s_w83781d[2][1024] ;
	char s_mtp008[2][1024] ;
} chips;

typedef struct {
        float temp;
} lm75;
 
typedef struct {
	int fan1;
	int fan2;
	int fan3;
	int fan_div;
	float temp;
	int alarms;
	float VCore1;
	float VCore2;
	float p33V;
	float p5V;
	float p12V;
	float n12V;
	float n5V;
	float vid;
} lm79;
 
typedef struct {
	int fan1;
	int fan2;
	float temp1;
	float CPUtemp;
	float Vccp1;
	float Vccp2;
	float p25V;
	float p33V;
	float p5V;
	float p12V;
	float vid;
} lm87;

typedef struct {
	int fan1;
	int fan2;
	int fan3;
	int fan_div;
	float temp1;
	float temp2;
	float temp3;
	int alarms;
	int beep;
	float VCore1;
	float VCore2;
	float p33V;
	float p5V;
	float p12V;
	float n12V;
	float n5V;
	float vid;
} w83781d;

typedef struct {
	int fan1;
	int fan2;
	int fan3;
	float temp1;
	float temp2;
	float VCore1;
	float VCore2;
	float p33V;
	float p12V;
	float n12V;
	float vid;
	float vtt;
} mtp008;

extern void get_chips();
extern lm75 get_lm75();
extern lm79 get_lm79();
extern lm87 get_lm87();
extern w83781d get_w83781d();
extern mtp008 get_mtp008();
extern int get_file(char *, char *);
extern int get_int(char * , int);
extern float get_float(char * , int);
