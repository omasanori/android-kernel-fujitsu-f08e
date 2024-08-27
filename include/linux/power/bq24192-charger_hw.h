/*----------------------------------------------------------------------------*/
// COPYRIGHT(C) FUJITSU LIMITED 2012
/*----------------------------------------------------------------------------*/

#ifndef BQ24192_CHARGER_HW_H
#define BQ24192_CHARGER_HW_H

extern int fj_bq_chg_hw_set_charger_enable(int charger_enable);
extern int fj_bq_chg_hw_set_voltage(int chg_voltage);
extern int fj_bq_chg_hw_set_current(int chg_current);
extern int fj_bq_chg_hw_set_vinmin(int chg_vinmin);
extern int fj_bq_chg_hw_charge_start(unsigned int tmp_current);
extern int fj_bq_chg_hw_charge_stop(void);
extern int fj_bq_chg_hw_init(struct bq24192_charger *bq);

#endif /* BQ24192_CHARGER_HW_H */
