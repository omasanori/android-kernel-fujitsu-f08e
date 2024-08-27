/*
 * Copyright(C) 2009-2013 FUJITSU LIMITED
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <linux/kernel.h>
#include <linux/time.h>
#include <linux/crashdump.h>

//#define dprintk printk
#define dprintk(...)

//#define dprintk2 printk
#define dprintk2(...)

static char *weekdaystr[7] = {
	"Sun",
	"Mon",
	"Tue",
	"Wed",
	"Thu",
	"Fri",
	"Sat"
};

static char *monthstr[12] = {
	"Jan",
	"Feb",
	"Mar",
	"Apr",
	"May",
	"Jun",
	"Jul",
	"Aug",
	"Sep",
	"Oct",
	"Nov",
	"Dec"
};

#define KSecsPerMin 		(60)
#define KSecsPerHour 		(60*KSecsPerMin)
#define KSecsPerDay 		(24*KSecsPerHour)
#define KSecsPerLeapYr 		(366*KSecsPerDay)
#define KSecsPerYr 			(365*KSecsPerDay)
#define KSecsDaysPer4Years 	((3*KSecsPerYr)+KSecsPerLeapYr)
#define KSecondsPerDay 		(86400)

#define KMinsPerHour 		(60)
#define KHoursPerDay 		(24)

// Days in each month
static int8_t mTab[2][12]=
	{
	{31,28,31,30,31,30,31,31,30,31,30,31}, // 28 days in Feb
	{31,29,31,30,31,30,31,31,30,31,30,31}  // 29 days in Feb
	};

static void GetMonthData(int aDayInYear, int aLeap, int* aDom, int* aMonth)
{
	int i;
	int runtot=0;
	for (i=0; i<12; i++) {
		if ((aDayInYear>=runtot) && (aDayInYear < mTab[aLeap][i]+runtot)) {
			// Month and day of the month both start from 1, rather than
			// zero (hence the +1)
			*aMonth=i+1;
			*aDom=aDayInYear-runtot+1;
			break;
		}
		runtot+=mTab[aLeap][i];
	}
}

static void secs_to_ymd(unsigned long aTime, int* aYear, int* aDom, int* aMonth) {
	int adjyear;

	aTime += KSecsPerLeapYr + KSecsPerYr; // 1968(LeapYr),1969
	// Work out year within 4 years first
	*aYear = (aTime / KSecsDaysPer4Years)*4;
	*aDom=0;
	*aMonth=0;
	adjyear = aTime % KSecsDaysPer4Years;
	if (adjyear<KSecsPerLeapYr)	{
		GetMonthData(adjyear/KSecsPerDay, 1, aDom, aMonth);
	}else{
		adjyear-=KSecsPerLeapYr;
		*aYear+=(adjyear/KSecsPerYr)+1;
		GetMonthData((adjyear%KSecsPerYr)/KSecsPerDay, 0, aDom, aMonth);
	}

	*aYear += 1968;
}

/* transform timeval to wallclock values */
void crashdump_timetr(struct timeval *tv, int *year0, int *month0, int *monthday0, int *hour0, int *minute0, int *second0, int *weekday0, char *weekdaystring0, char *monthstring0)
{
	int second;
	int minute;
	int hour;
	int monthday;
	int month;
	int year;
	int weekday;
	int year_calc, month_calc;
	unsigned long val;

	val = tv->tv_sec; // total seconds
	dprintk2(KERN_EMERG "(%s:%s:%d) %lu second\n", __FILE__, __FUNCTION__, __LINE__, val);

	second = val % KSecsPerMin;
	val /= KSecsPerMin; // total minutes
	dprintk2(KERN_EMERG "(%s:%s:%d) %lu minute %d second\n", __FILE__, __FUNCTION__, __LINE__, val, second);

	minute = val % KMinsPerHour;
	val /= KMinsPerHour; // total hours
	dprintk2(KERN_EMERG "(%s:%s:%d) %lu hour %d minute %d second\n", __FILE__, __FUNCTION__, __LINE__, val, minute, second);

	hour = val % KHoursPerDay;
	val /= KHoursPerDay; // total days
	dprintk2(KERN_EMERG "(%s:%s:%d) %lu day %d hour %d minute %d second\n", __FILE__, __FUNCTION__, __LINE__, val, hour, minute, second);

	secs_to_ymd(tv->tv_sec, &year, &monthday, &month);
	dprintk2(KERN_EMERG "(%s:%s:%d) %d year %d month %d day %d hour %d minute %d second\n", __FILE__, __FUNCTION__, __LINE__, year, month, monthday, hour, minute, second);

    if(month < 3){
        year_calc = year - 1;
        month_calc = month + 12;
    }else{
        year_calc = year;
        month_calc = month;
	}

    weekday = 365 * year_calc + year_calc / 4 - year_calc / 100 + year_calc / 400 + 306 * (month_calc + 1) / 10 + monthday - 428;
	weekday %= 7;

	dprintk2(KERN_EMERG "(%s:%s:%d) weekday: %d\n", __FILE__, __FUNCTION__, __LINE__, weekday);

	if (year0) {
		*year0 = year;
	}

	if (month0) {
		*month0 = month;
	}

	if (monthday0) {
		*monthday0 = monthday;
	}

	if (hour0) {
		*hour0 = hour;
	}

	if (minute0) {
		*minute0 = minute;
	}

	if (second0) {
		*second0 = second;
	}

	if (weekdaystring0) {
		dprintk2(KERN_EMERG "(%s:%s:%d) weekday: %d\n", __FILE__, __FUNCTION__, __LINE__, weekday);
		dprintk2(KERN_EMERG "(%s:%s:%d) weekdaystring0: %p\n", __FILE__, __FUNCTION__, __LINE__, weekdaystring0);

		if ( (weekday < 0) || (weekday > 6) ) {
			strncpy(weekdaystring0, "???", 4);
		}
		else {
			strncpy(weekdaystring0, weekdaystr[weekday], 4);
		}
	}

	if (monthstring0) {
		dprintk2(KERN_EMERG "(%s:%s:%d) month: %d\n", __FILE__, __FUNCTION__, __LINE__, month);
		dprintk2(KERN_EMERG "(%s:%s:%d) monthstring0: %p\n", __FILE__, __FUNCTION__, __LINE__, monthstring0);

		if ( (month < 1) || (month > 12) ) {
			strncpy(monthstring0, "???", 4);
		}
		else {
			strncpy(monthstring0, monthstr[month-1], 4);
		}
	}
}

