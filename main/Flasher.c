// Flasher
// See https://components.espressif.com/components/espressif/esp-serial-flasher/versions/1.3.1/examples/esp32_usb_cdc_acm_example?language=en

static const char TAG[] = "Flasher";

#include "revk.h"
#include "esp_sleep.h"
#include "esp_task_wdt.h"
#include <netdb.h>
#include <driver/gpio.h>
#include <driver/uart.h>
#include <driver/rtc_io.h>
#include <driver/sdmmc_host.h>
#include <sdmmc_cmd.h>
#include "esp_http_client.h"
#include <esp_http_server.h>
#include "esp_crt_bundle.h"
#include "esp_vfs_fat.h"
#include <sys/dirent.h>
#include "usb/usb_host.h"
#include "usb/cdc_acm_host.h"
#include "esp_loader.h"

struct
{
   uint8_t die:1;               // Shutdown
   uint8_t connected:1;         // Connected
   uint8_t starts:2;            // Client starts count (to spot unstable/rebooting)
   uint8_t downloader:1;        // Client is waiting for download.
   uint8_t empty:1;             // Client shows traffic suggesting not flashed
   uint8_t atefail:1;           // Client shows ATE PASS
   uint8_t atepass:1;           // Client shows ATE PASS
} volatile b;

#ifdef  CONFIG_TINYUSB_MSC_MOUNT_PATH
const char sd_dir[] = CONFIG_TINYUSB_MSC_MOUNT_PATH;
#else
const char sd_dir[] = "/sd";
#endif

led_strip_handle_t strip = NULL;

const char *
mqtt_client_callback (int client, const char *prefix, const char *target, const char *suffix, jo_t j)
{
   if (client || !prefix || target || strcmp (prefix, topiccommand) || !suffix)
      return NULL;              // Not for us or not a command from main MQTTS

   // TODO connect wifi, kick off upgrade check

   return NULL;
}

void
led_task (void *arg)
{
   if (strip)
   {
      while (!b.die)
      {
         // TODO set of main LEDs
         REVK_ERR_CHECK (led_strip_refresh (strip));
         usleep (100000);
      }
      for (int i = 0; i < 11; i++)
         revk_led (strip, i, 0, 0);
      REVK_ERR_CHECK (led_strip_refresh (strip));
   }
   vTaskDelete (NULL);
}

void
btn_task (void *arg)
{
   if (btn.set)
   {
      while (!b.die)
      {
         // TODO check button
         usleep (100000);
      }
   }
   vTaskDelete (NULL);
}

static void
usb_lib_task (void *arg)
{
   while (1)
   {
      uint32_t event_flags;
      usb_host_lib_handle_events (portMAX_DELAY, &event_flags);
      if (event_flags & USB_HOST_LIB_EVENT_FLAGS_NO_CLIENTS)
         usb_host_device_free_all ();
   }
}

static void
client_event (const cdc_acm_host_dev_event_data_t * event, void *user_ctx)
{
   switch (event->type)
   {
   case CDC_ACM_HOST_ERROR:
      ESP_LOGE (TAG, "CDC-ACM error has occurred, err_no = %i", event->data.error);
      break;
   case CDC_ACM_HOST_DEVICE_DISCONNECTED:
      ESP_LOGE (TAG, "Device suddenly disconnected");
      cdc_acm_host_close (event->data.cdc_hdl);
      b.connected = 0;
      break;
   case CDC_ACM_HOST_SERIAL_STATE:
      ESP_LOGE (TAG, "Serial state notif 0x%04X", event->data.serial_state.val);
      break;
   case CDC_ACM_HOST_NETWORK_CONNECTION:
   default:
      ESP_LOGE (TAG, "Unsupported CDC event: %i", event->type);
      break;
   }
}

