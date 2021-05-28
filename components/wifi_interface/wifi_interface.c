/*
    Wifi related functionality
    Connect to pre defined wifi

    Must be taken over/merge with wifi provision
*/

//#include "esp_event.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"

#include "wifi_interface.h"

#if ENABLE_WIFI_PROVISIONING
#include <string.h>  // for memcpy
#include <wifi_provisioning/manager.h>
#include <wifi_provisioning/scheme_softap.h>
#endif

static const char *TAG = "WIFI";

char mac_address[18];

EventGroupHandle_t s_wifi_event_group;

static int s_retry_num = 0;
static wifi_config_t wifi_config;

/* FreeRTOS event group to signal when we are connected & ready to make a
 * request */
// static EventGroupHandle_t wifi_event_group;

/* The event group allows multiple bits for each event,
   but we only care about one event - are we connected
   to the AP with an IP? */

// Event handler for catching system events
static void event_handler(void *arg, esp_event_base_t event_base, int event_id,
                          void *event_data) {
  if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
    esp_wifi_connect();
  } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
    ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
    ESP_LOGI(TAG, "Connected with IP Address:" IPSTR,
             IP2STR(&event->ip_info.ip));
    s_retry_num = 0;
    // Signal main application to continue execution
    xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
  } else if (event_base == WIFI_EVENT &&
             event_id == WIFI_EVENT_STA_DISCONNECTED) {
    if ((s_retry_num < WIFI_MAXIMUM_RETRY) || (WIFI_MAXIMUM_RETRY == 0)) {
      esp_wifi_connect();
      s_retry_num++;
      ESP_LOGI(TAG, "retry to connect to the AP");
    } else {
      xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
    }
    ESP_LOGI(TAG, "connect to the AP fail");
  } else {
#if ENABLE_WIFI_PROVISIONING
    if (event_base == WIFI_PROV_EVENT) {
      switch (event_id) {
        case WIFI_PROV_START:
          ESP_LOGI(TAG, "Provisioning started");
          break;
        case WIFI_PROV_CRED_RECV: {
          wifi_sta_config_t *wifi_sta_cfg = (wifi_sta_config_t *)event_data;
          ESP_LOGI(TAG,
                   "Received Wi-Fi credentials"
                   "\n\tSSID     : %s\n\tPassword : %s",
                   (const char *)wifi_sta_cfg->ssid,
                   (const char *)wifi_sta_cfg->password);
          memcpy(&(wifi_config.sta), wifi_sta_cfg, sizeof(wifi_sta_config_t));
          break;
        }
        case WIFI_PROV_CRED_FAIL: {
          wifi_prov_sta_fail_reason_t *reason =
              (wifi_prov_sta_fail_reason_t *)event_data;
          ESP_LOGE(TAG,
                   "Provisioning failed!\n\tReason : %s"
                   "\n\tPlease reset to factory and retry provisioning",
                   (*reason == WIFI_PROV_STA_AUTH_ERROR)
                       ? "Wi-Fi station authentication failed"
                       : "Wi-Fi access-point not found");
          break;
        }
        case WIFI_PROV_CRED_SUCCESS:
          ESP_LOGI(TAG, "Provisioning successful");
          break;
        case WIFI_PROV_END:
          /* De-initialize manager once provisioning is finished */
          wifi_prov_mgr_deinit();
          break;
        default:
          break;
      }
    }
#endif
  }
}

#if ENABLE_WIFI_PROVISIONING
static void get_device_service_name(char *service_name, size_t max) {
  uint8_t eth_mac[6];
  const char *ssid_prefix = "PROV_";
  esp_wifi_get_mac(WIFI_IF_STA, eth_mac);
  snprintf(service_name, max, "%s%02X%02X%02X", ssid_prefix, eth_mac[3],
           eth_mac[4], eth_mac[5]);
}
#endif

void wifi_init(void) {
  s_wifi_event_group = xEventGroupCreate();

  ESP_ERROR_CHECK(esp_netif_init());

  ESP_ERROR_CHECK(esp_event_loop_create_default());

  ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                             &event_handler, NULL));
  ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                             &event_handler, NULL));
#if ENABLE_WIFI_PROVISIONING
  ESP_ERROR_CHECK(esp_event_handler_register(WIFI_PROV_EVENT, ESP_EVENT_ANY_ID,
                                             &event_handler, NULL));
#endif

  esp_netif_create_default_wifi_sta();
#if ENABLE_WIFI_PROVISIONING
  esp_netif_create_default_wifi_ap();
#endif

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));

