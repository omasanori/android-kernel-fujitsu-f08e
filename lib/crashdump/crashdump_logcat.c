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
#include "logger.h"
#include <linux/sort.h>
#include <linux/slab.h>
#include <linux/crashdump.h>

//#define dprintk printk
#define dprintk(...)

//#define dprintk2 printk
#define dprintk2(...)

#define OUT_TAG "EventTagMap"
#define EVENT_TAG_MAP_FILE	"/system/etc/event-log-tags"

// from system/core/include/android/log.h
typedef enum android_LogPriority {
	ANDROID_LOG_UNKNOWN = 0,
	ANDROID_LOG_DEFAULT,	/* only for SetMinPriority() */
	ANDROID_LOG_VERBOSE,
	ANDROID_LOG_DEBUG,
	ANDROID_LOG_INFO,
	ANDROID_LOG_WARN,
	ANDROID_LOG_ERROR,
	ANDROID_LOG_FATAL,
	ANDROID_LOG_SILENT,		/* only for SetMinPriority(); must be last */
} android_LogPriority;

// from system/core/include/cutils/log.h
typedef enum {
	EVENT_TYPE_INT		= 0,
	EVENT_TYPE_LONG		= 1,
	EVENT_TYPE_STRING	= 2,
	EVENT_TYPE_LIST		= 3,
} AndroidEventLogType;

// from system/core/liblog/event_tag_map.c
/*
 * Single entry.
 */
typedef struct EventTag {
	unsigned int	tagIndex;
	const char*		tagStr;
} EventTag;

/*
 * Map.
 */
typedef struct EventTagMap {
	/* memory-mapped source file; we get strings from here */
	void*		mapAddr;
	size_t		mapLen;

	/* array of event tags, sorted numerically by tag index */
	EventTag*	tagArray;
	int			numTags;
} EventTagMap;

static char mapbuf[64*1024];
static char readbuf[LOGGER_ENTRY_MAX_PAYLOAD + sizeof(struct logger_entry)];
size_t readbuf_index = 0;
size_t entry_rem = 0;
EventTagMap* map = NULL;

static int processFile(EventTagMap* map);
static int countMapLines(const EventTagMap* map);
static int parseMapLines(EventTagMap* map);
static int scanTagLine(char** pData, EventTag* tag, int lineNum);
static int sortTags(EventTagMap* map);

// from system/core/liblog/logprint.c
/*
 * Extract a 4-byte value from a byte stream.
 */
static inline uint32_t get4LE(const uint8_t* src)
{
	return src[0] | (src[1] << 8) | (src[2] << 16) | (src[3] << 24);
}

/*
 * Extract an 8-byte value from a byte stream.
 */
static inline uint64_t get8LE(const uint8_t* src)
{
	uint32_t low, high;

	low = src[0] | (src[1] << 8) | (src[2] << 16) | (src[3] << 24);
	high = src[4] | (src[5] << 8) | (src[6] << 16) | (src[7] << 24);
	return ((long long) high << 32) | (long long) low;
}

static char filterPriToChar (char pri)
{
	switch (pri) {
	case ANDROID_LOG_VERBOSE:	return 'V';
	case ANDROID_LOG_DEBUG:		return 'D';
	case ANDROID_LOG_INFO:		return 'I';
	case ANDROID_LOG_WARN:		return 'W';
	case ANDROID_LOG_ERROR:		return 'E';
	case ANDROID_LOG_FATAL:		return 'F';
	case ANDROID_LOG_SILENT:	return 'S';

	case ANDROID_LOG_DEFAULT:
	case ANDROID_LOG_UNKNOWN:
	default:					return '?';
	}
}

// from system/core/liblog/event_tag_map.c
/*
 * Determine whether "c" is a whitespace char.
 */
static inline int isCharWhitespace(char c)
{
	return (c == ' ' || c == '\n' || c == '\r' || c == '\t');
}

/*
 * Determine whether "c" is a valid tag char.
 */
static inline int isCharValidTag(char c)
{
	return ((c >= 'A' && c <= 'Z') ||
			(c >= 'a' && c <= 'z') ||
			(c >= '0' && c <= '9') ||
			(c == '_'));
}

/*
 * Determine whether "c" is a valid decimal digit.
 */
