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
#ifdef  CONFIG_MBEDTLS_CERTIFICATE_BUNDLE
#include "esp_crt_bundle.h"
#endif
#include "esp_vfs_fat.h"
#include <sys/dirent.h>
#include "usb/usb_host.h"
#include "usb/cdc_acm_host.h"
#include "esp_loader.h"
#include "esp32_usb_cdc_acm_port.h"
#include <math.h>

struct
{
   uint8_t die:1;               // Shutdown
   uint8_t wifi:1;              // WiFi connected
   uint8_t reload:1;            // Re load manifest, etc
   uint8_t forceerase:1;        // Force erase
   uint8_t voltage3v3:1;        // 3.3V not 5V
   uint8_t erase:1;             // Manifest says erase
   uint8_t nobtn:1;             // Disable button erase
   uint8_t nocheck:1;           // Disable check
   uint8_t connected:1;         // Connected
   uint8_t fileerror:1;         // File error
   uint8_t checked:1;           // Upgrade checked
   uint8_t manifestidprefix:1;  // Manifest ID is prefix
} volatile b;
uint16_t manifests = 0;         // Bit map of manifests on SD
uint8_t progress = 0;           // Progress (LED)
char ledf = 'K',
   ledt = 'K',
   ledsd = 'B';                 // LED from/to and SD
uint8_t mac[6] = { 0 };         // Found mac
char chip[30] = { 0 };          // Found chip type

uint32_t badusb = 0;            // Last devices 0 even

uint32_t flashsize = 0;         // Found flash size

