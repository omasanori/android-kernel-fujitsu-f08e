/*----------------------------------------------------------------------------*/
// COPYRIGHT(C) FUJITSU LIMITED 2013
/*----------------------------------------------------------------------------*/
#ifndef _MAX9867CTRL_H_
#define _MAX9867CTRL_H_

void max9867start(int conf_num);
void max9867stop(void);
void max9867debug(void);

/* FUJITSU:2013-04-04 ADD start */
void max9867vddon(void);
/* FUJITSU:2013-04-04 ADD end */

#endif