static inline int isCharDigit(char c)
{
	return (c >= '0' && c <= '9');
}

static int countMapLines(const EventTagMap* map)
{
	int numTags, unknown;
	const char* cp;
	const char* endp;

	cp = (const char*) map->mapAddr;
	endp = cp + map->mapLen;

	numTags = 0;
	unknown = 1;
	while (cp < endp) {
		if (*cp == '\n') {
			unknown = 1;
		} else if (unknown) {
			if (isCharDigit(*cp)) {
				/* looks like a tag to me */
				numTags++;
				unknown = 0;
			} else if (isCharWhitespace(*cp)) {
				/* might be leading whitespace before tag num, keep going */
			} else {
				/* assume comment; second pass can complain in detail */
				unknown = 0;
			}
		} else {
			/* we've made up our mind; just scan to end of line */
		}
		cp++;
	}

	return numTags;
}

static int parseMapLines(EventTagMap* map)
{
	int tagNum, lineStart, lineNum;
	char* cp;
	char* endp;

	cp = (char*) map->mapAddr;
	endp = cp + map->mapLen;

	/* insist on EOL at EOF; simplifies parsing and null-termination */
	if (*(endp-1) != '\n') {
		printk(KERN_EMERG "%s: map file missing EOL on last line\n", OUT_TAG);
		return -1;
	}

	tagNum = 0;
	lineStart = 1;
	lineNum = 1;
	while (cp < endp) {
		dprintk(KERN_EMERG "{%02x}", *cp); //fflush(stdout);
		if (*cp == '\n') {
			lineStart = 1;
			lineNum++;
		} else if (lineStart) {
			if (*cp == '#') {
				/* comment; just scan to end */
				lineStart = 0;
			} else if (isCharDigit(*cp)) {
				/* looks like a tag; scan it out */
				if (tagNum >= map->numTags) {
					printk(KERN_EMERG
						   "%s: more tags than expected (%d)\n", OUT_TAG, tagNum);
					return -1;
				}
				if (scanTagLine(&cp, &map->tagArray[tagNum], lineNum) != 0)
					return -1;
				tagNum++;
				lineNum++;      // we eat the '\n'
				/* leave lineStart==1 */
			} else if (isCharWhitespace(*cp)) {
				/* looks like leading whitespace; keep scanning */
			} else {
				printk(KERN_EMERG
					   "%s: unexpected chars (0x%02x) in tag number on line %d\n",
					   OUT_TAG, *cp, lineNum);
				return -1;
			}
		} else {
			/* this is a blank or comment line */
		}
		cp++;
	}

	if (tagNum != map->numTags) {
		printk(KERN_EMERG "%s: parsed %d tags, expected %d\n",
			   OUT_TAG, tagNum, map->numTags);
		return -1;
	}

	return 0;
}

static int scanTagLine(char** pData, EventTag* tag, int lineNum)
{
	char* cp = *pData;
	char* startp;
	char* endp;
	unsigned long val;

	startp = cp;
	while (isCharDigit(*++cp))
		;
	*cp = '\0';

	val = simple_strtoul(startp, &endp, 10);
	//assert(endp == cp);
	if (endp != cp){
		printk(KERN_EMERG "ARRRRGH\n");
		return -1;
	}

	tag->tagIndex = val;

	while (*++cp != '\n' && isCharWhitespace(*cp))
		;

	if (*cp == '\n') {
		printk(KERN_EMERG
			   "%s: missing tag string on line %d\n", OUT_TAG, lineNum);
		return -1;
	}

	tag->tagStr = cp;

	while (isCharValidTag(*++cp))
		;

	if (*cp == '\n') {
		/* null terminate and return */
		*cp = '\0';
	} else if (isCharWhitespace(*cp)) {
		/* CRLF or trailin spaces; zap this char, then scan for the '\n' */
		*cp = '\0';

		/* just ignore the rest of the line till \n
		   TODO: read the tag description that follows the tag name
		*/
		while (*++cp != '\n') {
		}
	} else {
		printk(KERN_EMERG
			   "%s: invalid tag chars on line %d\n", OUT_TAG, lineNum);
		return -1;
	}

	*pData = cp;

	dprintk2(KERN_EMERG "+++ Line %d: got %d '%s'\n", lineNum, tag->tagIndex, tag->tagStr);
	return 0;
}