char *manifestjson = NULL;      // Loaded manifest JSON
uint32_t manifestsize = 0;      // Total flash bytes
jo_t j = NULL;                  // Parsed manifest JSON
char *manifestid = NULL;        // ID check appname+suffix
char *manifestversion = NULL;   // ID check version
char *manifestbuild = NULL;     // ID check builddate
char *manifestsetting = NULL;   // Manifest setting object
uint32_t manifestsettinglen = 0;        // Len

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
      uint8_t tick = 0;
      while (!b.die)
      {
         tick++;
         uint32_t f = revk_rgb (ledf);
         uint32_t t = revk_rgb (ledt);
         uint8_t p = progress;
         if (f != t && p < 5)
            p = 5;              // don't do all off
         for (int i = 0; i < 10; i++)
         {
            uint8_t l = p;
            if (l > 10)
               l = 10;
            p -= l;
            if ((b.fileerror && (tick & 1)) || (f == t && i != progress / 10))
               led_strip_set_pixel (strip, i, 0, 0, 0); // Off
            else
               led_strip_set_pixel (strip, i,   //
                                    gamma8[l * ((t >> 16) & 255) / 10 + (10 - l) * ((f >> 16) & 255) / 10],     //
                                    gamma8[l * ((t >> 8) & 255) / 10 + (10 - l) * ((f >> 8) & 255) / 10],       //
                                    gamma8[l * ((t >> 0) & 255) / 10 + (10 - l) * ((f >> 0) & 255) / 10]);
         }
         if (b.fileerror && !(tick & 1))
            revk_led (strip, 10, 255, revk_rgb ('R'));
         else
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
{                               // Set LED display state (if f==t then one LED at progress point)
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
         if (revk_gpio_get (btn))
         {
            if (b.connected)
            {
               if (!b.nobtn)
               {
                  b.forceerase = 1;
                  b.reload = 1;
               }
            } else if (manifests)
            {
               uint8_t m = manifest;
               do
               {
                  m++;
                  if (m == 10)
                     m = 0;
               }
               while (!(manifests & (1 << m)));
               if (m != manifest)
               {
                  jo_t j = jo_object_alloc ();
                  jo_int (j, "manifest", m);
                  revk_setting (j);
                  jo_free (&j);
                  b.checked = 0;
                  b.reload = 1;
               }
            }
            while (revk_gpio_get (btn))
               usleep (100000);
            continue;
         }
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
      if (!event_flags)
      {
         usb_host_lib_info_t i;
         usb_host_lib_info (&i);
         if (i.num_devices)
            badusb = 0;
         else
            badusb = uptime ();
      }
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
   ESP_LOGE (TAG, "Device disconnect");
   b.connected = 0;
}

void
device_connect_cb (usb_device_handle_t usb_dev)
{
   ESP_LOGE (TAG, "Device connect");
   b.connected = 1;
}

enum
{
   STATUS_ERROR,
   STATUS_SILENT,               // no data
   STATUS_EMPTY,                // 0xffffffff error
   STATUS_LOOPING,              // Starting multiple times
   STATUS_FAIL,                 // ATE: FAIL
   STATUS_PASS,                 // ATE: PASS
   STATUS_MISMATCH,             // Code needs flashing as version mismatch
   STATUS_TIMEOUT,              // Not looping but not pass/fail
} target_status (void)
{
   uint32_t to = uptime () + 5;
   char buf[100];
   uint8_t p = 0;
   uint8_t rst = 0;
   int8_t ate = 0;
   int8_t match = 0;
   char ok = !manifestsetting;
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
      while (buf[p] >= 'A' && buf[p] <= 'Z')
         p++;
      if (buf[p] == ':')
      {
         printf ("\033[1;34m%s\n", buf);
         buf[p++] = 0;
         while (buf[p] == ' ')
            p++;
         if (!strcmp (buf, "SPIWP"))
         {
            ok = !manifestsetting;
            if (rst++ > 5)
               return STATUS_LOOPING;
         } else if (!strcmp (buf, "ATE"))
         {
            if (!strcmp (buf + p, "PASS"))
            {
               ate = 1;
               if (ok)
                  break;
            } else if (!strcmp (buf + p, "FAIL"))
            {
               ate = -1;
               if (ok)
                  break;
            }
         } else if (!strcmp (buf, "ID"))
         {
            if (rst++ > 5)
               return STATUS_LOOPING;
            to = uptime () + 5;
            char *id = buf + p;
            while (buf[p] > ' ')
               p++;
            while (buf[p] && buf[p] <= ' ')
               buf[p++] = 0;
            char *version = buf + p;
            while (buf[p] > ' ')
               p++;
            while (buf[p] && buf[p] <= ' ')
               buf[p++] = 0;
            char *build = buf + p;
            while (buf[p] > ' ')
               p++;
            while (buf[p] && buf[p] <= ' ')
               buf[p++] = 0;
            if (manifestid && b.manifestidprefix ? strncmp (id, manifestid, strlen (manifestid)) : strcmp (id, manifestid))
            {
               ESP_LOGE (TAG, "ID mismatch [%s]/[%s]", id, manifestid);
               return STATUS_ERROR;
            }
            if (!match && manifestversion && strcmp (version, manifestversion))
            {
               match = -1;
               ESP_LOGE (TAG, "Version mismatch [%s]/[%s]", version, manifestversion);
            }
            if (!match && manifestbuild && strcmp (build, manifestbuild))
            {
               match = -1;
               ESP_LOGE (TAG, "Build mismatch [%s]/[%s]", build, manifestbuild);
            }
            if (!match && (manifestversion || manifestbuild))
               match = 1;
            if (match < 0)
               break;
            if (manifestsettinglen)
            {
               ESP_LOGE (TAG, "Setting %.*s", manifestsettinglen, manifestsetting);
               loader_port_write ((uint8_t *) manifestsetting, manifestsettinglen, 2000);
            }
         } else if (!strcmp (buf, "OK"))
         {
            if (ate)
               break;
            ok = 1;
         } else if (!strcmp (buf, "ERR"))
            return STATUS_ERROR;
      }
      p = 0;
   }
   if (match < 0)
      return STATUS_MISMATCH;
   if (!rst)
      return STATUS_SILENT;
   if (ate > 0)
      return STATUS_PASS;
   if (ate < 0)
      return STATUS_FAIL;
   return STATUS_TIMEOUT;
};

