/*
 * lmsensors, configurable PMDA
 *  
 * Original implementation by Troy Dawson (dawson@fnal.gov)
 *
 * Copyright (c) 2012,2014 Red Hat.
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
 */ 

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>

#include "pmapi.h"
#include "impl.h"
#include "pmda.h"
#include "domain.h"
#include "lmsensors.h"

static char *username;
static char buf[4096];
static chips schips;

/*
 * lmsensors PMDA
 *
 */

/*
 * all metrics supported in this PMDA - one table entry for each
 */

static pmdaMetric metrictab[] = {
/* n_total */
    { NULL, 
      { PMDA_PMID(0,0), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT, 
        PMDA_PMUNITS(0, 0, 0, 0, 0, 0) }, },
/* n_lm75 */
    { NULL, 
      { PMDA_PMID(0,1), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT, 
        PMDA_PMUNITS(0, 0, 0, 0, 0, 0) }, },
/* n_lm79 */
    { NULL, 
      { PMDA_PMID(0,2), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT, 
        PMDA_PMUNITS(0, 0, 0, 0, 0, 0) }, },
/* n_lm87 */
    { NULL, 
      { PMDA_PMID(0,3), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT, 
        PMDA_PMUNITS(0, 0, 0, 0, 0, 0) }, },
/* n_w83781d */
    { NULL, 
      { PMDA_PMID(0,4), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT, 
        PMDA_PMUNITS(0, 0, 0, 0, 0, 0) }, },
/* n_mtp008 */
    { NULL, 
      { PMDA_PMID(0,5), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT, 
        PMDA_PMUNITS(0, 0, 0, 0, 0, 0) }, },
/* lm75 temp */
    { NULL, 
      { PMDA_PMID(1,0), PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT, 
        PMDA_PMUNITS(0, 0, 0, 0, 0, 0) }, },
/* lm79 fan1 */
    { NULL, 
      { PMDA_PMID(2,0), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT, 
        PMDA_PMUNITS(0, 0, 0, 0, 0, 0) }, },
/* lm79 fan2 */
    { NULL, 
      { PMDA_PMID(2,1), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT, 
        PMDA_PMUNITS(0, 0, 0, 0, 0, 0) }, },
/* lm79 fan3 */
    { NULL, 
      { PMDA_PMID(2,2), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT, 
        PMDA_PMUNITS(0, 0, 0, 0, 0, 0) }, },
/* lm79 fan_div */
    { NULL, 
      { PMDA_PMID(2,3), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT, 
        PMDA_PMUNITS(0, 0, 0, 0, 0, 0) }, },
/* lm79 temp */
    { NULL, 
      { PMDA_PMID(2,4), PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT, 
        PMDA_PMUNITS(0, 0, 0, 0, 0, 0) }, },
/* lm79 alarms */
    { NULL, 
      { PMDA_PMID(2,5), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT, 
        PMDA_PMUNITS(0, 0, 0, 0, 0, 0) }, },
/* lm79 VCore1 */
    { NULL, 
      { PMDA_PMID(2,6), PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT, 
        PMDA_PMUNITS(0, 0, 0, 0, 0, 0) }, },
/* lm79 VCore2 */
    { NULL, 
      { PMDA_PMID(2,7), PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT, 
        PMDA_PMUNITS(0, 0, 0, 0, 0, 0) }, },
/* lm79 p33V */
    { NULL, 
      { PMDA_PMID(2,8), PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT, 
        PMDA_PMUNITS(0, 0, 0, 0, 0, 0) }, },
/* lm79 p5V */
    { NULL, 
      { PMDA_PMID(2,9), PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT, 
        PMDA_PMUNITS(0, 0, 0, 0, 0, 0) }, },
/* lm79 p12V */
    { NULL, 
      { PMDA_PMID(2,10), PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT, 
        PMDA_PMUNITS(0, 0, 0, 0, 0, 0) }, },
/* lm79 n12V */
    { NULL, 
      { PMDA_PMID(2,11), PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT, 
        PMDA_PMUNITS(0, 0, 0, 0, 0, 0) }, },
/* lm79 n5V */
    { NULL, 
      { PMDA_PMID(2,12), PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT, 
        PMDA_PMUNITS(0, 0, 0, 0, 0, 0) }, },
/* lm79 vid */
    { NULL, 
      { PMDA_PMID(2,13), PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT, 
        PMDA_PMUNITS(0, 0, 0, 0, 0, 0) }, },
/* lm87 fan1 */
    { NULL, 
      { PMDA_PMID(3,0), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT, 
        PMDA_PMUNITS(0, 0, 0, 0, 0, 0) }, },
/* lm87 fan2 */
    { NULL, 
      { PMDA_PMID(3,1), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT, 
        PMDA_PMUNITS(0, 0, 0, 0, 0, 0) }, },
/* lm87 temp1 */
    { NULL, 
      { PMDA_PMID(3,2), PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT, 
        PMDA_PMUNITS(0, 0, 0, 0, 0, 0) }, },
/* lm87 CPUtemp */
    { NULL, 
      { PMDA_PMID(3,3), PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT, 
        PMDA_PMUNITS(0, 0, 0, 0, 0, 0) }, },
/* lm87 Vccp1 */
    { NULL, 
      { PMDA_PMID(3,4), PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT, 
        PMDA_PMUNITS(0, 0, 0, 0, 0, 0) }, },
/* lm87 Vccp2 */
    { NULL, 
      { PMDA_PMID(3,5), PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT, 
        PMDA_PMUNITS(0, 0, 0, 0, 0, 0) }, },
/* lm87 p25V */
    { NULL, 
      { PMDA_PMID(3,6), PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT, 
        PMDA_PMUNITS(0, 0, 0, 0, 0, 0) }, },
/* lm87 p33V */
    { NULL, 
      { PMDA_PMID(3,7), PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT, 
        PMDA_PMUNITS(0, 0, 0, 0, 0, 0) }, },
/* lm87 p5V */
    { NULL, 
      { PMDA_PMID(3,8), PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT, 
        PMDA_PMUNITS(0, 0, 0, 0, 0, 0) }, },
/* lm87 p12V */
    { NULL, 
      { PMDA_PMID(3,9), PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT, 
        PMDA_PMUNITS(0, 0, 0, 0, 0, 0) }, },
/* lm87 vid */
    { NULL, 
      { PMDA_PMID(3,10), PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT, 
        PMDA_PMUNITS(0, 0, 0, 0, 0, 0) }, },
/* w83781d fan1 */
    { NULL, 
      { PMDA_PMID(4,0), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT, 
        PMDA_PMUNITS(0, 0, 0, 0, 0, 0) }, },
/* w83781d fan2 */
    { NULL, 
      { PMDA_PMID(4,1), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT, 
        PMDA_PMUNITS(0, 0, 0, 0, 0, 0) }, },
/* w83781d fan3 */
    { NULL, 
      { PMDA_PMID(4,2), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT, 
        PMDA_PMUNITS(0, 0, 0, 0, 0, 0) }, },
/* w83781d fan_div */
    { NULL, 
      { PMDA_PMID(4,3), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT, 
        PMDA_PMUNITS(0, 0, 0, 0, 0, 0) }, },
/* w83781d temp1 */
    { NULL, 
      { PMDA_PMID(4,4), PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT, 
        PMDA_PMUNITS(0, 0, 0, 0, 0, 0) }, },
/* w83781d temp2 */
    { NULL, 
      { PMDA_PMID(4,5), PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT, 
        PMDA_PMUNITS(0, 0, 0, 0, 0, 0) }, },
/* w83781d temp3 */
    { NULL, 
      { PMDA_PMID(4,6), PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT, 
        PMDA_PMUNITS(0, 0, 0, 0, 0, 0) }, },
/* w83781d alarms */
    { NULL, 
      { PMDA_PMID(4,7), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT, 
        PMDA_PMUNITS(0, 0, 0, 0, 0, 0) }, },
/* w83781d beep */
    { NULL, 
      { PMDA_PMID(4,8), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT, 
        PMDA_PMUNITS(0, 0, 0, 0, 0, 0) }, },
/* w83781d VCore1 */
    { NULL, 
      { PMDA_PMID(4,9), PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT, 
        PMDA_PMUNITS(0, 0, 0, 0, 0, 0) }, },
/* w83781d VCore2 */
    { NULL, 
      { PMDA_PMID(4,10), PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT, 
        PMDA_PMUNITS(0, 0, 0, 0, 0, 0) }, },
/* w83781d p33V */
    { NULL, 
      { PMDA_PMID(4,11), PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT, 
        PMDA_PMUNITS(0, 0, 0, 0, 0, 0) }, },
/* w83781d p5V */
    { NULL, 
      { PMDA_PMID(4,12), PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT, 
        PMDA_PMUNITS(0, 0, 0, 0, 0, 0) }, },
/* w83781d p12V */
    { NULL, 
      { PMDA_PMID(4,13), PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT, 
        PMDA_PMUNITS(0, 0, 0, 0, 0, 0) }, },
/* w83781d n12V */
    { NULL, 
      { PMDA_PMID(4,14), PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT, 
        PMDA_PMUNITS(0, 0, 0, 0, 0, 0) }, },
/* w83781d n5V */
    { NULL, 
      { PMDA_PMID(4,15), PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT, 
        PMDA_PMUNITS(0, 0, 0, 0, 0, 0) }, },
/* w83781d vid */
    { NULL, 
      { PMDA_PMID(4,16), PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT, 
        PMDA_PMUNITS(0, 0, 0, 0, 0, 0) }, },
/* mtp008 fan1 */
    { NULL, 
      { PMDA_PMID(5,0), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT, 
        PMDA_PMUNITS(0, 0, 0, 0, 0, 0) }, },
/* mtp008 fan2 */
    { NULL, 
      { PMDA_PMID(5,1), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT, 
        PMDA_PMUNITS(0, 0, 0, 0, 0, 0) }, },
/* mtp008 fan3 */
    { NULL, 
      { PMDA_PMID(5,2), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT, 
        PMDA_PMUNITS(0, 0, 0, 0, 0, 0) }, },
/* mtp008 temp1 */
    { NULL, 
      { PMDA_PMID(5,3), PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT, 
        PMDA_PMUNITS(0, 0, 0, 0, 0, 0) }, },
/* mtp008 temp2 */
    { NULL, 
      { PMDA_PMID(5,4), PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT, 
        PMDA_PMUNITS(0, 0, 0, 0, 0, 0) }, },
/* mtp008 VCore1 */
    { NULL, 
      { PMDA_PMID(5,5), PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT, 
        PMDA_PMUNITS(0, 0, 0, 0, 0, 0) }, },
/* mtp008 VCore2 */
    { NULL, 
      { PMDA_PMID(5,6), PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT, 
        PMDA_PMUNITS(0, 0, 0, 0, 0, 0) }, },
/* mtp008 p33V */
    { NULL, 
      { PMDA_PMID(5,7), PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT, 
        PMDA_PMUNITS(0, 0, 0, 0, 0, 0) }, },
/* mtp008 p12V */
    { NULL, 
      { PMDA_PMID(5,8), PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT, 
        PMDA_PMUNITS(0, 0, 0, 0, 0, 0) }, },
/* mtp008 n12V */
    { NULL, 
      { PMDA_PMID(5,9), PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT, 
        PMDA_PMUNITS(0, 0, 0, 0, 0, 0) }, },
/* mtp008 vid */
    { NULL, 
      { PMDA_PMID(5,10), PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT, 
        PMDA_PMUNITS(0, 0, 0, 0, 0, 0) }, },
/* mtp008 vtt */
    { NULL, 
      { PMDA_PMID(5,11), PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT, 
        PMDA_PMUNITS(0, 0, 0, 0, 0, 0) }, },
};


