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
#include <mbedtls/sha1.h>
#ifndef CONFIG_GFX_BUILD_SUFFIX_GFXNONE
#include "gfx.h"
#include <lwpng.h>
#endif

static httpd_handle_t webserver = NULL;

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
   uint8_t manifestappprefix:1; // manifestapp is prefix
} volatile b;
uint8_t atetimeout = 0;
uint16_t manifests = 0;         // Bit map of manifests on SD
uint8_t progress = 0;           // Progress (LED)
char ledf = 'K',
   ledt = 'K',
   ledsd = 'B';                 // LED from/to and SD
const char *ledstage = NULL;    // LED/LCD state
uint8_t mac[6] = { 0 };         // Found mac
char chip[30] = { 0 };          // Found chip type

uint32_t badusb = 0;            // Last devices 0 even

uint32_t flashsize = 0;         // Found flash size

uint32_t manifestsize = 0;      // Total flash bytes
char *mname[MANIFESTS];         // Manifest name (malloc)
char *murl[MANIFESTS]={0};          // Manifest url (malloc)
char *mpng=NULL;

SemaphoreHandle_t epd_mutex = NULL;

jo_t j = NULL;                  // Parsed manifest JSON (yeh, sorry)

#define	fields				\
	x (app);			\
	x (version);			\
	x (build);			\
	x (setting);			\
	x (start);			\
	x (pass);			\
	x (fail);			\
	xx(wifi,ssid);			\
	xx(wifi,pass);			\

#define x(f)	char *manifest##f=NULL;
#define xx(f1,f2)	char *manifest##f1##f2=NULL;
fields
#undef	x
#undef	xx
#define	BLOCK	4096
   uint8_t block[BLOCK];

#ifdef  CONFIG_TINYUSB_MSC_MOUNT_PATH
const char sd_dir[] = CONFIG_TINYUSB_MSC_MOUNT_PATH;
#else
const char sd_dir[] = "/sd";
#endif


const char *
mqtt_client_callback (int client, const char *prefix, const char *target, const char *suffix, jo_t j)
{
   if (client || !prefix || target || strcmp (prefix, topiccommand) || !suffix)
      return NULL;              // Not for us or not a command from main MQTTS
   if (!strcmp (suffix, "wifi"))
      b.wifi = 1;
   return NULL;
}

#ifndef CONFIG_GFX_BUILD_SUFFIX_GFXNONE
void
lcd_task (void *arg)
{                               // GFX
 const char *e = gfx_init (pwr: gfxpwr.num, ena: gfxena.num, cs: gfxcs.num, sck: gfxsck.num, mosi: gfxdin.num, dc: gfxdc.num, rst: gfxrst.num, busy: gfxbusy.num, flip: gfxflip, direct:1);
   if (e)
   {
      ESP_LOGE (TAG, "gfx %s", e);
      jo_t j = jo_object_alloc ();
      jo_string (j, "error", "Failed to start");
      jo_string (j, "description", e);
      revk_error ("gfx", &j);
   } else
   {
      revk_gfx_init (3);
      while (1)
      {
         xSemaphoreTake (epd_mutex, portMAX_DELAY);
         gfx_lock ();
         gfx_clear (0);
         gfx_pos (gfx_width () / 2, 0, GFX_T | GFX_C | GFX_V);

         // TODO image if init state

         if (mname[manifest] && *mname[manifest])
            gfx_text (GFX_TEXT_DESCENDERS, 5, mname[manifest]);
         else
            gfx_text (0, 5, "Manifest %d", manifest);

         gfx_foreground (revk_rgb (ledt));
         if (ledstage)
            gfx_text (0, 5, ledstage);

         if (ledf != ledt)
         {                      // Progress
            // TODO
            gfx_text (0, 2, "%d%%", progress);
         }

         gfx_refresh ();
         gfx_unlock ();
         xSemaphoreGive (epd_mutex);
         usleep (100000);
      }
   }
   vTaskDelete (NULL);
}
#endif

