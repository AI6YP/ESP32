#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "esp_err.h"
#include "esp_log.h"
#include "esp_event.h"
#include "esp_system.h"

#include "driver/spi_master.h"
#include "driver/gpio.h"

#include "nvs_flash.h"

#include "esp_wifi.h"
#include "esp_tls_crypto.h"
#include "esp_http_server.h"

#include "lwip/err.h"
#include "lwip/sys.h"

#include "secrets.h"
#include "sdkconfig.h"

#include "pages.h"

#define PIN_NUM_MISO 9
#define PIN_NUM_MOSI 20
#define PIN_NUM_CLK  19
#define PIN_NUM_CS   18

// LED PWR
#define PIN_NUM_LED_YELLOW 23

// LED RXTX
#define PIN_NUM_LED_GREEN  15

#define PIN_NUM_IRQ       21
#define PIN_NUM_RSTN      22


static const char *TAG = "SAKI";

static spi_device_handle_t spi_txrx;

DRAM_ATTR static uint8_t buf0[256];

static EventGroupHandle_t s_wifi_event_group;

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

void rdwr_spi(spi_device_handle_t spi, uint8_t *tx_data, uint8_t *rx_data, int length) {

  spi_device_acquire_bus(spi, portMAX_DELAY);

  spi_transaction_t t11;
  memset(&t11, 0, sizeof(t11));
  t11.length = 8 * length;
  t11.tx_buffer = tx_data;
  t11.rx_buffer = rx_data;

  esp_err_t ret = spi_device_polling_transmit(spi, &t11);
  assert(ret == ESP_OK);

  spi_device_release_bus(spi);
}

wifi_config_t wifi_config = {
  .sta = {
    .ssid = CONFIG_ESP_WIFI_SSID,
    .password = CONFIG_ESP_WIFI_PASSWORD,
    .threshold.authmode = WIFI_AUTH_WPA2_PSK // ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD,
  },
};

static int s_retry_num = 0;

static void wifi_event_handler(
  void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data
) {
  if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
     esp_wifi_connect();
  } else
  if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
    if (s_retry_num < 100 /* EXAMPLE_ESP_MAXIMUM_RETRY */) {
      esp_wifi_connect();
      s_retry_num++;
      ESP_LOGI(TAG, "retry to connect to the AP");
    } else {
      xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
    }
    ESP_LOGI(TAG,"connect to the AP fail");
  } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
    ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
    ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
    s_retry_num = 0;
    xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
  }
}

void wifi_init_sta() {
  s_wifi_event_group = xEventGroupCreate();
  ESP_ERROR_CHECK(esp_netif_init());

  ESP_ERROR_CHECK(esp_event_loop_create_default());
  esp_netif_create_default_wifi_sta();

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));

  esp_event_handler_instance_t instance_any_id;
  esp_event_handler_instance_t instance_got_ip;

  ESP_ERROR_CHECK(esp_event_handler_instance_register(
    WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, &instance_any_id
  ));

  ESP_ERROR_CHECK(esp_event_handler_instance_register(
    IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, &instance_got_ip
  ));

  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
  ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));
  ESP_ERROR_CHECK(esp_wifi_start());

  ESP_LOGI(TAG, "wifi_init_sta finished.");

  /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
   * number of re-tries (WIFI_FAIL_BIT). The bits are set by wifi_event_handler() (see above) */
  EventBits_t bits = xEventGroupWaitBits(
    s_wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT, pdFALSE, pdFALSE, portMAX_DELAY
  );

  /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually happened. */

  if (bits & WIFI_CONNECTED_BIT) {
    ESP_LOGI(TAG, "connected to ap SSID:%s password:%s", CONFIG_ESP_WIFI_SSID, CONFIG_ESP_WIFI_PASSWORD);
  } else if (bits & WIFI_FAIL_BIT) {
    ESP_LOGI(TAG, "Failed to connect to SSID:%s, password:%s", CONFIG_ESP_WIFI_SSID, CONFIG_ESP_WIFI_PASSWORD);
  } else {
    ESP_LOGE(TAG, "UNEXPECTED EVENT");
  }
}

// /* Our URI handler function to be called during GET /uri request */
esp_err_t index_get_handler(httpd_req_t *req) {
  /* Send a simple response */
  ESP_LOGI(TAG, "INDEX GET HANDLER");
  // const char resp[] = "{\"msg\": \"status\"}";
  httpd_resp_send(req, &PAGE_index, PAGE_index_length);
  return ESP_OK;
}

// /* Our URI handler function to be called during POST /uri request */

/*
 * Structure holding server handle
 * and internal socket fd in order
 * to use out of request send
 */
struct async_resp_arg {
    httpd_handle_t hd;
    int fd;
};