w83781d get_w83781d()
{
	float f;
	w83781d sensor= {0,0,0,0,00.01,00.01,00.01,0,0,00.01,00.01,00.01,00.01,00.01,00.01,00.01,00.01};
	
	if (schips.n_w83781d > 0) {

/* fan1 */
		get_file(schips.s_w83781d[0],"/fan1");
		sensor.fan1 = get_int(buf,2);
/* fan2 */
		get_file(schips.s_w83781d[0],"/fan2");
		sensor.fan2 = get_int(buf,2);
/* fan3 */
		get_file(schips.s_w83781d[0],"/fan3");
		sensor.fan3 = get_int(buf,2);
/* fan_div */
		get_file(schips.s_w83781d[0],"/fan_div");
		sensor.fan_div = get_int(buf,1);
/* temp1 */
		get_file(schips.s_w83781d[0],"/temp1");
		sensor.temp1 = get_float(buf,3);
/* temp2 */
		get_file(schips.s_w83781d[0],"/temp2");
		sensor.temp2 = get_float(buf,3);
/* temp3 */
		get_file(schips.s_w83781d[0],"/temp3");
		sensor.temp3 = get_float(buf,3);
/* alarms */
		get_file(schips.s_w83781d[0],"/alarms");
		sensor.alarms = get_int(buf,1);
/* beep */
		get_file(schips.s_w83781d[0],"/beep");
		sensor.beep = get_int(buf,1);
/* VCore1 */
		get_file(schips.s_w83781d[0],"/in0");
		sensor.VCore1 = get_float(buf,3);
/* VCore2 */
		get_file(schips.s_w83781d[0],"/in1");
		sensor.VCore2 = get_float(buf,3);
/* p33V */
		get_file(schips.s_w83781d[0],"/in2");
		sensor.p33V = get_float(buf,3);
/* p5V */
		get_file(schips.s_w83781d[0],"/in3");
		f = get_float(buf,3);
		sensor.p5V = f * ((6.80/10)+1);
/* p12V */
		get_file(schips.s_w83781d[0],"/in4");
		f = get_float(buf,3);
		sensor.p12V = f * ((28.00/10)+1);
/* n12V */
		get_file(schips.s_w83781d[0],"/in5");
		f = get_float(buf,3);
		sensor.n12V = -1 * f * (210/60.40);
/* n5V */
		get_file(schips.s_w83781d[0],"/in6");
		f = get_float(buf,3);
		sensor.n5V = -1 * f * (90.9/60.40);
/* vid */
		get_file(schips.s_w83781d[0],"/vid");
		sensor.vid = get_float(buf,1);
	}	
 	return sensor;
}

