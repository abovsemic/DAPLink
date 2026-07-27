/*
 * DAPLink Interface Firmware
 * Copyright (c) 2020 Arm Limited, All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may
 * not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "crc.h"
#include "daplink_addr.h"
#include "util.h"

#define TEST_CHECK_VALUE (0)
#define CHECK_VALUE     (0xCBF43926)

#define INITIAL_SEED    (0xFFFFFFFF)

void crc_init(void)
{

}

uint32_t crc32(const void *data, int nBytes)
{
    /* HW crc does not support CRC32. Only CRC-16 */
    (void)data;
    (void)nBytes;
    return 0;
}

uint32_t crc32_continue(uint32_t prev_crc, const void *data, int nBytes)
{
    /* HW crc does not support CRC32. Only CRC-16 */
    (void)prev_crc;
    (void)data;
    (void)nBytes;
    return 0;
}