static int compareEventTags(const void* v1, const void* v2)
{
	const EventTag* tag1 = (const EventTag*) v1;
	const EventTag* tag2 = (const EventTag*) v2;

	return tag1->tagIndex - tag2->tagIndex;
}

static int sortTags(EventTagMap* map)
{
	int i;

	sort(map->tagArray, map->numTags, sizeof(EventTag), compareEventTags, NULL);

	for (i = 1; i < map->numTags; i++) {
		if (map->tagArray[i].tagIndex == map->tagArray[i-1].tagIndex) {
			printk(KERN_EMERG "%s: duplicate tag entries (%d:%s and %d:%s)\n",
				   OUT_TAG,
				   map->tagArray[i].tagIndex, map->tagArray[i].tagStr,
				   map->tagArray[i-1].tagIndex, map->tagArray[i-1].tagStr);
			return -1;
		}
	}

	return 0;
}

static int processFile(EventTagMap* map)
{
	/* get a tag count */
	map->numTags = countMapLines(map);
	if (map->numTags < 0) {
		return -1;
	}

	dprintk(KERN_EMERG "+++ found %d tags\n", map->numTags);

	/* allocate storage for the tag index array */
	map->tagArray = kcalloc(1, sizeof(EventTag) * map->numTags, GFP_KERNEL);
	if (map->tagArray == NULL) {
		return -1;
	}

	/* parse the file, null-terminating tag strings */
	if (parseMapLines(map) != 0) {
		printk(KERN_EMERG "%s: file parse failed\n", OUT_TAG);
		kfree(map->tagArray);
		map->tagArray = NULL;
		return -1;
	}

	/* sort the tags and check for duplicates */
	if (sortTags(map) != 0) {
		kfree(map->tagArray);
		map->tagArray = NULL;
		return -1;
	}

	return 0;
}

const char* android_lookupEventTag(const EventTagMap* map, int tag)
{
	int hi, lo, mid;

	lo = 0;
	hi = map->numTags-1;

	while (lo <= hi) {
		int cmp;

		mid = (lo+hi)/2;
		cmp = map->tagArray[mid].tagIndex - tag;
		if (cmp < 0) {
			/* tag is bigger */
			lo = mid + 1;
		} else if (cmp > 0) {
			/* tag is smaller */
			hi = mid - 1;
		} else {
			/* found */
			return map->tagArray[mid].tagStr;
		}
	}

	return NULL;
}