void
led_task (void *arg)
{
   led_strip_t strip = NULL;
   led_strip (&strip, led, MANIFESTS + 1, 3, LED_GRB);
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
         for (int i = 0; i < MANIFESTS; i++)
         {
            uint8_t l = p;
            if (l > 10)
               l = 10;
            p -= l;
            if ((b.fileerror && (tick & 1)) || (f == t && i != progress / 10))
               led_set (strip, i, 0, 0, 0);     // Off
            else
               led_set (strip, i,       //
                        gamma8[l * ((t >> 16) & 255) / 10 + (10 - l) * ((f >> 16) & 255) / 10], //
                        gamma8[l * ((t >> 8) & 255) / 10 + (10 - l) * ((f >> 8) & 255) / 10],   //
                        gamma8[l * ((t >> 0) & 255) / 10 + (10 - l) * ((f >> 0) & 255) / 10]);
         }
         if (b.fileerror && !(tick & 1))
            revk_led (strip, MANIFESTS, 255, revk_rgb ('R'));
         else
            revk_led (strip, MANIFESTS, 255, revk_rgb (ledsd));
         led_send ();
         usleep (100000);
      }
      led_clear (strip);
      led_send ();
   }
   vTaskDelete (NULL);
}

void
set_led (uint8_t p, char f, char t, const char *tage)
{                               // Set LED display state (if f==t then one LED at progress point)
   ledstage = tage;
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
            ESP_LOGE (TAG, "Button press");
            if (b.connected)
            {
               if (!b.nobtn)
               {
                  ESP_LOGE (TAG, "Button force erase");
                  b.forceerase = 1;
                  b.reload = 1;
               } else
                  ESP_LOGE (TAG, "Button ignore");
            } else if (manifests)
            {
               uint8_t m = manifest;
               do
               {
                  m++;
                  if (m >= MANIFESTS)
                     m = 0;
               }
               while (!(manifests & (1 << m)));
               if (m != manifest)
               {
                  ESP_LOGE (TAG, "New manifest %d", m);
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

void
apply_settings ()
{
   char buf[266];
   loader_port_write ((uint8_t *) "\n", 1, 2000);
   if (manifestsetting)
   {
      ESP_LOGE (TAG, "Setting %s", manifestsetting);
      loader_port_write ((uint8_t *) manifestsetting, strlen (manifestsetting), 2000);
   }
   if (manifestwifissid)
   {                            // IMPROV https://www.improv-wifi.com/serial/
      char *e;
      uint8_t c;
      // ID
      e = buf;
      e = stpcpy (e, "IMPROV");
      *e++ = 1;                 // Version
      *e++ = 3;                 // Command RPC
      e++;                      // Len
      *e++ = 3;                 // ID command
      *e++ = 0;                 // Command len
      buf[8] = (e - (buf + 9)); // Set len
      c = 0;                    // Checksum
      for (uint8_t * q = (uint8_t *) buf; q < (uint8_t *) e; q++)
         c += *q;
      *e++ = c;
      loader_port_write ((uint8_t *) buf, e - buf, 2000);
      ESP_LOG_BUFFER_HEX_LEVEL ("IMPROV", buf, e - buf, ESP_LOG_INFO);
      // WiFI
      e = buf;
      e = stpcpy (e, "IMPROV");
      *e++ = 1;                 // Version
      *e++ = 3;                 // Command RPC
      e++;                      // Len
      *e++ = 1;                 // WiFi command
      e++;                      // Command len
      *e++ = strlen (manifestwifissid);
      e = stpcpy (e, manifestwifissid);
      if (manifestwifipass)
      {
         *e++ = strlen (manifestwifipass);
         e = stpcpy (e, manifestwifipass);
      }
      buf[10] = (e - (buf + 11));       // Set cmd len
      buf[8] = (e - (buf + 9)); // Set len
      c = 0;                    // Checksum
      for (uint8_t * q = (uint8_t *) buf; q < (uint8_t *) e; q++)
         c += *q;
      *e++ = c;
      loader_port_write ((uint8_t *) buf, e - buf, 2000);
      ESP_LOG_BUFFER_HEX_LEVEL ("IMPROV", buf, e - buf, ESP_LOG_INFO);
   }
}

enum
{
   STATUS_ERROR,                // error
   STATUS_SILENT,               // no data
   STATUS_EMPTY,                // 0xffffffff error
   STATUS_LOOPING,              // Starting multiple times
   STATUS_FAIL,                 // ATE: FAIL
   STATUS_PASS,                 // ATE: PASS
   STATUS_MISMATCH,             // Code needs flashing as version mismatch
   STATUS_TIMEOUT,              // Not looping but not pass/fail
} target_status (void)
{
   uint32_t to = uptime () + atetimeout;
   uint8_t rst = 0;
   int8_t ate = 0;
   int8_t match = 0;
   char ok = !manifestsetting;
   char buf[266];               // Enough for IMRPOV
   uint32_t p = 0;
   jo_t j = NULL;
   uint8_t status = STATUS_TIMEOUT;
   while (uptime () <= to && status == STATUS_TIMEOUT && !b.forceerase)
   {
      uint8_t c;
      esp_loader_error_t e = loader_port_read (&c, 1, 100);
      if (e == ESP_LOADER_ERROR_TIMEOUT)
      {
         if (ate && ok)
            break;
         continue;
      }
      if (e)
         return STATUS_ERROR;
      if (!j && !p && c == '{')
         j = jo_create_alloc ();
      if (j && jo_char (j, c) > 0)
         continue;
      if (!j && p >= 6 && !strncmp (buf, "IMPROV", 6))
      {
         if (p < sizeof (buf) - 1)
            buf[p++] = c;
         if (p < 8 || p < buf[8] + 10)
            continue;
      } else if (!j && c >= ' ')
      {
         if (p < sizeof (buf) - 1)
            buf[p++] = c;
         continue;
      }
      if (!j && !p)
         continue;
      void do_start (void)
      {
         if (rst++ > 5)
            status = STATUS_LOOPING;
         apply_settings ();
      }
      if (p)
      {                         // String
         buf[p] = 0;
         if (!strncmp (buf, "IMPROV", 6))
         {
            if (p < 9)
               ESP_LOG_BUFFER_HEX_LEVEL ("IMPROV (len error)", buf, p, ESP_LOG_ERROR);
            else if (buf[6] != 1)
               ESP_LOG_BUFFER_HEX_LEVEL ("IMPROV (ver error)", buf, p, ESP_LOG_ERROR);
            else
            {
               uint8_t c = 0;
               for (uint8_t * q = (uint8_t *) buf; q < (uint8_t *) buf + p - 1; q++)
                  c += *q;
               if (c != buf[p - 1])
                  ESP_LOG_BUFFER_HEX_LEVEL ("IMPROV (checksum error)", buf, p, ESP_LOG_ERROR);
               else
               {
                  ESP_LOG_BUFFER_HEX_LEVEL ("IMPROV", buf, p, ESP_LOG_INFO);
                  if (buf[7] == 1 && buf[8] == 1)
                  {             // Current state
                     if (buf[9] == 2)
                        ESP_LOGE ("IMPROV", "Ready");
                     if (buf[9] == 3)
                        ESP_LOGE ("IMPROV", "Provisioning");
                     if (buf[9] == 4)
                     {
                        ESP_LOGE ("IMPROV", "Provisioned");
                        ok = 1;
                        if (!manifestpass)
                           ate = 1;
                     }
                  } else if (buf[7] == 2 && buf[8] == 1)
                     ESP_LOGE ("IMPROV", "Error %d", buf[9]);
                  else if (buf[7] == 4)
                  {             // RPC response
                     int q = 11;
                     int n = 0;
                     while (q + buf[q] + 1 < p)
                     {
                        ESP_LOGE ("IMPROV", "%.*s", buf[q], buf + q + 1);
                        if (buf[9] == 3)
                        {       // ID check
                           if (n == 0 && manifestapp && strncmp (buf + q + 1, manifestapp, buf[q]))
                           {
                              ESP_LOGE (TAG, "App expected %s", manifestapp);
                              status = STATUS_ERROR;
                           }
                           if (n == 3 && manifestversion)
                           {
                              if (!strncmp (buf + q + 1, manifestversion, buf[q]))
                              {
                                 if (match >= 0)
                                    match = 1;
                              } else
                              {
                                 match = -1;
                                 ESP_LOGE (TAG, "Version expected %s", manifestversion);
                              }
                           }
                           if (n == 1 && manifestbuild)
                           {
                              if (!strncmp (buf + q + 1, manifestbuild, buf[q]))
                              {
                                 if (match >= 0)
                                    match = 1;
                              } else
                              {
                                 match = -1;
                                 ESP_LOGE (TAG, "Version expected %s", manifestbuild);
                              }
                           }
                        }
                        q += buf[q] + 1;
                        n++;
                     }
                  } else
                     ESP_LOG_BUFFER_HEX_LEVEL ("IMPROV (unknown)", buf, p, ESP_LOG_ERROR);
               }
            }
            p = 0;
         } else if (!strncmp (buf, "SPIWP:", 6))
         {
            to = uptime () + atetimeout;
            ok = !manifestsetting;
            if (rst++ > 5)
               return STATUS_LOOPING;
         } else if (manifeststart && !strcmp (buf, manifeststart))
            do_start ();
         else if (manifestpass && !strcmp (buf, manifestpass))
            ate = 1;
         else if (manifestfail && !strcmp (buf, manifestfail))
            ate = -1;
         else if (!strcmp (buf, "invalid header: 0xffffffff"))
            return STATUS_EMPTY;
         else
            p = 0;
         if (p)
            printf ("\033[1;34m%s\033[0m\n", buf);
         p = 0;
      } else if (j)
      {                         // JSON
         jo_skip (j);           // Check errors
         const char *e = jo_error (j, NULL);
         if (e)
            ESP_LOGE (TAG, "JSON Error: %s", e);
         else
         {
            jo_type_t t;
            if ((t = jo_find (j, "ate")))
            {
               if (t == JO_TRUE)
                  ate = 1;
               else if (t == JO_FALSE)
                  ate = -1;
            } else if ((t = jo_find (j, "app")))
            {
               if (rst++ > 5)
                  status = STATUS_LOOPING;
               else
               {
                  to = uptime () + atetimeout;
                  if (t == JO_STRING && manifestapp
                      && (b.manifestappprefix ? jo_strncmp (j, manifestapp, strlen (manifestapp)) : jo_strcmp (j, manifestapp)))
                  {
                     ESP_LOGE (TAG, "App expected %s", manifestapp);
                     status = STATUS_ERROR;
                  } else
                  {
                     if (match >= 0 && (t = jo_find (j, "version")) == JO_STRING && manifestversion)
                     {
                        if (!jo_strcmp (j, manifestversion))
                           match++;
                        else
                        {
                           match = -1;
                           ESP_LOGE (TAG, "Version expected %s", manifestversion);
                        }
                     }
                     if (match >= 0 && (t = jo_find (j, "build")) == JO_STRING && manifestbuild)
                     {
                        if (!jo_strcmp (j, manifestbuild))
                           match++;
                        else
                        {
                           match = -1;
                           ESP_LOGE (TAG, "Build expected %s", manifestbuild);
                        }
                     }
                     if (match >= 0)
                        do_start ();
                  }
               }
            } else if ((t = jo_find (j, "ok")))
            {
               if (t == JO_TRUE)
                  ok = 1;
               else if (t == JO_FALSE)
                  status = STATUS_ERROR;
            }
         }
         // Check JSON match
         char *s = jo_finisha (&j);
         printf ("\033[1;34m%s\033[0m\n", s);
         if (manifeststart && !strcmp (s, manifeststart))
            do_start ();
         if (manifestpass && !strcmp (s, manifestpass))
            ate = 1;
         if (manifestfail && !strcmp (s, manifestfail))
            ate = -1;
         free (s);
      }
   }
   if (match < 0)
      return STATUS_MISMATCH;
   if (!rst)
      return STATUS_SILENT;
   if (ate > 0)
      return STATUS_PASS;
   if (ate < 0)
      return STATUS_FAIL;
   return status;
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
   jo_free (&j);
}

typedef void manifest_t (char *fn, char *url, int app, uint32_t address, uint32_t size, uint8_t verify);

void
scan_manifest (manifest_t cb)
{
   if (!j)
   {
      ESP_LOGE (TAG, "Scan manifest not loaded");
      return;
   }
   jo_rewind (j);
   if (jo_find (j, "flash") == JO_ARRAY)
   {
      while (jo_next (j) == JO_OBJECT)
      {
         uint32_t address = 0;
         char *filename = NULL;
         char *url = NULL;
         int app = -1;
         uint8_t verify = flashverify;
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
            } else if (!jo_strcmp (j, "app") && app < 0)
            {
               jo_type_t t = jo_next (j);
               if (t == JO_NUMBER)
                  app = jo_read_int (j);
               else if (t == JO_TRUE)
                  app = 32;
            } else if (!jo_strcmp (j, "address"))
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
            } else if (!jo_strcmp (j, "verify"))
            {
               jo_type_t t = jo_next (j);
               if (t == JO_FALSE)
                  verify = 0;
               else if (t == JO_TRUE)
                  verify = 1;
               else if (t == JO_NUMBER)
                  verify = jo_read_int (j);
            } else
               jo_next (j);
         }
         if (!filename && url)
         {                      // Use hash
            const char *leaf = strrchr (url, '/');
            if (leaf && strlen (leaf) > 200)
               leaf = strrchr (url, '.');
            else if (leaf)
               leaf++;
            if (!leaf || strlen (leaf) > 200)
               leaf = "";
            unsigned char output[20];
            mbedtls_sha1 ((const uint8_t *) url, strlen (url), output);
            if ((filename = malloc (40 + 1 + strlen (leaf) + 1)))
            {
               for (int i = 0; i < 20; i++)
                  sprintf (filename + i * 2, "%02X", output[i]);
               filename[40] = '-';
               strcpy (filename + 41, leaf);
            }
         }
         if (filename)
         {
            char *fn = NULL;
            if (asprintf (&fn, "%s/%s", sd_dir, filename) >= 0)
            {
               struct stat s = { 0 };
               if (stat (fn, &s))
                  s.st_size = 0;
               cb (fn, url, app, address, s.st_size, verify);
               free (fn);
            }
         } else if (cb)
            cb (NULL, url, app, address, 0, verify);
         free (filename);
         free (url);
      }
   }
}

