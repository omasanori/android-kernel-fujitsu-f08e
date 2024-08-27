/*
 * Copyright(C) 2010-2013 FUJITSU LIMITED
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
#include <linux/ptrace.h>
#include <crypto/sha.h>
#include "fjsec_modinfo.h"
#include "../../fjfeat/FjFeatMac.h"
#include <linux/kdev_t.h>
#include <linux/spinlock.h>

//#define DEBUG_COPY_GUARD
//#define DEBUG_ACCESS_CONTROL
//#define DEBUG_KITTING_ACCESS_CONTROL
//#define DEBUG_SETID_ACCESS_CONTROL
//#define DEBUG_MAPPING

#define AID_ROOT              0  /* traditional unix root user */
#define AID_SYSTEM         1000  /* system server */
#define AID_AUDIO          1005  /* audio devices */
#define AID_MEDIA          1013
#define AID_PREINST_FELC   5003
#define PRNAME_NO_CHECK 0
#define UID_NO_CHECK -1
#define NO_CHECK_STR "no_check"
#ifdef DEBUG_COPY_GUARD
#define AID_LOG           1007  /* log devices */
#endif

#define JAVA_APP_PROCESS_PATH "/system/bin/app_process"

#define SECURITY_FJSEC_RECOVERY_PROCESS_NAME "recovery"
#define SECURITY_FJSEC_RECOVERY_PROCESS_PATH "/sbin/recovery"

#define S2B(x) ((loff_t)x * SECTOR_SIZE)
#define ADDR2PFN(x) (x >> PAGE_SHIFT)

#define INIT_PROCESS_NAME "init"
#define INIT_PROCESS_PATH "/init"

#define BOOT_ARGS_MODE_FOTA "fotamode"
#define BOOT_ARGS_MODE_SDDOWNLOADER "sddownloadermode"
//#define BOOT_ARGS_MODE_RECOVERY "recovery"
#define BOOT_ARGS_MODE_MAKERCMD "makermode"
#define BOOT_ARGS_MODE_OSUPDATE "osupdatemode"
#define BOOT_ARGS_MODE_RECOVERYMENU "recoverymenu"
#define BOOT_ARGS_MODE_KERNEL "kernelmode"
#define BOOT_ARGS_MODE_MASTERCLEAR "masterclear"

#define OFFSET_BYTE_LIMIT LLONG_MAX

#define FOTA_PTN_PATH "/dev/block/mmcblk0p34" /* fota */
#define FOTA_MOUNT_POINT "/fota/" /* fota */
#define FOTA_DIRS_PATH FOTA_MOUNT_POINT "*" /* fota */

#define FOTA2_PTN_PATH "/dev/block/mmcblk0p35" /* fota2 */
#define FOTA2_DIRS_PATH "/fota2/*" /* fota */

#ifdef DEBUG_ACCESS_CONTROL
#define TEST_PTN_PATH "/dev/block/mmcblk0p34"
#define TEST_MOUNT_POINT "/fota2/"
#define TEST_DIRS_PATH TEST_MOUNT_POINT "*"
#endif

enum boot_mode_type {
	BOOT_MODE_NONE = 0,
	BOOT_MODE_FOTA = 1,
	BOOT_MODE_SDDOWNLOADER = 2,
//	BOOT_MODE_RECOVERY = 3,
	BOOT_MODE_MASTERCLEAR = 3,	
	BOOT_MODE_MAKERCMD = 4,
	BOOT_MODE_OSUPDATE = 5,
};

#define ROOT_DIR "/"

enum result_mount_point {
	MATCH_SYSTEM_MOUNT_POINT,
#ifdef CONFIG_SECURITY_FJSEC_AC_SECURE_STORAGE
	MATCH_SECURE_STORAGE_MOUNT_POINT,
#endif /* CONFIG_SECURITY_FJSEC_AC_SECURE_STORAGE */
#ifdef CONFIG_SECURITY_FJSEC_AC_KITTING
	MATCH_KITTING_MOUNT_POINT,
#endif /* CONFIG_SECURITY_FJSEC_AC_KITTING */
	MATCH_MOUNT_POINT,
	MATCH_ROOT_MOUNT_POINT,
	MISMATCH_MOUNT_POINT,
	UNRELATED_MOUNT_POINT
};

/**
 * mapping control
 */
spinlock_t page_module_list_lock;
#define MAX_LOW_PFN (0x2FA00)
#define MAPPING_LIST_SIZE (MAX_LOW_PFN >> 3)
#define MAPPING_LIST_PAGE (ALIGN(MAPPING_LIST_SIZE, PAGE_SIZE) >> PAGE_SHIFT)
char *page_free_list;
char *page_write_list;
char *page_exec_list;
char *page_module_list;

#define FJSEC_LAOD 0
#define FJSEC_DELETE 1

/**
 * Limitation of device file.
 */
struct fs_path_config {
	mode_t		mode;
	uid_t		uid;
	gid_t		gid;
	dev_t		rdev;
	const char	*prefix;
};

static struct fs_path_config devs[] = {
	{ (S_IRUSR | S_IWUSR | S_IFBLK), AID_ROOT, AID_ROOT, MKDEV(179, 0), CONFIG_SECURITY_FJSEC_DISK_DEV_PATH },

#ifdef CONFIG_SECURITY_FJSEC_AC_SECURE_STORAGE
	{ (S_IRUSR | S_IWUSR | S_IFBLK), AID_ROOT, AID_ROOT, MKDEV(179, 21), CONFIG_SECURITY_FJSEC_SECURE_STORAGE_DEV_PATH },
#endif /* CONFIG_SECURITY_FJSEC_AC_SECURE_STORAGE */

#if (FJFEAT_PRODUCT == FJFEAT_PRODUCT_FJDEV005)
	{ (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IFCHR), AID_ROOT, AID_PREINST_FELC, MKDEV(243, 3), CONFIG_SECURITY_FJSEC_FELICA_DEV_PATH },
#else
	{ (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH |S_IFCHR), AID_ROOT, AID_ROOT, MKDEV(243, 3), CONFIG_SECURITY_FJSEC_FELICA_DEV_PATH },
#endif