void
chip_info (void)
{                               // Get chip info
   const char *const chips[] = { "ESP8266", "ESP32", "ESP32S2", "ESP32C3", "ESP32S3", "ESP32C2",
      "ESP32C5", "ESP32H2", "ESP32C6", "ESP32P4", "ESP_MAX",
      "ESP_UNKNOWN"
   };
   char *c = chip;
   *c = 0;
   target_chip_t id = esp_loader_get_target ();
   if (id < sizeof (chips) / sizeof (*chips))
      c = stpcpy (c, chips[id]);
   uint8_t psram = 0;
   switch (id)
   {
   case ESP32S3_CHIP:
      {                         // We can get some more info...
         c = stpcpy (c, "MC");  // Always MC
         const uint32_t efuse = 0x60007000;
         const uint32_t block1 = efuse + 0x44;
         uint32_t r1,
           r2;
         if (!esp_loader_read_register (block1 + 12, &r1))
         {
            r1 = (r1 >> 21) & 7;
            if (r1 == 1)
               c = stpcpy (c, "PICO");
         }
         if (!esp_loader_read_register (block1 + 16, &r1) && !esp_loader_read_register (block1 + 20, &r2))
            psram = ((r1 >> 3) & 3) | ((r2 >> 17) & 4);
         break;
      }
   default:
      break;                    // Other chip types may be needed
   }
   flashsize = 0;
   if (!esp_loader_flash_detect_size (&flashsize) && flashsize)
      c += sprintf (c, "N%u", (uint8_t) (flashsize / 1024 / 1024));
   if (psram)
      c += sprintf (c, "R%u", psram);
   *c = 0;
   if (b.connected && !esp_loader_read_mac (mac))
      ESP_LOGE (TAG, "Chip %02X:%02X:%02X:%02X:%02X:%02X %s", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5], chip);
}

void
close_manifest (void)
{
   if (!manifestjson)
      return;
   jo_free (&j);
   free (manifestjson);
   manifestjson = NULL;
}

typedef void manifest_t (char *filename, char *url, int build, int f, uint32_t address, uint32_t size);
void
scan_manifest (manifest_t cb)
{
   if (!j)
      return;
   jo_rewind (j);
   if (jo_find (j, "flash") == JO_ARRAY)
   {
      while (jo_next (j) == JO_OBJECT)
      {
         uint32_t address = 0;
         char *filename = NULL;
         char *url = NULL;
         int build = -1;
         while (jo_next (j) == JO_TAG)
         {
            if (!jo_strcmp (j, "filename") && !filename)
            {
               if (jo_next (j) == JO_STRING)
                  filename = jo_strdup (j);
            } else if (!jo_strcmp (j, "url") && !url)
            {
               if (jo_next (j) == JO_STRING)
                  url = jo_strdup (j);
            } else if (!jo_strcmp (j, "build") && build < 0)
            {
               jo_type_t t = jo_next (j);
               if (t == JO_NUMBER)
                  build = jo_read_int (j);
               else if (t == JO_TRUE)
                  build = 32;
            } else if (!jo_strcmp (j, "address") && !url)
            {
               jo_type_t t = jo_next (j);
               if (t == JO_NUMBER)
                  address = jo_read_int (j);
               else if (t == JO_STRING)
               {
                  char *a = jo_strdup (j);
                  address = strtoul (a, NULL, 16);
                  free (a);
               }
            } else
               jo_next (j);
         }
         if (filename)
         {
            char *fn = NULL;
            if (asprintf (&fn, "%s/%s", sd_dir, filename) >= 0)
            {
               int f = open (fn, O_RDONLY);
               if (f >= 0)
               {
                  struct stat s = { 0 };
                  fstat (f, &s);
                  if (cb)
                     cb (filename, url, build, f, address, s.st_size);
                  close (f);
               } else if (cb)
                  cb (filename, url, build, -1, address, 0);
               free (fn);
            }
         } else if (cb)
            cb (filename, url, build, -1, address, 0);
         free (filename);
         free (url);
      }
   }
}