// from system/core/liblog/logprint.c
static int android_log_printBinaryEvent(const unsigned char** pEventData,
										size_t* pEventDataLen, char** pOutBuf, size_t* pOutBufLen)
{
	const unsigned char* eventData = *pEventData;
	size_t eventDataLen = *pEventDataLen;
	char* outBuf = *pOutBuf;
	size_t outBufLen = *pOutBufLen;
	unsigned char type;
	size_t outCount;
	int result = 0;

	if (eventDataLen < 1)
		return -1;
	type = *eventData++;
	eventDataLen--;

	dprintk2(KERN_EMERG "--- type=%d (rem len=%d)\n", type, eventDataLen);

	switch (type) {
	case EVENT_TYPE_INT:
		/* 32-bit signed int */
		{
			int ival;

			if (eventDataLen < 4)
				return -1;
			ival = get4LE(eventData);
			eventData += 4;
			eventDataLen -= 4;

			outCount = snprintf(outBuf, outBufLen, "%d", ival);
			if (outCount < outBufLen) {
				outBuf += outCount;
				outBufLen -= outCount;
			} else {
				/* halt output */
				goto no_room;
			}
		}
		break;
	case EVENT_TYPE_LONG:
		/* 64-bit signed long */
		{
			long long lval;

			if (eventDataLen < 8)
				return -1;
			lval = get8LE(eventData);
			eventData += 8;
			eventDataLen -= 8;

			outCount = snprintf(outBuf, outBufLen, "%lld", lval);
			if (outCount < outBufLen) {
				outBuf += outCount;
				outBufLen -= outCount;
			} else {
				/* halt output */
				goto no_room;
			}
		}
		break;
	case EVENT_TYPE_STRING:
		/* UTF-8 chars, not NULL-terminated */
		{
			unsigned int strLen;

			if (eventDataLen < 4)
				return -1;
			strLen = get4LE(eventData);
			eventData += 4;
			eventDataLen -= 4;

			if (eventDataLen < strLen)
				return -1;

			if (strLen < outBufLen) {
				memcpy(outBuf, eventData, strLen);
				outBuf += strLen;
				outBufLen -= strLen;
			} else if (outBufLen > 0) {
				/* copy what we can */
				memcpy(outBuf, eventData, outBufLen);
				outBuf += outBufLen;
				outBufLen -= outBufLen;
				goto no_room;
			}
			eventData += strLen;
			eventDataLen -= strLen;
			break;
		}
	case EVENT_TYPE_LIST:
		/* N items, all different types */
		{
			unsigned char count;
			int i;

			if (eventDataLen < 1)
				return -1;

			count = *eventData++;
			eventDataLen--;

			if (outBufLen > 0) {
				*outBuf++ = '[';
				outBufLen--;
			} else {
				goto no_room;
			}

			for (i = 0; i < count; i++) {
				result = android_log_printBinaryEvent(&eventData, &eventDataLen,
													  &outBuf, &outBufLen);
				if (result != 0)
					goto bail;

				if (i < count-1) {
					if (outBufLen > 0) {
						*outBuf++ = ',';
						outBufLen--;
					} else {
						goto no_room;
					}
				}
			}

			if (outBufLen > 0) {
				*outBuf++ = ']';
				outBufLen--;
			} else {
				goto no_room;
			}
		}
		break;
	default:
		printk(KERN_EMERG "Unknown binary event type %d\n", type);
		return -1;
	}

 bail:
	*pEventData = eventData;
	*pEventDataLen = eventDataLen;
	*pOutBuf = outBuf;
	*pOutBufLen = outBufLen;
	return result;

 no_room:
	result = 1;
	goto bail;
}

static int crashdump_readbuf(char **ppbuf, char *end, struct logger_entry **pplentry)
{
	size_t copy_len;

	dprintk(KERN_EMERG "(%s:%s:%d) buf, end: 0x%08x, 0x%08x\n", __FILE__, __FUNCTION__, __LINE__, (unsigned int)*ppbuf, (unsigned int)end);

	if (*ppbuf >= end) {
		return false;
	}

	if (entry_rem == -1) {
		dprintk(KERN_EMERG "(%s:%s:%d)\n", __FILE__, __FUNCTION__, __LINE__);
		memcpy(readbuf + readbuf_index, *ppbuf, sizeof(struct logger_entry) - readbuf_index);

		*pplentry = (struct logger_entry *)readbuf;
		copy_len = (*pplentry)->len;

		memcpy(readbuf + sizeof(struct logger_entry), *ppbuf + sizeof(struct logger_entry) - readbuf_index, copy_len);

		*ppbuf += sizeof(struct logger_entry) - readbuf_index + copy_len;
		readbuf_index = 0;
		entry_rem = 0;
	}
	else if (readbuf_index > 0) {
		dprintk(KERN_EMERG "(%s:%s:%d)\n", __FILE__, __FUNCTION__, __LINE__);
		memcpy(readbuf + readbuf_index, *ppbuf, entry_rem);
		*pplentry = (struct logger_entry *)readbuf;

		*ppbuf += entry_rem;
		readbuf_index = 0;
		entry_rem = 0;
	}
	else {

		dprintk(KERN_EMERG "(%s:%s:%d)\n", __FILE__, __FUNCTION__, __LINE__);
		if ( (end - *ppbuf) < sizeof(struct logger_entry) ) {
			dprintk(KERN_EMERG "(%s:%s:%d)\n", __FILE__, __FUNCTION__, __LINE__);
			memcpy(readbuf, *ppbuf, end - *ppbuf);

			readbuf_index = end - *ppbuf;
			entry_rem = -1;

			return false;
		}
		else {

			dprintk(KERN_EMERG "(%s:%s:%d)\n", __FILE__, __FUNCTION__, __LINE__);
			*pplentry = (struct logger_entry *)*ppbuf;

			copy_len = sizeof(struct logger_entry) + (*pplentry)->len;

			if (sizeof(struct logger_entry) + (*pplentry)->len > end - *ppbuf) {
				dprintk(KERN_EMERG "(%s:%s:%d)\n", __FILE__, __FUNCTION__, __LINE__);
				memcpy(readbuf, *ppbuf, end - *ppbuf);

				readbuf_index = end - *ppbuf;
				entry_rem = sizeof(struct logger_entry) + (*pplentry)->len - (end - *ppbuf);
				return false;
			}
			else {
				dprintk(KERN_EMERG "(%s:%s:%d)\n", __FILE__, __FUNCTION__, __LINE__);
				memcpy(readbuf, *ppbuf, copy_len);
				*pplentry = (struct logger_entry *)readbuf;

				*ppbuf += copy_len;
				readbuf_index = 0;
				entry_rem = 0;
			}
		}

	}

	return true;
}