	{ (S_IRUSR | S_IWUSR | S_IFBLK), AID_ROOT, AID_ROOT, MKDEV(179, 1), "/dev/block/mmcblk0p1" }, /* modem */
	{ (S_IRUSR | S_IWUSR | S_IFBLK), AID_ROOT, AID_ROOT, MKDEV(179, 2), "/dev/block/mmcblk0p2" }, /* sbl1 */
	{ (S_IRUSR | S_IWUSR | S_IFBLK), AID_ROOT, AID_ROOT, MKDEV(179, 3), "/dev/block/mmcblk0p3" }, /* sbl2 */
	{ (S_IRUSR | S_IWUSR | S_IFBLK), AID_ROOT, AID_ROOT, MKDEV(179, 4), "/dev/block/mmcblk0p4" }, /* sbl3 */
	{ (S_IRUSR | S_IWUSR | S_IFBLK), AID_ROOT, AID_ROOT, MKDEV(179, 5), "/dev/block/mmcblk0p5" }, /* aboot */
	{ (S_IRUSR | S_IWUSR | S_IFBLK), AID_ROOT, AID_ROOT, MKDEV(179, 6), "/dev/block/mmcblk0p6" }, /* rpm */
	{ (S_IRUSR | S_IWUSR | S_IFBLK), AID_ROOT, AID_ROOT, MKDEV(179, 7), "/dev/block/mmcblk0p7" }, /* boot */
	{ (S_IRUSR | S_IWUSR | S_IFBLK), AID_ROOT, AID_ROOT, MKDEV(179, 8), "/dev/block/mmcblk0p8" }, /* tz */
	{ (S_IRUSR | S_IWUSR | S_IFBLK), AID_ROOT, AID_ROOT, MKDEV(179, 9), "/dev/block/mmcblk0p9" }, /* pad */
	{ (S_IRUSR | S_IWUSR | S_IFBLK), AID_ROOT, AID_ROOT, MKDEV(179, 10), "/dev/block/mmcblk0p10" }, /* modemst1 */
	{ (S_IRUSR | S_IWUSR | S_IFBLK), AID_ROOT, AID_ROOT, MKDEV(179, 11), "/dev/block/mmcblk0p11" }, /* modemst2 */
	{ (S_IRUSR | S_IWUSR | S_IFBLK), AID_ROOT, AID_ROOT, MKDEV(179, 12), "/dev/block/mmcblk0p12" }, /* m9kefs1 */
	{ (S_IRUSR | S_IWUSR | S_IFBLK), AID_ROOT, AID_ROOT, MKDEV(179, 13), "/dev/block/mmcblk0p13" }, /* m9kefs2 */
	{ (S_IRUSR | S_IWUSR | S_IFBLK), AID_ROOT, AID_ROOT, MKDEV(179, 14), "/dev/block/mmcblk0p14" }, /* m9kefs3 */
	{ (S_IRUSR | S_IWUSR | S_IFBLK), AID_ROOT, AID_ROOT, MKDEV(179, 15), "/dev/block/mmcblk0p15" }, /* m9kefsc */
	{ (S_IRUSR | S_IWUSR | S_IFBLK), AID_ROOT, AID_ROOT, MKDEV(179, 16), "/dev/block/mmcblk0p16" }, /* appsst1 */
	{ (S_IRUSR | S_IWUSR | S_IFBLK), AID_ROOT, AID_ROOT, MKDEV(179, 17), "/dev/block/mmcblk0p17" }, /* appsst2 */
	{ (S_IRUSR | S_IWUSR | S_IFBLK), AID_ROOT, AID_ROOT, MKDEV(179, 18), "/dev/block/mmcblk0p18" }, /* abost */
	{ (S_IRUSR | S_IWUSR | S_IFBLK), AID_ROOT, AID_ROOT, MKDEV(179, 19), "/dev/block/mmcblk0p19" }, /* pad2 */
	{ (S_IRUSR | S_IWUSR | S_IFBLK), AID_ROOT, AID_ROOT, MKDEV(179, 20), "/dev/block/mmcblk0p20" }, /* appsst3 */
	//{ (S_IRUSR | S_IWUSR | S_IFBLK), AID_ROOT, AID_ROOT, MKDEV(179, 21), "/dev/block/mmcblk0p21" }, /* sst */
	{ (S_IRUSR | S_IWUSR | S_IFBLK), AID_ROOT, AID_ROOT, MKDEV(179, 22), "/dev/block/mmcblk0p22" }, /* sst2 */
	{ (S_IRUSR | S_IWUSR | S_IFBLK), AID_ROOT, AID_ROOT, MKDEV(179, 23), "/dev/block/mmcblk0p23" }, /* system */
	{ (S_IRUSR | S_IWUSR | S_IFBLK), AID_ROOT, AID_ROOT, MKDEV(179, 24), "/dev/block/mmcblk0p24" }, /* persist */
	{ (S_IRUSR | S_IWUSR | S_IFBLK), AID_ROOT, AID_ROOT, MKDEV(179, 25), "/dev/block/mmcblk0p25" }, /* cache */
	{ (S_IRUSR | S_IWUSR | S_IFBLK), AID_ROOT, AID_ROOT, MKDEV(179, 26), "/dev/block/mmcblk0p26" }, /* misc */
	{ (S_IRUSR | S_IWUSR | S_IFBLK), AID_ROOT, AID_ROOT, MKDEV(179, 27), "/dev/block/mmcblk0p27" }, /* recovery */
	{ (S_IRUSR | S_IWUSR | S_IFBLK), AID_ROOT, AID_ROOT, MKDEV(179, 28), "/dev/block/mmcblk0p28" }, /* DDR */
	{ (S_IRUSR | S_IWUSR | S_IFBLK), AID_ROOT, AID_ROOT, MKDEV(179, 29), "/dev/block/mmcblk0p29" }, /* syschk */
	{ (S_IRUSR | S_IWUSR | S_IFBLK), AID_ROOT, AID_ROOT, MKDEV(179, 30), "/dev/block/mmcblk0p30" }, /* persist2 */
	{ (S_IRUSR | S_IWUSR | S_IFBLK), AID_ROOT, AID_ROOT, MKDEV(179, 31), "/dev/block/mmcblk0p31" }, /* persist3 */
	{ (S_IRUSR | S_IWUSR | S_IRGRP | S_IFBLK), AID_ROOT, AID_SYSTEM, MKDEV(259, 0), "/dev/block/mmcblk0p32" }, /* kernellog */
	{ (S_IRUSR | S_IWUSR | S_IRGRP | S_IFBLK), AID_ROOT, AID_SYSTEM, MKDEV(259, 1), "/dev/block/mmcblk0p33" }, /* loaderlog */
	{ (S_IRUSR | S_IWUSR | S_IFBLK), AID_ROOT, AID_ROOT, MKDEV(259, 2), "/dev/block/mmcblk0p34" }, /* fota */
	{ (S_IRUSR | S_IWUSR | S_IFBLK), AID_ROOT, AID_ROOT, MKDEV(259, 3), "/dev/block/mmcblk0p35" }, /* fota2 */
#if (FJFEAT_PRODUCT == FJFEAT_PRODUCT_FJDEV005)
	{ (S_IRUSR | S_IWUSR | S_IFBLK), AID_ROOT, AID_ROOT, MKDEV(259, 4), "/dev/block/mmcblk0p36" }, /* kitting */
	{ (S_IRUSR | S_IWUSR | S_IFBLK), AID_ROOT, AID_ROOT, MKDEV(259, 5), "/dev/block/mmcblk0p37" }, /* userdata */
	{ (S_IRUSR | S_IWUSR | S_IFBLK), AID_ROOT, AID_ROOT, MKDEV(259, 6), "/dev/block/mmcblk0p38" }, /* grow */
#else
	{ (S_IRUSR | S_IWUSR | S_IFBLK), AID_ROOT, AID_ROOT, MKDEV(259, 4), "/dev/block/mmcblk0p36" }, /* dic */
	{ (S_IRUSR | S_IWUSR | S_IFBLK), AID_ROOT, AID_ROOT, MKDEV(259, 5), "/dev/block/mmcblk0p37" }, /* cnt */
	{ (S_IRUSR | S_IWUSR | S_IFBLK), AID_ROOT, AID_ROOT, MKDEV(259, 6), "/dev/block/mmcblk0p38" }, /* mmdata */
	{ (S_IRUSR | S_IWUSR | S_IFBLK), AID_ROOT, AID_ROOT, MKDEV(259, 7), "/dev/block/mmcblk0p39" }, /* kitting */
	{ (S_IRUSR | S_IWUSR | S_IFBLK), AID_ROOT, AID_ROOT, MKDEV(259, 8), "/dev/block/mmcblk0p40" }, /* userdata */
	{ (S_IRUSR | S_IWUSR | S_IFBLK), AID_ROOT, AID_ROOT, MKDEV(259, 9), "/dev/block/mmcblk0p41" }, /* grow */
#endif
	{ 0, 0, 0, 0, 0 },
};

