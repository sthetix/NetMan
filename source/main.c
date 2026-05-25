/*
 * Copyright (c) 2018 naehrwert
 *
 * Copyright (c) 2018-2021 CTCaer
 * Copyright (c) 2019-2021 shchmue
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

#include "config.h"
#include <display/di.h>
#include <gfx_utils.h>
#include "gfx/gfx.h"
#include "gfx/tui.h"
#include "hid/hid.h"
#include <libs/fatfs/ff.h>
#include <mem/heap.h>
#include <mem/minerva.h>
#include <power/bq24193.h>
#include <power/max17050.h>
#include <power/max77620.h>
#include <rtc/max77620-rtc.h>
#include <soc/bpmp.h>
#include <soc/hw_init.h>
#include <storage/nx_sd.h>
#include <storage/sdmmc.h>
#include <utils/btn.h>
#include <input/touch.h>
#include <utils/dirlist.h>
#include <utils/ini.h>
#include <utils/list.h>
#include <utils/sprintf.h>
#include <utils/util.h>

hekate_config h_cfg;
boot_cfg_t __attribute__((section ("._boot_cfg"))) b_cfg;
const volatile ipl_ver_meta_t __attribute__((section ("._ipl_version"))) ipl_ver = {
	.magic = LP_MAGIC,
	.version = (LP_VER_MJ + '0') | ((LP_VER_MN + '0') << 8) | ((LP_VER_BF + '0') << 16),
	.rsvd0 = 0,
	.rsvd1 = 0
};

volatile nyx_storage_t *nyx_str = (nyx_storage_t *)NYX_STORAGE_ADDR;

// This is a safe and unused DRAM region for our payloads.
#define RELOC_META_OFF      0x7C
#define PATCHED_RELOC_SZ    0x94
#define PATCHED_RELOC_STACK 0x40007000
#define PATCHED_RELOC_ENTRY 0x40010000
#define EXT_PAYLOAD_ADDR    0xC0000000
#define RCM_PAYLOAD_ADDR    (EXT_PAYLOAD_ADDR + ALIGN(PATCHED_RELOC_SZ, 0x10))
#define COREBOOT_END_ADDR   0xD0000000
#define COREBOOT_VER_OFF    0x41
#define CBFS_DRAM_EN_ADDR   0x4003e000
#define  CBFS_DRAM_MAGIC    0x4452414D // "DRAM"

static void *coreboot_addr;

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
	if (!path)
		return 1;

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

void launch_tools()
{
	u8 max_entries = 61;
	char *filelist = NULL;
	char *file_sec = NULL;
	char *dir = NULL;

	ment_t *ments = (ment_t *)malloc(sizeof(ment_t) * (max_entries + 3));

	gfx_clear_grey(0x1B);
	gfx_con_setpos(0, 0);

	if (sd_mount())
	{
		dir = (char *)malloc(256);

		memcpy(dir, "sd:/bootloader/payloads", 24);

		filelist = dirlist(dir, NULL, false, false);

		u32 i = 0;
		u32 i_off = 2;

		if (filelist)
		{
			// Build configuration menu.
			u32 color_idx = 0;

			ments[0].type = MENT_BACK;
			ments[0].caption = "Back";
			ments[0].color = colors[(color_idx++) % 6];
			ments[1].type = MENT_CHGLINE;
			ments[1].color = colors[(color_idx++) % 6];
			if (!f_stat("sd:/atmosphere/reboot_payload.bin", NULL))
			{
				ments[i_off].type = INI_CHOICE;
				ments[i_off].caption = "reboot_payload.bin";
				ments[i_off].color = colors[(color_idx++) % 6];
				ments[i_off].data = "sd:/atmosphere/reboot_payload.bin";
				i_off++;
			}
			if (!f_stat("sd:/ReiNX.bin", NULL))
			{
				ments[i_off].type = INI_CHOICE;
				ments[i_off].caption = "ReiNX.bin";
				ments[i_off].color = colors[(color_idx++) % 6];
				ments[i_off].data = "sd:/ReiNX.bin";
				i_off++;
			}

			while (true)
			{
				if (i > max_entries || !filelist[i * 256])
					break;
				ments[i + i_off].type = INI_CHOICE;
				ments[i + i_off].caption = &filelist[i * 256];
				ments[i + i_off].color = colors[(color_idx++) % 6];
				ments[i + i_off].data = &filelist[i * 256];

				i++;
			}
		}

		if (i > 0 || i_off > 2)
		{
			memset(&ments[i + i_off], 0, sizeof(ment_t));
			menu_t menu = { ments, "Choose file", 0, 0 };

			file_sec = (char *)tui_do_menu(&menu);

			if (!file_sec)
			{
				free(ments);
				free(dir);
				free(filelist);
				sd_end();

				gfx_clear_grey(0x1B);
				return;
			}
		}
		else
			EPRINTF("No payloads or modules found.");

		free(ments);
		free(filelist);
	}
	else
	{
		free(ments);
		goto out;
	}

	if (file_sec)
	{
		if (memcmp("sd:/", file_sec, 4) != 0)
		{
			memcpy(dir + strlen(dir), "/", 2);
			memcpy(dir + strlen(dir), file_sec, strlen(file_sec) + 1);
		}
		else
			memcpy(dir, file_sec, strlen(file_sec) + 1);

		launch_payload(dir, true);
		EPRINTF("Failed to launch payload.");
	}

out:
	sd_end();
	free(dir);

	hidWait();
}

void launch_hekate()
{
	sd_mount();
	if (!f_stat("bootloader/update.bin", NULL))
		launch_payload("bootloader/update.bin", false);
	else
	{
		gfx_clear_grey(0x1B);
		gfx_con_setpos(0, 0);
		EPRINTF("bootloader/update.bin not found!");
		hidWait();
	}
}

#define EXOSPHERE_PATH "sd:/exosphere.ini"
#define HOSTS_SYSMMC_PATH "sd:/atmosphere/hosts/sysmmc.txt"
#define SYS_SETTINGS_PATH "sd:/atmosphere/config/system_settings.ini"
#define SYS_PATCH_CONFIG_PATH "sd:/config/sys-patch/config.ini"

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

const char *exosphere_sysmmc_blocked =
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

const char *exosphere_sysmmc_online =
"[exosphere]\n"
"debugmode=1\n"
"debugmode_user=0\n"
"disable_user_exception_handlers=0\n"
"enable_user_pmu_access=0\n"
"blank_prodinfo_sysmmc=0\n"
"blank_prodinfo_emummc=0\n"
"allow_writing_to_cal_sysmmc=0\n"
"log_port=0\n"
"log_baud_rate=115200\n"
"log_inverted=0\n";

const char *sys_settings_offline =
"[atmosphere]\n"
"enable_dns_mitm = u8!0x1\n"
"add_defaults_to_dns_hosts = u8!0x1\n";

const char *sys_settings_mitm_on_defaults_off =
"[atmosphere]\n"
"enable_dns_mitm = u8!0x1\n"
"add_defaults_to_dns_hosts = u8!0x0\n";

const char *sys_settings_both_online =
"[atmosphere]\n"
"enable_dns_mitm = u8!0x0\n"
"add_defaults_to_dns_hosts = u8!0x0\n";

static bool save_file(const char *path, const char *data, u32 size);

static bool save_sys_patch_config(bool enable_network_patches, bool block_firmware_updates)
{
	char config[768];
	s_printf(config,
		"[options]\n"
		"patch_sysmmc=1\n"
		"patch_emummc=1\n"
		"enable_logging=1\n"
		"version_skip=1\n"
		"[olsc]\n"
		"olsc_6.0.0-14.1.2=%d\n"
		"olsc_15.0.0-18.1.0=%d\n"
		"olsc_19.0.0+=%d\n"
		"[nifm]\n"
		"ctest_1.0.0-19.0.1=%d\n"
		"ctest_20.0.0+=%d\n"
		"[nim]\n"
		"blankcal0crashfix_17.0.0+=%d\n"
		"blockfirmwareupdates_1.0.0-5.1.0=%d\n"
		"blockfirmwareupdates_6.0.0-6.2.0=%d\n"
		"blockfirmwareupdates_7.0.0-10.2.0=%d\n"
		"blockfirmwareupdates_11.0.0-11.0.1=%d\n"
		"blockfirmwareupdates_12.0.0+=%d\n",
		enable_network_patches ? 1 : 0,
		enable_network_patches ? 1 : 0,
		enable_network_patches ? 1 : 0,
		enable_network_patches ? 1 : 0,
		enable_network_patches ? 1 : 0,
		enable_network_patches ? 1 : 0,
		block_firmware_updates ? 1 : 0,
		block_firmware_updates ? 1 : 0,
		block_firmware_updates ? 1 : 0,
		block_firmware_updates ? 1 : 0,
		block_firmware_updates ? 1 : 0);

	return save_file(SYS_PATCH_CONFIG_PATH, config, strlen(config));
}

static bool save_file(const char *path, const char *data, u32 size)
{
	FIL fp;
	if (f_open(&fp, path, FA_CREATE_ALWAYS | FA_WRITE))
	{
		EPRINTFARGS("Failed to open %s for writing!", path);
		return false;
	}

	UINT bw;
	if (f_write(&fp, data, size, &bw) || bw != size)
	{
		f_close(&fp);
		EPRINTFARGS("Failed to write to %s!", path);
		return false;
	}

	f_close(&fp);
	return true;
}

static char *read_file(const char *path)
{
	FIL fp;
	if (f_open(&fp, path, FA_READ))
		return NULL;

	u32 size = f_size(&fp);
	char *buf = malloc(size + 1);
	if (!buf)
	{
		f_close(&fp);
		return NULL;
	}

	UINT br;
	if (f_read(&fp, buf, size, &br) || br != size)
	{
		f_close(&fp);
		free(buf);
		return NULL;
	}

	f_close(&fp);
	buf[size] = 0;
	return buf;
}

static bool update_dns_mitm_settings(bool enable_mitm, bool enable_defaults)
{
	char *content = read_file(SYS_SETTINGS_PATH);

	if (!content)
	{
		const char *tmpl = enable_mitm
			? (enable_defaults ? sys_settings_offline : sys_settings_mitm_on_defaults_off)
			: sys_settings_both_online;
		return save_file(SYS_SETTINGS_PATH, tmpl, strlen(tmpl));
	}

	char *dns_mitm_pos = strstr(content, "enable_dns_mitm = u8!0x");
	if (dns_mitm_pos)
		dns_mitm_pos[strlen("enable_dns_mitm = u8!0x")] = enable_mitm ? '1' : '0';

	char *defaults_pos = strstr(content, "add_defaults_to_dns_hosts = u8!0x");
	if (defaults_pos)
		defaults_pos[strlen("add_defaults_to_dns_hosts = u8!0x")] = enable_defaults ? '1' : '0';

	if (!dns_mitm_pos || !defaults_pos)
	{
		free(content);
		const char *tmpl = enable_mitm
			? (enable_defaults ? sys_settings_offline : sys_settings_mitm_on_defaults_off)
			: sys_settings_both_online;
		return save_file(SYS_SETTINGS_PATH, tmpl, strlen(tmpl));
	}

	bool result = save_file(SYS_SETTINGS_PATH, content, strlen(content));
	free(content);
	return result;
}

static void draw_netman_screen(const char *subtitle)
{
	gfx_clear_grey(0x1B);

	char title[64];
	s_printf(title, "[NetMan v%d.%d.%d] - %s", LP_VER_MJ, LP_VER_MN, LP_VER_BF, subtitle);
	gfx_draw_title_bar(title);
	gfx_draw_bottom_bar("Hold VOL+: Screenshot   Any Button: Return");
	gfx_con_setpos(UI_CONTENT_START_X, UI_CONTENT_START_Y);
}

static void wait_for_return()
{
	gfx_printf("\n%kPress any button to return.", COLOR_CYAN_L);
	msleep(500);

	u32 vol_press_start = 0;
	while (true)
	{
		Input_t *inp = hidRead();
		u32 btn = inp->buttons;

		if (btn & (BtnVolP | JoyLUp))
		{
			if (vol_press_start == 0)
				vol_press_start = get_tmr_ms();
			else if (get_tmr_ms() - vol_press_start > 2000)
			{
				int save_fb_to_bmp();
				int res = save_fb_to_bmp();

				gfx_con_setpos(UI_NOTIFY_X, UI_NOTIFY_Y);
				if (!res)
					gfx_printf("%kScreenshot saved!%k                              ", COLOR_CYAN_L, COLOR_SOFT_WHITE);
				else
					gfx_printf("%kScreenshot failed!%k                             ", COLOR_ERROR, COLOR_SOFT_WHITE);

				msleep(1000);
				vol_press_start = 0;

				while (hidRead()->buttons & (BtnVolP | JoyLUp))
					msleep(10);
			}
		}
		else
		{
			vol_press_start = 0;
			if (btn)
				break;
		}

		msleep(10);
	}

	while (hidRead()->buttons)
		msleep(10);
}

void set_default_config()
{
	draw_netman_screen("Blocking sysMMC");
	gfx_printf("%kBlocking Nintendo connectivity for sysMMC.\n\n", COLOR_WHITE);

	if (!sd_mount())
	{
		EPRINTF("Failed to mount SD card!");
		goto out;
	}

	f_mkdir("sd:/atmosphere");
	f_mkdir("sd:/atmosphere/hosts");
	f_mkdir("sd:/atmosphere/config");
	f_mkdir("sd:/config");
	f_mkdir("sd:/config/sys-patch");

	gfx_printf("%kApplying protected sysMMC settings...\n\n", COLOR_WHITE);
	gfx_printf("Prodinfo blanking: %s\n", save_file(EXOSPHERE_PATH, exosphere_sysmmc_blocked, strlen(exosphere_sysmmc_blocked)) ? "ON" : "Failed");
	gfx_printf("Nintendo hosts block: %s\n", save_file(HOSTS_SYSMMC_PATH, hosts_block_all, strlen(hosts_block_all)) ? "ON" : "Failed");
	gfx_printf("DNS MITM: %s\n", update_dns_mitm_settings(true, false) ? "ON, defaults OFF" : "Failed");
	gfx_printf("sys-patch net patches: %s\n", save_sys_patch_config(true, true) ? "ON" : "Failed");
	gfx_printf("Firmware update block: ON\n");
	gfx_printf("\n%ksysMMC Nintendo connectivity is blocked.\n", COLOR_GREEN);

out:
	sd_end();
	wait_for_return();
}

void set_sysmmc_online()
{
	draw_netman_screen("Allowing sysMMC");
	gfx_printf("%kWARNING: sysMMC CFW will connect to Nintendo.\n", COLOR_RED);
	gfx_printf("%kPress VOL+ to continue or any other button to cancel.\n\n", COLOR_WHITE);

	Input_t *inp = hidWait();
	if (!(inp->buttons & (BtnVolP | JoyLUp)))
	{
		gfx_printf("%kCancelled.\n", COLOR_WHITE);
		wait_for_return();
		return;
	}

	draw_netman_screen("Allowing sysMMC");
	if (!sd_mount())
	{
		EPRINTF("Failed to mount SD card!");
		goto out;
	}

	f_mkdir("sd:/atmosphere");
	f_mkdir("sd:/atmosphere/hosts");
	f_mkdir("sd:/atmosphere/config");
	f_mkdir("sd:/config");
	f_mkdir("sd:/config/sys-patch");

	gfx_printf("%kApplying open sysMMC settings...\n\n", COLOR_WHITE);
	gfx_printf("Prodinfo blanking: %s\n", save_file(EXOSPHERE_PATH, exosphere_sysmmc_online, strlen(exosphere_sysmmc_online)) ? "OFF" : "Failed");
	gfx_printf("Nintendo hosts block: %s\n", save_file(HOSTS_SYSMMC_PATH, hosts_open, strlen(hosts_open)) ? "OFF" : "Failed");
	gfx_printf("DNS MITM: %s\n", update_dns_mitm_settings(true, false) ? "ON, defaults OFF" : "Failed");
	gfx_printf("sys-patch net patches: %s\n", save_sys_patch_config(true, false) ? "ON, updates allowed" : "Failed");
	gfx_printf("Firmware update block: OFF\n");
	gfx_printf("\n%ksysMMC can connect to Nintendo servers.\n", COLOR_GREEN);

out:
	sd_end();
	wait_for_return();
}

void show_current_config()
{
	draw_netman_screen("Current Status");

	if (!sd_mount())
	{
		EPRINTF("Failed to mount SD card!");
		goto out;
	}

	bool sys_online = false;
	char *buf = read_file(HOSTS_SYSMMC_PATH);
	if (buf)
	{
		sys_online = strstr(buf, "# No Nintendo server blocks") != NULL;
		free(buf);
	}

	gfx_printf("%ksysMMC Nintendo Connectivity\n\n", COLOR_WHITE);
	if (sys_online)
		gfx_printf("Nintendo servers: %kALLOWED\n", COLOR_RED);
	else
		gfx_printf("Nintendo servers: %kBLOCKED\n", COLOR_GREEN);

	buf = read_file(SYS_SETTINGS_PATH);
	if (buf)
	{
		bool mitm_on = strstr(buf, "enable_dns_mitm = u8!0x1") != NULL;
		bool defaults_on = strstr(buf, "add_defaults_to_dns_hosts = u8!0x1") != NULL;
		gfx_printf("%kDNS MITM: %s\n", COLOR_WHITE, mitm_on ? "ON" : "OFF");
		gfx_printf("%kDefault DNS blocks: %s\n", COLOR_WHITE, defaults_on ? "ON" : "OFF");
		free(buf);
	}
	else
		gfx_printf("\n%kDNS MITM: Could not read status\n", COLOR_WARNING);

	buf = read_file(SYS_PATCH_CONFIG_PATH);
	if (buf)
	{
		bool olsc = strstr(buf, "olsc_19.0.0+=1") != NULL || strstr(buf, "olsc_15.0.0-18.1.0=1") != NULL || strstr(buf, "olsc_6.0.0-14.1.2=1") != NULL;
		bool ctest = strstr(buf, "ctest_20.0.0+=1") != NULL || strstr(buf, "ctest_1.0.0-19.0.1=1") != NULL;
		bool blankcal_fix = strstr(buf, "blankcal0crashfix_17.0.0+=1") != NULL;
		bool block_updates = strstr(buf, "blockfirmwareupdates_12.0.0+=1") != NULL;
		gfx_printf("\n%ksys-patch: ACTIVE\n", COLOR_WHITE);
		gfx_printf("%kOLSC patch: %s\n", COLOR_WHITE, olsc ? "ON" : "OFF");
		gfx_printf("%kConnection test patch: %s\n", COLOR_WHITE, ctest ? "ON" : "OFF");
		gfx_printf("%kBlank CAL0 fix: %s\n", COLOR_WHITE, blankcal_fix ? "ON" : "OFF");
		gfx_printf("%kFirmware update block: %s\n", COLOR_WHITE, block_updates ? "ON" : "OFF");
		free(buf);
	}
	else
		gfx_printf("\n%ksys-patch: Could not read status\n", COLOR_WARNING);

out:
	sd_end();
	wait_for_return();
}

void hekate_launch()
{
	launch_payload("bootloader/update.bin", false);
}

power_state_t STATE_POWER_OFF           = POWER_OFF_RESET;

ment_t ment_top[] = {
	MDEF_CAPTION("--- sysMMC Connectivity ---", COLOR_WHITE),
	MDEF_HANDLER("Block Nintendo Connectivity", set_default_config, COLOR_TURQUOISE),
	MDEF_HANDLER("Allow Nintendo Connectivity", set_sysmmc_online, COLOR_TURQUOISE),
	MDEF_CHGLINE(),
	MDEF_CAPTION("--- Information ---", COLOR_WHITE),
	MDEF_HANDLER("View Current Status", show_current_config, COLOR_TURQUOISE),
	MDEF_CHGLINE(),
	MDEF_CAPTION("--- Navigation ---", COLOR_WHITE),
	MDEF_HANDLER("Payloads...", launch_tools, COLOR_TURQUOISE),
	MDEF_HANDLER("Back to hekate", hekate_launch, COLOR_TURQUOISE),
	MDEF_HANDLER_EX("Power Off", &STATE_POWER_OFF, power_set_state_ex, COLOR_TURQUOISE),
	MDEF_END()
};

menu_t menu_top = { ment_top, NULL, 0, 0 };

void grey_out_menu_item(ment_t *menu)
{
	menu->type = MENT_CAPTION;
	menu->color = 0xFF555555;
	menu->handler = NULL;
}

extern void pivot_stack(u32 stack_top);

void ipl_main()
{
	// Do initial HW configuration. This is compatible with consecutive reruns without a reset.
	hw_init();

	// Pivot the stack so we have enough space.
	pivot_stack(IPL_STACK_TOP);

	// Tegra/Horizon configuration goes to 0x80000000+, package2 goes to 0xA9800000, we place our heap in between.
	heap_init(IPL_HEAP_START);

#ifdef DEBUG_UART_PORT
	uart_send(DEBUG_UART_PORT, (u8 *)"hekate: Hello!\r\n", 16);
	uart_wait_idle(DEBUG_UART_PORT, UART_TX_IDLE);
#endif

	// Set bootloader's default configuration.
	set_default_configuration();

	// Mount SD Card.
	h_cfg.errors |= !sd_mount() ? ERR_SD_BOOT_EN : 0;
	load_netman_configuration();

	// Train DRAM and switch to max frequency.
	if (minerva_init()) //!TODO: Add Tegra210B01 support to minerva.
		h_cfg.errors |= ERR_LIBSYS_MTC;

	display_init();

	u32 *fb = display_init_framebuffer_pitch();
	gfx_init_ctxt(fb, 720, 1280, 720);

	gfx_con_init();

	display_backlight_pwm_init();

	// Initialize HID input (Joy-Con support)
	hidInit();

	// Overclock BPMP.
	bpmp_clk_rate_set(h_cfg.t210b01 ? BPMP_CLK_DEFAULT_BOOST : BPMP_CLK_LOWER_BOOST);

	// Grey out reboot to hekate option if no update.bin found.
	if (f_stat("bootloader/update.bin", NULL))
	{
		grey_out_menu_item(&ment_top[9]); // Back to hekate
	}

	minerva_change_freq(FREQ_800);

	while (true)
		tui_do_menu(&menu_top);

	// Halt BPMP if we managed to get out of execution.
	while (true)
		bpmp_halt();
}