esp_http_client_handle_t client = NULL;
void
upgrade_check (int f, char *filename, char *url)
{
   if (!filename || !url)
      return;
   if (!client)
   {
      esp_http_client_config_t config = {
         .url = url,
         .timeout_ms = 30000,
      };
#ifdef  CONFIG_MBEDTLS_CERTIFICATE_BUNDLE
      config.crt_bundle_attach = esp_crt_bundle_attach;
#else
      config.use_global_ca_store = true;        /* Global cert */
#endif
      client = esp_http_client_init (&config);
      if (!client)
         return;
   } else
      esp_http_client_set_url (client, url);
   char *dl = NULL;
   asprintf (&dl, "%s/DOWNLOAD", sd_dir);
   char *fn = NULL;
   asprintf (&fn, "%s/%s", sd_dir, filename);
   esp_err_t e = 0;
   char *h = NULL;
   struct stat s = { 0 };
   if (!stat (fn, &s) && s.st_mtime)
   {
      struct tm t;
      gmtime_r (&s.st_mtime, &t);
      asprintf (&h, "%.3s, %02d %.3s %4d %02d:%02d:%02d GMT",
                "SunMonTueWedThuFriSat" + t.tm_wday * 3, t.tm_mday,
                "JanFebMarAprMayJunJulAugSepOctNovDec" + t.tm_mon * 3, t.tm_year + 1900, t.tm_hour, t.tm_min, t.tm_sec);
      e = esp_http_client_set_header (client, "If-Modified-Since", h);
   }
   if (!e)
      e = esp_http_client_open (client, 0);
   if (!e)
   {
      esp_http_client_fetch_headers (client);
      int status = esp_http_client_get_status_code (client);
      int len = 0;
      if (status / 100 == 2)
      {
         set_led (100, 'K', 'Y');
         b.fileerror = 1;
         int o = open (dl, O_CREAT | O_TRUNC | O_WRONLY, 0777);
         if (o < 0)
            ESP_LOGE (TAG, "Cannot write %s", dl);
         else
         {
            uint32_t size = 0;
            while (1)
            {
               len = esp_http_client_read_response (client, (void *) block, sizeof (block));
               if (len <= 0)
                  break;
               write (o, block, len);
               size += len;
            }
            close (o);
            if (!e)
               unlink (fn);     // fails if not there, duh
            if (!e && (e = rename (dl, fn)))    // Fails if target there, duh
               ESP_LOGE (TAG, "Rename fail %s", fn);
            if (!e)
            {
               ESP_LOGE (TAG, "Upgraded %s %u", filename, size);
               b.reload = 1;
            }
         }
      } else
      {
         if (status != 304)
            ESP_LOGE (TAG, "Status %d %s", status, url);
         else
            ESP_LOGE (TAG, "Unchanged %s %s (%u)", h ? : "", filename, s.st_size);
      }
      esp_http_client_flush_response (client, &len);
   }
   if (e)
      ESP_LOGE (TAG, "Failed %s: %s", url, esp_err_to_name (e));
   if (h)
      esp_http_client_delete_header (client, "If-Modified-Since");
   free (h);
   free (dl);
   free (fn);
}

void
load_cb (char *filename, char *url, int build, int f, uint32_t address, uint32_t size)
{
   if (!size)
      manifestsize = -1;
   else if (manifestsize != 1)
      manifestsize += size;
   if (build >= 0 && f >= 0)
   {                            // ID check info
      if (lseek (f, build, SEEK_SET) == build)
      {
         esp_app_desc_t app;
         if (read (f, &app, sizeof (app)) == sizeof (app))
         {
            if (!manifestid)
            {
               manifestid = strndup (app.project_name, 32);
               b.manifestidprefix = 1;
            }
            if (!manifestversion)
               manifestversion = strndup (app.version, 32);
            if (!manifestbuild)
            {
               char temp[20];
               if (revk_build_date_app (&app, temp))
                  manifestbuild = strdup (temp);
            }
         }
      }
   }
}

