#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>

#include "physfs.h"
#include "libretro.h"

static uint32_t *frame_buf;
static struct retro_log_callback logging;
static retro_log_printf_t log_cb;
static retro_video_refresh_t video_cb;
static retro_environment_t environ_cb;

static void fallback_log(enum retro_log_level level, const char *fmt, ...)
{
   (void)level;
   va_list va;
   va_start(va, fmt);
   vfprintf(stderr, fmt, va);
   va_end(va);
}

void retro_init(void)
{
   frame_buf = calloc(320 * 240, sizeof(uint32_t));

   // Initialize PhysFS
   //if (PHYSFS_init(NULL) == 0) {

   // Pass in the environment callback to initialize PhysFS
   if (PHYSFS_init((const char*)environ_cb) == 0) {
      log_cb(RETRO_LOG_ERROR, "Failed to initialize PhysFS: %s\n", PHYSFS_getErrorByCode(PHYSFS_getLastErrorCode()));
   }
   else {
      log_cb(RETRO_LOG_INFO, "Initialized PhysFS.\n");
   }
}

void retro_deinit(void)
{
   free(frame_buf);
   frame_buf = NULL;

   PHYSFS_deinit();
}

unsigned retro_api_version(void)
{
   return RETRO_API_VERSION;
}

void retro_set_controller_port_device(unsigned port, unsigned device)
{
   log_cb(RETRO_LOG_INFO, "Plugging device %u into port %u.\n", device, port);
}

void retro_get_system_info(struct retro_system_info *info)
{
   memset(info, 0, sizeof(*info));
   info->library_name     = "libretro_physfs_test";
   info->library_version  = "v1";
   info->need_fullpath    = false;
   info->valid_extensions = NULL; // Anything is fine, we don't care.
}

void retro_get_system_av_info(struct retro_system_av_info *info)
{
   float aspect = 4.0f / 3.0f;

   info->timing = (struct retro_system_timing) {
      .fps = 60.0,
      .sample_rate = 0.0,
   };

   info->geometry = (struct retro_game_geometry) {
      .base_width   = 320,
      .base_height  = 240,
      .max_width    = 320,
      .max_height   = 240,
      .aspect_ratio = aspect,
   };
}

void retro_set_environment(retro_environment_t cb)
{
   environ_cb = cb;

   bool no_content = true;
   cb(RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME, &no_content);

   if (cb(RETRO_ENVIRONMENT_GET_LOG_INTERFACE, &logging))
      log_cb = logging.log;
   else
      log_cb = fallback_log;
}

void retro_set_audio_sample(retro_audio_sample_t cb)
{
}

void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb)
{
}

void retro_set_input_poll(retro_input_poll_t cb)
{
}

void retro_set_input_state(retro_input_state_t cb)
{
}

void retro_set_video_refresh(retro_video_refresh_t cb)
{
   video_cb = cb;
}

static unsigned x_coord;
static unsigned y_coord;

void retro_reset(void)
{
   x_coord = 0;
   y_coord = 0;
}

static void render_checkered(void)
{
   uint32_t *buf    = frame_buf;
   unsigned stride  = 320;
   uint32_t color_r = 0xff << 16;
   uint32_t color_g = 0xff <<  8;
   uint32_t *line   = buf;

   for (unsigned y = 0; y < 240; y++, line += stride)
   {
      unsigned index_y = ((y - y_coord) >> 4) & 1;
      for (unsigned x = 0; x < 320; x++)
      {
         unsigned index_x = ((x - x_coord) >> 4) & 1;
         line[x] = (index_y ^ index_x) ? color_r : color_g;
      }
   }

   video_cb(buf, 320, 240, stride << 2);
}

void retro_run(void)
{
   render_checkered();
}

bool retro_load_game(const struct retro_game_info *info)
{
   enum retro_pixel_format fmt = RETRO_PIXEL_FORMAT_XRGB8888;
   if (!environ_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &fmt))
   {
      log_cb(RETRO_LOG_INFO, "XRGB8888 is not supported.\n");
      return false;
   }

   // Test to make sure it initialized correctly.
   if (PHYSFS_isInit() == 0) {
      return false;
   }

   // Mount the current working directory
   if (PHYSFS_mount(".", "res", 1) == 0) {
      log_cb(RETRO_LOG_ERROR, "Failed to mount directory: %s\n", PHYSFS_getErrorByCode(PHYSFS_getLastErrorCode()));
      return false;
   }

   // Open the test file
   PHYSFS_File *file = PHYSFS_openRead("res/test.txt");
   if (file == NULL) {
      log_cb(RETRO_LOG_ERROR, "Failed to open file: %s\n", PHYSFS_getErrorByCode(PHYSFS_getLastErrorCode()));
      return false;
   }

   // Get the file size
   int size = PHYSFS_fileLength(file);
   if (size == -1) {
      log_cb(RETRO_LOG_ERROR, "Failed to get file length: %s\n", PHYSFS_getErrorByCode(PHYSFS_getLastErrorCode()));
      return false;
   }

   // Read the text file
   char buffer[256];
   PHYSFS_readBytes(file, buffer, size > 256 ? 255 : size);
   log_cb(RETRO_LOG_INFO, "Read: %s\n", buffer);

   // Display the text message as a message
   struct retro_message msg = {
      buffer,
      100
   };
   environ_cb(RETRO_ENVIRONMENT_SET_MESSAGE, &msg);

   (void)info;
   return true;
}

void retro_unload_game(void)
{
   if (PHYSFS_isInit() == 0) {
      return;
   }

   PHYSFS_unmount("res");
}

unsigned retro_get_region(void)
{
   return RETRO_REGION_NTSC;
}

bool retro_load_game_special(unsigned type, const struct retro_game_info *info, size_t num)
{
    (void)info;
   if (type != 0x200)
      return false;
   if (num != 2)
      return false;
   return retro_load_game(NULL);
}

size_t retro_serialize_size(void)
{
   return 2;
}

bool retro_serialize(void *data_, size_t size)
{
   return true;
}

bool retro_unserialize(const void *data_, size_t size)
{
   return true;
}

void *retro_get_memory_data(unsigned id)
{
   (void)id;
   return NULL;
}

size_t retro_get_memory_size(unsigned id)
{
   (void)id;
   return 0;
}

void retro_cheat_reset(void)
{}

void retro_cheat_set(unsigned index, bool enabled, const char *code)
{
   (void)index;
   (void)enabled;
   (void)code;
}