/**
 * add module list.
 */
struct module_list {
	const char *name;
	char checksum[SHA256_DIGEST_SIZE];
};

static const struct module_list modlist[] = {
	{ "stb6000", CHECKSUM_STB6000 },
	{ "m88rs2000", CHECKSUM_M88RS2000 },
	{ "dib7000m", CHECKSUM_DIB7000M },
	{ "atbm8830", CHECKSUM_ATBM8830 },
	{ "control_trace", CHECKSUM_CONTROL_TRACE },
	{ "dib9000", CHECKSUM_DIB9000 },
	{ "stv6110", CHECKSUM_STV6110 },
	{ "stv090x", CHECKSUM_STV090X },
	{ "stv0367", CHECKSUM_STV0367 },
	{ "tda18271c2dd", CHECKSUM_TDA18271C2DD },
	{ "dma_test", CHECKSUM_DMA_TEST },
	{ "qcedev", CHECKSUM_QCEDEV },
	{ "zl10353", CHECKSUM_ZL10353 },
	{ "zl10036", CHECKSUM_ZL10036 },
	{ "dib0070", CHECKSUM_DIB0070 },
	{ "ds3000", CHECKSUM_DS3000 },
	{ "dib3000mb", CHECKSUM_DIB3000MB },
	{ "test_iosched", CHECKSUM_TEST_IOSCHED },
	{ "tda10023", CHECKSUM_TDA10023 },
	{ "ec100", CHECKSUM_EC100 },
	{ "tua6100", CHECKSUM_TUA6100 },
	{ "s5h1432", CHECKSUM_S5H1432 },
	{ "spidev", CHECKSUM_SPIDEV },
	{ "scsi_wait_scan", CHECKSUM_SCSI_WAIT_SCAN },
	{ "l64781", CHECKSUM_L64781 },
	{ "tda10048", CHECKSUM_TDA10048 },
	{ "mcdrvmodule", CHECKSUM_MCDRVMODULE },
	{ "evbug", CHECKSUM_EVBUG },
	{ "or51211", CHECKSUM_OR51211 },
	{ "af9013", CHECKSUM_AF9013 },
	{ "s5h1411", CHECKSUM_S5H1411 },
	{ "texfat", CHECKSUM_TEXFAT },
	{ "stb0899", CHECKSUM_STB0899 },
	{ "mmc_test", CHECKSUM_MMC_TEST },
	{ "itd1000", CHECKSUM_ITD1000 },
	{ "ves1820", CHECKSUM_VES1820 },
	{ "tda10071", CHECKSUM_TDA10071 },
	{ "stv0288", CHECKSUM_STV0288 },
	{ "mt312", CHECKSUM_MT312 },
	{ "mckernelapi", CHECKSUM_MCKERNELAPI },
	{ "cx22702", CHECKSUM_CX22702 },
	{ "rtl2830", CHECKSUM_RTL2830 },
	{ "qcrypto", CHECKSUM_QCRYPTO },
	{ "cx24113", CHECKSUM_CX24113 },
	{ "lnbp22", CHECKSUM_LNBP22 },
	{ "isl6421", CHECKSUM_ISL6421 },
	{ "cx22700", CHECKSUM_CX22700 },
	{ "cx24110", CHECKSUM_CX24110 },
	{ "it913x_fe", CHECKSUM_IT913X_FE },
	{ "isl6405", CHECKSUM_ISL6405 },
	{ "sp8870", CHECKSUM_SP8870 },
	{ "ves1x93", CHECKSUM_VES1X93 },
	{ "au8522", CHECKSUM_AU8522 },
	{ "adsprpc", CHECKSUM_ADSPRPC },
	{ "tda1004x", CHECKSUM_TDA1004X },
	{ "mt352", CHECKSUM_MT352 },
	{ "wlan", CHECKSUM_PRIMA_WLAN },
	{ "cx24116", CHECKSUM_CX24116 },
	{ "bcm3510", CHECKSUM_BCM3510 },
	{ "eeprom_93cx6", CHECKSUM_EEPROM_93CX6 },
	{ "reset_modem", CHECKSUM_RESET_MODEM },
	{ "or51132", CHECKSUM_OR51132 },
	{ "s5h1409", CHECKSUM_S5H1409 },
	{ "lgdt330x", CHECKSUM_LGDT330X },
	{ "tda665x", CHECKSUM_TDA665X },
	{ "mb86a16", CHECKSUM_MB86A16 },
	{ "stv0299", CHECKSUM_STV0299 },
	{ "drxk", CHECKSUM_DRXK },
	{ "lgs8gxx", CHECKSUM_LGS8GXX },
	{ "cxd2820r", CHECKSUM_CXD2820R },
	{ "sp887x", CHECKSUM_SP887X },
	{ "tda8083", CHECKSUM_TDA8083 },
	{ "stv0900", CHECKSUM_STV0900 },
	{ "stv6110x", CHECKSUM_STV6110X },
	{ "stb6100", CHECKSUM_STB6100 },
	{ "cx24123", CHECKSUM_CX24123 },
	{ "s5h1420", CHECKSUM_S5H1420 },
	{ "ks8851", CHECKSUM_KS8851 },
	{ "radio_iris_transport", CHECKSUM_RADIO_IRIS_TRANSPORT },
	{ "tda826x", CHECKSUM_TDA826X },
	{ "isl6423", CHECKSUM_ISL6423 },
	{ "dib7000p", CHECKSUM_DIB7000P },
	{ "qce40", CHECKSUM_QCE40 },
	{ "mb86a20s", CHECKSUM_MB86A20S },
	{ "tda10021", CHECKSUM_TDA10021 },
	{ "zl10039", CHECKSUM_ZL10039 },
	{ "stv0297", CHECKSUM_STV0297 },
	{ "si21xx", CHECKSUM_SI21XX },
	{ "nxt200x", CHECKSUM_NXT200X },
	{ "lgs8gl5", CHECKSUM_LGS8GL5 },
	{ "lgdt3305", CHECKSUM_LGDT3305 },
	{ "dvb_pll", CHECKSUM_DVB_PLL },
	{ "nxt6000", CHECKSUM_NXT6000 },
	{ "s921", CHECKSUM_S921 },
	{ "ix2505v", CHECKSUM_IX2505V },
	{ "tda8261", CHECKSUM_TDA8261 },
	{ "dibx000_common", CHECKSUM_DIBX000_COMMON },
	{ "dib0090", CHECKSUM_DIB0090 },
	{ "drxd", CHECKSUM_DRXD },
	{ "tda10086", CHECKSUM_TDA10086 },
	{ "ansi_cprng", CHECKSUM_ANSI_CPRNG },
	{ "lcd", CHECKSUM_LCD },
	{ "dib8000", CHECKSUM_DIB8000 },
	{ "dib3000mc", CHECKSUM_DIB3000MC },
	{ "msm_buspm_dev", CHECKSUM_MSM_BUSPM_DEV },
	{ "a8293", CHECKSUM_A8293 },
	{ "lnbp21", CHECKSUM_LNBP21 },
	{ "gspca_main", CHECKSUM_GSPCA_MAIN },
	{ "hd29l2", CHECKSUM_HD29L2 },
#ifdef DEBUG_MAPPING
	{ "remap_pfn_range_mapping", CHECKSUM_REMAP_PFN_RANGE_MAPPING },
	{ "ioremap_page_range_mapping", CHECKSUM_IOREMAP_PAGE_RANGE_MAPPING },
	{ "insert_page_mapping", CHECKSUM_INSERT_PAGE_MAPPING },
	{ "insert_pfn_mapping", CHECKSUM_INSERT_PFN_MAPPING },
	{ "insert_page_fault", CHECKSUM_INSERT_PAGE_FAULT},
	{ "insert_pfn_fault", CHECKSUM_INSERT_PFN_FAULT },
#endif //DEBUG_MAPPING
	{ 0, { 0 } },
};