void
upgrade_cb (char *filename, char *url, int build, int f, uint32_t address, uint32_t size)
{
   load_cb (filename, url, build, f, address, size);
   upgrade_check (f, filename, url);
}

const char *
load_manifest (void)
{
   close_manifest ();
   char *fn = NULL;
   asprintf (&fn, "%s/manifest%d.json", sd_dir, manifest);
   int f = open (fn, O_RDONLY);
   if (f < 0)
   {
      free (fn);
      return "Failed to open";
   }
   struct stat s = { 0 };
   fstat (f, &s);
   if (!s.st_size || s.st_size > 10000)
   {                            // Silly
      close (f);
      free (fn);
      return "Silly size";
   }
   manifestjson = malloc (s.st_size);
   if (!manifestjson)
   {
      close (f);
      free (fn);
      return "Malloc fail";
   }
   if (read (f, manifestjson, s.st_size) != s.st_size)
   {
      close (f);
      free (manifestjson);
      manifestjson = NULL;
      free (fn);
      return "Read fail";
   }
   j = jo_parse_mem (manifestjson, s.st_size);
   if (!j)
   {
      close (f);
      free (manifestjson);
      manifestjson = NULL;
      free (fn);
      return "Bad JSON";
   }
   free (manifestid);
   b.manifestidprefix = 0;
   manifestid = NULL;
   if (jo_find (j, "id") == JO_STRING)
      manifestid = jo_strdup (j);
   manifestversion = NULL;
   if (jo_find (j, "version") == JO_STRING)
      manifestversion = jo_strdup (j);
   manifestbuild = NULL;
   if (jo_find (j, "build") == JO_STRING)
      manifestbuild = jo_strdup (j);
   manifestsetting = NULL;
   manifestsettinglen = 0;
   if (jo_find (j, "setting"))
   {
      int p = jo_pos (j);
      if (p >= 0)
      {
         manifestsetting = manifestjson + p;
         jo_skip (j);
         int q = jo_pos (j);
         if (q > p)
         {
            while (q > p && manifestjson[q - 1] <= ' ')
               q--;             // Messy
            if (q > p && manifestjson[q - 1] == ',')
               q--;
            while (q > p && manifestjson[q - 1] <= ' ')
               q--;             // Messy
            manifestsettinglen = q - p;
         }
      }
   }
   b.erase = (jo_find (j, "erase") == JO_TRUE);
   b.nobtn = (jo_find (j, "button") == JO_FALSE);
   b.nocheck = (jo_find (j, "check") == JO_FALSE);
   b.voltage3v3 = 0;
   if (jo_find (j, "voltage") == JO_NUMBER)
   {
      int v = round (1000 * jo_read_float (j));
      if (v == 3300)
         b.voltage3v3 = 1;
      else if (v != 5000)
         b.fileerror = 1;
   }
   if (!b.checked && !revk_link_down () && time (0) > 1000000000)
   {                            // Can check for new files
      if (jo_find (j, "url") == JO_STRING)
      {
         char *url = jo_strdup (j);
         upgrade_check (f, fn + sizeof (sd_dir), url);
         free (url);
      }
      close (f);
      free (fn);
      manifestsize = 0;
      scan_manifest (upgrade_cb);
      if (client)
      {
         esp_http_client_cleanup (client);
         client = NULL;
      }
      if (!b.reload)
         b.checked = 1;
   } else
   {
      close (f);
      free (fn);
      scan_manifest (load_cb);
   }
   if (manifestsize == -1)
      return "Missing files";
   if (!manifestsize)
      return "No files to flash";
   return NULL;
}

esp_loader_error_t
do_erase (void)
{
   b.forceerase = 0;
   ESP_LOGE (TAG, "Erase whole flash");
   uint32_t p = 0;
   while (p < flashsize && !b.reload)
   {
      set_led (p * 100 / flashsize, 'B', 'K');
      esp_loader_error_t e = esp_loader_flash_erase_region (p, BLOCK);
      if (e)
         return e;
      p += BLOCK;
   }
   return 0;
}

