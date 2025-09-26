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
#include "esp32_usb_cdc_acm_port.h"

struct
{
   uint8_t die:1;               // Shutdown
   uint8_t wifi:1;              // WiFi connected
   uint8_t connected:1;         // Connected
} volatile b;
uint8_t progress = 0;           // Progress (LED)
char ledf = 'K',
   ledt = 'K',
   ledsd = 'B';                 // LED from/to and SD

#define	BLOCK	4096
uint8_t block[BLOCK];

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
   if (!strcmp (suffix, "wifi"))
      b.wifi = 1;
   return NULL;
}

void
led_task (void *arg)
{
   if (strip)
   {
      while (!b.die)
      {
         uint32_t f = revk_rgb (ledf);
         uint32_t t = revk_rgb (ledt);
         uint8_t p = progress;
         for (int i = 0; i < 10; i++)
         {
            uint8_t l = p;
            if (l > 10)
               l = 10;
            p -= l;
            led_strip_set_pixel (strip, i,      //
                                 gamma8[l * ((t >> 16) & 255) / 10 + (10 - l) * ((f >> 16) & 255) / 10],        //
                                 gamma8[l * ((t >> 8) & 255) / 10 + (10 - l) * ((f >> 8) & 255) / 10],  //
                                 gamma8[l * ((t >> 0) & 255) / 10 + (10 - l) * ((f >> 0) & 255) / 10]);
         }
         revk_led (strip, 10, 255, revk_rgb (ledsd));
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
set_led (uint8_t p, char f, char t)
{                               // Set LED display state
   progress = p;
   if (f)
      ledf = f;
   if (t)
      ledt = t;
   //ESP_LOGE (TAG, "LED %c %c %d%%", ledf, ledt, progress);
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

void
host_error_cb (void)
{
   ESP_LOGE (TAG, "Host error");
}

void
device_disconnect_cb (void)
{
   ESP_LOGE (TAG, "Host disconnect");
   b.connected = 0;
}

enum
{
   STATUS_ERROR,
   STATUS_SILENT,               // no data
   STATUS_EMPTY,                // 0xffffffff error
   STATUS_LOOPING,              // Starting multiple times
   STATUS_FAIL,                 // ATE: FAIL
   STATUS_PASS,                 // ATE: PASS
   STATUS_TIMEOUT,              // Not looping but not pass/fail
} target_status (void)
{
   uint32_t to = uptime () + 5;
   char buf[100];
   uint8_t p = 0;
   uint8_t rst = 0;
   while (uptime () <= to)
   {
      uint8_t c;
      esp_loader_error_t e = loader_port_read (&c, 1, 100);
      if (e == ESP_LOADER_ERROR_TIMEOUT)
         continue;
      if (e)
         return STATUS_ERROR;
      if (c >= ' ')
      {
         if (p < sizeof (buf) - 1)
            buf[p++] = c;
         continue;
      }
      if (!p)
         continue;
      buf[p] = 0;
      p = 0;
      if (!strcmp (buf, "invalid header: 0xffffffff"))
         return STATUS_EMPTY;
      if (!strcmp (buf, "ATE: PASS"))
         return STATUS_PASS;
      if (!strcmp (buf, "ATE: FAIL"))
         return STATUS_FAIL;
      if (!strncmp (buf, "Build:", 6) && rst++ > 4)
         return STATUS_LOOPING;
   }
   if (!rst)
      return STATUS_SILENT;
   return STATUS_TIMEOUT;
};

uint32_t
chip_id (char *dir)
{                               // Get chip ID, dir has to have enough space
   const char *const chips[] =
      { "ESP8266", "ESP32", "ESP32S2", "ESP32C3", "ESP32S3", "ESP32C2", "ESP32C5", "ESP32H2", "ESP32C6", "ESP32P4", "ESP_MAX",
      "ESP_UNKNOWN"
   };
   *dir = 0;
   dir = stpcpy (dir, sd_dir);
   dir = stpcpy (dir, "/");
   target_chip_t chip = esp_loader_get_target ();
   if (chip < sizeof (chips) / sizeof (*chips))
      dir = stpcpy (dir, chips[chip]);
   uint8_t psram = 0;
   switch (chip)
   {
   case ESP32S3_CHIP:
      {                         // We can get some more info...
         dir = stpcpy (dir, "MC");      // Always MC
         const uint32_t efuse = 0x60007000;
         const uint32_t block1 = efuse + 0x44;
         uint32_t r1,
           r2;
         if (!esp_loader_read_register (block1 + 12, &r1))
         {
            r1 = (r1 >> 21) & 7;
            if (r1 == 1)
               dir = stpcpy (dir, "PICO");
         }
         if (!esp_loader_read_register (block1 + 16, &r1) && !esp_loader_read_register (block1 + 20, &r2))
            psram = ((r1 >> 3) & 3) | ((r2 >> 17) & 4);
         break;
      }
   default:
      break;                    // Other chip types may be needed
   }
   uint32_t size = 0;
   if (!esp_loader_flash_detect_size (&size) && size)
      dir += sprintf (dir, "N%u", (uint8_t) (size / 1024 / 1024));
   if (psram)
      dir += sprintf (dir, "R%u", psram);
   dir = stpcpy (dir, "/");
   return size;
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
         ledsd = 'R';
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
      ledsd = 'Y';
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
         ledsd = 'R';
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
      ledsd = 'G';

      // TODO how do we do serial via UART?

      ESP_LOGE (TAG, "Power on");
      revk_gpio_output (pwr5, 1);       // device on
      usb_host_lib_set_root_port_power (true);
      while (revk_gpio_get (sdcd) && !b.die)
      {
         set_led (0, 'K', 'K');
         if (b.wifi)
         {
            b.wifi = 0;
            revk_command ("upgrade", NULL);
         }
         if (revk_shutting_down (NULL))
            break;
         loader_esp32_usb_cdc_acm_config_t config = {
            .acm_host_error_callback = host_error_cb,
            .device_disconnected_callback = device_disconnect_cb,
            .device_pid = 0x1001,       // USB direct not serial chip...
            .connection_timeout_ms = 1000,
            .out_buffer_size = BLOCK,
            //.acm_host_serial_state_callback = host_serial_cb,
         };
         esp_loader_error_t e = loader_port_esp32_usb_cdc_acm_init (&config);
         if (!e)
         {
            set_led (10, 'K', 'O');
            b.connected = 1;
            uint8_t status = target_status ();
            if (b.connected && (flashalways || status != STATUS_PASS) && status != STATUS_ERROR)
            {
               ESP_LOGE (TAG, "Bootload");
               esp_loader_connect_args_t a = ESP_LOADER_CONNECT_DEFAULT ();
               e = esp_loader_connect_with_stub (&a);   // Some chips don't work with stub
               if (!e)
               {
                  char dir[200] = "";   // For dir and file names
                  uint32_t size = chip_id (dir);
                  uint8_t mac[6] = { 0 };
                  if (b.connected && !esp_loader_read_mac (mac))
                     ESP_LOGE (TAG, "MAC %02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
                  ESP_LOGE (TAG, "Dir %s", dir);
                  char *fn = dir + strlen (dir);
                  char *manifest = NULL;
                  jo_t j = NULL;
                  strcpy (fn, "manifest.json");
                  int f = open (dir, O_RDONLY);
                  if (f < 0)
                  {
                     ESP_LOGE (TAG, "Failed to open %s", dir);
                     status = STATUS_ERROR;
                  } else
                  {
                     struct stat s = { 0 };
                     fstat (f, &s);
                     if (s.st_size && s.st_size < 10000)
                     {
                        manifest = malloc (s.st_size);
                        if (read (f, manifest, s.st_size) != s.st_size)
                        {
                           status = STATUS_ERROR;
                           ESP_LOGE (TAG, "Manifest read fail %s", dir);
                        } else
                        {
                           j = jo_parse_mem (manifest, s.st_size);
                           if (!j)
                           {
                              status = STATUS_ERROR;
                              ESP_LOGE (TAG, "Manifest bad JSON %s", dir);
                           }
                        }
                     } else
                     {
                        status = STATUS_ERROR;
                        ESP_LOGE (TAG, "Manifest bad size (%u) %s", s.st_size, dir);
                     }
                     close (f);
                  }
                  if (flasherase && status != STATUS_EMPTY && status != STATUS_ERROR)
                  {             // Full erase
                     ESP_LOGE (TAG, "Erase whole flash");
                     uint32_t p = 0;
                     while (p < size)
                     {
                        set_led (p * 100 / size, 'B', 'K');
                        e = esp_loader_flash_erase_region (p, BLOCK);
                        if (e)
                        {
                           status = STATUS_ERROR;
                           ESP_LOGE (TAG, "Flash failed");
                           break;
                        }
                        p += BLOCK;
                     }
                  }
                  uint32_t total = 0;   // Total flash size
                  if (status != STATUS_ERROR && j)
                  {             // Check manifest
                     jo_rewind (j);
                     if (jo_here (j) == JO_OBJECT)
                        jo_find (j, "manifest");
                     if (jo_here (j) != JO_ARRAY)
                     {
                        status = STATUS_ERROR;
                        ESP_LOGE (TAG, "Manifest not array");
                     } else
                     {
                        while (jo_next (j) == JO_OBJECT && status != STATUS_ERROR)
                        {
                           *fn = 0;
                           while (jo_next (j) == JO_TAG)
                           {
                              if (!jo_strcmp (j, "file"))
                              {
                                 if (jo_next (j) == JO_STRING)
                                    jo_strncpy (j, fn, sizeof (dir) - (fn - dir) - 1);
                              } else
                                 jo_next (j);
                           }
                           jo_skip (j);
                           if (status != STATUS_ERROR && !*fn)
                           {
                              status = STATUS_ERROR;
                              ESP_LOGE (TAG, "Missing filename");
                           }
                           if (status != STATUS_ERROR)
                           {
                              int f = open (dir, O_RDONLY);
                              if (f < 0)
                              {
                                 status = STATUS_ERROR;
                                 ESP_LOGE (TAG, "Missing file %s", dir);
                              } else
                              {
                                 struct stat s = { 0 };
                                 fstat (f, &s);
                                 total += s.st_size;
                                 close (f);
                              }
                           }
                        }
                     }
                  }
                  if (status != STATUS_ERROR)
                  {             // Flash
                     ESP_LOGE (TAG, "Flash %u bytes", total);
                     uint32_t bytes = 0;
                     jo_rewind (j);
                     if (jo_here (j) == JO_OBJECT)
                        jo_find (j, "manifest");
                     while (jo_next (j) == JO_OBJECT && status != STATUS_ERROR)
                     {
                        uint32_t address = 0;
                        *fn = 0;
                        while (jo_next (j) == JO_TAG)
                        {
                           if (!jo_strcmp (j, "file"))
                           {
                              if (jo_next (j) == JO_STRING)
                                 jo_strncpy (j, fn, sizeof (dir) - (fn - dir) - 1);
                           } else if (!jo_strcmp (j, "address"))
                           {
                              jo_type_t t = jo_next (j);
                              if (t == JO_STRING)
                              {
                                 char temp[10];
                                 jo_strncpy (j, temp, sizeof (temp));
                                 address = strtoul (temp, NULL, 16);
                              } else if (t == JO_NUMBER)
                                 address = jo_read_int (j);
                           } else
                              jo_next (j);
                        }
                        int f = open (dir, O_RDONLY);
                        if (f >= 0)
                        {
                           struct stat s = { 0 };
                           fstat (f, &s);
                           ESP_LOGE (TAG, "Flash %s to 0x%lX size %u", dir, address, s.st_size);
                           e = esp_loader_flash_start (address, s.st_size, BLOCK);
                           if (!e)
                           {
                              uint32_t p = 0;
                              while (p < s.st_size)
                              {
                                 set_led (bytes * 100 / total, 'K', 'B');
                                 uint32_t s = read (f, block, BLOCK);
                                 e = esp_loader_flash_write (block, s);
                                 if (e)
                                 {
                                    ESP_LOGE (TAG, "Flash fail");
                                    status = STATUS_ERROR;
                                    break;
                                 }
                                 p += s;
                                 bytes += s;
                              }
                           }
                           e = esp_loader_flash_finish (false);
                           close (f);
                        }
                     }
                     // TODO verify
                  }
                  // TODO log file
                  jo_free (&j);
                  free (manifest);
               }

               if (b.connected)
               {
                  ESP_LOGE (TAG, "Reset");
                  esp_loader_reset_target ();
                  status = target_status ();
               }
            }
            switch (status)
            {
            case STATUS_PASS:
               set_led (100, 'K', 'G');
               break;
            case STATUS_FAIL:
               set_led (100, 'K', 'R');
               break;
            case STATUS_TIMEOUT:
               set_led (50, 'R', 'G');
               break;
            case STATUS_LOOPING:
               set_led (90, 'K', 'R');
               break;
            case STATUS_SILENT:
               set_led (80, 'K', 'R');
               break;
            case STATUS_EMPTY:
               set_led (70, 'K', 'R');
               break;
            default:
               set_led (10, 'K', 'R');
            }
            if (b.connected)
            {
               ESP_LOGE (TAG, "Wait disconnect");
               while (b.connected)
                  usleep (100000);
            }
            loader_port_esp32_usb_cdc_acm_deinit ();
         }
      }
      ESP_LOGE (TAG, "Power off");
      if (b.connected)
         usb_host_lib_set_root_port_power (false);
      revk_gpio_output (pwr5, 0);
      ESP_LOGI (TAG, "Dismounting SD");
      esp_vfs_fat_sdcard_unmount (sd_dir, card);
      ledsd = 'B';
      if (revk_shutting_down (NULL))
         break;
   }
   usb_host_uninstall ();

   while (revk_shutting_down (NULL))
   {
      int p = revk_ota_progress ();
      if (p >= 0 && p <= 100)
         set_led (p, 'K', 'Y');
      usleep (100000);
   }
}