struct accessible_area_disk_dev {
	loff_t head;
	loff_t tail;
};

static struct accessible_area_disk_dev accessible_areas_fota[] = {
//#if (FJFEAT_PRODUCT == FJFEAT_PRODUCT_FJDEV001 || FJFEAT_PRODUCT == FJFEAT_PRODUCT_FJDEV002)
#if (FJFEAT_PRODUCT == FJFEAT_PRODUCT_FJDEV001 || FJFEAT_PRODUCT == FJFEAT_PRODUCT_FJDEV002 || FJFEAT_PRODUCT == FJFEAT_PRODUCT_FJDEV005)
	{ S2B(0x00000000), OFFSET_BYTE_LIMIT },
#endif
	{ 0,          0 },
};

static struct accessible_area_disk_dev accessible_areas_sddownloader[] = {
	{ 0,          0 },
};

static struct accessible_area_disk_dev accessible_areas_osupdate[] = {
	{ 0,          0 },
};

static struct accessible_area_disk_dev accessible_areas_recovery[] = {
	{ 0,          0 },
};

static struct accessible_area_disk_dev accessible_areas_maker[] = {
	{ 0,          0 },
};

struct access_control_mmcdl_device {
	enum boot_mode_type boot_mode;
	char *process_name;
	char *process_path;
};

static struct access_control_mmcdl_device mmcdl_device_list[] = {
	{ BOOT_MODE_SDDOWNLOADER, CONFIG_SECURITY_FJSEC_SDDOWNLOADER_MODE_ACCESS_PROCESS_NAME, CONFIG_SECURITY_FJSEC_SDDOWNLOADER_MODE_ACCESS_PROCESS_PATH },
	{ BOOT_MODE_OSUPDATE, CONFIG_SECURITY_FJSEC_OSUPDATE_MODE_ACCESS_PROCESS_NAME, CONFIG_SECURITY_FJSEC_OSUPDATE_MODE_ACCESS_PROCESS_PATH },
	{ BOOT_MODE_MAKERCMD, CONFIG_SECURITY_FJSEC_MAKERCMD_MODE_ACCESS_PROCESS_NAME, CONFIG_SECURITY_FJSEC_MAKERCMD_MODE_ACCESS_PROCESS_PATH },
	{ 0, 0, 0 },
};

static struct access_control_mmcdl_device mkdrv_device_list[] = {
	{ BOOT_MODE_MAKERCMD, CONFIG_SECURITY_FJSEC_MAKERCMD_MODE_ACCESS_PROCESS_NAME, CONFIG_SECURITY_FJSEC_MAKERCMD_MODE_ACCESS_PROCESS_PATH },
	{ 0, 0, 0 },
};

static long ptrace_read_request_policy[] = {
	PTRACE_TRACEME,
	PTRACE_PEEKTEXT,
	PTRACE_PEEKDATA,
	PTRACE_PEEKUSR,
	PTRACE_CONT,
	PTRACE_GETREGS,
	PTRACE_GETFPREGS,
	PTRACE_ATTACH,
	PTRACE_DETACH,
	PTRACE_GETWMMXREGS,
	PTRACE_GET_THREAD_AREA,
	PTRACE_GETCRUNCHREGS,
	PTRACE_GETVFPREGS,
	PTRACE_GETHBPREGS,
	PTRACE_GETEVENTMSG,
	PTRACE_GETSIGINFO,
	PTRACE_GETREGSET,
	PTRACE_SEIZE,
	PTRACE_INTERRUPT,
	PTRACE_LISTEN,
};

struct read_write_access_control_process {
	char *process_name;
	char *process_path;
	uid_t uid;
};

static struct read_write_access_control_process vdsp_process_list[] = {
	{ "wild", "/system/bin/wild", UID_NO_CHECK },
	{ "ueventd", "/init", UID_NO_CHECK },
	{ 0, 0, 0 },
};

static struct read_write_access_control_process casdrm_process_list_lib[] = {
	{ "jp.co.mmbi.app", JAVA_APP_PROCESS_PATH, UID_NO_CHECK },
	{ "jp.co.mmbi.bookviewer", JAVA_APP_PROCESS_PATH, UID_NO_CHECK },
	{ "com.nttdocomo.mmb.android.mmbsv.process", JAVA_APP_PROCESS_PATH, UID_NO_CHECK },
	{ "jp.pixela.stationtv.localtuner.full.app", JAVA_APP_PROCESS_PATH, AID_MEDIA },
	{ "MmbCaCasDrmMw", "/system/bin/MmbCaCasDrmMw", AID_ROOT },
	{ "MmbFcCtlMw", "/system/bin/MmbFcCtlMw", AID_ROOT },
	{ "MmbFcLiceMwServ", "/system/bin/MmbFcLiceMwServer", AID_ROOT }, /* "MmbFcLiceMwServ" is first 15 characters of "MmbFcLiceMwServer" */
	{ "MmbStCtlMwServi", "/system/bin/MmbStCtlMwService", AID_ROOT }, /* "MmbStCtlMwServi" is first 15 characters of "MmbStCtlMwService" */
	{ "MmbFcMp4MwServe", "/system/bin/MmbFcMp4MwServer", AID_ROOT }, /* "MmbFcMp4MwServe" is first 15 characters of "MmbFcMp4MwServer" */
	{ "MmbStRecCmMwSer", "/system/bin/MmbStRecCmMwService", AID_ROOT }, /* "MmbStRecCmMwSer" is first 15 characters of "MmbFcMp4MwServer" */
	{ "fmt_dtv_conflic", "/system/bin/fmt_dtv_conflict", AID_ROOT },
	{ "ficsd", "/system/bin/ficsd", AID_SYSTEM },
	{ "debuggerd", "/system/bin/debuggerd", AID_ROOT },
	{ "installd", "/system/bin/installd", AID_ROOT },
    { 0, 0, 0 },
};

static struct read_write_access_control_process casdrm_process_list_bin[] = {
	{ INIT_PROCESS_NAME, INIT_PROCESS_PATH, AID_ROOT },
	{ "ficsd", "/system/bin/ficsd", AID_SYSTEM },
	{ 0, 0, 0 },
};

