// Copyright 2015-2016 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#ifndef __CAPTIVE_PORTAL_H__
#define __CAPTIVE_PORTAL_H__

#ifdef __cplusplus 
extern "C" {
#endif

#include "esp_err.h"

/**
  * @brief Start captive portal
  *
  * @param ap_ssid  SSID of softap, if use default SSID, set it as NULL
  * @param ap_pwd  password of softap, if no need password, set it as NULL
  * @param configured Is it configured
  *
  * @return
  *    - ESP_OK: succeed
  *    - ESP_FAIL: fail
  */
esp_err_t captive_portal_start(const char *ap_ssid, const char *ap_pwd, bool *configured);

/**
  * @brief Waiting for webpage configure to complete
  *
  * @param ticks_to_wait  webpage config timeout
  *
  * @return
  *    - ESP_OK: succeed
  *    - ESP_FAIL: fail
  */
esp_err_t captive_portal_wait(uint32_t ticks_to_wait);

#ifdef __cplusplus 
}
#endif

#endif // __CAPTIVE_PORTAL_H__
