#ifndef __OTA_EXAMPLE_H__
#define __OTA_EXAMPLE_H__


#include "esp_ota_ops.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"
#include "string.h"
#define CONFIG_EXAMPLE_USE_CERT_BUNDLE
#ifdef CONFIG_EXAMPLE_USE_CERT_BUNDLE
#include "esp_crt_bundle.h"
#endif

#include "nvs.h"
#include "nvs_flash.h"
#include "http_ota.h"

void ota_main(const char* URL);
uint8_t OTA_Get_Enable(void);
#endif
