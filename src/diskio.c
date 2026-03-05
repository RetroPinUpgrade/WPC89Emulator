/*-----------------------------------------------------------------------*/
/* Low level disk I/O module SKELETON for FatFs     (C)ChaN, 2019        */
/*-----------------------------------------------------------------------*/
/* If a working storage control module is available, it should be        */
/* attached to the FatFs via a glue function rather than modifying it.   */
/* This is an example of glue functions to attach various exsisting      */
/* storage control modules to the FatFs module with a defined API.       */
/*-----------------------------------------------------------------------*/
#include "ff.h"
#include "diskio.h"
#include "gd32f4xx.h"
#include "sdcard.h" 

/* Drive number for SD card */
#define DEV_SD 0

/* Allocate the memory for the card info struct here */
sd_card_info_struct sd_cardinfo;

/* Internal check for the PA15 Card Detect pin */
static int is_sd_card_inserted(void) {
    return (gpio_input_bit_get(GPIOA, GPIO_PIN_15) == RESET) ? 1 : 0;
}

/* Get Drive Status */
DSTATUS disk_status(BYTE pdrv) {
    if (pdrv != DEV_SD) return STA_NOINIT;
    if (!is_sd_card_inserted()) return STA_NODISK;
    return 0; /* Drive is ready */
}

/* Initialize Drive */
DSTATUS disk_initialize_old(BYTE pdrv) {
    if (pdrv != DEV_SD) return STA_NOINIT;
    if (!is_sd_card_inserted()) return STA_NODISK;

    /* Calls the GD32 BSP SD initialization routine */
    if (sd_init() == SD_OK) {
        /* Populate the sd_cardinfo struct so read/write functions know the addressing mode */
        sd_card_information_get(&sd_cardinfo);
        return 0;
    }
    return STA_NOINIT;
}


/* Initialize Drive */
DSTATUS disk_initialize(BYTE pdrv) {
    if (pdrv != DEV_SD) return STA_NOINIT;
    if (!is_sd_card_inserted()) return STA_NODISK;

    /* 1. Calls the GD32 BSP SD initialization routine */
    if (sd_init() == SD_OK) {
        
        /* 2. Populate the sd_cardinfo struct to get the RCA */
        sd_card_information_get(&sd_cardinfo);
        
        /* 3. Select the card using its RCA to enter Transfer Mode. */
        if (sd_card_select_deselect(sd_cardinfo.card_rca) == SD_OK) {
            
            /* Optional: Configure for 4-bit bus mode here if needed later */
            // sd_bus_mode_config(SDIO_BUSMODE_4BIT);
            
            return 0; /* Drive is fully ready */
        }
    }
    return STA_NOINIT;
}


/* Read Sector(s) */
DRESULT disk_read_old(BYTE pdrv, BYTE* buff, LBA_t sector, UINT count) {
    sd_error_enum res = SD_ERROR;

    if (pdrv != DEV_SD) return RES_PARERR;
    if (!is_sd_card_inserted()) return RES_NOTRDY;

    /* Address translation: SDHC uses block addressing, standard SD uses byte addressing */
    uint32_t addr = (sd_cardinfo.card_type == SDIO_HIGH_CAPACITY_SD_CARD) ? sector : (sector * 512);

    if (count == 1) {
        res = sd_block_read((uint32_t*)buff, addr, 512);
    } else {
        res = sd_multiblocks_read((uint32_t*)buff, addr, 512, count);
    }

    return (res == SD_OK) ? RES_OK : RES_ERROR;
}

/* Write Sector(s) */
DRESULT disk_write_old(BYTE pdrv, const BYTE* buff, LBA_t sector, UINT count) {
    sd_error_enum res = SD_ERROR;

    if (pdrv != DEV_SD) return RES_PARERR;
    if (!is_sd_card_inserted()) return RES_NOTRDY;

    uint32_t addr = (sd_cardinfo.card_type == SDIO_HIGH_CAPACITY_SD_CARD) ? sector : (sector * 512);

    if (count == 1) {
        res = sd_block_write((uint32_t*)buff, addr, 512);
    } else {
        res = sd_multiblocks_write((uint32_t*)buff, addr, 512, count);
    }

    return (res == SD_OK) ? RES_OK : RES_ERROR;
}



/* Read Sector(s) */
DRESULT disk_read(BYTE pdrv, BYTE* buff, LBA_t sector, UINT count) {
    sd_error_enum res = SD_ERROR;

    if (pdrv != DEV_SD) return RES_PARERR;
    if (!is_sd_card_inserted()) return RES_NOTRDY;

    /* The GD32 driver expects a byte address and divides by 512 internally */
//    uint32_t addr = sector * 512;
    uint32_t addr = sector;

    if (count == 1) {
        res = sd_block_read((uint32_t*)buff, addr, 512);
    } else {
        res = sd_multiblocks_read((uint32_t*)buff, addr, 512, count);
    }

    return (res == SD_OK) ? RES_OK : RES_ERROR;
}

/* Write Sector(s) */
DRESULT disk_write(BYTE pdrv, const BYTE* buff, LBA_t sector, UINT count) {
    sd_error_enum res = SD_ERROR;

    if (pdrv != DEV_SD) return RES_PARERR;
    if (!is_sd_card_inserted()) return RES_NOTRDY;

    /* The GD32 driver expects a byte address and divides by 512 internally */
    uint32_t addr = sector * 512;

    if (count == 1) {
        res = sd_block_write((uint32_t*)buff, addr, 512);
    } else {
        res = sd_multiblocks_write((uint32_t*)buff, addr, 512, count);
    }

    return (res == SD_OK) ? RES_OK : RES_ERROR;
}


/* Miscellaneous Functions */
DRESULT disk_ioctl(BYTE pdrv, BYTE cmd, void* buff) {
    DRESULT res = RES_ERROR;

    if (pdrv != DEV_SD) return RES_PARERR;
    if (!is_sd_card_inserted()) return RES_NOTRDY;

    switch (cmd) {
        case CTRL_SYNC:
            res = RES_OK;
            break;
        case GET_SECTOR_COUNT:
            *(DWORD*)buff = sd_cardinfo.card_capacity / 512;
            res = RES_OK;
            break;
        case GET_SECTOR_SIZE:
            *(WORD*)buff = 512;
            res = RES_OK;
            break;
        case GET_BLOCK_SIZE:
            *(DWORD*)buff = 1; 
            res = RES_OK;
            break;
        default:
            res = RES_PARERR;
            break;
    }
    return res;
}