mtp008 get_mtp008()
{
	float f;
	mtp008 sensor= {0,0,0,00.01,00.01,00.01,00.01,00.01,00.01,00.01,00.01,00.01};
	
	if (schips.n_mtp008 > 0) {

/* fan1 */
		get_file(schips.s_mtp008[0],"/fan1");
		sensor.fan1 = get_int(buf,2);
/* fan2 */
		get_file(schips.s_mtp008[0],"/fan2");
		sensor.fan2 = get_int(buf,2);
/* fan3 */
		get_file(schips.s_mtp008[0],"/fan3");
		sensor.fan3 = get_int(buf,2);
/* temp1 */
		get_file(schips.s_mtp008[0],"/temp1");
		sensor.temp1 = get_float(buf,3);
/* temp2 */
		get_file(schips.s_mtp008[0],"/temp2");
		sensor.temp2 = get_float(buf,3);
/* VCore1 */
		get_file(schips.s_mtp008[0],"/in0");
		sensor.VCore1 = get_float(buf,3);
/* VCore2 */
		get_file(schips.s_mtp008[0],"/in3");
		sensor.VCore2 = get_float(buf,3);
/* p33V */
		get_file(schips.s_mtp008[0],"/in1");
		sensor.p33V = get_float(buf,3);
/* p12V */
		get_file(schips.s_mtp008[0],"/in2");
		f = get_float(buf,3);
		sensor.p12V = f * ((38.00/10)+1);
/* n12V */
		get_file(schips.s_mtp008[0],"/in5");
		f = get_float(buf,3);
		sensor.n12V = ( f * 36 - 118.61 ) / 7;
/* vid */
		get_file(schips.s_mtp008[0],"/vid");
		sensor.vid = get_float(buf,1);
/* vtt */
		get_file(schips.s_mtp008[0],"/in6");
		sensor.vid = get_float(buf,3);
	}	
 	return sensor;
}