static void crashdump_logcat_do(char *start, char *end)
{
	char *buf;
	struct logger_entry *lentry;
	char priChar;
	char *tag;
	size_t tag_len;
	char *msg;
	size_t msg_len;
	struct timeval tv;
	int second;
	int minute;
	int hour;
	int monthday;
	int month;

	dprintk(KERN_EMERG "(%s:%s:%d) start, end: 0x%08x, 0x%08x\n", __FILE__, __FUNCTION__, __LINE__, (unsigned int)start, (unsigned int)end);

	buf = start;
	while (1) {

		if(!crashdump_readbuf(&buf, end, &lentry)){
			break;
		}

		priChar = filterPriToChar(lentry->msg[0]);

		tag = lentry->msg + 1;

		tv.tv_sec = lentry->sec;
		tv.tv_usec = 0;
		crashdump_timetr(&tv, NULL, &month, &monthday, &hour, &minute, &second, NULL, NULL, NULL);

		crashdump_write_str("%02d-%02d %02d:%02d:%02d.%03ld %5d %5d %c %-8s: ", month, monthday, hour, minute, second, lentry->nsec / 1000000, lentry->pid, lentry->tid, priChar, tag);

		tag_len = strlen(tag);
		msg = tag + tag_len + 1;
		msg_len = lentry->len - tag_len - 3;

		crashdump_write(msg, msg_len);

		crashdump_write_str("\n");
	}

}

static void crashdump_logcat_events_do(char *start, char *end)
{
	char *buf;
	struct logger_entry *lentry;
	char priChar;
	int tagid;
	const char *tag;
	char *msg;
	size_t msg_len;
	size_t inCount;
	const unsigned char* eventData;
	static char messageBuf[1024];
	char *outBuf;
	size_t outRemaining;
	int result;
	struct timeval tv;
	int second;
	int minute;
	int hour;
	int monthday;
	int month;

	dprintk(KERN_EMERG "(%s:%s:%d) start, end: 0x%08x, 0x%08x\n", __FILE__, __FUNCTION__, __LINE__, (unsigned int)start, (unsigned int)end);

	priChar = 'I';

	buf = start;
	while (1) {

		if(!crashdump_readbuf(&buf, end, &lentry)){
			break;
		}


		tagid = get4LE(lentry->msg);
		tag = NULL;

		if (map != NULL) {
			tag = android_lookupEventTag(map, tagid);
		}

		tv.tv_sec = lentry->sec;
		tv.tv_usec = 0;
		crashdump_timetr(&tv, NULL, &month, &monthday, &hour, &minute, &second, NULL, NULL, NULL);

		if (tag) {
			crashdump_write_str("%02d-%02d %02d:%02d:%02d.%03ld %5d %5d %c %-8s: ", month, monthday, hour, minute, second, lentry->nsec / 1000000, lentry->pid, lentry->tid, priChar, tag);
		}
		else {
			crashdump_write_str("%02d-%02d %02d:%02d:%02d.%03ld %5d %5d %c %-8d: ", month, monthday, hour, minute, second, lentry->nsec / 1000000, lentry->pid, lentry->tid, priChar, tagid);
		}

		eventData = lentry->msg + 4;
		inCount = lentry->len - 4;

		outBuf = messageBuf;
		outRemaining = sizeof(messageBuf)-1;      /* leave one for nul byte */
		result = android_log_printBinaryEvent(&eventData, &inCount, &outBuf,
											  &outRemaining);
		if (result < 0) {
			printk(KERN_EMERG "Binary log entry conversion failed\n");
			crashdump_write_str("\n");
			continue;
		} else if (result == 1) {
			if (outBuf > messageBuf) {
				/* leave an indicator */
				*(outBuf-1) = '!';
			} else {
				/* no room to output anything at all */
				*outBuf++ = '!';
				outRemaining--;
			}
			/* pretend we ate all the data */
			inCount = 0;
		}

		/* eat the silly terminating '\n' */
		if (inCount == 1 && *eventData == '\n') {
			eventData++;
			inCount--;
		}

		if (inCount != 0) {
			printk(KERN_EMERG
				   "Warning: leftover binary log data (%d bytes)\n", inCount);
		}

		/*
		 * Terminate the buffer.  The NUL byte does not count as part of
		 * entry->messageLen.
		 */
		*outBuf = '\0';

		msg = messageBuf;
		msg_len = outBuf - messageBuf;

		crashdump_write_str(msg);

		crashdump_write_str("\n");
	}

}

