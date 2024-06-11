/*
 * Copyright (c) @CompanyNameMagicTag 2023-2023. All rights reserved.
 * Description: audio sample data
 * This file should be changed only infrequently and with great care.
 */
#ifndef DIAG_AUDIO_SAMPLE_DATA_H
#define DIAG_AUDIO_SAMPLE_DATA_H
#include <stdint.h>
#include "errcode.h"

errcode_t diag_audio_sample_data_report_start(uint32_t transmit_id);
errcode_t diag_audio_sample_data_report_stop(uint32_t transmit_id);
errcode_t diag_audio_sample_data_report(uint32_t transmit_id, uint8_t *buf, uint32_t size);

errcode_t diag_audio_sample_data_init(void);

#endif