lm79 get_lm79()
{
	float f;
	lm79 sensor= {0,0,0,00.01,0,00.01,00.01,00.01,00.01,00.01,00.01,00.01,00.01,00.01};
	
	if (schips.n_lm79 > 0) {
/* fan1 */
		get_file(schips.s_lm79[0],"/fan1");
		sensor.fan1 = get_int(buf,2);
/* fan2 */
		get_file(schips.s_lm79[0],"/fan2");
		sensor.fan2 = get_int(buf,2);
/* fan3 */
		get_file(schips.s_lm79[0],"/fan3");
		sensor.fan3 = get_int(buf,2);
/* fan_div */
		get_file(schips.s_lm79[0],"/fan_div");
		sensor.fan_div = get_int(buf,1);
/* temp */
		get_file(schips.s_lm79[0],"/temp");
		sensor.temp = get_float(buf,3);
/* alarms */
		get_file(schips.s_lm79[0],"/alarms");
		sensor.alarms = get_int(buf,1);
/* VCore1 */
		get_file(schips.s_lm79[0],"/in0");
		sensor.VCore1 = get_float(buf,3);
/* VCore2 */
		get_file(schips.s_lm79[0],"/in1");
		sensor.VCore2 = get_float(buf,3);
/* p33V */
		get_file(schips.s_lm79[0],"/in2");
		sensor.p33V = get_float(buf,3);
/* p5V */
		get_file(schips.s_lm79[0],"/in3");
		f = get_float(buf,3);
		sensor.p5V = f * ((6.80/10)+1);
/* p12V */
		get_file(schips.s_lm79[0],"/in4");
		f = get_float(buf,3);
		sensor.p12V = f * ((28.00/10)+1);
/* n12V */
		get_file(schips.s_lm79[0],"/in5");
		f = get_float(buf,3);
		sensor.n12V = -1 * f * (210/60.40);
/* n5V */
		get_file(schips.s_lm79[0],"/in6");
		f = get_float(buf,3);
		sensor.n5V = -1 * f * (90.9/60.40);
/* vid */
		get_file(schips.s_lm79[0],"/vid");
		sensor.vid = get_float(buf,1);
	}
		
 	return sensor;
}