#ifdef DEBUG_COPY_GUARD
static struct read_write_access_control_process test_process_list[] = {
	{ "com.fujitsu.FjsecTest", JAVA_APP_PROCESS_PATH, AID_LOG },
	{ "ok_native", "/data/lsm-test/tmp/regza1", AID_AUDIO },
	{ PRNAME_NO_CHECK, "/data/lsm-test/tmp/regza2", AID_AUDIO },
	{ "ok_native", "/data/lsm-test/tmp/regza3", UID_NO_CHECK },
	{ 0, 0, 0 },
};
#endif

struct read_write_access_control {
	char *prefix;
	struct read_write_access_control_process *process_list;
};

static struct read_write_access_control rw_access_control_list[] = {
	{ "/system/etc/firmware/vw.bin", vdsp_process_list },
	{ "/system/etc/firmware/vw_th.bin", vdsp_process_list },
	{ "/system/etc/firmware/vp_vw.bin", vdsp_process_list },
	{ "/system/etc/firmware/vp_vw_th.bin", vdsp_process_list },
	{ "/system/etc/firmware/2b.bin", vdsp_process_list },
	{ "/system/lib/libMmbCaCasDrmMw.so", casdrm_process_list_lib },
	{ "/system/lib/libMmbCaKyMngMw.so", casdrm_process_list_lib },
	{ "/system/bin/MmbCaCasDrmMw", casdrm_process_list_bin },
#ifdef DEBUG_COPY_GUARD
	{ "/system/lib/libptct.so", test_process_list },
	{ "/data/lsm-test/tmp/ptct/*", test_process_list },
#endif
	{ 0, 0 },
};

struct ac_config {
	char *prefix;
	enum boot_mode_type boot_mode;
	char *process_name;
	char *process_path;
};