#if ENABLE_WIFI_PROVISIONING
  // Configuration for the provisioning manager
  wifi_prov_mgr_config_t config = {
      .scheme = wifi_prov_scheme_softap,
      .scheme_event_handler = WIFI_PROV_EVENT_HANDLER_NONE};

  // Initialize provisioning manager with the
  // configuration parameters set above
  ESP_ERROR_CHECK(wifi_prov_mgr_init(config));

  bool provisioned = false;
  /* Let's find out if the device is provisioned */
  ESP_ERROR_CHECK(wifi_prov_mgr_is_provisioned(&provisioned));

  /* If device is not yet provisioned start provisioning service */
  if (!provisioned) {
    ESP_LOGI(TAG, "Starting provisioning");

    // Wi-Fi SSID when scheme is wifi_prov_scheme_softap
    char service_name[12];
    get_device_service_name(service_name, sizeof(service_name));

    /* What is the security level that we want (0 or 1):
     *      - WIFI_PROV_SECURITY_0 is simply plain text communication.
     *      - WIFI_PROV_SECURITY_1 is secure communication which consists of
     * secure handshake using X25519 key exchange and proof of possession (pop)
     * and AES-CTR for encryption/decryption of messages.
     */
    wifi_prov_security_t security = WIFI_PROV_SECURITY_1;

    /* Do we want a proof-of-possession (ignored if Security 0 is selected):
     *      - this should be a string with length > 0
     *      - NULL if not used
     */
    const char *pop = NULL;  //"abcd1234";

    /* What is the service key (could be NULL)
     * This translates to :
     *     - Wi-Fi password when scheme is wifi_prov_scheme_softap
     *     - simply ignored when scheme is wifi_prov_scheme_ble
     */
    const char *service_key = "12345678";

    /* An optional endpoint that applications can create if they expect to
     * get some additional custom data during provisioning workflow.
     * The endpoint name can be anything of your choice.
     * This call must be made before starting the provisioning.
     */
    // wifi_prov_mgr_endpoint_create("custom-data");
    /* Start provisioning service */
    ESP_ERROR_CHECK(wifi_prov_mgr_start_provisioning(
        security, pop, service_name, service_key));

    /* The handler for the optional endpoint created above.
     * This call must be made after starting the provisioning, and only if the
     * endpoint has already been created above.
     */
    // wifi_prov_mgr_endpoint_register("custom-data", custom_prov_data_handler,
    // NULL);

    /* Uncomment the following to wait for the provisioning to finish and then
     * release the resources of the manager. Since in this case
     * de-initialization is triggered by the default event loop handler, we
     * don't need to call the following */
    // wifi_prov_mgr_wait();
    // wifi_prov_mgr_deinit();
  } else {
    ESP_LOGI(TAG, "Already provisioned, starting Wi-Fi STA");

    /* We don't need the manager as device is already provisioned,
     * so let's release it's resources */
    wifi_prov_mgr_deinit();

    /* Start Wi-Fi station */
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "wifi_init_sta finished.");
  }
#else
  wifi_config_t wifi_config = {
      .sta =
          {
              .ssid = WIFI_SSID,
              .password = WIFI_PASSWORD,
              .threshold.authmode = WIFI_AUTH_WPA2_PSK,
              .pmf_cfg = {.capable = true, .required = false},
          },
  };
  ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));

  /* Start Wi-Fi station */
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  ESP_ERROR_CHECK(esp_wifi_start());

  ESP_LOGI(TAG, "wifi_init_sta finished.");
#endif

  /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or
   * connection failed for the maximum number of re-tries (WIFI_FAIL_BIT). The
   * bits are set by event_handler() (see above) */
  EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                         WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                         pdFALSE, pdFALSE, portMAX_DELAY);

  /* xEventGroupWaitBits() returns the bits before the call returned, hence we
   * can test which event actually happened. */
  if (bits & WIFI_CONNECTED_BIT) {
    ESP_LOGI(TAG, "connected to ap SSID: %s", wifi_config.sta.ssid);
  } else if (bits & WIFI_FAIL_BIT) {
    ESP_LOGI(TAG, "Failed to connect to SSID:%s, password:%s",
             wifi_config.sta.ssid, wifi_config.sta.password);
  } else {
    ESP_LOGE(TAG, "UNEXPECTED EVENT");
  }

  uint8_t base_mac[6];
  // Get MAC address for WiFi station
  esp_read_mac(base_mac, ESP_MAC_WIFI_STA);
  sprintf(mac_address, "%02X:%02X:%02X:%02X:%02X:%02X", base_mac[0],
          base_mac[1], base_mac[2], base_mac[3], base_mac[4], base_mac[5]);

  // ESP_ERROR_CHECK(esp_event_handler_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP,
  // &event_handler)); ESP_ERROR_CHECK(esp_event_handler_unregister(WIFI_EVENT,
  // ESP_EVENT_ANY_ID, &event_handler)); vEventGroupDelete(s_wifi_event_group);
}