void crashdump_logcat(int type)
{
	struct logger_log *log;
	void (*dofunc)(char *start, char *end);
	size_t maplen;

#ifdef CONFIG_CRASHDUMP_TO_FLASH
	printk(KERN_EMERG "%s:%d\n", __FUNCTION__, type);
#endif

	dprintk(KERN_EMERG "(%s:%s:%d)\n", __FILE__, __FUNCTION__, __LINE__);
	crashdump_logcat_register();

	switch (type) {
	case LOGCAT_MAIN:
		crashdump_write_str("\n=== logcat main ===\n");
		log = cctr.lctr.log_main;
		dofunc = crashdump_logcat_do;
		break;
	case LOGCAT_EVENTS:
		crashdump_write_str("\n=== logcat events ===\n");
		log = cctr.lctr.log_events;
		dofunc = crashdump_logcat_events_do;
		break;
	case LOGCAT_RADIO:
		crashdump_write_str("\n=== logcat radio ===\n");
		log = cctr.lctr.log_radio;
		dofunc = crashdump_logcat_do;
		break;
	case LOGCAT_SYSTEM:
		crashdump_write_str("\n=== logcat system ===\n");
		log = cctr.lctr.log_system;
		dofunc = crashdump_logcat_do;
		break;
	default:
		goto exit;
	}

	dprintk(KERN_EMERG "(%s:%s:%d) log->buffer/log->size/log->head/log->w_off = 0x%08x/0x%08x/0x%08x/0x%08x\n", __FILE__, __FUNCTION__, __LINE__, (unsigned int)log->buffer, (unsigned int)log->size, (unsigned int)log->head, (unsigned int)log->w_off);

	if (type == LOGCAT_EVENTS) {
		map = kcalloc(1, sizeof(EventTagMap), GFP_KERNEL);
		if (map == NULL) {
			goto call_dofunc;
		}

		maplen = crashdump_read_file(EVENT_TAG_MAP_FILE, (void *)mapbuf, sizeof(mapbuf));
		if (maplen <= 0) {
			kfree(map);
			map = NULL;
			goto call_dofunc;
		}

		map->mapAddr = (void *)mapbuf;
		map->mapLen = maplen;

		if (processFile(map) != 0) {
			kfree(map);
			map = NULL;
		}
	}

	// If failed to read/process EVENT_TAG_MAP_FILE, make sure to clear map ptr at this point.
	// Because map ptr is used to check the result of reading/processing EVENT_TAG_MAP_FILE.

 call_dofunc:
	readbuf_index = 0;
	entry_rem = 0;

	if (log->head == 0) {
		dofunc(log->buffer, log->buffer + log->w_off);
	}
	else if (log->head <= log->w_off) {
		dofunc(log->buffer + log->head, log->buffer + log->w_off);
	}
	else {
		dofunc(log->buffer + log->head, log->buffer + log->size);
		dofunc(log->buffer, log->buffer + log->w_off);
	}

	if (type == LOGCAT_EVENTS && map && map->tagArray) {
		kfree(map->tagArray);
		kfree(map);
		map = NULL;
	}

 exit:
	return;
}