static struct ac_config ptn_acl[] = {
//#if (FJFEAT_PRODUCT == FJFEAT_PRODUCT_FJDEV001 || FJFEAT_PRODUCT == FJFEAT_PRODUCT_FJDEV002)
#if (FJFEAT_PRODUCT == FJFEAT_PRODUCT_FJDEV001 || FJFEAT_PRODUCT == FJFEAT_PRODUCT_FJDEV002 || FJFEAT_PRODUCT == FJFEAT_PRODUCT_FJDEV005)
	{ "/dev/block/mmcblk0p1", 0, 0, 0 }, /* modem */
	{ "/dev/block/mmcblk0p2", 0, 0, 0 }, /* sbl1 */
	{ "/dev/block/mmcblk0p3", 0, 0, 0 }, /* sbl2 */
	{ "/dev/block/mmcblk0p4", 0, 0, 0 }, /* sbl3 */
	{ "/dev/block/mmcblk0p5", 0, 0, 0 }, /* aboot */
	{ "/dev/block/mmcblk0p6", 0, 0, 0 }, /* rpm */
	{ "/dev/block/mmcblk0p7", 0, 0, 0 }, /* boot */
	{ "/dev/block/mmcblk0p8", 0, 0, 0 }, /* tz */
	{ "/dev/block/mmcblk0p9", 0, 0, 0 }, /* pad */
	{ "/dev/block/mmcblk0p10", BOOT_MODE_NONE, "rmt_storage", "/system/bin/rmt_storage" }, /* modemst1 */
	{ "/dev/block/mmcblk0p10", BOOT_MODE_FOTA, "rmt_storage", "/system/bin/rmt_storage" }, /* modemst1 */
	{ "/dev/block/mmcblk0p10", BOOT_MODE_SDDOWNLOADER, "rmt_storage", "/system/bin/rmt_storage" }, /* modemst1 */
	{ "/dev/block/mmcblk0p10", BOOT_MODE_MAKERCMD, "rmt_storage", "/system/bin/rmt_storage" }, /* modemst1 */
	{ "/dev/block/mmcblk0p10", BOOT_MODE_OSUPDATE, "rmt_storage", "/system/bin/rmt_storage" }, /* modemst1 */
	{ "/dev/block/mmcblk0p10", BOOT_MODE_MASTERCLEAR, "rmt_storage", "/system/bin/rmt_storage" }, /* modemst1 */
	{ "/dev/block/mmcblk0p11", BOOT_MODE_NONE, "rmt_storage", "/system/bin/rmt_storage" }, /* modemst2 */
	{ "/dev/block/mmcblk0p11", BOOT_MODE_FOTA, "rmt_storage", "/system/bin/rmt_storage" }, /* modemst2 */
	{ "/dev/block/mmcblk0p11", BOOT_MODE_SDDOWNLOADER, "rmt_storage", "/system/bin/rmt_storage" }, /* modemst2 */
	{ "/dev/block/mmcblk0p11", BOOT_MODE_MAKERCMD, "rmt_storage", "/system/bin/rmt_storage" }, /* modemst2 */
	{ "/dev/block/mmcblk0p11", BOOT_MODE_OSUPDATE, "rmt_storage", "/system/bin/rmt_storage" }, /* modemst2 */
	{ "/dev/block/mmcblk0p11", BOOT_MODE_MASTERCLEAR, "rmt_storage", "/system/bin/rmt_storage" }, /* modemst2 */
	// { "/dev/block/mmcblk0p12", 0, 0, 0 }, /* m9kefs1 */
	// { "/dev/block/mmcblk0p13", 0, 0, 0 }, /* m9kefs2 */
	// { "/dev/block/mmcblk0p14", 0, 0, 0 }, /* m9kefs3 */
	// { "/dev/block/mmcblk0p15", 0, 0, 0 }, /* m9kefsc */
	{ "/dev/block/mmcblk0p16", 0, 0, 0 }, /* appsst1 */
	{ "/dev/block/mmcblk0p17", 0, 0, 0 }, /* appsst2 */
	{ "/dev/block/mmcblk0p18", 0, 0, 0 }, /* abost */
	{ "/dev/block/mmcblk0p19", 0, 0, 0 }, /* pad2 */
	{ "/dev/block/mmcblk0p20", 0, 0, 0 }, /* appsst3 */
	// { "/dev/block/mmcblk0p21", 0, 0, 0 }, /* sst */
	// { "/dev/block/mmcblk0p22", 0, 0, 0 }, /* sst2 */
	// { "/dev/block/mmcblk0p23", 0, 0, 0 }, /* system */
	// { "/dev/block/mmcblk0p24", 0, 0, 0 }, /* persist */
	// { "/dev/block/mmcblk0p25", 0, 0, 0 }, /* cache */
	{ "/dev/block/mmcblk0p26", 0, 0, 0 }, /* misc */
	{ "/dev/block/mmcblk0p27", BOOT_MODE_NONE, "applypatch", "/system/bin/applypatch" }, /* recovery */
	{ "/dev/block/mmcblk0p28", 0, 0, 0 }, /* DDR */
	{ "/dev/block/mmcblk0p29", BOOT_MODE_FOTA, SECURITY_FJSEC_RECOVERY_PROCESS_NAME, SECURITY_FJSEC_RECOVERY_PROCESS_PATH }, /* syschk */
	{ "/dev/block/mmcblk0p29", BOOT_MODE_SDDOWNLOADER, CONFIG_SECURITY_FJSEC_SDDOWNLOADER_MODE_ACCESS_PROCESS_NAME, CONFIG_SECURITY_FJSEC_SDDOWNLOADER_MODE_ACCESS_PROCESS_PATH}, /* syschk */
	{ "/dev/block/mmcblk0p29", BOOT_MODE_MASTERCLEAR, CONFIG_SECURITY_FJSEC_RECOVERY_MODE_ACCESS_PROCESS_NAME, CONFIG_SECURITY_FJSEC_RECOVERY_MODE_ACCESS_PROCESS_PATH }, /* syschk */
	{ "/dev/block/mmcblk0p29", BOOT_MODE_SDDOWNLOADER, CONFIG_SECURITY_FJSEC_SDDOWNLOADER_MODE_ACCESS_PROCESS_NAME, CONFIG_SECURITY_FJSEC_SDDOWNLOADER_MODE_ACCESS_PROCESS_PATH }, /* syschk */
	// { "/dev/block/mmcblk0p30", 0, 0, 0 }, /* persist2 */
	// { "/dev/block/mmcblk0p31", 0, 0, 0 }, /* persist3 */
	// { "/dev/block/mmcblk0p32", 0, 0, 0 }, /* kernellog */
	// { "/dev/block/mmcblk0p33", 0, 0, 0 }, /* loaderlog */
	{ "/dev/block/mmcblk0p34", 0, 0, 0 }, /* fota */
	{ "/dev/block/mmcblk0p35", BOOT_MODE_MASTERCLEAR, CONFIG_SECURITY_FJSEC_RECOVERY_MODE_ACCESS_PROCESS_NAME, CONFIG_SECURITY_FJSEC_RECOVERY_MODE_ACCESS_PROCESS_PATH }, /* fota2 */
	{ "/dev/block/mmcblk0p35", BOOT_MODE_SDDOWNLOADER, CONFIG_SECURITY_FJSEC_SDDOWNLOADER_MODE_ACCESS_PROCESS_NAME, CONFIG_SECURITY_FJSEC_SDDOWNLOADER_MODE_ACCESS_PROCESS_PATH }, /* fota2 */
	// { "/dev/block/mmcblk0p36", 0, 0, 0 }, /* dic */
#if (FJFEAT_PRODUCT == FJFEAT_PRODUCT_FJDEV005)
	{ "/dev/block/mmcblk0p36", BOOT_MODE_MASTERCLEAR, CONFIG_SECURITY_FJSEC_RECOVERY_MODE_ACCESS_PROCESS_NAME, CONFIG_SECURITY_FJSEC_RECOVERY_MODE_ACCESS_PROCESS_PATH }, /* kitting */
	{ "/dev/block/mmcblk0p36", BOOT_MODE_SDDOWNLOADER, CONFIG_SECURITY_FJSEC_SDDOWNLOADER_MODE_ACCESS_PROCESS_NAME, CONFIG_SECURITY_FJSEC_SDDOWNLOADER_MODE_ACCESS_PROCESS_PATH }, /* kitting */
	{ "/dev/block/mmcblk0p38", 0, 0, 0 }, /* grow */
#else
	{ "/dev/block/mmcblk0p37", 0, 0, 0 }, /* cnt */
	{ "/dev/block/mmcblk0p38", BOOT_MODE_MASTERCLEAR, CONFIG_SECURITY_FJSEC_RECOVERY_MODE_ACCESS_PROCESS_NAME, CONFIG_SECURITY_FJSEC_RECOVERY_MODE_ACCESS_PROCESS_PATH }, /* mmdata */
	{ "/dev/block/mmcblk0p38", BOOT_MODE_SDDOWNLOADER, CONFIG_SECURITY_FJSEC_SDDOWNLOADER_MODE_ACCESS_PROCESS_NAME, CONFIG_SECURITY_FJSEC_SDDOWNLOADER_MODE_ACCESS_PROCESS_PATH }, /* mmdata */
	{ "/dev/block/mmcblk0p39", BOOT_MODE_MASTERCLEAR, CONFIG_SECURITY_FJSEC_RECOVERY_MODE_ACCESS_PROCESS_NAME, CONFIG_SECURITY_FJSEC_RECOVERY_MODE_ACCESS_PROCESS_PATH }, /* kitting */
	{ "/dev/block/mmcblk0p39", BOOT_MODE_SDDOWNLOADER, CONFIG_SECURITY_FJSEC_SDDOWNLOADER_MODE_ACCESS_PROCESS_NAME, CONFIG_SECURITY_FJSEC_SDDOWNLOADER_MODE_ACCESS_PROCESS_PATH }, /* kitting */
	// { "/dev/block/mmcblk0p40", 0, 0, 0 }, /* userdata */
	{ "/dev/block/mmcblk0p41", 0, 0, 0 }, /* grow */
#endif
#elif (FJFEAT_PRODUCT == FJFEAT_PRODUCT_FJDEV004)
	{ "/dev/block/mmcblk0p1", BOOT_MODE_FOTA, CONFIG_SECURITY_FJSEC_FOTA_MODE_ACCESS_PROCESS_NAME, CONFIG_SECURITY_FJSEC_FOTA_MODE_ACCESS_PROCESS_PATH }, /* modem */
	{ "/dev/block/mmcblk0p2", BOOT_MODE_FOTA, CONFIG_SECURITY_FJSEC_FOTA_MODE_ACCESS_PROCESS_NAME, CONFIG_SECURITY_FJSEC_FOTA_MODE_ACCESS_PROCESS_PATH }, /* sbl1 */
	{ "/dev/block/mmcblk0p3", BOOT_MODE_FOTA, CONFIG_SECURITY_FJSEC_FOTA_MODE_ACCESS_PROCESS_NAME, CONFIG_SECURITY_FJSEC_FOTA_MODE_ACCESS_PROCESS_PATH }, /* sbl2 */
	{ "/dev/block/mmcblk0p4", BOOT_MODE_FOTA, CONFIG_SECURITY_FJSEC_FOTA_MODE_ACCESS_PROCESS_NAME, CONFIG_SECURITY_FJSEC_FOTA_MODE_ACCESS_PROCESS_PATH }, /* sbl3 */
	{ "/dev/block/mmcblk0p5", BOOT_MODE_FOTA, CONFIG_SECURITY_FJSEC_FOTA_MODE_ACCESS_PROCESS_NAME, CONFIG_SECURITY_FJSEC_FOTA_MODE_ACCESS_PROCESS_PATH }, /* aboot */
	{ "/dev/block/mmcblk0p6", BOOT_MODE_FOTA, CONFIG_SECURITY_FJSEC_FOTA_MODE_ACCESS_PROCESS_NAME, CONFIG_SECURITY_FJSEC_FOTA_MODE_ACCESS_PROCESS_PATH }, /* rpm */
	{ "/dev/block/mmcblk0p7", BOOT_MODE_FOTA, CONFIG_SECURITY_FJSEC_FOTA_MODE_ACCESS_PROCESS_NAME, CONFIG_SECURITY_FJSEC_FOTA_MODE_ACCESS_PROCESS_PATH }, /* boot */
	{ "/dev/block/mmcblk0p8", BOOT_MODE_FOTA, CONFIG_SECURITY_FJSEC_FOTA_MODE_ACCESS_PROCESS_NAME, CONFIG_SECURITY_FJSEC_FOTA_MODE_ACCESS_PROCESS_PATH }, /* tz */
	{ "/dev/block/mmcblk0p9", 0, 0, 0 }, /* pad */
	{ "/dev/block/mmcblk0p10", BOOT_MODE_NONE, "rmt_storage", "/system/bin/rmt_storage" }, /* modemst1 */
	{ "/dev/block/mmcblk0p10", BOOT_MODE_FOTA, "rmt_storage", "/system/bin/rmt_storage" }, /* modemst1 */
	{ "/dev/block/mmcblk0p10", BOOT_MODE_SDDOWNLOADER, "rmt_storage", "/system/bin/rmt_storage" }, /* modemst1 */
	{ "/dev/block/mmcblk0p10", BOOT_MODE_MAKERCMD, "rmt_storage", "/system/bin/rmt_storage" }, /* modemst1 */
	{ "/dev/block/mmcblk0p10", BOOT_MODE_OSUPDATE, "rmt_storage", "/system/bin/rmt_storage" }, /* modemst1 */
	{ "/dev/block/mmcblk0p10", BOOT_MODE_MASTERCLEAR, "rmt_storage", "/system/bin/rmt_storage" }, /* modemst1 */
	{ "/dev/block/mmcblk0p11", BOOT_MODE_NONE, "rmt_storage", "/system/bin/rmt_storage" }, /* modemst2 */
	{ "/dev/block/mmcblk0p11", BOOT_MODE_FOTA, "rmt_storage", "/system/bin/rmt_storage" }, /* modemst2 */
	{ "/dev/block/mmcblk0p11", BOOT_MODE_SDDOWNLOADER, "rmt_storage", "/system/bin/rmt_storage" }, /* modemst2 */
	{ "/dev/block/mmcblk0p11", BOOT_MODE_MAKERCMD, "rmt_storage", "/system/bin/rmt_storage" }, /* modemst2 */
	{ "/dev/block/mmcblk0p11", BOOT_MODE_OSUPDATE, "rmt_storage", "/system/bin/rmt_storage" }, /* modemst2 */
	{ "/dev/block/mmcblk0p11", BOOT_MODE_MASTERCLEAR, "rmt_storage", "/system/bin/rmt_storage" }, /* modemst2 */
	// { "/dev/block/mmcblk0p12", 0, 0, 0 }, /* m9kefs1 */
	// { "/dev/block/mmcblk0p13", 0, 0, 0 }, /* m9kefs2 */
	// { "/dev/block/mmcblk0p14", 0, 0, 0 }, /* m9kefs3 */
	// { "/dev/block/mmcblk0p15", 0, 0, 0 }, /* m9kefsc */
	{ "/dev/block/mmcblk0p16", 0, 0, 0 }, /* appsst1 */
	{ "/dev/block/mmcblk0p17", 0, 0, 0 }, /* appsst2 */
	{ "/dev/block/mmcblk0p18", 0, 0, 0 }, /* abost */
	{ "/dev/block/mmcblk0p19", 0, 0, 0 }, /* pad2 */
	{ "/dev/block/mmcblk0p20", BOOT_MODE_FOTA, CONFIG_SECURITY_FJSEC_FOTA_MODE_ACCESS_PROCESS_NAME, CONFIG_SECURITY_FJSEC_FOTA_MODE_ACCESS_PROCESS_PATH }, /* appsst3 */
	// { "/dev/block/mmcblk0p21", 0, 0, 0 }, /* sst */
	// { "/dev/block/mmcblk0p22", 0, 0, 0 }, /* sst2 */
	// { "/dev/block/mmcblk0p23", 0, 0, 0 }, /* system */
	// { "/dev/block/mmcblk0p24", 0, 0, 0 }, /* persist */
	// { "/dev/block/mmcblk0p25", 0, 0, 0 }, /* cache */
	{ "/dev/block/mmcblk0p26", 0, 0, 0 }, /* misc */
	{ "/dev/block/mmcblk0p27", BOOT_MODE_FOTA, CONFIG_SECURITY_FJSEC_FOTA_MODE_ACCESS_PROCESS_NAME, CONFIG_SECURITY_FJSEC_FOTA_MODE_ACCESS_PROCESS_PATH }, /* recovery */
	{ "/dev/block/mmcblk0p27", BOOT_MODE_NONE, "applypatch", "/system/bin/applypatch" }, /* recovery */
	{ "/dev/block/mmcblk0p28", 0, 0, 0 }, /* DDR */
	{ "/dev/block/mmcblk0p29", BOOT_MODE_FOTA, SECURITY_FJSEC_RECOVERY_PROCESS_NAME, SECURITY_FJSEC_RECOVERY_PROCESS_PATH }, /* syschk */
	{ "/dev/block/mmcblk0p29", BOOT_MODE_FOTA, CONFIG_SECURITY_FJSEC_FOTA_MODE_ACCESS_PROCESS_NAME, CONFIG_SECURITY_FJSEC_FOTA_MODE_ACCESS_PROCESS_PATH }, /* syschk */
	{ "/dev/block/mmcblk0p29", BOOT_MODE_SDDOWNLOADER, CONFIG_SECURITY_FJSEC_SDDOWNLOADER_MODE_ACCESS_PROCESS_NAME, CONFIG_SECURITY_FJSEC_SDDOWNLOADER_MODE_ACCESS_PROCESS_PATH}, /* syschk */
	{ "/dev/block/mmcblk0p29", BOOT_MODE_MASTERCLEAR, CONFIG_SECURITY_FJSEC_RECOVERY_MODE_ACCESS_PROCESS_NAME, CONFIG_SECURITY_FJSEC_RECOVERY_MODE_ACCESS_PROCESS_PATH }, /* syschk */
	{ "/dev/block/mmcblk0p29", BOOT_MODE_SDDOWNLOADER, CONFIG_SECURITY_FJSEC_SDDOWNLOADER_MODE_ACCESS_PROCESS_NAME, CONFIG_SECURITY_FJSEC_SDDOWNLOADER_MODE_ACCESS_PROCESS_PATH }, /* syschk */
	// { "/dev/block/mmcblk0p30", 0, 0, 0 }, /* persist2 */
	// { "/dev/block/mmcblk0p31", 0, 0, 0 }, /* persist3 */
	// { "/dev/block/mmcblk0p32", 0, 0, 0 }, /* kernellog */
	// { "/dev/block/mmcblk0p33", 0, 0, 0 }, /* loaderlog */
	{ "/dev/block/mmcblk0p34", BOOT_MODE_FOTA, CONFIG_SECURITY_FJSEC_FOTA_MODE_ACCESS_PROCESS_NAME, CONFIG_SECURITY_FJSEC_FOTA_MODE_ACCESS_PROCESS_PATH }, /* fota */
	{ "/dev/block/mmcblk0p35", BOOT_MODE_MASTERCLEAR, CONFIG_SECURITY_FJSEC_RECOVERY_MODE_ACCESS_PROCESS_NAME, CONFIG_SECURITY_FJSEC_RECOVERY_MODE_ACCESS_PROCESS_PATH }, /* fota2 */
	{ "/dev/block/mmcblk0p35", BOOT_MODE_SDDOWNLOADER, CONFIG_SECURITY_FJSEC_SDDOWNLOADER_MODE_ACCESS_PROCESS_NAME, CONFIG_SECURITY_FJSEC_SDDOWNLOADER_MODE_ACCESS_PROCESS_PATH }, /* fota2 */
	// { "/dev/block/mmcblk0p36", 0, 0, 0 }, /* dic */
	{ "/dev/block/mmcblk0p37", 0, 0, 0 }, /* cnt */
	{ "/dev/block/mmcblk0p38", BOOT_MODE_MASTERCLEAR, CONFIG_SECURITY_FJSEC_RECOVERY_MODE_ACCESS_PROCESS_NAME, CONFIG_SECURITY_FJSEC_RECOVERY_MODE_ACCESS_PROCESS_PATH }, /* mmdata */
	{ "/dev/block/mmcblk0p38", BOOT_MODE_SDDOWNLOADER, CONFIG_SECURITY_FJSEC_SDDOWNLOADER_MODE_ACCESS_PROCESS_NAME, CONFIG_SECURITY_FJSEC_SDDOWNLOADER_MODE_ACCESS_PROCESS_PATH }, /* mmdata */
	{ "/dev/block/mmcblk0p39", BOOT_MODE_MASTERCLEAR, CONFIG_SECURITY_FJSEC_RECOVERY_MODE_ACCESS_PROCESS_NAME, CONFIG_SECURITY_FJSEC_RECOVERY_MODE_ACCESS_PROCESS_PATH }, /* kitting */
	{ "/dev/block/mmcblk0p39", BOOT_MODE_SDDOWNLOADER, CONFIG_SECURITY_FJSEC_SDDOWNLOADER_MODE_ACCESS_PROCESS_NAME, CONFIG_SECURITY_FJSEC_SDDOWNLOADER_MODE_ACCESS_PROCESS_PATH }, /* kitting */
	// { "/dev/block/mmcblk0p40", 0, 0, 0 }, /* userdata */
	{ "/dev/block/mmcblk0p41", 0, 0, 0 }, /* grow */
#endif

#ifdef DEBUG_ACCESS_CONTROL
	{ "/dev/block/mmcblk0p39", BOOT_MODE_NONE, "test", "/data/lsm-test/tmp/test_tool" },
	{ TEST_PTN_PATH, 0, 0, 0 },
#endif

