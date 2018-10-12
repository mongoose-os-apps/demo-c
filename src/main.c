#include "mgos.h"
#include "mgos_mqtt.h"
#ifdef MGOS_HAVE_WIFI
#include "mgos_wifi.h"
#endif

static void led_timer_cb(void *arg) {
  bool val = mgos_gpio_toggle(mgos_sys_config_get_board_led1_pin());
  LOG(LL_INFO, ("%s uptime: %.2lf, RAM: %lu, %lu free", val ? "Tick" : "Tock",
                mgos_uptime(), (unsigned long) mgos_get_heap_size(),
                (unsigned long) mgos_get_free_heap_size()));
  (void) arg;
}

static void net_cb(int ev, void *evd, void *arg) {
  switch (ev) {
    case MGOS_NET_EV_DISCONNECTED:
      LOG(LL_INFO, ("%s", "Net disconnected"));
      break;
    case MGOS_NET_EV_CONNECTING:
      LOG(LL_INFO, ("%s", "Net connecting..."));
      break;
    case MGOS_NET_EV_CONNECTED:
      LOG(LL_INFO, ("%s", "Net connected"));
      break;
    case MGOS_NET_EV_IP_ACQUIRED:
      LOG(LL_INFO, ("%s", "Net got IP address"));
      break;
  }

  (void) evd;
  (void) arg;
}

#ifdef MGOS_HAVE_WIFI
static void wifi_cb(int ev, void *evd, void *arg) {
  switch (ev) {
    case MGOS_WIFI_EV_STA_DISCONNECTED:
      LOG(LL_INFO, ("WiFi STA disconnected %p", arg));
      break;
    case MGOS_WIFI_EV_STA_CONNECTING:
      LOG(LL_INFO, ("WiFi STA connecting %p", arg));
      break;
    case MGOS_WIFI_EV_STA_CONNECTED:
      LOG(LL_INFO, ("WiFi STA connected %p", arg));
      break;
    case MGOS_WIFI_EV_STA_IP_ACQUIRED:
      LOG(LL_INFO, ("WiFi STA IP acquired %p", arg));
      break;
    case MGOS_WIFI_EV_AP_STA_CONNECTED: {
      struct mgos_wifi_ap_sta_connected_arg *aa =
          (struct mgos_wifi_ap_sta_connected_arg *) evd;
      LOG(LL_INFO, ("WiFi AP STA connected MAC %02x:%02x:%02x:%02x:%02x:%02x",
                    aa->mac[0], aa->mac[1], aa->mac[2], aa->mac[3], aa->mac[4],
                    aa->mac[5]));
      break;
    }
    case MGOS_WIFI_EV_AP_STA_DISCONNECTED: {
      struct mgos_wifi_ap_sta_disconnected_arg *aa =
          (struct mgos_wifi_ap_sta_disconnected_arg *) evd;
      LOG(LL_INFO,
          ("WiFi AP STA disconnected MAC %02x:%02x:%02x:%02x:%02x:%02x",
           aa->mac[0], aa->mac[1], aa->mac[2], aa->mac[3], aa->mac[4],
           aa->mac[5]));
      break;
    }
  }
  (void) arg;
}
#endif /* MGOS_HAVE_WIFI */

static void button_cb(int pin, void *arg) {
  char topic[100];
  snprintf(topic, sizeof(topic), "/devices/%s/events",
           mgos_sys_config_get_device_id());
  bool res = mgos_mqtt_pubf(topic, 0, false /* retain */,
                            "{total_ram: %lu, free_ram: %lu}",
                            (unsigned long) mgos_get_heap_size(),
                            (unsigned long) mgos_get_free_heap_size());
  char buf[8];
  int x = mgos_gpio_toggle(mgos_sys_config_get_board_led3_pin());
  LOG(LL_INFO, ("Pin: %s, published: %s x %d", mgos_gpio_str(pin, buf),
                res ? "yes" : "no", x));
  (void) arg;
}

enum mgos_app_init_result mgos_app_init(void) {
  char buf[8];
  /* Blink built-in LED every second */
  int led_pin = mgos_sys_config_get_board_led1_pin();
  if (led_pin >= 0) {
    LOG(LL_INFO, ("LED pin %s", mgos_gpio_str(led_pin, buf)));
    mgos_gpio_set_mode(led_pin, MGOS_GPIO_MODE_OUTPUT);
    mgos_set_timer(1000, MGOS_TIMER_REPEAT, led_timer_cb, NULL);
  }
  mgos_gpio_set_mode(mgos_sys_config_get_board_led3_pin(),
                     MGOS_GPIO_MODE_OUTPUT);
  mgos_gpio_write(mgos_sys_config_get_board_led3_pin(), 0);

  /* Publish to MQTT on button press */
  int btn_pin = mgos_sys_config_get_board_btn1_pin();
  if (btn_pin >= 0) {
    enum mgos_gpio_pull_type btn_pull;
    enum mgos_gpio_int_mode btn_int_edge;
    if (mgos_sys_config_get_board_btn1_pull_up()) {
      btn_pull = MGOS_GPIO_PULL_UP;
      btn_int_edge = MGOS_GPIO_INT_EDGE_NEG;
    } else {
      btn_pull = MGOS_GPIO_PULL_DOWN;
      btn_int_edge = MGOS_GPIO_INT_EDGE_POS;
    }
    LOG(LL_INFO, ("Button pin %s, active %s", mgos_gpio_str(btn_pin, buf),
                  (mgos_sys_config_get_board_btn1_pull_up() ? "low" : "high")));
    mgos_gpio_set_button_handler(btn_pin, btn_pull, btn_int_edge, 20, button_cb,
                                 NULL);
  }

  /* Network connectivity events */
  mgos_event_add_group_handler(MGOS_EVENT_GRP_NET, net_cb, NULL);

#ifdef MGOS_HAVE_WIFI
  mgos_event_add_group_handler(MGOS_WIFI_EV_BASE, wifi_cb, NULL);
#endif

  return MGOS_APP_INIT_SUCCESS;
}