lm87 get_lm87()
{
	lm87 sensor= {0,0,00.01,00.01,00.01,00.01,00.01,00.01,00.01,00.01,00.01};
	
	if (schips.n_lm87 > 0) {
/* fan1 */
		get_file(schips.s_lm87[0],"/fan");
		sensor.fan1 = get_int(buf,2);
/* fan2 */
		get_file(schips.s_lm87[0],"/fan2");
		sensor.fan2 = get_int(buf,2);
/* temp1 */
		get_file(schips.s_lm87[0],"/temp1");
		sensor.temp1 = get_float(buf,3);
/* CPUtemp */
		get_file(schips.s_lm87[0],"/temp2");
		sensor.CPUtemp = get_float(buf,3);
/* Vccp1 */
		get_file(schips.s_lm87[0],"/in1");
		sensor.Vccp1 = get_float(buf,3);
/* Vccp2 */
		get_file(schips.s_lm87[0],"/in5");
		sensor.Vccp2 = get_float(buf,3);
/* p25V */
		get_file(schips.s_lm87[0],"/in0");
		sensor.p25V = get_float(buf,3);
/* p33V */
		get_file(schips.s_lm87[0],"/in2");
		sensor.p33V = get_float(buf,3);
/* p5V */
		get_file(schips.s_lm87[0],"/in3");
		sensor.p5V = get_float(buf,3);
/* p12V */
		get_file(schips.s_lm87[0],"/in4");
		sensor.p12V = get_float(buf,3);
/* vid */
		get_file(schips.s_lm87[0],"/vid");
		sensor.vid = get_float(buf,1);
	}
		
 	return sensor;
}