uint32_t flashcount = 0;
esp_loader_error_t flashe = 0;
void
flash_cb (char *filename, char *url, int build, int f, uint32_t address, uint32_t size)
{
   ESP_LOGE (TAG, "Flash to %06X len %06X %s", address, size, filename);
   if (flashe || f < 0 || !size)
      return;
   flashe = esp_loader_flash_start (address, size, BLOCK);
   if (!flashe)
   {
      uint32_t p = 0;
      while (p < size)
      {
         set_led (flashcount * 100 / manifestsize, 'K', 'B');
         uint32_t s = read (f, block, BLOCK);
         flashe = esp_loader_flash_write (block, s);
         if (flashe)
            return;
         p += s;
         flashcount += s;
      }
   }
   if (flashe)
      ESP_LOGE (TAG, "Cannot flash %06X len %06X %s %s", address, size, filename, esp_err_to_name (flashe));
   if (!flashe)
      flashe = esp_loader_flash_finish (false);
   if (!flashe)
      flashe = esp_loader_flash_verify ();
}

esp_loader_error_t
do_flash (void)
{
   ESP_LOGE (TAG, "Flash %u bytes", manifestsize);
   flashe = 0;
   flashcount = 0;
   scan_manifest (flash_cb);
   return flashe;
}

void
flash_task (void *arg)
{
   esp_err_t e = 0;
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
      cdc_acm_host_driver_config_t usb_config = {
         .driver_task_priority = 10,
         .driver_task_stack_size = 4096,
         .xCoreID = 0,
         .new_dev_cb = device_connect_cb,
      };
      e = usb_host_install (&host_config);
      if (!e)
         e = cdc_acm_host_install (&usb_config);
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
   while (!b.die && !revk_shutting_down (NULL))
   {
      ledsd = 'Y';
      ESP_LOGI (TAG, "Waiting SD");
      // Wait for SD card
      while (!revk_gpio_get (sdcd))
      {
         b.checked = 0;
         usleep (100000);
         continue;
      }
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
      b.fileerror = 0;
      set_led (manifest * 10, 'Y', 'Y');
      {                         // Check manifests
         manifests = 0;
         for (int i = 0; i < 10; i++)
         {
            char *fn;
            asprintf (&fn, "%s/manifest%d.json", sd_dir, i);
            if (!access (fn, R_OK))
               manifests |= (1 << i);
         }
      }
      {                         // Manifest
         const char *e = load_manifest ();
         if (e)
         {
            ESP_LOGE (TAG, "Manifest fail: %s", e);
            set_led (manifest * 10, 'R', 'R');
            b.fileerror = 1;
         }
      }

      if (!b.fileerror)
      {
         // TODO how do we do serial via UART?
         ESP_LOGE (TAG, "Power on %s", b.voltage3v3 ? "3.3V" : "5V");
         revk_gpio_output (b.voltage3v3 ? pwr3 : pwr5, 1);      // device on
         usb_host_lib_set_root_port_power (true);
         while (revk_gpio_get (sdcd) && !b.die && !b.reload && !revk_shutting_down (NULL))
         {
            b.fileerror = 0;
            if (badusb && badusb + 2 > uptime ())
               set_led (manifest * 10, 'R', 'R');       // Likely diff device
            else
               set_led (manifest * 10, 'M', 'M');
            if (b.wifi && time (0) > 1000000000)
            {
               ESP_LOGE (TAG, "Online");
               b.wifi = 0;
               revk_command ("upgrade", NULL);
               b.reload = 1;
               continue;
            }
            if (!b.connected)
            {
               usleep (100000);
               continue;
            }
            loader_esp32_usb_cdc_acm_config_t config = {
               .acm_host_error_callback = host_error_cb,
               .device_disconnected_callback = device_disconnect_cb,
               .device_pid = 0x1001,    // USB direct not serial chip...
               .connection_timeout_ms = 1000,
               .out_buffer_size = BLOCK,
               //.acm_host_serial_state_callback = host_serial_cb,
            };
            esp_loader_error_t e = loader_port_esp32_usb_cdc_acm_init (&config);
            if (e)
            {                   // try non JTAG
               config.device_pid = 0;
               e = loader_port_esp32_usb_cdc_acm_init (&config);
            }
            if (!e)
            {                   // Connected
               set_led (manifest * 10, 'O', 'O');
               uint8_t status = STATUS_TIMEOUT;
               if (!b.forceerase && !b.erase && !b.nocheck)
                  status = target_status ();
               if (b.connected
                   && (b.forceerase || b.erase || (status != STATUS_PASS && status != STATUS_FAIL)) && status != STATUS_ERROR)
               {
                  ESP_LOGE (TAG, "Bootload");
                  esp_loader_connect_args_t a = ESP_LOADER_CONNECT_DEFAULT ();
                  e = esp_loader_connect_with_stub (&a);        // Some chips don't work with stub
                  if (e)
                  {
                     ESP_LOGE (TAG, "Failed to connect in downloader mode");
                     status = STATUS_ERROR;
                  }
                  if (status != STATUS_ERROR)
                     chip_info ();
                  if (status != STATUS_ERROR && !flashsize)
                  {
                     ESP_LOGE (TAG, "Failed to get chip info");
                     status = STATUS_ERROR;
                  }
                  if (status != STATUS_ERROR && jo_find (j, "chip") == JO_STRING && jo_strcmp (j, chip))
                  {
                     status = STATUS_ERROR;
                     char *c = jo_strdup (j);
                     ESP_LOGE (TAG, "Wrong chip %s expecting %s", chip, c);
                     free (c);
                     b.fileerror = 1;
                  }
                  if ((b.forceerase || b.erase) && status != STATUS_EMPTY && status != STATUS_ERROR && do_erase ())
                  {
                     status = STATUS_ERROR;
                     ESP_LOGE (TAG, "Erase failed");
                  }
                  if (status != STATUS_ERROR && do_flash ())
                  {
                     status = STATUS_ERROR;
                     ESP_LOGE (TAG, "Flash failed");
                  }
                  if (b.connected && !b.reload)
                  {
                     ESP_LOGE (TAG, "Reset");
                     esp_loader_reset_target ();
                     status = target_status ();
                  }
               }
               switch (status)
               {
               case STATUS_MISMATCH:
                  set_led (10, 'K', 'Y');
                  break;
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
                  set_led (manifest * 10, 'R', 'R');
               }
               if (b.connected && !b.reload)
               {
                  ESP_LOGE (TAG, "Wait disconnect");
                  while (revk_gpio_get (sdcd) && b.connected && !b.reload && !revk_shutting_down (NULL))
                     usleep (100000);
               }
            }
            close_manifest ();
            loader_port_esp32_usb_cdc_acm_deinit ();
         }
         set_led (0, 'K', 'K');
         ESP_LOGE (TAG, "Power off");
         if (b.connected)
            usb_host_lib_set_root_port_power (false);
         revk_gpio_output (pwr3, 0);
         revk_gpio_output (pwr5, 0);
      } else
         sleep (1);
      ESP_LOGI (TAG, "Dismounting SD");
      esp_vfs_fat_sdcard_unmount (sd_dir, card);
      ledsd = 'B';
      b.reload = 0;
   }

   usb_host_uninstall ();
   b.fileerror = 0;
   while (revk_shutting_down (NULL))
   {
      int p = revk_ota_progress ();
      if (p >= 0 && p <= 100)
         set_led (p, 'K', 'Y');
      usleep (100000);
   }
   vTaskDelete (NULL);
}

//--------------------------------------------------------------------------------
// Main
void
app_main ()
{
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
   revk_task ("flash", flash_task, NULL, 16);
}
