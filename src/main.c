#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <malloc.h>

#include <unistd.h>
#include <fcntl.h>
#include "dynamic_libs/os_functions.h"
#include "dynamic_libs/fs_functions.h"
#include "dynamic_libs/gx2_functions.h"
#include "dynamic_libs/sys_functions.h"
#include "dynamic_libs/vpad_functions.h"
#include "dynamic_libs/padscore_functions.h"
#include "dynamic_libs/socket_functions.h"
#include "dynamic_libs/ax_functions.h"
#include "patcher/function_hooks.h"
#include "fs/fs_utils.h"
#include "fs/sd_fat_devoptab.h"
#include "kernel/kernel_functions.h"
#include "system/exception_handler.h"
#include "system/memory.h"
#include "utils/logger.h"
#include "utils/utils.h"
#include "common/common.h"
#include "pygecko.h"

#define BUFFER_SIZE 40000

int pygecko;
int CCHandler;
char *buffer[BUFFER_SIZE] __attribute__((section(".data")));

#define PRINT_TEXT2(x, y, ...) { snprintf(msg, 80, __VA_ARGS__); OSScreenPutFontEx(0, x, y, msg); OSScreenPutFontEx(1, x, y, msg); }

/* Entry point */
int Menu_Main(void)
{
        //!*******************************************************************
        //!                   Initialize function pointers                   *
        //!*******************************************************************
        //! do OS (for acquire) and sockets first so we got logging
        InitOSFunctionPointers();
        InitSocketFunctionPointers();
        InitFSFunctionPointers();
        InitVPadFunctionPointers();
        InitSysFunctionPointers();



        log_init("192.168.178.49");
        log_deinit();
        log_init("192.168.178.49");
        log_printf("Started %s\n", cosAppXmlInfoStruct.rpx_name);

        if(strcasecmp("men.rpx", cosAppXmlInfoStruct.rpx_name) == 0)
        {
                return EXIT_RELAUNCH_ON_LOAD;
        }
        else if(strlen(cosAppXmlInfoStruct.rpx_name) > 0 && strcasecmp("ffl_app.rpx", cosAppXmlInfoStruct.rpx_name) != 0)
        {

                return EXIT_RELAUNCH_ON_LOAD;
        }

        //! *******************************************************************
        //! *                     Setup EABI registers                        *
        //! *******************************************************************
        register int old_sdata_start, old_sdata2_start;
        asm volatile (
                "mr %0, 13\n"
                "mr %1, 2\n"
                "lis 2, __sdata2_start@h\n"
                "ori 2, 2,__sdata2_start@l\n" // Set the Small Data 2 (Read Only) base register.
                "lis 13, __sdata_start@h\n"
                "ori 13, 13, __sdata_start@l\n"// # Set the Small Data (Read\Write) base register.
                : "=r" (old_sdata_start), "=r" (old_sdata2_start)
                );

        //!*******************************************************************
        //!                    Initialize BSS sections                       *
        //!*******************************************************************
        asm volatile (
                "lis 3, __bss_start@h\n"
                "ori 3, 3,__bss_start@l\n"
                "lis 5, __bss_end@h\n"
                "ori 5, 5, __bss_end@l\n"
                "subf 5, 3, 5\n"
                "li 4, 0\n"
                "bl memset\n"
                );

        SetupKernelCallback();
        PatchMethodHooks();

        memoryInitialize();

        VPADInit();

        // Prepare screen
        int screen_buf0_size = 0;
        int screen_buf1_size = 0;

        // Init screen and screen buffers
        OSScreenInit();
        screen_buf0_size = OSScreenGetBufferSizeEx(0);
        screen_buf1_size = OSScreenGetBufferSizeEx(1);

        unsigned char *screenBuffer = MEM1_alloc(screen_buf0_size + screen_buf1_size, 0x40);

        OSScreenSetBufferEx(0, screenBuffer);
        OSScreenSetBufferEx(1, (screenBuffer + screen_buf0_size));

        OSScreenEnableEx(0, 1);
        OSScreenEnableEx(1, 1);

        char msg[80];
        int launchMethod = 0;
        int update_screen = 1;
        int vpadError = -1;
        VPADData vpad_data;

        while (1)
        {
                // Read vpad
                VPADRead(0, &vpad_data, 1, &vpadError);

                if(update_screen)
                {
                        OSScreenClearBufferEx(0, 0);
                        OSScreenClearBufferEx(1, 0);

                        // Print message
                        PRINT_TEXT2(14, 1, "-- TCPGecko Installer --");
                        PRINT_TEXT2(0, 5, "Press A to install TCPGecko.");
                        PRINT_TEXT2(0, 6, "Press X to install TCPGecko with CosmoCortney's codehandler...");


                        PRINT_TEXT2(0, 17, "Press home button to exit ...");


                        OSScreenFlipBuffersEx(0);
                        OSScreenFlipBuffersEx(1);
                }

                u32 pressedBtns = vpad_data.btns_d | vpad_data.btns_h;

                // Check for buttons
                // Home Button
                if (pressedBtns & VPAD_BUTTON_HOME) {
                        launchMethod = 0;
                        break;
                }
                // A Button
                if (pressedBtns & VPAD_BUTTON_A) {
                        launchMethod = 2;
                        break;
                }
                // X Button
                if (pressedBtns & VPAD_BUTTON_X) {
                        mount_sd_fat("sd");

                        unsigned char* Badbuffer = 0;
                        unsigned int filesize = 0;
                        int ret = LoadFileToMem("sd:/wiiu/apps/TCPGecko/codehandler.bin", &Badbuffer, &filesize);
						if(ret == -1)
						{
							OSScreenClearBufferEx(0, 0);
                        	OSScreenClearBufferEx(1, 0);
                        	PRINT_TEXT2(14, 5, "Codehandler.bin not found");
                        	OSScreenFlipBuffersEx(0);
                        	OSScreenFlipBuffersEx(1);
                        	launchMethod = 0;
                        	sleep(2);
                       		break;
						}
						if(filesize>BUFFER_SIZE)
						{
							OSScreenClearBufferEx(0, 0);
                        	OSScreenClearBufferEx(1, 0);
                        	PRINT_TEXT2(14, 5, "Codehandler.bin is too big");
                        	OSScreenFlipBuffersEx(0);
                        	OSScreenFlipBuffersEx(1);
                        	launchMethod = 0;
                        	sleep(2);
                       		break;
						}
                        memcpy(buffer, Badbuffer, filesize);
                        free(Badbuffer);

                        unsigned int phys_cafe_codehandler_loc = (unsigned int)OSEffectiveToPhysical((void*)INSTALL_ADDR);

                        DCFlushRange(&buffer, filesize);
                        SC0x25_KernelCopyData((u32)phys_cafe_codehandler_loc, (int)buffer, filesize);
                        m_DCInvalidateRange((u32)phys_cafe_codehandler_loc, filesize);


                        unmount_sd_fat("sd");
                        CCHandler = 1;

                        launchMethod = 2;
                        break;
                }




                // Button pressed ?
                update_screen = (pressedBtns & (VPAD_BUTTON_LEFT | VPAD_BUTTON_RIGHT | VPAD_BUTTON_UP | VPAD_BUTTON_DOWN)) ? 1 : 0;
                usleep(20000);
        }
        asm volatile ("mr 13, %0" : : "r" (old_sdata_start));
        asm volatile ("mr 2,  %0" : : "r" (old_sdata2_start));

        MEM1_free(screenBuffer);
        screenBuffer = NULL;

        log_deinit();

        memoryRelease();



        if(launchMethod == 0)
        {
                RestoreInstructions();
                return EXIT_SUCCESS;
        }
        else if(launchMethod == 1)
        {
                char buf_vol_odd[20];
                snprintf(buf_vol_odd, sizeof(buf_vol_odd), "%s", "/vol/storage_odd03");
                _SYSLaunchTitleByPathFromLauncher(buf_vol_odd, 18, 0);
        }
        else
        {
                pygecko = 1;
                SYSLaunchMenu();
        }

        return EXIT_RELAUNCH_ON_LOAD;
}
