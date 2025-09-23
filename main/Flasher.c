// Flasher

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

struct
{
   uint8_t die:1;
} b;

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
   while (1)
   {
      REVK_ERR_CHECK (led_strip_refresh (strip));
      usleep (100000);
   }
}

//--------------------------------------------------------------------------------
// Main
void
app_main ()
{
   ESP_LOGE (TAG, "Started");
   revk_boot (&mqtt_client_callback);
   revk_start ();
   revk_gpio_output (pwr3, 0);
   revk_gpio_output (pwr5, 0);
   // LED
   led_strip_config_t strip_config = {
      .strip_gpio_num = led.num,
      .max_leds = 11,
      .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB,
      .led_model = LED_MODEL_WS2812,    // LED strip model
      .flags.invert_out = led.invert,
   };
   led_strip_rmt_config_t rmt_config = {
      .clk_src = RMT_CLK_SRC_DEFAULT,   // different clock source can lead to different power consumption
      .resolution_hz = 10 * 1000 * 1000,        // 10 MHz
   };
#ifdef	CONFIG_IDF_TARGET_ESP32S3
   rmt_config.flags.with_dma = true;
#endif
   REVK_ERR_CHECK (led_strip_new_rmt_device (&strip_config, &rmt_config, &strip));
   revk_led (strip, 10, 255, revk_rgb ('B'));
   revk_task ("led", led_task, NULL, 4);
   // TODO button task
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
   // Main loop
   while (!b.die)
   {
      revk_led (strip, 10, 255, revk_rgb ('Y'));
      ESP_LOGE (TAG, "Waiting SD");
      // Wait for SD card
      while (!revk_gpio_get (sdcd))
         usleep (100000);
      ESP_LOGE (TAG, "Mounting SD");
      // Mount SD card
      esp_vfs_fat_sdmmc_mount_config_t mount_config = {
         .format_if_mount_failed = 1,
         .max_files = 2,
         .allocation_unit_size = 16 * 1024,
         .disk_status_check_enable = 1,
      };
      esp_err_t e = esp_vfs_fat_sdmmc_mount (sd_dir, &host, &slot, &mount_config, &card);
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
      ESP_LOGE (TAG, "Mounted SD");
      revk_led (strip, 10, 255, revk_rgb ('G'));

      usb_host_config_t host_config = {
         .skip_phy_setup = false,
         .intr_flags = ESP_INTR_FLAG_LEVEL1,
         //.peripheral_map = BIT0,
      };
      e = usb_host_install (&host_config);
      if (e)
      {
         revk_led (strip, 10, 255, revk_rgb ('R'));
         ESP_LOGE (TAG, "USB init failed %s", esp_err_to_name (e));
         jo_t j = jo_object_alloc ();
         jo_string (j, "error", esp_err_to_name (e));
         jo_int (j, "code", e);
         revk_error ("USB", &j);
         while (revk_gpio_get (sdcd))
            usleep (100000);
         continue;
      }
      revk_gpio_output (pwr5, 1);       // device on
      while (revk_gpio_get (sdcd))
      {
         // Wait for device

         // TODO this is dummy for now.
         bool has_clients = true;
         bool has_devices = false;
         while (has_clients && revk_gpio_get (sdcd))
         {
            uint32_t event_flags;
            usb_host_lib_handle_events (portMAX_DELAY, &event_flags);
            if (event_flags & USB_HOST_LIB_EVENT_FLAGS_NO_CLIENTS)
            {
               ESP_LOGE (TAG, "Get FLAGS_NO_CLIENTS");
               if (ESP_OK == usb_host_device_free_all ())
               {
                  ESP_LOGE (TAG, "All devices marked as free, no need to wait FLAGS_ALL_FREE event");
                  has_clients = false;
               } else
               {
                  ESP_LOGE (TAG, "Wait for the FLAGS_ALL_FREE");
                  has_devices = true;
               }
            }
            if (has_devices && event_flags & USB_HOST_LIB_EVENT_FLAGS_ALL_FREE)
            {
               ESP_LOGE (TAG, "Get FLAGS_ALL_FREE");
               has_clients = false;
            }
         }
         ESP_LOGE (TAG, "No more clients and devices, uninstall USB Host library");


         sleep (1);
      }
      revk_gpio_output (pwr5, 0);
      usb_host_uninstall ();
      ESP_LOGE (TAG, "Dismounting SD");
      esp_vfs_fat_sdcard_unmount (sd_dir, card);
   }
}
