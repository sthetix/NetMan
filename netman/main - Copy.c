/*
 * Copyright (c) 2021 HWFLY (original), 2025 HATSIFY
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <string.h>
#include <stdlib.h>
#include <bdk.h>
#include <memory_map.h> // Add this line
#include "config.h"
#include "gfx/tui.h"
#include <libs/fatfs/ff.h>
#include <ianos/ianos.h>
#include <utils/util.h>   
#include <soc/hw_init.h>

hekate_config h_cfg;

// Version metadata
const volatile ipl_ver_meta_t __attribute__((section ("._ipl_version"))) ipl_ver = {
    .magic = BL_MAGIC,
    .version = (1 + '0') | ((0 + '0') << 8) | ((0 + '0') << 16), // v1.0.0
    .rsvd0 = 0,
    .rsvd1 = 0
};

char __end__;

volatile nyx_storage_t *nyx_str = (nyx_storage_t *)NYX_STORAGE_ADDR;

#define EXOSPHERE_PATH "exosphere.ini"
#define HOSTS_DEFAULT_PATH "atmosphere/hosts/default.txt"
#define HOSTS_SYSMMC_PATH "atmosphere/hosts/sysmmc.txt"
#define HOSTS_EMUMMC_PATH "atmosphere/hosts/emummc.txt"
#define SYS_SETTINGS_PATH "atmosphere/config/system_settings.ini"
#define BACKUP_DIR "HATSIFY/backups"

// Add these defines for proper hekate launching
#define RELOC_META_OFF      0x7C
#define PATCHED_RELOC_SZ    0x94
#define PATCHED_RELOC_STACK 0x40007000
#define PATCHED_RELOC_ENTRY 0x40010000
#define EXT_PAYLOAD_ADDR    0xC0000000
#define RCM_PAYLOAD_ADDR    (EXT_PAYLOAD_ADDR + ALIGN(PATCHED_RELOC_SZ, 0x10))
#define COREBOOT_END_ADDR   0xD0000000
#define COREBOOT_VER_OFF    0x41
#define CBFS_DRAM_EN_ADDR   0x4003e000
#define CBFS_DRAM_MAGIC     0x4452414D // "DRAM"

#ifndef EXCP_EN_ADDR
#define EXCP_EN_ADDR     0x4003FF00
#define EXCP_TYPE_ADDR   0x4003FF04
#define EXCP_LR_ADDR     0x4003FF08
#define EXCP_MAGIC       0x30454348

#define EXCP_TYPE_RESET  0
#define EXCP_TYPE_UNDEF  1
#define EXCP_TYPE_PABRT  2
#define EXCP_TYPE_DABRT  3
#endif


static void *coreboot_addr;

// Network configuration templates
const char *hosts_block_all =
"# Nintendo Servers\n"
"127.0.0.1 *nintendo.*\n"
"127.0.0.1 *nintendoswitch.*\n"
"127.0.0.1 *.nintendo.com\n"
"127.0.0.1 *.nintendo.net\n"
"127.0.0.1 *.nintendo.jp\n"
"127.0.0.1 *.nintendo.co.jp\n"
"127.0.0.1 *.nintendo.co.uk\n"
"127.0.0.1 *.nintendo-europe.com\n"
"127.0.0.1 *.nintendowifi.net\n"
"127.0.0.1 *.nintendo.es\n"
"127.0.0.1 *.nintendo.co.kr\n"
"127.0.0.1 *.nintendo.tw\n"
"127.0.0.1 *.nintendo.com.hk\n"
"127.0.0.1 *.nintendo.com.au\n"
"127.0.0.1 *.nintendo.co.nz\n"
"127.0.0.1 *.nintendo.at\n"
"127.0.0.1 *.nintendo.be\n"
"127.0.0.1 *.nintendods.cz\n"
"127.0.0.1 *.nintendo.dk\n"
"127.0.0.1 *.nintendo.de\n"
"127.0.0.1 *.nintendo.fi\n"
"127.0.0.1 *.nintendo.fr\n"
"127.0.0.1 *.nintendo.gr\n"
"127.0.0.1 *.nintendo.hu\n"
"127.0.0.1 *.nintendo.it\n"
"127.0.0.1 *.nintendo.nl\n"
"127.0.0.1 *.nintendo.no\n"
"127.0.0.1 *.nintendo.pt\n"
"127.0.0.1 *.nintendo.ru\n"
"127.0.0.1 *.nintendo.co.za\n"
"127.0.0.1 *.nintendo.se\n"
"127.0.0.1 *.nintendo.ch\n"
"127.0.0.1 *.nintendoswitch.com\n"
"127.0.0.1 *.nintendoswitch.com.cn\n"
"127.0.0.1 *.nintendoswitch.cn\n"
"127.0.0.1 receive-*.dg.srv.nintendo.net\n"
"127.0.0.1 receive-*.er.srv.nintendo.net\n"
"# Nintendo CDN\n"
"95.216.149.205 conntest.nintendowifi.net\n"
"95.216.149.205 ctest.cdn.nintendo.net\n";

const char *hosts_open = "# No Nintendo server blocks\n";

// Exosphere.ini templates
const char *exosphere_offline =
"[exosphere]\n"
"debugmode=1\n"
"debugmode_user=0\n"
"disable_user_exception_handlers=0\n"
"enable_user_pmu_access=0\n"
"blank_prodinfo_sysmmc=1\n"
"blank_prodinfo_emummc=1\n"
"allow_writing_to_cal_sysmmc=0\n"
"log_port=0\n"
"log_baud_rate=115200\n"
"log_inverted=0\n";

const char *exosphere_sysmmc_online =
"[exosphere]\n"
"debugmode=1\n"
"debugmode_user=0\n"
"disable_user_exception_handlers=0\n"
"enable_user_pmu_access=0\n"
"blank_prodinfo_sysmmc=0\n"
"blank_prodinfo_emummc=1\n"
"allow_writing_to_cal_sysmmc=0\n"
"log_port=0\n"
"log_baud_rate=115200\n"
"log_inverted=0\n";

const char *exosphere_emummc_online =
"[exosphere]\n"
"debugmode=1\n"
"debugmode_user=0\n"
"disable_user_exception_handlers=0\n"
"enable_user_pmu_access=0\n"
"blank_prodinfo_sysmmc=1\n"
"blank_prodinfo_emummc=0\n"
"allow_writing_to_cal_sysmmc=0\n"
"log_port=0\n"
"log_baud_rate=115200\n"
"log_inverted=0\n";

// Configuration without any prodinfo blanking - MAXIMUM RISK
const char* exosphere_full_online =
"[exosphere]\n"
"debugmode=1\n"
"debugmode_user=0\n"
"disable_user_exception_handlers=0\n"
"enable_user_pmu_access=0\n"
"blank_prodinfo_sysmmc=0\n"  // No blanking for sysMMC
"blank_prodinfo_emummc=0\n"  // No blanking for emuMMC
"allow_writing_to_cal_sysmmc=0\n"
"log_port=0\n"
"log_baud_rate=115200\n"
"log_inverted=0\n";

// System settings templates
const char *sys_settings_mitm_on =
"[atmosphere]\n"
"enable_dns_mitm=1\n"
"add_defaults_to_dns_hosts=1\n";

const char *sys_settings_mitm_off =
"[atmosphere]\n"
"enable_dns_mitm=0\n"
"add_defaults_to_dns_hosts=0\n";

// Proper payload launching functions from hwfly-toolbox
void reloc_patcher(u32 payload_dst, u32 payload_src, u32 payload_size)
{
    memcpy((u8 *)payload_src, (u8 *)IPL_LOAD_ADDR, PATCHED_RELOC_SZ);

    volatile reloc_meta_t *relocator = (reloc_meta_t *)(payload_src + RELOC_META_OFF);

    relocator->start = payload_dst - ALIGN(PATCHED_RELOC_SZ, 0x10);
    relocator->stack = PATCHED_RELOC_STACK;
    relocator->end   = payload_dst + payload_size;
    relocator->ep    = payload_dst;

    if (payload_size == 0x7000)
    {
        memcpy((u8 *)(payload_src + ALIGN(PATCHED_RELOC_SZ, 0x10)), coreboot_addr, 0x7000); //Bootblock
        *(vu32 *)CBFS_DRAM_EN_ADDR = CBFS_DRAM_MAGIC;
    }
}

int launch_payload(char *path, bool clear_screen)
{
    if (clear_screen)
        gfx_clear_grey(0x1B);
    gfx_con_setpos(0, 0);

    if (sd_mount())
    {
        FIL fp;
        if (f_open(&fp, path, FA_READ))
        {
            gfx_con.mute = false;
            EPRINTFARGS("Payload file is missing!\n(%s)", path);
            goto out;
        }

        // Read and copy the payload to our chosen address
        void *buf;
        u32 size = f_size(&fp);

        if (size < 0x30000)
            buf = (void *)RCM_PAYLOAD_ADDR;
        else
        {
            coreboot_addr = (void *)(COREBOOT_END_ADDR - size);
            buf = coreboot_addr;
            if (h_cfg.t210b01)
            {
                f_close(&fp);
                gfx_con.mute = false;
                EPRINTF("Coreboot not allowed on Mariko!");
                goto out;
            }
        }

        if (f_read(&fp, buf, size, NULL))
        {
            f_close(&fp);
            goto out;
        }

        f_close(&fp);
        sd_end();

        if (size < 0x30000)
        {
            reloc_patcher(PATCHED_RELOC_ENTRY, EXT_PAYLOAD_ADDR, ALIGN(size, 0x10));
            hw_reinit_workaround(false, byte_swap_32(*(u32 *)(buf + size - sizeof(u32))));
        }
        else
        {
            reloc_patcher(PATCHED_RELOC_ENTRY, EXT_PAYLOAD_ADDR, 0x7000);

            // Get coreboot seamless display magic.
            u32 magic = 0;
            char *magic_ptr = buf + COREBOOT_VER_OFF;
            memcpy(&magic, magic_ptr + strlen(magic_ptr) - 4, 4);
            hw_reinit_workaround(true, magic);
        }

        // Some cards (Sandisk U1), do not like a fast power cycle. Wait min 100ms.
        sdmmc_storage_init_wait_sd();

        void (*ext_payload_ptr)() = (void *)EXT_PAYLOAD_ADDR;

        // Launch our payload.
        (*ext_payload_ptr)();
    }

out:
    sd_end();
    return 1;
}

// Function to save a file
static bool save_file(const char *path, const char *data, u32 size) {
    FIL fp;
    if (f_open(&fp, path, FA_CREATE_ALWAYS | FA_WRITE)) {
        EPRINTFARGS("Failed to open %s for writing!", path);
        return false;
    }
    UINT bw;
    if (f_write(&fp, data, size, &bw) || bw != size) {
        f_close(&fp);
        EPRINTFARGS("Failed to write to %s!", path);
        return false;
    }
    f_close(&fp);
    return true;
}

// Function to backup a file
static bool backup_file(const char *src_path, const char *backup_path) {
    FIL fp;
    if (f_open(&fp, src_path, FA_READ)) {
        EPRINTFARGS("Failed to open %s for backup!", src_path);
        return false;
    }
    u32 size = f_size(&fp);
    void *buf = malloc(size);
    if (!buf) {
        f_close(&fp);
        EPRINTF("Memory allocation failed!");
        return false;
    }
    UINT br;
    if (f_read(&fp, buf, size, &br) || br != size) {
        f_close(&fp);
        free(buf);
        EPRINTFARGS("Failed to read %s!", src_path);
        return false;
    }
    f_close(&fp);
    bool result = sd_save_to_file(buf, size, backup_path);
    free(buf);
    return result;
}

// Function to restore a file
static bool restore_file(const char *backup_path, const char *dst_path) {
    u32 size;
    void *buf = sd_file_read(backup_path, &size);
    if (!buf) {
        EPRINTFARGS("Failed to read backup %s!", backup_path);
        return false;
    }
    bool result = save_file(dst_path, buf, size);
    free(buf);
    return result;
}

// Function to read a file and return its contents
static char* read_file(const char *path) {
    FIL fp;
    if (f_open(&fp, path, FA_READ)) {
        return NULL;
    }
    u32 size = f_size(&fp);
    char *buf = malloc(size + 1);
    if (!buf) {
        f_close(&fp);
        return NULL;
    }
    UINT br;
    if (f_read(&fp, buf, size, &br) || br != size) {
        f_close(&fp);
        free(buf);
        return NULL;
    }
    f_close(&fp);
    buf[size] = '\0';
    return buf;
}

// Properly update DNS MITM settings in system_settings.ini
static bool update_dns_mitm_settings(bool enable_mitm) {
    // Read the existing file
    char *content = read_file(SYS_SETTINGS_PATH);
    
    if (!content) {
        gfx_printf("Error: Could not read system_settings.ini\n");
        return false;
    }
    
    // Find the DNS MITM settings
    char *dns_mitm_pos = strstr(content, "enable_dns_mitm = u8!0x");
    char *defaults_pos = strstr(content, "add_defaults_to_dns_hosts = u8!0x");
    
    if (dns_mitm_pos && defaults_pos) {
        // Change just the single character after the prefix
        dns_mitm_pos[strlen("enable_dns_mitm = u8!0x")] = enable_mitm ? '1' : '0';
        defaults_pos[strlen("add_defaults_to_dns_hosts = u8!0x")] = enable_mitm ? '1' : '0';
        
        // Save the updated content
        bool result = save_file(SYS_SETTINGS_PATH, content, strlen(content));
        free(content);
        return result;
    }
    
    free(content);
    gfx_printf("Error: Could not find DNS MITM settings in system_settings.ini\n");
    return false;
}

// Menu functions
// Modified function for set_default_config() with proper warning
void set_default_config() {
    // First screen with warning
    gfx_clear_partial_grey(0x1B, 0, 1256);
    gfx_con_setpos(0, 0);
    
    gfx_printf("%kDefault Mode Information:%k\n\n", 0xFFFF8000, 0xFFFFFFFF);
    
    gfx_printf("  This mode keeps both sysMMC and emuMMC\n");
    gfx_printf("  completely offline from Nintendo.\n\n");
    
    gfx_printf("  - All connections to Nintendo servers\n");
    gfx_printf("    will be blocked\n");
    gfx_printf("  - Console identifiers will be blanked\n");
    gfx_printf("  - This is the safest mode to prevent\n");
    gfx_printf("    ban risk\n\n");
    
    gfx_printf("%kContinue with Default Mode?%k\n\n", 0xFFFF8000, 0xFFFFFFFF);
    gfx_printf("Press any key...\n\n");
    btn_wait();
    
    // COMPLETELY RESET SCREEN AND CONSOLE
    gfx_clear_grey(0x1B);
    gfx_con_setpos(0, 0);
    
    // Separate processing screen with new title
    gfx_printf("%kSetting Default Configuration%k\n\n", 0xFFFF8000, 0xFFFFFFFF);
    
    if (!sd_mount()) {
        EPRINTF("Failed to mount SD card!");
        goto out;
    }
    
    // Instead of text on the same line, break it into clearly separate lines
    gfx_printf("Applying settings for both offline mode...\n\n");
    
    if (save_file(EXOSPHERE_PATH, exosphere_offline, strlen(exosphere_offline)))
        gfx_printf("exosphere.ini: Updated\n");
    else
        gfx_printf("exosphere.ini: Failed to update\n");
    
    if (save_file(HOSTS_DEFAULT_PATH, hosts_block_all, strlen(hosts_block_all)))
        gfx_printf("default.txt: Updated\n");
    else
        gfx_printf("default.txt: Failed to update\n");
        
    if (save_file(HOSTS_SYSMMC_PATH, hosts_block_all, strlen(hosts_block_all)))
        gfx_printf("sysmmc.txt: Updated\n");
    else
        gfx_printf("sysmmc.txt: Failed to update\n");
        
    if (save_file(HOSTS_EMUMMC_PATH, hosts_block_all, strlen(hosts_block_all)))
        gfx_printf("emummc.txt: Updated\n");
    else
        gfx_printf("emummc.txt: Failed to update\n");
    
    if (update_dns_mitm_settings(true))
        gfx_printf("DNS MITM: Enabled\n");
    else
        gfx_printf("DNS MITM: Failed to update\n");
    
    gfx_printf("\nAll Nintendo connections blocked.\n");
    gfx_printf("Console identifiers blanked.\n");
    
out:
    gfx_printf("\nPress any key to return...\n");
    msleep(500);
    btn_wait();
    sd_end();
}

// Modified function for set_sysmmc_online() with proper warning
void set_sysmmc_online() {
    // First screen with warning
    gfx_clear_grey(0x1B);  // Clear the entire screen
    gfx_con_setpos(0, 0);
    
    gfx_printf("%kWARNING - BAN RISK:%k\n\n", 0xFFFF0000, 0xFFFFFFFF);
    
    gfx_printf("  This mode allows sysMMC to connect\n");
    gfx_printf("  to Nintendo online services.\n\n");
    
    gfx_printf("  - Your REAL console identity will\n");
    gfx_printf("    be visible to Nintendo\n");
    gfx_printf("  - If detected with unauthorized mods,\n");
    gfx_printf("    your console may be banned\n");
    gfx_printf("  - emuMMC will remain safely offline\n\n");
    
    gfx_printf("%kProceed at your own risk!%k\n\n", 0xFFFF0000, 0xFFFFFFFF);
    gfx_printf("Press any key to continue\n");
    gfx_printf("or VOL- to cancel...\n\n");  // Separated into two lines
    
    u32 btn = btn_wait();
    if (btn & BTN_VOL_DOWN) {
        gfx_printf("Operation cancelled. Press any key to return...\n");
        msleep(500);
        btn_wait();
        return;
    }
    
    // COMPLETELY RESET SCREEN AND CONSOLE
    gfx_clear_grey(0x1B);
    gfx_con_setpos(0, 0);
    
    // Separate processing screen with new title
    gfx_printf("%kSetting sysMMC Online%k\n\n", 0xFFFF0000, 0xFFFFFFFF);
    
    if (!sd_mount()) {
        EPRINTF("Failed to mount SD card!");
        goto out;
    }
    
    gfx_printf("Applying settings for sysMMC online...\n\n");
    
    if (save_file(EXOSPHERE_PATH, exosphere_sysmmc_online, strlen(exosphere_sysmmc_online)))
        gfx_printf("exosphere.ini: Updated\n");
    else
        gfx_printf("exosphere.ini: Failed to update\n");
    
    if (save_file(HOSTS_SYSMMC_PATH, hosts_open, strlen(hosts_open)))
        gfx_printf("sysmmc.txt: Updated\n");
    else
        gfx_printf("sysmmc.txt: Failed to update\n");
        
    if (save_file(HOSTS_EMUMMC_PATH, hosts_block_all, strlen(hosts_block_all)))
        gfx_printf("emummc.txt: Updated\n");
    else
        gfx_printf("emummc.txt: Failed to update\n");
        
    if (save_file(HOSTS_DEFAULT_PATH, hosts_block_all, strlen(hosts_block_all)))
        gfx_printf("default.txt: Updated\n");
    else
        gfx_printf("default.txt: Failed to update\n");
    
    if (update_dns_mitm_settings(false))
        gfx_printf("DNS MITM: Disabled\n");
    else
        gfx_printf("DNS MITM: Failed to update\n");
    
    gfx_printf("\nsysMMC can now connect to Nintendo services.\n");
    gfx_printf("emuMMC remains protected and offline.\n");
    
out:
    gfx_printf("\nPress any key to return...\n");
    msleep(500);
    btn_wait();
    sd_end();
}

// Modified function for set_emummc_online() with proper warning
void set_emummc_online() {
    // First screen with warning
    gfx_clear_grey(0x1B);  // Clear the entire screen
    gfx_con_setpos(0, 0);
    
    gfx_printf("%kWARNING - BAN RISK:%k\n\n", 0xFFFF0000, 0xFFFFFFFF);
    
    gfx_printf("  This mode allows emuMMC to connect\n");
    gfx_printf("  to Nintendo online services.\n\n");
    
    gfx_printf("  - While your sysMMC is protected, your\n");
    gfx_printf("    emuMMC can still be banned\n");
    gfx_printf("  - Nintendo may detect unauthorized\n");
    gfx_printf("    modifications on emuMMC\n");
    gfx_printf("  - Not recommended for emuMMCs with\n");
    gfx_printf("    pirated content or significant mods\n\n");
    
    gfx_printf("%kProceed at your own risk!%k\n\n", 0xFFFF0000, 0xFFFFFFFF);
    gfx_printf("Press any key to continue\n");
    gfx_printf("or VOL- to cancel...\n\n");  // Separated into two lines
    
    u32 btn = btn_wait();
    if (btn & BTN_VOL_DOWN) {
        gfx_printf("Operation cancelled. Press any key to return...\n");
        msleep(500);
        btn_wait();
        return;
    }
    
    // COMPLETELY RESET SCREEN AND CONSOLE
    gfx_clear_grey(0x1B);
    gfx_con_setpos(0, 0);
    
    // Separate processing screen with new title
    gfx_printf("%kSetting emuMMC Online%k\n\n", 0xFFFF0000, 0xFFFFFFFF);
    
    if (!sd_mount()) {
        EPRINTF("Failed to mount SD card!");
        goto out;
    }
    
    gfx_printf("Applying settings for emuMMC online...\n\n");
    
    if (save_file(EXOSPHERE_PATH, exosphere_emummc_online, strlen(exosphere_emummc_online)))
        gfx_printf("exosphere.ini: Updated\n");
    else
        gfx_printf("exosphere.ini: Failed to update\n");
    
    if (save_file(HOSTS_SYSMMC_PATH, hosts_block_all, strlen(hosts_block_all)))
        gfx_printf("sysmmc.txt: Updated\n");
    else
        gfx_printf("sysmmc.txt: Failed to update\n");
        
    if (save_file(HOSTS_EMUMMC_PATH, hosts_open, strlen(hosts_open)))
        gfx_printf("emummc.txt: Updated\n");
    else
        gfx_printf("emummc.txt: Failed to update\n");
        
    if (save_file(HOSTS_DEFAULT_PATH, hosts_block_all, strlen(hosts_block_all)))
        gfx_printf("default.txt: Updated\n");
    else
        gfx_printf("default.txt: Failed to update\n");
    
    if (update_dns_mitm_settings(false))
        gfx_printf("DNS MITM: Disabled\n");
    else
        gfx_printf("DNS MITM: Failed to update\n");
    
    gfx_printf("\nemuMMC can now connect to Nintendo services.\n");
    gfx_printf("sysMMC remains protected and offline.\n");
    
out:
    gfx_printf("\nPress any key to return...\n");
    msleep(500);
    btn_wait();
    sd_end();
}

// Modified function for set_both_online() with proper warning
void set_both_online() {
    // First screen with warning
    gfx_clear_grey(0x1B);  // Clear the entire screen
    gfx_con_setpos(0, 0);
    
    gfx_printf("%kSEVERE WARNING - MAXIMUM BAN RISK:%k\n\n", 0xFFFF0000, 0xFFFFFFFF);
    
    gfx_printf("  This mode allows both sysMMC and emuMMC\n");
    gfx_printf("  full online access to Nintendo servers.\n\n");
    
    gfx_printf("  - %kEXTREMELY HIGH RISK%k of console ban\n", 0xFFFF0000, 0xFFFFFFFF);
    gfx_printf("  - No protection against Nintendo\n");
    gfx_printf("    detecting unauthorized modifications\n");
    gfx_printf("  - Full prodinfo access - console serial\n");
    gfx_printf("    numbers and identifiers will be visible\n");
    gfx_printf("  - For advanced users only who fully\n");
    gfx_printf("    understand and accept the consequences\n\n");
    
    gfx_printf("%kAre you absolutely certain?%k\n\n", 0xFFFF0000, 0xFFFFFFFF);
    gfx_printf("Press VOL+ to confirm\n");
    gfx_printf("or VOL- to cancel...\n\n");  // Separated into two lines
    
    u32 btn = btn_wait();
    if (!(btn & BTN_VOL_UP) || (btn & BTN_VOL_DOWN)) {
        gfx_printf("Operation cancelled. Press any key to return...\n");
        msleep(500);
        btn_wait();
        return;
    }
    
    // COMPLETELY RESET SCREEN AND CONSOLE
    gfx_clear_grey(0x1B);
    gfx_con_setpos(0, 0);
    
    // Separate processing screen with new title
    gfx_printf("%kSetting Both Online (Maximum Risk)%k\n\n", 0xFFFF0000, 0xFFFFFFFF);
    
    if (!sd_mount()) {
        EPRINTF("Failed to mount SD card!");
        goto out;
    }
    
    // Fixed the truncated text by splitting across multiple lines
    gfx_printf("Applying settings for full online mode...\n\n");
    
    // Use either existing exosphere configuration
    // If you have exosphere_offline that blanks both, we can use exosphere_sysmmc_online or exosphere_emummc_online
    // which likely have only one of the MMCs blanked
    if (save_file(EXOSPHERE_PATH, exosphere_sysmmc_online, strlen(exosphere_sysmmc_online)))
        gfx_printf("exosphere.ini: Updated (no blanking)\n");
    else
        gfx_printf("exosphere.ini: Failed to update\n");
    
    if (save_file(HOSTS_SYSMMC_PATH, hosts_open, strlen(hosts_open)))
        gfx_printf("sysmmc.txt: Updated\n");
    else
        gfx_printf("sysmmc.txt: Failed to update\n");
        
    if (save_file(HOSTS_EMUMMC_PATH, hosts_open, strlen(hosts_open)))
        gfx_printf("emummc.txt: Updated\n");
    else
        gfx_printf("emummc.txt: Failed to update\n");
        
    if (save_file(HOSTS_DEFAULT_PATH, hosts_open, strlen(hosts_open)))
        gfx_printf("default.txt: Updated\n");
    else
        gfx_printf("default.txt: Failed to update\n");
    
    if (update_dns_mitm_settings(false))
        gfx_printf("DNS MITM: Disabled\n");
    else
        gfx_printf("DNS MITM: Failed to update\n");
    
    // Split the multi-line summary into separate lines to avoid truncation
    gfx_printf("\nBoth sysMMC and emuMMC can now\n");
    gfx_printf("connect to Nintendo.\n\n");
    gfx_printf("All console identifiers are\n");
    gfx_printf("being sent to Nintendo servers.\n\n");
    gfx_printf("%kMAXIMUM BAN RISK: No protection\n", 0xFFFF0000);
    gfx_printf("is active with this configuration!%k\n", 0xFFFFFFFF);
    
out:
    gfx_printf("\nPress any key to return...\n");
    msleep(500);
    btn_wait();
    sd_end();
}

void backup_configs() {
    // COMPLETELY RESET SCREEN AND CONSOLE
    gfx_clear_grey(0x1B);
    gfx_con_setpos(0, 0);
    
    // Separate processing screen with new title
    gfx_printf("%kBackup Configuration Files%k\n\n", 0xFFFFFFFF, 0xFFFFFFFF);
    
    if (!sd_mount()) {
        EPRINTF("Failed to mount SD card!");
        goto out;
    }
    
    gfx_printf("Creating backup directory...\n\n");
    f_mkdir(BACKUP_DIR);
    
    if (backup_file(EXOSPHERE_PATH, BACKUP_DIR "/exosphere.ini.bak"))
        gfx_printf("exosphere.ini: Backed up\n");
    else
        gfx_printf("exosphere.ini: Backup failed\n");
    
    if (backup_file(HOSTS_DEFAULT_PATH, BACKUP_DIR "/default.txt.bak"))
        gfx_printf("default.txt: Backed up\n");
    else
        gfx_printf("default.txt: Backup failed\n");
        
    if (backup_file(HOSTS_SYSMMC_PATH, BACKUP_DIR "/sysmmc.txt.bak"))
        gfx_printf("sysmmc.txt: Backed up\n");
    else
        gfx_printf("sysmmc.txt: Backup failed\n");
        
    if (backup_file(HOSTS_EMUMMC_PATH, BACKUP_DIR "/emummc.txt.bak"))
        gfx_printf("emummc.txt: Backed up\n");
    else
        gfx_printf("emummc.txt: Backup failed\n");
        
    if (backup_file(SYS_SETTINGS_PATH, BACKUP_DIR "/system_settings.ini.bak"))
        gfx_printf("system_settings.ini: Backed up\n");
    else
        gfx_printf("system_settings.ini: Backup failed\n");
    
    gfx_printf("\nBackup completed to %s\n", BACKUP_DIR);
    
out:
    gfx_printf("\nPress any key to return...\n");
    msleep(500);
    btn_wait();
    sd_end();
}

void restore_configs() {
    // COMPLETELY RESET SCREEN AND CONSOLE
    gfx_clear_grey(0x1B);
    gfx_con_setpos(0, 0);
    
    // Separate processing screen with new title
    gfx_printf("%kRestore Configuration Files%k\n\n", 0xFFFFFFFF, 0xFFFFFFFF);
    
    if (!sd_mount()) {
        EPRINTF("Failed to mount SD card!");
        goto out;
    }
    
    gfx_printf("Restoring from backup directory...\n\n");
    
    if (restore_file(BACKUP_DIR "/exosphere.ini.bak", EXOSPHERE_PATH))
        gfx_printf("exosphere.ini: Restored\n");
    else
        gfx_printf("exosphere.ini: Restore failed\n");
    
    if (restore_file(BACKUP_DIR "/default.txt.bak", HOSTS_DEFAULT_PATH))
        gfx_printf("default.txt: Restored\n");
    else
        gfx_printf("default.txt: Restore failed\n");
        
    if (restore_file(BACKUP_DIR "/sysmmc.txt.bak", HOSTS_SYSMMC_PATH))
        gfx_printf("sysmmc.txt: Restored\n");
    else
        gfx_printf("sysmmc.txt: Restore failed\n");
        
    if (restore_file(BACKUP_DIR "/emummc.txt.bak", HOSTS_EMUMMC_PATH))
        gfx_printf("emummc.txt: Restored\n");
    else
        gfx_printf("emummc.txt: Restore failed\n");
        
    if (restore_file(BACKUP_DIR "/system_settings.ini.bak", SYS_SETTINGS_PATH))
        gfx_printf("system_settings.ini: Restored\n");
    else
        gfx_printf("system_settings.ini: Restore failed\n");
    
    gfx_printf("\nRestore completed from %s\n", BACKUP_DIR);
    
out:
    gfx_printf("\nPress any key to return...\n");
    msleep(500);
    btn_wait();
    sd_end();
}

void show_current_config() {
    // COMPLETELY RESET SCREEN AND CONSOLE
    gfx_clear_grey(0x1B);
    gfx_con_setpos(0, 0);
    
    // Separate processing screen with new title
    gfx_printf("%kCurrent Network Configuration%k\n\n", 0xFFFFFFFF, 0xFFFFFFFF);
    
    if (!sd_mount()) {
        EPRINTF("Failed to mount SD card!");
        goto out;
    }
    
    FIL fp;
    u32 size;
    char *buf;
    bool found = false;

    // Read exosphere.ini
    gfx_printf("exosphere.ini:\n");
    if (!f_open(&fp, EXOSPHERE_PATH, FA_READ)) {
        size = f_size(&fp);
        buf = malloc(size + 1);
        if (buf) {
            UINT br;
            f_read(&fp, buf, size, &br);
            buf[size] = '\0';
            gfx_printf("%s\n\n", buf);
            free(buf);
            found = true;
        }
        f_close(&fp);
    }
    if (!found) {
        gfx_printf("File not found\n\n");
    }
    
    found = false;
    // Read default.txt
    gfx_printf("default.txt:\n");
    if (!f_open(&fp, HOSTS_DEFAULT_PATH, FA_READ)) {
        size = f_size(&fp);
        buf = malloc(size + 1);
        if (buf) {
            UINT br;
            f_read(&fp, buf, size, &br);
            buf[size] = '\0';
            
            // Show just a summary if the file is large
            if (size > 500) {
                gfx_printf("(File is large, showing summary)\n");
                if (strstr(buf, "# No Nintendo server blocks")) {
                    gfx_printf("No Nintendo server blocks\n");
                } else {
                    gfx_printf("Blocking Nintendo servers\n");
                }
            } else {
                gfx_printf("%s\n", buf);
            }
            gfx_printf("\n");
            free(buf);
            found = true;
        }
        f_close(&fp);
    }
    if (!found) {
        gfx_printf("File not found\n\n");
    }
    
    found = false;
    // Read sysmmc.txt
    gfx_printf("sysmmc.txt:\n");
    if (!f_open(&fp, HOSTS_SYSMMC_PATH, FA_READ)) {
        size = f_size(&fp);
        buf = malloc(size + 1);
        if (buf) {
            UINT br;
            f_read(&fp, buf, size, &br);
            buf[size] = '\0';
            
            // Show just a summary if the file is large
            if (size > 500) {
                gfx_printf("(File is large, showing summary)\n");
                if (strstr(buf, "# No Nintendo server blocks")) {
                    gfx_printf("No Nintendo server blocks - sysMMC can go online\n");
                } else {
                    gfx_printf("Blocking Nintendo servers - sysMMC is offline\n");
                }
            } else {
                gfx_printf("%s\n", buf);
            }
            gfx_printf("\n");
            free(buf);
            found = true;
        }
        f_close(&fp);
    }
    if (!found) {
        gfx_printf("File not found\n\n");
    }
    
    found = false;
    // Read emummc.txt
    gfx_printf("emummc.txt:\n");
    if (!f_open(&fp, HOSTS_EMUMMC_PATH, FA_READ)) {
        size = f_size(&fp);
        buf = malloc(size + 1);
        if (buf) {
            UINT br;
            f_read(&fp, buf, size, &br);
            buf[size] = '\0';
            
            // Show just a summary if the file is large
            if (size > 500) {
                gfx_printf("(File is large, showing summary)\n");
                if (strstr(buf, "# No Nintendo server blocks")) {
                    gfx_printf("No Nintendo server blocks - emuMMC can go online\n");
                } else {
                    gfx_printf("Blocking Nintendo servers - emuMMC is offline\n");
                }
            } else {
                gfx_printf("%s\n", buf);
            }
            gfx_printf("\n");
            free(buf);
            found = true;
        }
        f_close(&fp);
    }
    if (!found) {
        gfx_printf("File not found\n\n");
    }
    
    found = false;
    // Read system_settings.ini - just check for DNS MITM
    if (!f_open(&fp, SYS_SETTINGS_PATH, FA_READ)) {
        size = f_size(&fp);
        buf = malloc(size + 1);
        if (buf) {
            UINT br;
            f_read(&fp, buf, size, &br);
            buf[size] = '\0';
            
            gfx_printf("DNS MITM Status:\n");
            if (strstr(buf, "enable_dns_mitm = u8!0x1")) {
                gfx_printf("DNS MITM is ENABLED - blocking active\n\n");
            } else {
                gfx_printf("DNS MITM is DISABLED - connections allowed\n\n");
            }
            
            free(buf);
            found = true;
        }
        f_close(&fp);
    }
    if (!found) {
        gfx_printf("Could not read DNS MITM status\n\n");
    }
    
    // Add a summary of the current mode
    gfx_printf("\n%kCurrent Mode Summary:%k\n", 0xFFFF8000, 0xFFFFFFFF);
    bool sys_online = false;
    bool emu_online = false;
    
    // Check sysmmc.txt for online status
    if (!f_open(&fp, HOSTS_SYSMMC_PATH, FA_READ)) {
        size = f_size(&fp);
        buf = malloc(size + 1);
        if (buf) {
            UINT br;
            f_read(&fp, buf, size, &br);
            buf[size] = '\0';
            if (strstr(buf, "# No Nintendo server blocks")) {
                sys_online = true;
            }
            free(buf);
        }
        f_close(&fp);
    }
    
    // Check emummc.txt for online status
    if (!f_open(&fp, HOSTS_EMUMMC_PATH, FA_READ)) {
        size = f_size(&fp);
        buf = malloc(size + 1);
        if (buf) {
            UINT br;
            f_read(&fp, buf, size, &br);
            buf[size] = '\0';
            if (strstr(buf, "# No Nintendo server blocks")) {
                emu_online = true;
            }
            free(buf);
        }
        f_close(&fp);
    }
    
    // Display current mode
    if (sys_online && emu_online) {
        gfx_printf("%kBOTH ONLINE (HIGH RISK)%k\n", 0xFFFF0000, 0xFFFFFFFF);
        gfx_printf("Both sysMMC and emuMMC can connect to Nintendo\n");
    } else if (sys_online) {
        gfx_printf("%ksysMMC ONLINE%k\n", 0xFFFF0000, 0xFFFFFFFF);
        gfx_printf("sysMMC can connect to Nintendo, emuMMC is offline\n");
    } else if (emu_online) {
        gfx_printf("%kemuMMC ONLINE%k\n", 0xFFFF0000, 0xFFFFFFFF);
        gfx_printf("emuMMC can connect to Nintendo, sysMMC is offline\n");
    } else {
        gfx_printf("%kBOTH OFFLINE (DEFAULT)%k\n", 0xFFFF8000, 0xFFFFFFFF);
        gfx_printf("Both sysMMC and emuMMC are offline from Nintendo\n");
    }

out:
    gfx_printf("\nPress any key to return...\n");
    msleep(500);
    btn_wait();
    sd_end();
}

// Replace the simple hekate_launch with the proper one
void hekate_launch()
{
    launch_payload("bootloader/update.bin", false);
}

power_state_t STATE_POWER_OFF = POWER_OFF_RESET;

ment_t ment_top[] = {
    MDEF_CAPTION("--- Network Modes ---", 0xFFDAFF7F),
    MDEF_HANDLER("Default (Both Offline)", set_default_config),
    MDEF_HANDLER("sysMMC Online", set_sysmmc_online),
    MDEF_HANDLER("emuMMC Online", set_emummc_online),
    MDEF_HANDLER("Both Online (High Risk)", set_both_online),
    MDEF_CAPTION("--- Config Management ---", 0xFFDAFF7F),
    MDEF_HANDLER("Backup Configs", backup_configs),
    MDEF_HANDLER("Restore Configs", restore_configs),
    MDEF_HANDLER("Show Current Config", show_current_config),
    MDEF_CAPTION("--- Navigation ---", 0xFFDAFF7F),
    MDEF_HANDLER("Back to hekate", hekate_launch),
    MDEF_HANDLER_EX("Power Off", &STATE_POWER_OFF, power_set_state_ex),
    MDEF_END()
};

menu_t menu_top = { ment_top, "HATSIFY v1.0.0", 0, 0 };

extern void pivot_stack(u32 stack_top);

// Remove the custom simple_menu - we'll use TUI instead

void ipl_main() {
    // Do initial HW configuration. This is compatible with consecutive reruns without a reset.
    hw_init();

    // Pivot the stack so we have enough space.
    pivot_stack(IPL_STACK_TOP);

    // Tegra/Horizon configuration goes to 0x80000000+, package2 goes to 0xA9800000, we place our heap in between.
    heap_init(IPL_HEAP_START);

    // Set bootloader's default configuration.
    set_default_configuration();

    // Initialize display.
    display_init();

    // Mount SD Card.
    h_cfg.errors |= !sd_mount() ? ERR_SD_BOOT_EN : 0;

    // Train DRAM and switch to max frequency.
    if (minerva_init()) //!TODO: Add Tegra210B01 support to minerva.
        h_cfg.errors |= ERR_LIBSYS_MTC;

    // Initialize display window, backlight and gfx console.
    u32 *fb = display_init_framebuffer_pitch();
    gfx_init_ctxt(fb, 720, 1280, 720);
    gfx_con_init();

    display_backlight_pwm_init();
    display_backlight_brightness(150, 1000);

    // Overclock BPMP.
    bpmp_clk_rate_set(h_cfg.t210b01 ? BPMP_CLK_DEFAULT_BOOST : BPMP_CLK_LOWER_BOOST);

    // Failed to launch Nyx, unmount SD Card.
    sd_end();

    // Set ram to a freq that doesn't need periodic training.
    minerva_change_freq(FREQ_800);

    while (true)
        tui_do_menu(&menu_top);

    // Halt BPMP if we managed to get out of execution.
    while (true)
        bpmp_halt();
}