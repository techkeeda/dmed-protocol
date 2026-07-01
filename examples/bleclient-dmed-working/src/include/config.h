/*
 * config.h
 *
 *
 * Copyright (C) 2016-2017 Ooma Incorporated. All rights reserved.
 */

#ifndef CONFIG_H_
#define CONFIG_H_

typedef struct bleclient_config {
	gboolean cleartext_enabled;
	gint lockout_timeout;
}bleclient_cfg_t;

extern bleclient_cfg_t bleclient_cfg;

void load_config(void);
void apply_configuration_settings(void);
void feature_config_init(void);
void config_cleanup(void);

#endif /* CONFIG_H_ */