	{ 0, 0, 0, 0 },
};

static struct ac_config fs_acl[] = {
#ifdef DEBUG_ACCESS_CONTROL
	{ "/data/lsm-test/tmp/fspt-ptct/*", BOOT_MODE_NONE, "test", "/data/lsm-test/tmp/test_tool" },
	{ "/data/lsm-test/tmp/fspt-ptct.txt", BOOT_MODE_NONE, "test", "/data/lsm-test/tmp/test_tool" },
	{ "/data/lsm-test/tmp/fspt-ptct2.txt", 0, 0, 0 },
	{ "/data/lsm-test/tmp/fspt-ptct/*", BOOT_MODE_MASTERCLEAR, "prepare", "/data/lsm-test/tmp/test_tool" },
	{ TEST_DIRS_PATH, BOOT_MODE_NONE, "test", "/data/lsm-test/tmp/test_tool" },
#endif

	{ 0, 0, 0, 0 },
};

struct ac_config_devmem {
	unsigned long head;
	unsigned long tail;
	char *process_name;
	char *process_path;
};

static struct ac_config_devmem devmem_acl[] = {
	{ ADDR2PFN(0x8FF00000), ADDR2PFN(0x8FF3FFFF)-1, "rmt_storage", "/system/bin/rmt_storage" },
};

