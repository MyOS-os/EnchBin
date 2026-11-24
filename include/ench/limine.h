// include/ench/limine.h — минимальный поднабор Limine boot protocol (v8.x).
// Значения магий и раскладка структур взяты из limine.h v8.x-binary.
#pragma once

#include <ench/base.h>

#define LIMINE_REQUESTS_START_MARKER \
    u64 limine_requests_start_marker[4] = { 0xf6b8f4b39de7d1ae, 0xfab91a6940fcb9cf, \
                                            0x785c6ed015d3e316, 0x181e920a7852b9d9 };
#define LIMINE_REQUESTS_END_MARKER \
    u64 limine_requests_end_marker[2] = { 0xadc0e0531bb10d03, 0x9572709f31764c62 };

#define LIMINE_BASE_REVISION(N) \
    u64 limine_base_revision[3] = { 0xf9562b2d5c95a6c8, 0x6a7b384944536bdc, (N) };
#define LIMINE_BASE_REVISION_SUPPORTED (limine_base_revision[2] == 0)

#define LIMINE_COMMON_MAGIC 0xc7b1dd30df4c8b88, 0x0a82e883a194f07b

#define LIMINE_HHDM_REQUEST { LIMINE_COMMON_MAGIC, 0x48dcf1cb8ad2b852, 0x63984e959a98244b }
#define LIMINE_FRAMEBUFFER_REQUEST { LIMINE_COMMON_MAGIC, 0x9d5827dcd881dd75, 0xa3148604f6fab11b }
#define LIMINE_MEMMAP_REQUEST { LIMINE_COMMON_MAGIC, 0x67cf3d9d378a806f, 0xe304acdfc50c3c62 }
#define LIMINE_EXECUTABLE_FILE_REQUEST { LIMINE_COMMON_MAGIC, 0xad97e90e83f1ed67, 0x31eb5d1c5ff23b69 }
#define LIMINE_MODULE_REQUEST { LIMINE_COMMON_MAGIC, 0x3e7e279702be32af, 0xca1c4f3bd1280cee }

#define LIMINE_FRAMEBUFFER_RGB 1

#define LIMINE_MEMMAP_USABLE                 0
#define LIMINE_MEMMAP_RESERVED               1
#define LIMINE_MEMMAP_ACPI_RECLAIMABLE       2
#define LIMINE_MEMMAP_ACPI_NVS               3
#define LIMINE_MEMMAP_BAD_MEMORY             4
#define LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE 5
#define LIMINE_MEMMAP_KERNEL_AND_MODULES     6
#define LIMINE_MEMMAP_FRAMEBUFFER            7

struct limine_hhdm_response {
    u64 revision;
    u64 offset;
};

struct limine_hhdm_request {
    u64 id[4];
    u64 revision;
    struct limine_hhdm_response *response;
};

struct limine_framebuffer {
    void *address;
    u64 width;
    u64 height;
    u64 pitch;
    u16 bpp;
    u8 memory_model;
    u8 red_mask_size;
    u8 red_mask_shift;
    u8 green_mask_size;
    u8 green_mask_shift;
    u8 blue_mask_size;
    u8 blue_mask_shift;
    u8 unused[7];
    u64 edid_size;
    void *edid;
    u64 mode_count;
    struct limine_video_mode **modes;
};

struct limine_video_mode {
    u64 pitch;
    u64 width;
    u64 height;
    u16 bpp;
    u8 memory_model;
    u8 red_mask_size;
    u8 red_mask_shift;
    u8 green_mask_size;
    u8 green_mask_shift;
    u8 blue_mask_size;
    u8 blue_mask_shift;
};

struct limine_framebuffer_response {
    u64 revision;
    u64 framebuffer_count;
    struct limine_framebuffer **framebuffers;
};

struct limine_framebuffer_request {
    u64 id[4];
    u64 revision;
    struct limine_framebuffer_response *response;
};

struct limine_memmap_entry {
    u64 base;
    u64 length;
    u64 type;
};

struct limine_memmap_response {
    u64 revision;
    u64 entry_count;
    struct limine_memmap_entry **entries;
};

struct limine_memmap_request {
    u64 id[4];
    u64 revision;
    struct limine_memmap_response *response;
};

struct limine_file {
    u64 revision;
    void *address;
    u64 size;
    char *path;
    char *cmdline;
    u32 media_type;
    u32 unused;
    u32 tftp_ip;
    u32 tftp_port;
    u32 partition_index;
    u32 mbr_disk_id;
    u64 gpt_disk_uuid[2];
    u64 gpt_part_uuid[2];
    u64 part_uuid[2];
};

struct limine_executable_file_response {
    u64 revision;
    struct limine_file *executable_file;
};

struct limine_executable_file_request {
    u64 id[4];
    u64 revision;
    struct limine_executable_file_response *response;
};

struct limine_module_response {
    u64 revision;
    u64 module_count;
    struct limine_file **modules;
};

struct limine_module_request {
    u64 id[4];
    u64 revision;
    struct limine_module_response *response;
};