static bool
client_rx (const uint8_t * data, size_t data_len, void *arg)
{
   ESP_LOGD (TAG, "Rx %d bytes", data_len);
   //printf ("{%.*s}", data_len, data);
   // We look for specific patterns
   static char buf[30];
   while (data_len--)
   {
      memmove (buf, buf + 1, sizeof (buf) - 1);
      buf[sizeof (buf) - 1] = *data++;
      if (!memcmp (buf, "invalid header: 0xffffffff\r\n", 28))
         b.empty = 1;
      if (!memcmp (buf, "Waiting for download\r\n", 22))
         b.downloader = 1;
      if (!memcmp (buf, "\nATE: PASS\n", 11))
         b.atepass = 1;
      if (!memcmp (buf, "\nATE: FAIL\n", 11))
         b.atefail = 1;
      if (!memcmp (buf, "ESP-ROM:", 8) && b.starts < 3)
         b.starts++;
   }
   return true;
}

//--------------------------------------------------------------------------------
// Main
void
app_main ()
{
   esp_err_t e = 0;
   ESP_LOGE (TAG, "Started");
   revk_boot (&mqtt_client_callback);
   revk_start ();
   revk_gpio_output (pwr3, 0);
   revk_gpio_output (pwr5, 0);
   {                            // LED
      led_strip_config_t strip_config = {
         .strip_gpio_num = led.num,
         .max_leds = 11,
         .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB,
         .led_model = LED_MODEL_WS2812, // LED strip model
         .flags.invert_out = led.invert,
      };
      led_strip_rmt_config_t rmt_config = {
         .clk_src = RMT_CLK_SRC_DEFAULT,        // different clock source can lead to different power consumption
         .resolution_hz = 10 * 1000 * 1000,     // 10 MHz
      };
#ifdef	CONFIG_IDF_TARGET_ESP32S3
      rmt_config.flags.with_dma = true;
#endif
      REVK_ERR_CHECK (led_strip_new_rmt_device (&strip_config, &rmt_config, &strip));
      revk_led (strip, 10, 255, revk_rgb ('B'));
      revk_task ("led", led_task, NULL, 4);
   }
   revk_gpio_input (btn);
   revk_task ("btn", btn_task, NULL, 4);
   // SD card set up
   revk_gpio_input (sdcd);
   sdmmc_card_t *card = NULL;
   sdmmc_slot_config_t slot = SDMMC_SLOT_CONFIG_DEFAULT ();
   sdmmc_host_t host = SDMMC_HOST_DEFAULT ();
   slot.clk = sdclk.num;
   slot.cmd = sdcmd.num;
   slot.d0 = sddat0.num;
   slot.d1 = sddat1.set ? sddat1.num : -1;
   slot.d2 = sddat2.set ? sddat2.num : -1;
   slot.d3 = sddat3.set ? sddat3.num : -1;
   slot.width = (sddat2.set && sddat3.set ? 4 : sddat1.set ? 2 : 1);
   if (slot.width == 1)
      slot.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;    // Old boards?
   host.max_freq_khz = SDMMC_FREQ_HIGHSPEED;
   host.slot = SDMMC_HOST_SLOT_1;
   if (serial3v3)
   {
      // TODO
      ESP_LOGE (TAG, "Serial 3v3 is not done yet");
   } else
   {                            // USB init
      const usb_host_config_t host_config = {
         .skip_phy_setup = false,
         .intr_flags = ESP_INTR_FLAG_LEVEL1,
      };
      e = usb_host_install (&host_config);
      if (!e)
         e = cdc_acm_host_install (NULL);
      if (e)
      {
         revk_led (strip, 10, 255, revk_rgb ('R'));
         ESP_LOGE (TAG, "USB init failed %s", esp_err_to_name (e));
         jo_t j = jo_object_alloc ();
         jo_string (j, "error", esp_err_to_name (e));
         jo_int (j, "code", e);
         revk_error ("USB", &j);
      }
   }
   revk_task ("usb-lib", usb_lib_task, NULL, 4);
   // Main loop
   while (!b.die)
   {
      revk_led (strip, 10, 255, revk_rgb ('Y'));
      ESP_LOGI (TAG, "Waiting SD");
      // Wait for SD card
      while (!revk_gpio_get (sdcd))
         usleep (100000);
      ESP_LOGI (TAG, "Mounting SD");
      // Mount SD card
      esp_vfs_fat_sdmmc_mount_config_t mount_config = {
         .format_if_mount_failed = 1,
         .max_files = 2,
         .allocation_unit_size = 16 * 1024,
         .disk_status_check_enable = 1,
      };
      e = esp_vfs_fat_sdmmc_mount (sd_dir, &host, &slot, &mount_config, &card);
      if (e != ESP_OK)
      {
         revk_led (strip, 10, 255, revk_rgb ('R'));
         ESP_LOGE (TAG, "SD mount failed %s", esp_err_to_name (e));
         jo_t j = jo_object_alloc ();
         if (e == ESP_FAIL)
            jo_string (j, "error", "Failed to mount");
         else
            jo_string (j, "error", "Failed to initialise");
         jo_int (j, "code", e);
         revk_error ("SD", &j);
         while (revk_gpio_get (sdcd))
            usleep (100000);
         continue;
      }
      ESP_LOGI (TAG, "Mounted SD");
      revk_led (strip, 10, 255, revk_rgb ('G'));

      if (serial3v3)
      {
         // TODO
      } else
      {
         revk_gpio_output (pwr5, 1);    // device on
         usb_host_lib_set_root_port_power (true);
         const cdc_acm_host_device_config_t dev_config = {
            .connection_timeout_ms = 1000,
            .out_buffer_size = 512,
            .in_buffer_size = 512,
            .user_arg = NULL,
            .event_cb = client_event,
            .data_cb = client_rx,
         };
         while (revk_gpio_get (sdcd) && !b.die)
         {
            cdc_acm_dev_hdl_t cdc_dev = NULL;
            b.downloader = b.empty = b.starts = b.atepass = b.atefail = 0;
            b.connected = 1;
            e = cdc_acm_host_open (0, 0, 0, &dev_config, &cdc_dev);
            if (e)
            {
               b.connected = 0;
               usleep (100000);
               continue;
            }
            void reportstate (void)
            {
               if (!b.connected)
                  ESP_LOGE (TAG, "Disconnected");
               else if (b.downloader)
                  ESP_LOGE (TAG, "Ready for download");
               else if (b.empty)
                  ESP_LOGE (TAG, "Empty ready to flash");
               else if (b.starts == 3)
                  ESP_LOGE (TAG, "Looping");
               else if (b.atepass)
                  ESP_LOGE (TAG, "ATE PASS");
               else if (b.atefail)
                  ESP_LOGE (TAG, "ATE FAIL");
               else
                  ESP_LOGE (TAG, "Timeout");
            }
            ESP_LOGE (TAG, "USB Connect");
            int count = 50;     // 5s
            while (revk_gpio_get (sdcd) && !b.die && b.connected && !b.downloader && !b.empty && b.starts < 3 && !b.atepass
                   && !b.atefail && count--)
               usleep (100000);
            reportstate ();

            if (b.connected && (b.empty || b.starts == 3 || b.atefail))
            {                   // Flashing
               ESP_LOGE (TAG, "Restart for download");
               cdc_acm_host_set_control_line_state (cdc_dev, true, true); // TODO
               usleep (100000);
               b.downloader = b.empty = b.starts = b.atepass = b.atefail = 0;
               cdc_acm_host_set_control_line_state (cdc_dev, true, false); // TODO
               count = 50;
               while (revk_gpio_get (sdcd) && !b.die && b.connected && !b.downloader && !b.empty && b.starts < 3 && !b.atepass
                      && !b.atefail && count--)
                  usleep (100000);
               reportstate ();
               if (b.downloader)
               {                // ready to download

               }
            }

            if (b.connected)
            {
               ESP_LOGE (TAG, "Wait try again");
               while (b.connected)
                  usleep (100000);
            }
            sleep (1);
         }
         usb_host_lib_set_root_port_power (false);
         revk_gpio_output (pwr5, 0);
      }
      ESP_LOGI (TAG, "Dismounting SD");
      esp_vfs_fat_sdcard_unmount (sd_dir, card);
   }
   usb_host_uninstall ();
}