#ifdef CONFIG_SECURITY_FJSEC_AC_KITTING
struct kitting_directory_access_process {
	char *process_name;
	char *process_path;
	uid_t uid;
};

static struct kitting_directory_access_process kitting_directory_access_process_list[] = {
	{ "com.android.settings", JAVA_APP_PROCESS_PATH, AID_SYSTEM },
	{ "com.android.defcontainer", JAVA_APP_PROCESS_PATH, UID_NO_CHECK },
#ifdef DEBUG_KITTING_ACCESS_CONTROL
	{ "com.android.settings", "/system/bin/busybox", AID_SYSTEM },
#endif
	{ 0, 0, 0 },
};
#endif /* CONFIG_SECURITY_FJSEC_AC_KITTING */

struct ac_config_setid{
	char *process_name;
	char *process_path;
};

static struct ac_config_setid setid_acl[] = {
	{ "init", "/init"},
	{ "ueventd", "/init"},
	{ "adbd", "/sbin/adbd"},
#ifdef DEBUG_SETID_ACCESS_CONTROL
	{ "setuid_test", "/data/lsm-test/setuid_test"},
	{ "setuid_test_NG", "/data/lsm-test-bin/setuid_test_NG"},
#endif
#ifdef DEBUG_COPY_GUARD
	{ "ok_native", "/data/lsm-test/tmp/busybox"},
	{ "ok_native", "/data/lsm-test/tmp/busyboxng"},
	{ "ok_native", "/data/lsm-test/tmp/regza1"},
	{ "ok_native", "/data/lsm-test/tmp/regza2"},
	{ "ok_native", "/data/lsm-test/tmp/regza3"},
	{ "ng_native", "/data/lsm-test/tmp/regza1"},
	{ "ng_native", "/data/lsm-test/tmp/regza2"},
#endif
#ifdef DEBUG_KITTING_ACCESS_CONTROL
	{ "busybox", "/data/busybox/busybox"},
	{ "ndroid.settings", "/data/busybox/busybox"},
#endif

	{ 0, 0 },
};

#define FJSEC_ACP_NAME_MAX 64
#define FJSEC_ACP_PATH_MAX 64

struct ac_config_page{
	char id[FJSEC_ACP_NAME_MAX];
	char process_name[FJSEC_ACP_NAME_MAX];
	char process_path[FJSEC_ACP_PATH_MAX];
	uid_t uid;
	unsigned long start_pfn;
 	unsigned long end_pfn;
};

static struct ac_config_page page_acl_pre[] = {
 	{"apq8064", "SurfaceFlinger", "/system/bin/surfaceflinger", AID_SYSTEM, 0, 0},
	{"makercmd", CONFIG_SECURITY_FJSEC_MAKERCMD_MODE_ACCESS_PROCESS_NAME, CONFIG_SECURITY_FJSEC_MAKERCMD_MODE_ACCESS_PROCESS_PATH, AID_ROOT, 0, 0},
	{"recovery", CONFIG_SECURITY_FJSEC_SDDOWNLOADER_MODE_ACCESS_PROCESS_NAME, CONFIG_SECURITY_FJSEC_SDDOWNLOADER_MODE_ACCESS_PROCESS_PATH, AID_ROOT, 0, 0},
	{"fota", CONFIG_SECURITY_FJSEC_FOTA_MODE_ACCESS_PROCESS_NAME, CONFIG_SECURITY_FJSEC_FOTA_MODE_ACCESS_PROCESS_PATH, AID_ROOT, 0, 0},
 	{"kgsl", NO_CHECK_STR, NO_CHECK_STR, UID_NO_CHECK, 0, 0},
 	{"wcnss", "sh", "/system/bin/mksh", AID_ROOT, 0, 0},
 	{"q6", NO_CHECK_STR, NO_CHECK_STR, UID_NO_CHECK, 0, 0},
	{"splash_screen", "swapper/0", NO_CHECK_STR, AID_ROOT, 0, 0},
 	{"0", "0","0",0,0,0},
};

static struct ac_config_page *page_acl=NULL;