void crashdump_time(void)
{
	struct timeval tv;
	int second;
	int minute;
	int hour;
	int monthday;
	int month;
	int year;
	int weekday;
	char weekdaystring[4];
	char monthstring[4];

#ifdef CONFIG_CRASHDUMP_TO_FLASH
	printk(KERN_EMERG "%s\n", __FUNCTION__);
#endif

	dprintk(KERN_EMERG "(%s:%s:%d)\n", __FILE__, __FUNCTION__, __LINE__);
	crashdump_write_str("\n=== Panic time ===\n");

	do_gettimeofday(&tv);
	dprintk2(KERN_EMERG "(%s:%s:%d) tv.tv_sec, tv.tv_usec: %ld, %ld\n", __FILE__, __FUNCTION__, __LINE__, tv.tv_sec, tv.tv_usec);

	crashdump_timetr(&tv, &year, &month, &monthday, &hour, &minute, &second, &weekday, weekdaystring, monthstring);

	// Ex: Panic time: Thu Jan  1 00:00:27 UTC 1970

	dprintk(KERN_EMERG "Panic time: %s %s %2d %02d:%02d:%02d UTC %4d\n", weekdaystring, monthstring, monthday, hour, minute, second, year);

	crashdump_write_str("Panic time: %s %s %2d %02d:%02d:%02d UTC %4d\n", weekdaystring, monthstring, monthday, hour, minute, second, year);
}