/*
 * async send function, which we put into the httpd work queue
 */
static void ws_async_send(void *arg) {
  static const char * data = "Async data";
  struct async_resp_arg *resp_arg = arg;
  httpd_handle_t hd = resp_arg->hd;
  int fd = resp_arg->fd;
  httpd_ws_frame_t ws_pkt;
  memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
  ws_pkt.payload = (uint8_t*)data;
  ws_pkt.len = strlen(data);
  ws_pkt.type = HTTPD_WS_TYPE_BINARY;

  httpd_ws_send_frame_async(hd, fd, &ws_pkt);
  free(resp_arg);
}

static esp_err_t trigger_async_send(httpd_handle_t handle, httpd_req_t *req)
{
    struct async_resp_arg *resp_arg = malloc(sizeof(struct async_resp_arg));
    if (resp_arg == NULL) {
        return ESP_ERR_NO_MEM;
    }
    resp_arg->hd = req->handle;
    resp_arg->fd = httpd_req_to_sockfd(req);
    esp_err_t ret = httpd_queue_work(handle, ws_async_send, resp_arg);
    if (ret != ESP_OK) {
        free(resp_arg);
    }
    return ret;
}

esp_err_t meter0_handler(httpd_req_t *req) {
  if (req->method == HTTP_GET) {
    ESP_LOGI(TAG, "Handshake done, the new connection was opened");
    return ESP_OK;
  }
  httpd_ws_frame_t ws_pkt;
  uint8_t *buf = NULL;
  uint8_t *tx_buf = NULL;
  memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
  ws_pkt.type = HTTPD_WS_TYPE_BINARY;
  /* Set max_len = 0 to get the frame len */
  esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, 0);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "httpd_ws_recv_frame failed to get frame len with %d", ret);
    return ret;
  }
  // ESP_LOGI(TAG, "frame len is %d", ws_pkt.len);
  if (ws_pkt.len) {
    /* ws_pkt.len + 1 is for NULL termination as we are expecting a string */
    buf = calloc(ws_pkt.len, 1);
    tx_buf = calloc(ws_pkt.len, 1);
    // buf = malloc(ws_pkt.len);
    if (buf == NULL) {
      // ESP_LOGE(TAG, "Failed to calloc memory for buf");
      ESP_LOGE(TAG, "Failed to malloc memory for buf");
      return ESP_ERR_NO_MEM;
    }
    ws_pkt.payload = buf;
    /* Set max_len = ws_pkt.len to get the frame payload */
    ret = httpd_ws_recv_frame(req, &ws_pkt, ws_pkt.len);
    if (ret != ESP_OK) {
      ESP_LOGE(TAG, "httpd_ws_recv_frame failed with %d", ret);
      free(buf);
      return ret;
    }
    // ESP_LOGI(TAG, "To SPI:");
    // for (int i = 0; i < ws_pkt.len; i++) {
    //   ESP_LOGI(TAG, "%x", buf[i]);
    // }
    rdwr_spi(spi_txrx, buf, tx_buf, ws_pkt.len);
    // ESP_LOGI(TAG, "From SPI:");
    // for (int i = 0; i < ws_pkt.len; i++) {
    //   ESP_LOGI(TAG, "%x", buf[i]);
    // }
    ws_pkt.payload = tx_buf;
  }
  // ESP_LOGI(TAG, "Packet type: %d", ws_pkt.type);
  // if (
  //   ws_pkt.type == HTTPD_WS_TYPE_BINARY &&
  //   strcmp((char*)ws_pkt.payload,"Trigger async") == 0
  // ) {
  //   free(buf);
  //   return trigger_async_send(req->handle, req);
  // }

  ret = httpd_ws_send_frame(req, &ws_pkt);

  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "httpd_ws_send_frame failed with %d", ret);
  }

  // free(buf);
  return ret;
}

// esp_err_t skin_post_handler1(httpd_req_t *req) {
//   size_t recv_size = MIN(req->content_len, sizeof(buf0));
//   int ret = httpd_req_recv(req, (char *)&buf0[4096], recv_size);
//   if (ret <= 0) {  /* 0 return value indicates connection closed */
//     if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
//       httpd_resp_send_408(req);
//     }
//     return ESP_FAIL;
//   }
//   const char resp[] = "{\"msg\":\"OK\"}";
//   httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
//   return ESP_OK;
// }

// esp_err_t skin_post_handler2(httpd_req_t *req) {
//   size_t recv_size = MIN(req->content_len, sizeof(buf0));
//   int ret = httpd_req_recv(req, (char *)&buf0[8192], recv_size);
//   if (ret <= 0) {  /* 0 return value indicates connection closed */
//     if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
//       httpd_resp_send_408(req);
//     }
//     return ESP_FAIL;
//   }
//   const char resp[] = "{\"msg\":\"OK\"}";
//   httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
//   update_skin(DISPLAY_BUSY);
//   return ESP_OK;
// }