lm75 get_lm75()
{
	lm75 sensor= {00.01};
	
	if (schips.n_lm75 > 0) {
		get_file(schips.s_lm75[0],"/temp");
		sensor.temp = get_float(buf,3);
	}
 	return sensor;
}

void
get_chips()
{
	int i;
	int n;
	int nbufindex;
	char *bufindex[64];
	char *temp;
	
	n = get_file("chips", "");
	
	buf[sizeof(buf)-1] = '\0';

	nbufindex = 0;
	bufindex[nbufindex++] = &buf[0];
	for (i=0; i < n; i++) {
		if (buf[i] == '\n') {
			buf[i] = '\0';
			bufindex[nbufindex++] = buf + i + 1;
		}
	}
	
	for ( i=0; i < nbufindex ; i++ ) {
		temp="";
		if (strncmp("lm75", bufindex[i]+4, 4) == 0 ) {
			temp = strtok(bufindex[i]+4," ");
			strcat(schips.s_lm75[schips.n_lm75], temp);
			schips.total++;
			schips.n_lm75++;
		}
		else if (strncmp("lm79", bufindex[i]+4, 4) == 0 ) {
			temp = strtok(bufindex[i]+4," ");
			strcat(schips.s_lm79[schips.n_lm79], temp);
			schips.total++;
			schips.n_lm79++;
		}
		else if (strncmp("lm87", bufindex[i]+4, 4) == 0 ) {
			temp = strtok(bufindex[i]+4," ");
			strcat(schips.s_lm87[schips.n_lm87], temp);
			schips.total++;
			schips.n_lm87++;
		}
		else if (strncmp("w83781d", bufindex[i]+4, 7) == 0 ) {
			temp = strtok(bufindex[i]+4," ");
			strcat(schips.s_w83781d[schips.n_w83781d], temp);
			schips.total++;
			schips.n_w83781d++;
		}
		else if (strncmp("mtp008", bufindex[i]+4, 6) == 0 ) {
			temp = strtok(bufindex[i]+4," ");
			strcat(schips.s_mtp008[schips.n_mtp008], temp);
			schips.total++;
			schips.n_mtp008++;
		}
	}	
}


/*
 * Get the contents of a file and return them
 */
int get_file(char *middle, char *end){

	int fd;
	int n;
	char s[1024]="/proc/sys/dev/sensors/";
	
/*
 * create the new string, the end result being the actual file name
 */
	strcat(s,middle);
	strcat(s,end);

/*
 * read in the file into the buffer buf
 */
	if ((fd = open(s, O_RDONLY)) < 0) {
		return -1;
	}
	n = read(fd, buf, sizeof(buf));
	close(fd);
	return n;
}

/*
 * Pull a certain float value out of a string of floats
 */
float get_float(char *s, int i){
	char *temp;
	float f;
	int  j;
	
	temp = strtok(s," ");
	
	for (j=1;j<i;j++)
		temp = strtok(NULL," ");
	
	f = atof(temp);
	return f;
}

/*
 * Pull a certain int value out of a string of int's
 */
int get_int(char *s, int i){
	char *temp;
	int f;
	int  j;
	
	temp = strtok(s," ");
	
	for (j=1;j<i;j++)
		temp = strtok(NULL," ");
	
	f = atoi(temp);
	return f;
}

/*
 * callback provided to pmdaFetch
 */