esp_http_client_handle_t client = NULL;

void
upgrade_check (const char *fn, char *url)
{
   if (!fn || !url)
      return;
   const char *filename = fn + sizeof (sd_dir);
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
         set_led (100, 'K', 'Y', "File update");
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
               ESP_LOGE (TAG, "Upgraded %s %s %u", filename, url, size);
               b.reload = 1;
            }
         }
      } else
      {
         if (status != 304)
            ESP_LOGE (TAG, "Status %d %s", status, url);
         else
            ESP_LOGE (TAG, "Unchanged %s %s %s (%u)", h ? : "", filename, url, s.st_size);
         while (esp_http_client_read_response (client, (void *) block, sizeof (block)) > 0);    // Yes, not using flush as seems not to actually work
      }
   }
   if (e)
      ESP_LOGE (TAG, "Failed %s: %s", url, esp_err_to_name (e));
   if (h)
      esp_http_client_delete_header (client, "If-Modified-Since");
   free (h);
   free (dl);
}

void
load_cb (char *fn, char *url, int app, uint32_t address, uint32_t size, uint8_t verify)
{
   if (!size)
      manifestsize = -1;
   else if (manifestsize != -1)
      manifestsize += size;
   if (app < 0)
      return;
   int f = open (fn, O_RDONLY);
   if (f < 0)
      return;
   if (lseek (f, app, SEEK_SET) == app)
   {
      esp_app_desc_t app;
      if (read (f, &app, sizeof (app)) == sizeof (app))
      {
         if (!manifestapp)
         {
            manifestapp = strndup (app.project_name, 32);
            b.manifestappprefix = 1;
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
   close (f);
}

void
upgrade_cb (char *fn, char *url, int app, uint32_t address, uint32_t size, uint8_t verify)
{
   load_cb (fn, url, app, address, size, verify);
   upgrade_check (fn, url);
}

const char *
manifest_fn (uint8_t m)
{                               // Get manifest filename (static)
   static char temp[50];
   snprintf (temp, sizeof (temp), "%s/manifest%d.json", sd_dir, m);
   return temp;
}

jo_t
read_manifest (uint8_t m)
{                               // Read a manifest
   const char *fn = manifest_fn (m);
   int f = open (fn, O_RDONLY);
   if (f < 0 && *manifesturl[m])
   {
      upgrade_check (fn, manifesturl[m]);
      f = open (fn, O_RDONLY);
   }
   if (f < 0)
      return NULL;
   struct stat s = { 0 };
   fstat (f, &s);
   if (!s.st_size || s.st_size > 10000)
   {                            // Silly
      close (f);
      ESP_LOGE (TAG, "Manifest %d bad size %ld", m, s.st_size);
      return NULL;
   }
   char *mem = malloc (s.st_size);
   if (!mem)
   {
      close (f);
      return NULL;
   }
   if (read (f, mem, s.st_size) != s.st_size)
   {
      close (f);
      free (mem);
      return NULL;
   }
   close (f);
   jo_t j = jo_parse_malloc (mem, s.st_size);
   if (!j)
      return NULL;
   jo_skip (j);
   int pos = 0;
   const char *e = jo_error (j, &pos);
   if (e)
   {
      ESP_LOGE (TAG, "Manifest %d failed: %s at %d", m, e, pos);
      jo_free (&j);
      return NULL;
   }
   jo_rewind (j);
   return j;
}

const char *
load_manifest (void)
{
   xSemaphoreTake (epd_mutex, portMAX_DELAY);
   close_manifest ();
   free (mname[manifest]);
   mname[manifest] = NULL;
   free (murl[manifest]);
   murl[manifest] = NULL;
   j = read_manifest (manifest);
   if (!j)
   {
      xSemaphoreGive (epd_mutex);
      return "Cannot load manifest";
   }
   if (jo_find (j, "name"))
      mname[manifest] = jo_strdup (j);
   if (jo_find (j, "url"))
      murl[manifest] = jo_strdup (j);
   b.manifestappprefix = 0;
#define x(f) free (manifest##f); manifest##f = NULL; if (jo_find (j, #f)) manifest##f = jo_strdup (j);
#define xx(f1,f2) free (manifest##f1##f2); manifest##f1##f2 = NULL; if (jo_find (j, #f1"."#f2)) manifest##f1##f2 = jo_strdup (j);
   fields
#undef x
#undef xx
      jo_type_t t;
   b.erase = (flasherase ? 1 : 0);
   b.nocheck = (flashcheck ? 0 : 1);
   b.nobtn = (flashbutton ? 0 : 1);
   atetimeout = flashwait;
   if ((t = jo_find (j, "erase")) >= JO_TRUE)
      b.erase = (t == JO_TRUE ? 1 : 0);
   if ((t = jo_find (j, "button")) >= JO_TRUE)
      b.nobtn = (t == JO_TRUE ? 0 : 1);
   if ((t = jo_find (j, "check")) >= JO_TRUE)
      b.nocheck = (t == JO_TRUE ? 0 : 1);
   if ((t = jo_find (j, "wait")) == JO_NUMBER)
      atetimeout = jo_read_int (j);
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
      char *url = NULL;
      if (jo_find (j, "url") == JO_STRING)
         url = jo_strdup (j);
      const char *fn = manifest_fn (manifest);
      if (*manifesturl[manifest])
      {
         upgrade_check (fn, manifesturl[manifest]);
         if (url && !strcasecmp (strchr (url, ':') ? : url, strchr (manifesturl[manifest], ':') ? : manifesturl[manifest]))
         {                      // Yes case for domain really, but close enough - we are using same URL so remove from settings
            char tag[20];
            sprintf (tag, "manifesturl%d", manifest + 1);
            jo_t j = jo_object_alloc ();
            jo_string (j, tag, "");
            revk_setting (j);
            jo_free (&j);
         }
      } else
         upgrade_check (fn, url);
      free (url);
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
      manifestsize = 0;
      scan_manifest (load_cb);
   }
   xSemaphoreGive (epd_mutex);
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
      set_led (p * 100 / flashsize, 'B', 'K', "Erase");
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
flash_cb (char *fn, char *url, int app, uint32_t address, uint32_t size, uint8_t verify)
{
   if (!fn || flashe || !size)
      return;
   char *filename = fn + sizeof (sd_dir);
   int try = (verify ? : 1);
   while (try--)
   {
      ESP_LOGE (TAG, "Flash to %06X len %06X %s", address, size, filename);
      int f = open (fn, O_RDONLY);
      if (f < 0)
      {
         ESP_LOGE (TAG, "Missing %s", filename);
         return;
      }
      flashe = esp_loader_flash_start (address, size, BLOCK);
      uint32_t p = 0;
      if (!flashe)
      {
         while (p < size)
         {
            set_led (flashcount * 100 / manifestsize, 'K', 'B', "Flash");
            uint32_t s = read (f, block, BLOCK);
            if (s <= 0)
               break;
            flashe = esp_loader_flash_write (block, s);
            if (flashe)
               break;
            p += s;
            flashcount += s;
         }
      }
      close (f);
      if (!flashe)
         flashe = esp_loader_flash_finish (false);
      if (!flashe && verify)
      {
         flashe = esp_loader_flash_verify ();   // This does not seem to work if flashing to end of flash!
         if (flashe)
         {
            ESP_LOGE (TAG, "Verify fail %06X len %06X %s", address, size, filename);
            if (try)
            {
               flashcount -= p;
               flashe = 0;
               continue;
            }
         }
      }
      if (flashe)
         ESP_LOGE (TAG, "Flash fail %06X len %06X at %06X %s (%d)", address, size, p, filename, flashe);
      break;
   }
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
      set_led (manifest * 10, 'Y', 'Y', "Wait");
      {                         // Check manifests
         manifests = 0;
         for (int i = 0; i < MANIFESTS; i++)
         {
            if (*manifesturl[i])
               manifests |= (1 << i);
            jo_t j = read_manifest (i);
            if (j)
            {
               manifests |= (1 << i);
               if (jo_find (j, "name"))
                  mname[i] = jo_strdup (j);
               if (jo_find (j, "url"))
                  murl[i] = jo_strdup (j);
               jo_free (&j);
            }
         }
      }
      {                         // Manifest
         const char *e = load_manifest ();
         if (e)
         {
            ESP_LOGE (TAG, "Manifest fail: %s", e);
            set_led (manifest * 10, 'R', 'R', "FAIL");
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
               set_led (manifest * 10, 'R', 'R', "FAIL");       // Likely duff device
            else
               set_led (manifest * 10, 'M', 'M', NULL);
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
               loader_port_esp32_usb_cdc_acm_deinit ();
               e = loader_port_esp32_usb_cdc_acm_init (&config);
               if (!e)
                  ESP_LOGE (TAG, "Serial connect");
            } else
               ESP_LOGE (TAG, "JTAG connect");
            if (!e)
            {                   // Connected
               set_led (manifest * 10, 'O', 'O', "Check");
               uint8_t status = STATUS_TIMEOUT;
               if (!b.forceerase && !b.erase && !b.nocheck)
               {
                  esp_loader_reset_target ();
                  status = target_status ();
               }
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
                  set_led (manifest * 10, 'Y', 'Y', "Mismatch");
                  break;
               case STATUS_PASS:
                  set_led (100, 'K', 'G', "PASS");
                  break;
               case STATUS_FAIL:
                  set_led (100, 'K', 'R', "FAIL");
                  break;
               case STATUS_LOOPING:
                  set_led (90, 'K', 'R', "FAIL LOOP");
                  break;
               case STATUS_SILENT:
                  set_led (80, 'K', 'R', "FAIL SILENT");
                  break;
               case STATUS_EMPTY:
                  set_led (70, 'K', 'R', "FAIL EMPTY");
                  break;
               case STATUS_ERROR:
                  b.fileerror = 1;
                  set_led (60, 'K', 'R', "FAIL ERROR");
                  break;
               case STATUS_TIMEOUT:
                  set_led (50, 'R', 'G', "TIMEOUT");
                  break;
               default:
                  set_led (manifest * 10, 'R', 'R', "FAIL?");
               }
               if (!b.forceerase && b.connected && !b.reload)
               {
                  ESP_LOGE (TAG, "Wait disconnect");
                  while (revk_gpio_get (sdcd) && b.connected && !b.reload && !b.forceerase && !revk_shutting_down (NULL))
                     usleep (100000);
               }
            }
            //close_manifest ();
            loader_port_esp32_usb_cdc_acm_deinit ();
         }
         set_led (0, 'K', 'K', "OFF");
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
         set_led (p, 'K', 'Y', "OTA");
      usleep (100000);
   }
   vTaskDelete (NULL);
}

void
revk_web_extra (httpd_req_t *req, int page)
{
   revk_web_setting (req, NULL, "manifest");
   revk_web_setting_title (req, "URLs for manifest files, overriding the url in the file if different");
   for (int i = 0; i < MANIFESTS; i++)
   {
      char name[30];
      sprintf (name, "manifesturl%d", i + 1);
      char tag[30];
      sprintf (tag, "manifest%d.json", i);
      revk_web_setting_edit (req, tag, name, (manifests & (1 << i)) ? "From file" : NULL);
   }
}

static void
register_uri (const httpd_uri_t *uri_struct)
{
   esp_err_t res = httpd_register_uri_handler (webserver, uri_struct);
   if (res != ESP_OK)
   {
      ESP_LOGE (TAG, "Failed to register %s, error code %d", uri_struct->uri, res);
   }
}

static void
register_get_uri (const char *uri, esp_err_t (*handler) (httpd_req_t *r))
{
   httpd_uri_t uri_struct = {
      .uri = uri,
      .method = HTTP_GET,
      .handler = handler,
   };
   register_uri (&uri_struct);
}

static esp_err_t
web_root (httpd_req_t *req)
{
   static char *temp = NULL;
   size_t len = httpd_req_get_url_query_len (req);
   char q[3] = { };
   if (len && len <= 2)
   {
      httpd_req_get_url_query_str (req, q, sizeof (q));
      jo_t j = jo_object_alloc ();
      jo_int (j, "manifest", atoi (q));
      revk_setting (j);
      jo_free (&j);
      b.checked = 0;
      b.reload = 1;
   }
   int i;
   revk_web_head (req, hostname);
   revk_web_send (req, "<h1>%s</h1><table border=1><tr><th></th><th>Name</th><th>URL</th>", hostname);
   for (i = 0; i < MANIFESTS && !*manifesturl[i]; i++);
   if (i < MANIFESTS)
      revk_web_send (req, "<th>Override</th>");
   revk_web_send (req, "</tr>");
   for (int i = 0; i < MANIFESTS; i++)
      if (manifests & (1 << i))
      {
         revk_web_send (req, "<tr%s><td><a href='?%d'>%d</a></td>", manifest == i ? " style='background:yellow;'" : "", i, i);
         revk_web_send (req, "<td>%s</td>", mname[i] ? revk_web_safe (&temp, mname[i]) : "-");
         revk_web_send (req, "<td>%s</td>", murl[i] ? revk_web_safe (&temp, murl[i]) : "-");
         if (*manifesturl[i])
            revk_web_send (req, "<td>%s</td>", revk_web_safe (&temp, manifesturl[i]));
         revk_web_send (req, "</tr>");
      }
   revk_web_send (req, "</table>");
   return revk_web_foot (req, 0, 1, NULL);
}

//--------------------------------------------------------------------------------
// Main
void
app_main ()
{
   revk_boot (&mqtt_client_callback);
   revk_start ();
   epd_mutex = xSemaphoreCreateMutex ();
   xSemaphoreGive (epd_mutex);
   memset (mname, 0, sizeof (mname));
   memset (murl, 0, sizeof (murl));
   // Web interface
   httpd_config_t config = HTTPD_DEFAULT_CONFIG ();
   config.stack_size += 4096;   // Being on the safe side
   // When updating the code below, make sure this is enough
   // Note that we're also adding revk's own web config handlers
   config.max_uri_handlers = 1 + revk_num_web_handlers ();
   if (!httpd_start (&webserver, &config))
   {
      revk_web_settings_add (webserver);
      register_get_uri ("/", web_root);
   }
   revk_gpio_output (pwr3, 0);
   revk_gpio_output (pwr5, 0);
   revk_task ("led", led_task, NULL, 4);
#ifndef CONFIG_GFX_BUILD_SUFFIX_GFXNONE
   revk_task ("lcd", lcd_task, NULL, 4);
#endif
   revk_gpio_input (btn);
   revk_task ("btn", btn_task, NULL, 4);
   revk_task ("flash", flash_task, NULL, 16);
}