httpd_uri_t uri_get = {
  .uri      = "/",
  .method   = HTTP_GET,
  .handler  = index_get_handler,
  .user_ctx = NULL
};

httpd_uri_t meter0_get = {
  .uri      = "/dev1",
  .method   = HTTP_GET,
  .handler  = meter0_handler,
  .user_ctx = NULL,
  .is_websocket = true
};

httpd_uri_t meter0_post = {
  .uri      = "/dev1",
  .method   = HTTP_POST,
  .handler  = meter0_handler,
  .user_ctx = NULL,
  .is_websocket = true
};

// httpd_uri_t uri_post1 = {
//   .uri      = "/skin1",
//   .method   = HTTP_POST,
//   .handler  = skin_post_handler1,
//   .user_ctx = NULL
// };

// httpd_uri_t uri_post2 = {
//   .uri      = "/skin2",
//   .method   = HTTP_POST,
//   .handler  = skin_post_handler2,
//   .user_ctx = NULL
// };

httpd_handle_t start_webserver() {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.stack_size = 16384;
  httpd_handle_t server = NULL;
  if (httpd_start(&server, &config) == ESP_OK) {
      httpd_register_uri_handler(server, &uri_get);
      httpd_register_uri_handler(server, &meter0_get);
      httpd_register_uri_handler(server, &meter0_post);
      // httpd_register_uri_handler(server, &uri_post1);
      // httpd_register_uri_handler(server, &uri_post2);
  }
  return server;
}

void spi_pre_transfer_callback(spi_transaction_t *t) {
    // int dc = (int)t->user;
    // gpio_set_level(PIN_NUM_DC, dc);
}



void txrx_init(spi_device_handle_t spi) {

  // Initialize non-SPI GPIOs
  gpio_config_t io_conf = {};
  io_conf.pin_bit_mask = (
        (1ULL << PIN_NUM_LED_YELLOW)
      | (1ULL << PIN_NUM_LED_GREEN)
      // | (1ULL << PIN_NUM_IRQ)
      // | (1ULL << PIN_NUM_RSTN)
  );
  io_conf.mode = GPIO_MODE_OUTPUT;
  io_conf.pull_up_en = true;

  gpio_config(&io_conf);

  // gpio_set_level(PIN_NUM_LED_YELLOW, 1);
  // gpio_set_level(PIN_NUM_LED_GREEN,  1);
  // gpio_set_level(PIN_NUM_LED_GREEN,  1);

}

void app_main(void) {
  static httpd_handle_t server = NULL;
  ESP_LOGI(TAG, "HELLO");

  esp_err_t ret;

  spi_bus_config_t buscfg = {
    .miso_io_num = PIN_NUM_MISO,
    .mosi_io_num = PIN_NUM_MOSI,
    .sclk_io_num = PIN_NUM_CLK,
    .quadwp_io_num = -1,
    .quadhd_io_num = -1,
    .max_transfer_sz = 256
  };

  spi_device_interface_config_t devcfg = {
    .clock_speed_hz = 1 * 1000 * 1000, // Clock out at 8 MHz
    .mode = 0,                          // SPI mode 0
    .spics_io_num = PIN_NUM_CS,         // CS pin
    .queue_size = 7,                    // We want to be able to queue 7 transactions at a time
    .pre_cb = spi_pre_transfer_callback // Specify pre-transfer callback to handle D/C line
  };

  //Initialize the SPI bus
  ret = spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO);
  ESP_ERROR_CHECK(ret);

  // Attach the TXRX to the SPI bus
  ret = spi_bus_add_device(SPI2_HOST, &devcfg, &spi_txrx);

  ESP_ERROR_CHECK(ret);

  // Initialize the RXRX
  txrx_init(spi_txrx);

  {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
  }

  wifi_init_sta();

  server = start_webserver();

  uint8_t txrx_data [4];

  while (server) {
    // txrx_data[0] = (0 << 5) /* mode=read */ | 0 /* addr[13:8] */;
    // txrx_data[1] = 0x0d /* addr[7:0] */ ;
    // txrx_data[2] = 0;
    // txrx_data[3] = 0;
    // ESP_LOGI(TAG, "%x.%x.%x.%x", txrx_data[0], txrx_data[1], txrx_data[2], txrx_data[3]);
    // rdwr_spi(spi_txrx, &txrx_data, 3);
    // ESP_LOGI(TAG, "%x.%x.%x.%x", txrx_data[0], txrx_data[1], txrx_data[2], txrx_data[3]);
    sleep(1);
  }
}