static int
lmsensors_fetchCallBack(pmdaMetric *mdesc, unsigned int inst, pmAtomValue *atom)
{
   mtp008 sensormtp008;
   w83781d sensorw83781d;
   lm87 sensor87;
   lm79 sensor79;
   lm75 sensor75;
    __pmID_int		*idp = (__pmID_int *)&(mdesc->m_desc.pmid);

    if (idp->cluster > 5)
	return PM_ERR_PMID;
    else if (inst != PM_IN_NULL)
	return PM_ERR_INST;

	if (idp->cluster == 0) {	/*lmsensors*/
	    switch (idp->item) {
		case 0:
			atom->l = schips.total;
			break ;
		case 1:
			atom->l = schips.n_lm75;
			break ;
		case 2:
			atom->l = schips.n_lm79;
			break ;
		case 3:
			atom->l = schips.n_lm87;
			break ;
		case 4:
			atom->l = schips.n_w83781d;
			break ;
		case 5:
			atom->l = schips.n_mtp008;
			break ;
		default:
			return PM_ERR_PMID;
	   }
	}
	if (idp->cluster == 1) {	/*lmsensors.lm75*/
		if (schips.n_lm75 > 0) {
		    sensor75=get_lm75();
		    switch (idp->item) {
			case 0:
				atom->f = sensor75.temp;
				break ;
			default:
				return PM_ERR_PMID;
			}
		} else atom->f=9999;
	}
	if (idp->cluster == 2) {	/*lmsensors.lm79*/
		if (schips.n_lm79 > 0) {
		    sensor79=get_lm79();
		    switch (idp->item) {
			case 0:
				atom->l = sensor79.fan1;
				break ;
			case 1:
				atom->l = sensor79.fan2;
				break ;
			case 2:
				atom->l = sensor79.fan3;
				break ;
			case 3:
				atom->l = sensor79.fan_div;
				break ;
			case 4:
				atom->f = sensor79.temp;
				break ;
			case 5:
				atom->l = sensor79.alarms;
				break ;
			case 6:
				atom->f = sensor79.VCore1;
				break ;
			case 7:
				atom->f = sensor79.VCore2;
				break ;
			case 8:
				atom->f = sensor79.p33V;
				break ;
			case 9:
				atom->f = sensor79.p5V;
				break ;
			case 10:
				atom->f = sensor79.p12V;
				break ;
			case 11:
				atom->f = sensor79.n12V;
				break ;
			case 12:
				atom->f = sensor79.n5V;
				break ;
			case 13:
				atom->f = sensor79.vid;
				break ;
			default:
				return PM_ERR_PMID;
			}
		} else atom->f=9999;
	}
	if (idp->cluster == 3) {	/*lmsensors.lm87*/
		if (schips.n_lm87 > 0) {
		    sensor87=get_lm87();
		    switch (idp->item) {
			case 0:
				atom->l = sensor87.fan1;
				break ;
			case 1:
				atom->l = sensor87.fan2;
				break ;
			case 2:
				atom->f = sensor87.temp1;
				break ;
			case 3:
				atom->f = sensor87.CPUtemp;
				break ;
			case 4:
				atom->f = sensor87.Vccp1;
				break ;
			case 5:
				atom->f = sensor87.Vccp2;
				break ;
			case 6:
				atom->f = sensor87.p25V;
				break ;
			case 7:
				atom->f = sensor87.p33V;
				break ;
			case 8:
				atom->f = sensor87.p5V;
				break ;
			case 9:
				atom->f = sensor87.p12V;
				break ;
			case 10:
				atom->f = sensor87.vid;
				break ;
			default:
				return PM_ERR_PMID;
			}
		} else atom->f=9999;
	}
	if (idp->cluster == 4) {	/*lmsensors.w83781d*/
		if (schips.n_w83781d > 0) {
		    sensorw83781d=get_w83781d();
		    switch (idp->item) {
			case 0:
				atom->l = sensorw83781d.fan1;
				break ;
			case 1:
				atom->l = sensorw83781d.fan2;
				break ;
			case 2:
				atom->l = sensorw83781d.fan3;
				break ;
			case 3:
				atom->l = sensorw83781d.fan_div;
				break ;
			case 4:
				atom->f = sensorw83781d.temp1;
				break ;
			case 5:
				atom->f = sensorw83781d.temp2;
				break ;
			case 6:
				atom->f = sensorw83781d.temp3;
				break ;
			case 7:
				atom->l = sensorw83781d.alarms;
				break ;
			case 8:
				atom->l = sensorw83781d.beep;
				break ;
			case 9:
				atom->f = sensorw83781d.VCore1;
				break ;
			case 10:
				atom->f = sensorw83781d.VCore2;
				break ;
			case 11:
				atom->f = sensorw83781d.p33V;
				break ;
			case 12:
				atom->f = sensorw83781d.p5V;
				break ;
			case 13:
				atom->f = sensorw83781d.p12V;
				break ;
			case 14:
				atom->f = sensorw83781d.n12V;
				break ;
			case 15:
				atom->f = sensorw83781d.n5V;
				break ;
			case 16:
				atom->f = sensorw83781d.vid;
				break ;
			default:
				return PM_ERR_PMID;
			}
		} else atom->f=9999;
	}
	if (idp->cluster == 5) {	/*lmsensors.mtp008*/
		if (schips.n_mtp008 > 0) {
		    sensormtp008=get_mtp008();
		    switch (idp->item) {
			case 0:
				atom->l = sensormtp008.fan1;
				break ;
			case 1:
				atom->l = sensormtp008.fan2;
				break ;
			case 2:
				atom->l = sensormtp008.fan3;
				break ;
			case 3:
				atom->f = sensormtp008.temp1;
				break ;
			case 4:
				atom->f = sensormtp008.temp2;
				break ;
			case 5:
				atom->f = sensormtp008.VCore1;
				break ;
			case 6:
				atom->f = sensormtp008.VCore2;
				break ;
			case 7:
				atom->f = sensormtp008.p33V;
				break ;
			case 8:
				atom->f = sensormtp008.p12V;
				break ;
			case 9:
				atom->f = sensormtp008.n12V;
				break ;
			case 10:
				atom->f = sensormtp008.vid;
				break ;
			case 11:
				atom->f = sensormtp008.vtt;
				break ;
			default:
				return PM_ERR_PMID;
			}
		} else atom->f=9999;
	}

    return 0;
}

/*
 * Initialise the agent (both daemon and DSO).
 */
void 
lmsensors_init(pmdaInterface *dp)
{
    get_chips();

    __pmSetProcessIdentity(username);
    pmdaSetFetchCallBack(dp, lmsensors_fetchCallBack);
    pmdaInit(dp, NULL, 0, 
	     metrictab, sizeof(metrictab)/sizeof(metrictab[0]));
}

pmLongOptions   longopts[] = {
    PMDA_OPTIONS_HEADER("Options"),
    PMOPT_DEBUG,
    PMDAOPT_DOMAIN,
    PMDAOPT_LOGFILE,
    PMDAOPT_USERNAME,
    PMOPT_HELP,
    PMDA_OPTIONS_END
};

pmdaOptions     opts = {
    .short_options = "D:d:l:U:?",
    .long_options = longopts,
};

/*
 * Set up the agent if running as a daemon.
 */
int
main(int argc, char **argv)
{
    int			sep = __pmPathSeparator();
    pmdaInterface	desc;
    char		mypath[MAXPATHLEN];

    __pmSetProgname(argv[0]);
    __pmGetUsername(&username);

    snprintf(mypath, sizeof(mypath), "%s%c" "lmsensors" "%c" "help",
		pmGetConfig("PCP_PMDAS_DIR"), sep, sep);
    pmdaDaemon(&desc, PMDA_INTERFACE_2, pmProgname, LMSENSORS,
		"lmsensors.log", mypath);

    pmdaGetOptions(argc, argv, &opts, &desc);
    if (opts.errors) {
	pmdaUsageMessage(&opts);
	exit(1);
    }
    if (opts.username)
	username = opts.username;

    pmdaOpenLog(&desc);
    lmsensors_init(&desc);
    pmdaConnect(&desc);
    pmdaMain(&desc);
    exit(0);
}
