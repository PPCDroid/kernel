/*
 * XG20, XG21, XG40, XG42 frame buffer device
 * for Linux kernels  2.5.x, 2.6.x
 * Base on TW's sis fbdev code.
 */

/* frame buffer driver for Linux kernels >= 2.4.14 and >=2.6.3
 *
 * Copyright (C) 2001-2005 Thomas Winischhofer, Vienna, Austria.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the named License,
 * or any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA
 *
 * Author:	Thomas Winischhofer <thomas@winischhofer.net>
 *
 * Author of (practically wiped) code base:
 *		SiS (www.sis.com)
 *		Copyright (C) 1999 Silicon Integrated Systems, Inc.
 *
 * See http://www.winischhofer.net/ for more information and updates
 *
 * Originally based on the VBE 2.0 compliant graphic boards framebuffer driver,
 * which is (c) 1998 Gerd Knorr <kraxel@goldbach.in-berlin.de>
 *
 */



//#include <linux/config.h>
#include <linux/version.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/tty.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/fb.h>
#include <linux/console.h>
#include <linux/selection.h>
#include <linux/ioport.h>
#include <linux/init.h>
#include <linux/pci.h>
#include <linux/vmalloc.h>
#include <linux/vt_kern.h>
#include <linux/capability.h>
#include <linux/fs.h>
#include <linux/types.h>
#include <linux/proc_fs.h>
#include <linux/delay.h> /* udelay */

#include "osdef.h"


#ifndef XGIFB_PAN
#undef XGIFB_PAN
#endif

#include <asm/io.h>
#ifdef CONFIG_MTRR
#include <asm/mtrr.h>
#endif

#include "XGIfb.h"
#include "vgatypes.h"
#include "XGI_main.h"



#define Index_CR_GPIO_Reg1 0x48
#define Index_CR_GPIO_Reg2 0x49
#define Index_CR_GPIO_Reg3 0x4a

#define GPIOG_EN    (1<<6)
#define GPIOG_WRITE (1<<6)
#define GPIOG_READ  (1<<1)

/* -------------------- Macro definitions ---------------------------- */

#undef XGIFBDEBUG 	

#ifdef XGIFBDEBUG
#define DPRINTK(fmt, args...) printk(KERN_DEBUG "%s: " fmt, __FUNCTION__ , ## args)
#else
#define DPRINTK(fmt, args...)
#endif

void dumpVGAReg(void)
{
    u8 i,reg;

    outXGIIDXREG(XGISR, 0x05, 0x86);
 
    for(i=0; i < 0x4f; i++)
    {
        inXGIIDXREG(XGISR, i, reg);
        printk("\no 3c4 %x",i);
        printk("\ni 3c5 => %x",reg);
    }

    for(i=0; i < 0xF0; i++)
    {   
        inXGIIDXREG(XGICR, i, reg);
        printk("\no 3d4 %x",i);
        printk("\ni 3d5 => %x",reg);
    }
#ifdef  DUMP_VB
    
    outXGIIDXREG(XGIPART1,0x2F,1);    
    for(i=1; i < 0x50; i++)
    {
        inXGIIDXREG(XGIPART1, i, reg);
        printk("\no d004 %x",i);
        printk("\ni d005 => %x",reg);
    }
    
    for(i=0; i < 0x50; i++)
    {
        inXGIIDXREG(XGIPART2, i, reg);
        printk("\no d010 %x",i);
        printk("\ni d011 => %x",reg);
    }
    for(i=0; i < 0x50; i++)
    {
        inXGIIDXREG(XGIPART3, i, reg);
        printk("\no d012 %x",i);
        printk("\ni d013 => %x",reg);
    }
    for(i=0; i < 0x50; i++)
    {
        inXGIIDXREG(XGIPART4, i, reg);
        printk("\no d014 %x",i);
        printk("\ni d015 => %x",reg);
    }
#endif
}


/* data for XGI components */
//extern struct video_info ;
struct video_info  xgi_video_info;


#if 1
#define DEBUGPRN(x)
#else
#define DEBUGPRN(x) printk(KERN_INFO x "\n");
#endif


/* --------------- Hardware Access Routines -------------------------- */

#ifdef LINUX_KERNEL
int XGIfb_mode_rate_to_dclock(VB_DEVICE_INFO *XGI_Pr, PXGI_HW_DEVICE_INFO HwDeviceExtension,
			  unsigned char modeno, unsigned char rateindex)
{
    USHORT ModeNo = modeno;
    USHORT ModeIdIndex = 0, ClockIndex = 0;
    USHORT RefreshRateTableIndex = 0;
    
/*    ULONG  temp = 0; */
    int    Clock;
    XGI_Pr->ROMAddr  = HwDeviceExtension->pjVirtualRomBase;
    InitTo330Pointer( HwDeviceExtension->jChipType, XGI_Pr ) ;

    RefreshRateTableIndex = XGI_GetRatePtrCRT2( HwDeviceExtension, ModeNo , ModeIdIndex, XGI_Pr ) ;

/*    
    temp = XGI_SearchModeID( ModeNo , &ModeIdIndex,  XGI_Pr ) ;
    if(!temp) {
    	printk(KERN_ERR "Could not find mode %x\n", ModeNo);
    	return 65000;
    }
    
    RefreshRateTableIndex = XGI_Pr->EModeIDTable[ModeIdIndex].REFindex;
    RefreshRateTableIndex += (rateindex - 1);

*/
    ClockIndex = XGI_Pr->RefIndex[RefreshRateTableIndex].Ext_CRTVCLK;
    if(HwDeviceExtension->jChipType < XGI_315H) {
       ClockIndex &= 0x3F;
    }
    Clock = XGI_Pr->VCLKData[ClockIndex].CLOCK * 1000 ;
    
    return(Clock);
}

int
XGIfb_mode_rate_to_ddata(VB_DEVICE_INFO *XGI_Pr, PXGI_HW_DEVICE_INFO HwDeviceExtension,
			 unsigned char modeno, unsigned char rateindex,
			 ULONG *left_margin, ULONG *right_margin, 
			 ULONG *upper_margin, ULONG *lower_margin,
			 ULONG *hsync_len, ULONG *vsync_len,
			 ULONG *sync, ULONG *vmode)
{
    USHORT ModeNo = modeno;
    USHORT ModeIdIndex = 0, index = 0;
    USHORT RefreshRateTableIndex = 0;
    
    unsigned short VRE, VBE, VRS, VBS, VDE, VT;
    unsigned short HRE, HBE, HRS, HBS, HDE, HT;
    unsigned char  sr_data, cr_data, cr_data2, cr_data3;
    int            A, B, C, D, E, F, temp, j;
    XGI_Pr->ROMAddr  = HwDeviceExtension->pjVirtualRomBase;
    InitTo330Pointer( HwDeviceExtension->jChipType, XGI_Pr ) ;
    RefreshRateTableIndex = XGI_GetRatePtrCRT2( HwDeviceExtension, ModeNo , ModeIdIndex, XGI_Pr ) ; 
/*   
    temp = XGI_SearchModeID( ModeNo, &ModeIdIndex, XGI_Pr);
    if(!temp) return 0;
    
    RefreshRateTableIndex = XGI_Pr->EModeIDTable[ModeIdIndex].REFindex;
    RefreshRateTableIndex += (rateindex - 1);
*/
    index = XGI_Pr->RefIndex[RefreshRateTableIndex].Ext_CRT1CRTC;

    sr_data = XGI_Pr->XGINEWUB_CRT1Table[index].CR[5];

    cr_data = XGI_Pr->XGINEWUB_CRT1Table[index].CR[0];

    /* Horizontal total */
    HT = (cr_data & 0xff) |
         ((unsigned short) (sr_data & 0x03) << 8);
    A = HT + 5;

    /*cr_data = XGI_Pr->XGINEWUB_CRT1Table[index].CR[1];
	
     Horizontal display enable end 
    HDE = (cr_data & 0xff) |
          ((unsigned short) (sr_data & 0x0C) << 6);*/
    HDE = (XGI_Pr->RefIndex[RefreshRateTableIndex].XRes >> 3) -1;      
    E = HDE + 1;

    cr_data = XGI_Pr->XGINEWUB_CRT1Table[index].CR[3];
	
    /* Horizontal retrace (=sync) start */
    HRS = (cr_data & 0xff) |
          ((unsigned short) (sr_data & 0xC0) << 2);
    F = HRS - E - 3;

    cr_data = XGI_Pr->XGINEWUB_CRT1Table[index].CR[1];
	
    /* Horizontal blank start */
    HBS = (cr_data & 0xff) |
          ((unsigned short) (sr_data & 0x30) << 4);

    sr_data = XGI_Pr->XGINEWUB_CRT1Table[index].CR[6];
	
    cr_data = XGI_Pr->XGINEWUB_CRT1Table[index].CR[2];

    cr_data2 = XGI_Pr->XGINEWUB_CRT1Table[index].CR[4];
	
    /* Horizontal blank end */
    HBE = (cr_data & 0x1f) |
          ((unsigned short) (cr_data2 & 0x80) >> 2) |
	  ((unsigned short) (sr_data & 0x03) << 6);

    /* Horizontal retrace (=sync) end */
    HRE = (cr_data2 & 0x1f) | ((sr_data & 0x04) << 3);

    temp = HBE - ((E - 1) & 255);
    B = (temp > 0) ? temp : (temp + 256);

    temp = HRE - ((E + F + 3) & 63);
    C = (temp > 0) ? temp : (temp + 64);

    D = B - F - C;
    
    *left_margin = D * 8;
    *right_margin = F * 8;
    *hsync_len = C * 8;

    sr_data = XGI_Pr->XGINEWUB_CRT1Table[index].CR[14];

    cr_data = XGI_Pr->XGINEWUB_CRT1Table[index].CR[8];
    
    cr_data2 = XGI_Pr->XGINEWUB_CRT1Table[index].CR[9];
    
    /* Vertical total */
    VT = (cr_data & 0xFF) |
         ((unsigned short) (cr_data2 & 0x01) << 8) |
	 ((unsigned short)(cr_data2 & 0x20) << 4) |
	 ((unsigned short) (sr_data & 0x01) << 10);
    A = VT + 2;

    //cr_data = XGI_Pr->XGINEWUB_CRT1Table[index].CR[10];
	
    /* Vertical display enable end */
/*    VDE = (cr_data & 0xff) |
          ((unsigned short) (cr_data2 & 0x02) << 7) |
	  ((unsigned short) (cr_data2 & 0x40) << 3) |
	  ((unsigned short) (sr_data & 0x02) << 9); */
    VDE = XGI_Pr->RefIndex[RefreshRateTableIndex].YRes  -1;
    E = VDE + 1;

    cr_data = XGI_Pr->XGINEWUB_CRT1Table[index].CR[10];

    /* Vertical retrace (=sync) start */
    VRS = (cr_data & 0xff) |
          ((unsigned short) (cr_data2 & 0x04) << 6) |
	  ((unsigned short) (cr_data2 & 0x80) << 2) |
	  ((unsigned short) (sr_data & 0x08) << 7);
    F = VRS + 1 - E;

    cr_data =  XGI_Pr->XGINEWUB_CRT1Table[index].CR[12];

    cr_data3 = (XGI_Pr->XGINEWUB_CRT1Table[index].CR[14] & 0x80) << 5;

    /* Vertical blank start */
    VBS = (cr_data & 0xff) |
          ((unsigned short) (cr_data2 & 0x08) << 5) |
	  ((unsigned short) (cr_data3 & 0x20) << 4) |
	  ((unsigned short) (sr_data & 0x04) << 8);

    cr_data =  XGI_Pr->XGINEWUB_CRT1Table[index].CR[13];

    /* Vertical blank end */
    VBE = (cr_data & 0xff) |
          ((unsigned short) (sr_data & 0x10) << 4);
    temp = VBE - ((E - 1) & 511);
    B = (temp > 0) ? temp : (temp + 512);

    cr_data = XGI_Pr->XGINEWUB_CRT1Table[index].CR[11];

    /* Vertical retrace (=sync) end */
    VRE = (cr_data & 0x0f) | ((sr_data & 0x20) >> 1);
    temp = VRE - ((E + F - 1) & 31);
    C = (temp > 0) ? temp : (temp + 32);

    D = B - F - C;
      
    *upper_margin = D;
    *lower_margin = F;
    *vsync_len = C;

    if(XGI_Pr->RefIndex[RefreshRateTableIndex].Ext_InfoFlag & 0x8000)
       *sync &= ~FB_SYNC_VERT_HIGH_ACT;
    else
       *sync |= FB_SYNC_VERT_HIGH_ACT;

    if(XGI_Pr->RefIndex[RefreshRateTableIndex].Ext_InfoFlag & 0x4000)       
       *sync &= ~FB_SYNC_HOR_HIGH_ACT;
    else
       *sync |= FB_SYNC_HOR_HIGH_ACT;
		
    *vmode = FB_VMODE_NONINTERLACED;       
    if(XGI_Pr->RefIndex[RefreshRateTableIndex].Ext_InfoFlag & 0x0080)
       *vmode = FB_VMODE_INTERLACED;
    else 
    {
        j = 0;
        while(XGI_Pr->EModeIDTable[j].Ext_ModeID != 0xff) 
        {
            if(XGI_Pr->EModeIDTable[j].Ext_ModeID == XGI_Pr->RefIndex[RefreshRateTableIndex].ModeID) 
            {
                if(XGI_Pr->EModeIDTable[j].Ext_ModeFlag & DoubleScanMode) 
                {
	      	        *vmode = FB_VMODE_DOUBLE;
                }
	            break;
            }
            j++;
        }
    }       
    return 1;       
}			  

#endif



void XGIRegInit(VB_DEVICE_INFO *XGI_Pr, ULONG BaseAddr)
{
    XGI_Pr->P3c4 = BaseAddr + 0x14;
    XGI_Pr->P3d4 = BaseAddr + 0x24;
    XGI_Pr->P3c0 = BaseAddr + 0x10;
    XGI_Pr->P3ce = BaseAddr + 0x1e;
    XGI_Pr->P3c2 = BaseAddr + 0x12;
    XGI_Pr->P3ca = BaseAddr + 0x1a;
    XGI_Pr->P3c6 = BaseAddr + 0x16;
    XGI_Pr->P3c7 = BaseAddr + 0x17;
    XGI_Pr->P3c8 = BaseAddr + 0x18;
    XGI_Pr->P3c9 = BaseAddr + 0x19;
    XGI_Pr->P3da = BaseAddr + 0x2A;
    XGI_Pr->Part1Port = BaseAddr + XGI_CRT2_PORT_04;   /* Digital video interface registers (LCD) */
    XGI_Pr->Part2Port = BaseAddr + XGI_CRT2_PORT_10;   /* 301 TV Encoder registers */
    XGI_Pr->Part3Port = BaseAddr + XGI_CRT2_PORT_12;   /* 301 Macrovision registers */
    XGI_Pr->Part4Port = BaseAddr + XGI_CRT2_PORT_14;   /* 301 VGA2 (and LCD) registers */
    XGI_Pr->Part5Port = BaseAddr + XGI_CRT2_PORT_14+2; /* 301 palette address port registers */
  
}


void XGIfb_set_reg4(u16 port, unsigned long data)
{
	outl((u32) (data & 0xffffffff), port);
}

u32 XGIfb_get_reg3(u16 port)
{
	u32 data;

	data = inl(port);
	return (data);
}

/* ------------ Interface for init & mode switching code ------------- */

BOOLEAN XGIfb_query_VGA_config_space(PXGI_HW_DEVICE_INFO pXGIhw_ext, unsigned long offset, 
        unsigned long set, unsigned long *value)
{
	static struct pci_dev *pdev = NULL;
	static unsigned char init = 0, valid_pdev = 0;

	if (!set)
		DPRINTK("XGIfb: Get VGA offset 0x%lx\n", offset);
	else
		DPRINTK("XGIfb: Set offset 0x%lx to 0x%lx\n", offset, *value);

	if (!init) {
		init = TRUE;
//		pdev = pci_find_device(PCI_VENDOR_ID_XG, xgi_video_info.chip_id, pdev);
		pdev = pci_get_device(PCI_VENDOR_ID_XG, xgi_video_info.chip_id, pdev);
		if (pdev)
			valid_pdev = TRUE;
	}

	if (!valid_pdev) {
		printk(KERN_DEBUG "XGIfb: Can't find XGI %d VGA device.\n",
				xgi_video_info.chip_id);
		return FALSE;
	}

	if (set == 0)
		pci_read_config_dword(pdev, offset, (u32 *)value);
	else
		pci_write_config_dword(pdev, offset, (u32)(*value));

	return TRUE;
}

/* ------------------ Internal helper routines ----------------- */

static void XGIfb_search_mode(const char *name)
{
	int i = 0, j = 0;

	if(name == NULL) 
	{
	    printk(KERN_ERR "XGIfb: Internal error, using default mode.\n");
	    xgifb_mode_idx = DEFAULT_MODE;
	    return;
	}

    if (!strcmp(name, XGIbios_mode[MODE_INDEX_NONE].name)) 
    {
	    printk(KERN_ERR "XGIfb: Mode 'none' not supported anymore. Using default.\n");
	    xgifb_mode_idx = DEFAULT_MODE;
	    return;
	}

	while(XGIbios_mode[i].mode_no != 0) 
	{
		if (!strcmp(name, XGIbios_mode[i].name)) 
		{
            xgifb_mode_idx = i;
            j = 1;
            break;
		}
		i++;
	}
	if(!j) 
	    printk(KERN_INFO "XGIfb: Invalid mode '%s'\n", name);
}

static void XGIfb_search_vesamode(unsigned int vesamode)
{
	int i = 0, j = 0;

	if(vesamode == 0) {

		printk(KERN_ERR "XGIfb: Mode 'none' not supported anymore. Using default.\n");
		xgifb_mode_idx = DEFAULT_MODE;

		return;
	}

	vesamode &= 0x1dff;  /* Clean VESA mode number from other flags */

	while(XGIbios_mode[i].mode_no != 0) {
		if( (XGIbios_mode[i].vesa_mode_no_1 == vesamode) ||
		    (XGIbios_mode[i].vesa_mode_no_2 == vesamode) ) {
			xgifb_mode_idx = i;
			j = 1;
			break;
		}
		i++;
	}
	if(!j) printk(KERN_INFO "XGIfb: Invalid VESA mode 0x%x'\n", vesamode);
}

static int XGIfb_validate_mode(int myindex)
{
   u16 xres, yres;




    return myindex;  //yilin allow all mode for now.
    if(!(XGIbios_mode[myindex].chipset & MD_XGI315)) {
        return(-1);
    }
   

   switch (xgi_video_info.disp_state & DISPTYPE_DISP2) {
     case DISPTYPE_LCD:
	switch (XGIhw_ext.ulCRT2LCDType) {
	case LCD_640x480:
		xres =  640; yres =  480;  break;
	case LCD_800x600:
		xres =  800; yres =  600;  break;
        case LCD_1024x600:
		xres = 1024; yres =  600;  break;		
	case LCD_1024x768:
	 	xres = 1024; yres =  768;  break;
	case LCD_1152x768:
		xres = 1152; yres =  768;  break;		
	case LCD_1280x960:
	        xres = 1280; yres =  960;  break;		
	case LCD_1280x768:
		xres = 1280; yres =  768;  break;
	case LCD_1280x1024:
		xres = 1280; yres = 1024;  break;
	case LCD_1400x1050:
		xres = 1400; yres = 1050;  break;		
	case LCD_1600x1200:
		xres = 1600; yres = 1200;  break;
//	case LCD_320x480:				// TW: FSTN 
//		xres =  320; yres =  480;  break;
	default:
	        xres =    0; yres =    0;  break;
	}
	if(XGIbios_mode[myindex].xres > xres) {
	        return(-1);
	}
        if(XGIbios_mode[myindex].yres > yres) {
	        return(-1);
	}
	if((XGIhw_ext.ulExternalChip == 0x01) ||   // LVDS 
           (XGIhw_ext.ulExternalChip == 0x05))    // LVDS+Chrontel 
	{	
	   switch (XGIbios_mode[myindex].xres) {
	   	case 512:
	       		if(XGIbios_mode[myindex].yres != 512) return -1;
			if(XGIhw_ext.ulCRT2LCDType == LCD_1024x600) return -1;
	       		break;
	   	case 640:
		       	if((XGIbios_mode[myindex].yres != 400) &&
	           	   (XGIbios_mode[myindex].yres != 480))
		          	return -1;
	       		break;
	   	case 800:
		       	if(XGIbios_mode[myindex].yres != 600) return -1;
	       		break;
	   	case 1024:
		       	if((XGIbios_mode[myindex].yres != 600) &&
	           	   (XGIbios_mode[myindex].yres != 768))
		          	return -1;
			if((XGIbios_mode[myindex].yres == 600) &&
			   (XGIhw_ext.ulCRT2LCDType != LCD_1024x600))
			   	return -1;
			break;
		case 1152:
			if((XGIbios_mode[myindex].yres) != 768) return -1;
			if(XGIhw_ext.ulCRT2LCDType != LCD_1152x768) return -1;
			break;
	   	case 1280:
		   	if((XGIbios_mode[myindex].yres != 768) &&
	           	   (XGIbios_mode[myindex].yres != 1024))
		          	return -1;
			if((XGIbios_mode[myindex].yres == 768) &&
			   (XGIhw_ext.ulCRT2LCDType != LCD_1280x768))
			   	return -1;				
			break;
	   	case 1400:
		   	if(XGIbios_mode[myindex].yres != 1050) return -1;
			break;
	   	case 1600:
		   	if(XGIbios_mode[myindex].yres != 1200) return -1;
			break;
	   	default:
		        return -1;		
	   }
	} else {
	   switch (XGIbios_mode[myindex].xres) {
	   	case 512:
	       		if(XGIbios_mode[myindex].yres != 512) return -1;
	       		break;
	   	case 640:
		       	if((XGIbios_mode[myindex].yres != 400) &&
	           	   (XGIbios_mode[myindex].yres != 480))
		          	return -1;
	       		break;
	   	case 800:
		       	if(XGIbios_mode[myindex].yres != 600) return -1;
	       		break;
	   	case 1024:
		       	if(XGIbios_mode[myindex].yres != 768) return -1;
			break;
	   	case 1280:
		   	if((XGIbios_mode[myindex].yres != 960) &&
	           	   (XGIbios_mode[myindex].yres != 1024))
		          	return -1;
			if(XGIbios_mode[myindex].yres == 960) {
			    if(XGIhw_ext.ulCRT2LCDType == LCD_1400x1050) 
			   	return -1;
			}
			break;
	   	case 1400:
		   	if(XGIbios_mode[myindex].yres != 1050) return -1;
			break;
	   	case 1600:
		   	if(XGIbios_mode[myindex].yres != 1200) return -1;
			break;
	   	default:
		        return -1;		
	   }
	}
	break;
     case DISPTYPE_TV:
	switch (XGIbios_mode[myindex].xres) {
	case 512:
	case 640:
	case 800:
		break;
	case 720:
		if (xgi_video_info.TV_type == TVMODE_NTSC) {
			if (XGIbios_mode[myindex].yres != 480) {
				return(-1);
			}
		} else if (xgi_video_info.TV_type == TVMODE_PAL) {
			if (XGIbios_mode[myindex].yres != 576) {
				return(-1);
			}
		}
		// TW: LVDS/CHRONTEL does not support 720 
		if (xgi_video_info.hasVB == HASVB_LVDS_CHRONTEL ||
					xgi_video_info.hasVB == HASVB_CHRONTEL) {
				return(-1);
		}
		break;
	case 1024:
		if (xgi_video_info.TV_type == TVMODE_NTSC) {
			if(XGIbios_mode[myindex].bpp == 32) {
			       return(-1);
			}
		}
		// TW: LVDS/CHRONTEL only supports < 800 (1024 on 650/Ch7019)
		if (xgi_video_info.hasVB == HASVB_LVDS_CHRONTEL ||
					xgi_video_info.hasVB == HASVB_CHRONTEL) {
		    if(xgi_video_info.chip < XGI_315H) {
				return(-1);
		    }
		}
		break;
	default:
		return(-1);
	}
	break;
     case DISPTYPE_CRT2:	
        if(XGIbios_mode[myindex].xres > 1280) return -1;
	break;	
     }
     return(myindex);

}

static void XGIfb_search_crt2type(const char *name)
{
	int i = 0;

	if(name == NULL)
		return;

	while(XGI_crt2type[i].type_no != -1) {
		if (!strcmp(name, XGI_crt2type[i].name)) {
			XGIfb_crt2type = XGI_crt2type[i].type_no;
			XGIfb_tvplug = XGI_crt2type[i].tvplug_no;
			break;
		}
		i++;
	}
	if(XGIfb_crt2type < 0)
		printk(KERN_INFO "XGIfb: Invalid CRT2 type: %s\n", name);
}



static u8 XGIfb_search_refresh_rate(unsigned int rate)
{
	u16 xres, yres;
	int i = 0;

	xres = XGIbios_mode[xgifb_mode_idx].xres;
	yres = XGIbios_mode[xgifb_mode_idx].yres;

	XGIfb_rate_idx = 0;
	while ((XGIfb_vrate[i].idx != 0) && (XGIfb_vrate[i].xres <= xres)) {
		if ((XGIfb_vrate[i].xres == xres) && (XGIfb_vrate[i].yres == yres)) {
			if (XGIfb_vrate[i].refresh == rate) {
				XGIfb_rate_idx = XGIfb_vrate[i].idx;
				break;
			} else if (XGIfb_vrate[i].refresh > rate) {
				if ((XGIfb_vrate[i].refresh - rate) <= 3) {
					DPRINTK("XGIfb: Adjusting rate from %d up to %d\n",
						rate, XGIfb_vrate[i].refresh);
					XGIfb_rate_idx = XGIfb_vrate[i].idx;
					xgi_video_info.refresh_rate = XGIfb_vrate[i].refresh;
				} else if (((rate - XGIfb_vrate[i-1].refresh) <= 2)
						&& (XGIfb_vrate[i].idx != 1)) {
					DPRINTK("XGIfb: Adjusting rate from %d down to %d\n",
						rate, XGIfb_vrate[i-1].refresh);
					XGIfb_rate_idx = XGIfb_vrate[i-1].idx;
					xgi_video_info.refresh_rate = XGIfb_vrate[i-1].refresh;
				} 
				break;
			} else if((rate - XGIfb_vrate[i].refresh) <= 2) {
				DPRINTK("XGIfb: Adjusting rate from %d down to %d\n",
						rate, XGIfb_vrate[i].refresh);
	           		XGIfb_rate_idx = XGIfb_vrate[i].idx;
		   		break;
	       		}
		}
		i++;
	}
	if (XGIfb_rate_idx > 0) {
		return XGIfb_rate_idx;
	} else {
		printk(KERN_INFO
			"XGIfb: Unsupported rate %d for %dx%d\n", rate, xres, yres);
		return 0;
	}
}

static void XGIfb_search_tvstd(const char *name)
{
	int i = 0;

	if(name == NULL)
		return;

	while (XGI_tvtype[i].type_no != -1) {
		if (!strcmp(name, XGI_tvtype[i].name)) {
			XGIfb_tvmode = XGI_tvtype[i].type_no;
			break;
		}
		i++;
	}
}

static BOOLEAN XGIfb_bridgeisslave(void)
{
   unsigned char usScratchP1_00;

   if(xgi_video_info.hasVB == HASVB_NONE) return FALSE;

   inXGIIDXREG(XGIPART1,0x00,usScratchP1_00);
   if( (usScratchP1_00 & 0x50) == 0x10)  {
	   return TRUE;
   } else {
           return FALSE;
   }
}

static BOOLEAN XGIfbcheckvretracecrt1(void)
{
   unsigned char temp;

   inXGIIDXREG(XGICR,0x17,temp);
   if(!(temp & 0x80)) return FALSE;
   

   inXGIIDXREG(XGISR,0x1f,temp);
   if(temp & 0xc0) return FALSE;
   

   if(inXGIREG(XGIINPSTAT) & 0x08) return TRUE;
   else 			   return FALSE;
}

static BOOLEAN XGIfbcheckvretracecrt2(void)
{
   unsigned char temp;
   if(xgi_video_info.hasVB == HASVB_NONE) return FALSE;
   inXGIIDXREG(XGIPART1, 0x30, temp);
   if(temp & 0x02) return FALSE;
   else 	   return TRUE;
}

static BOOLEAN XGIfb_CheckVBRetrace(void) 
{
   if(xgi_video_info.disp_state & DISPTYPE_DISP2) {
      if(XGIfb_bridgeisslave()) {
         return(XGIfbcheckvretracecrt1());
      } else {
         return(XGIfbcheckvretracecrt2());
      }
   } 
   return(XGIfbcheckvretracecrt1());
}

/* ----------- FBDev related routines for all series ----------- */


static void XGIfb_bpp_to_var(struct fb_var_screeninfo *var)
{
	switch(var->bits_per_pixel) {
	   case 8:
	   	var->red.offset = var->green.offset = var->blue.offset = 0;
		var->red.length = var->green.length = var->blue.length = 6;
		xgi_video_info.video_cmap_len = 256;
		break;
	   case 16:
		var->red.offset = 11;
		var->red.length = 5;
		var->green.offset = 5;
		var->green.length = 6;
		var->blue.offset = 0;
		var->blue.length = 5;
		var->transp.offset = 0;
		var->transp.length = 0;
		xgi_video_info.video_cmap_len = 16;
		break;
	   case 32:
		var->red.offset = 16;
		var->red.length = 8;
		var->green.offset = 8;
		var->green.length = 8;
		var->blue.offset = 0;
		var->blue.length = 8;
		var->transp.offset = 24;
		var->transp.length = 8;
		xgi_video_info.video_cmap_len = 16;
		break;
	}
}


static struct fb_var_screeninfo temp_set_var;
static int check_var_valid = 0;
static struct fb_var_screeninfo check_var;
static unsigned int check_var_yoffset;

static int XGIfb_pan_var(struct fb_var_screeninfo *var)
{
	unsigned int base;

	if (var->yoffset == 0)
	    base = 0;
	else if (var->yoffset == var->yres)
            base = var->yres * var->xres;

        /* calculate base bpp dep. */
        switch(var->bits_per_pixel) {
        case 16:
        	base >>= 1;
        	break;
	    case 32:
            	break;
	    case 8:
        default:
        	base >>= 2;
            	break;
        }
	
        outXGIIDXREG(XGISR, IND_XGI_PASSWORD, XGI_PASSWORD);

        outXGIIDXREG(XGICR, 0x0D, base & 0xFF);
        outXGIIDXREG(XGICR, 0x0C, (base >> 8) & 0xFF);
        outXGIIDXREG(XGISR, 0x0D, (base >> 16) & 0xFF);
        outXGIIDXREG(XGISR, 0x37, (base >> 24) & 0x03);
        setXGIIDXREG(XGISR, 0x37, 0xDF, (base >> 21) & 0x04);

//	printk("End of pan_var");
	return 0;
}

static int XGIfb_do_set_var(struct fb_var_screeninfo *var, int isactive,
		      struct fb_info *info)
{

	unsigned int htotal = var->left_margin + var->xres + 
		var->right_margin + var->hsync_len;
	unsigned int vtotal = var->upper_margin + var->yres + 
		var->lower_margin + var->vsync_len;
        u8  cr_data;
	unsigned int drate = 0, hrate = 0;
	int found_mode = 0;
	int old_mode;
//	unsigned char reg,reg1;
	
	if (check_var_valid) {
		temp_set_var = info->var;
		temp_set_var.yoffset = 0;
		temp_set_var.activate = 0;
		memset(&temp_set_var.reserved, 0, sizeof(temp_set_var.reserved));
		if (memcmp(&temp_set_var, &check_var, sizeof(temp_set_var)) == 0) {
			if (info->var.yoffset == check_var_yoffset)	/* exactly the same */
				return 0;
			/* do the pan */
			XGIfb_pan_var(&info->var);
			return 0;
		}
	}

        info->var.xres_virtual = var->xres_virtual;
        info->var.yres_virtual = var->yres_virtual;
        info->var.bits_per_pixel = var->bits_per_pixel;

	if ((var->vmode & FB_VMODE_MASK) == FB_VMODE_NONINTERLACED) 
		vtotal <<= 1;
	else if ((var->vmode & FB_VMODE_MASK) == FB_VMODE_DOUBLE)
		vtotal <<= 2;
	else if ((var->vmode & FB_VMODE_MASK) == FB_VMODE_INTERLACED)
	{	
//		vtotal <<= 1;
//		var->yres <<= 1;
	}

	if(!htotal || !vtotal) {
		DPRINTK("XGIfb: Invalid 'var' information\n");
		return -EINVAL;
	}



	if(var->pixclock && htotal && vtotal) {
		drate = 1000000000 / var->pixclock;
		hrate = (drate * 1000) / htotal;
		xgi_video_info.refresh_rate = (unsigned int) (hrate * 2 / vtotal);
	} else {
		xgi_video_info.refresh_rate = 60;
	}

    xgi_video_info.refresh_rate = 60;

	printk(KERN_DEBUG "XGIfb: Change mode to %dx%dx%d-%dHz\n",
		var->xres,var->yres,var->bits_per_pixel,xgi_video_info.refresh_rate);

	old_mode = xgifb_mode_idx;
	xgifb_mode_idx = 0;

	while( (XGIbios_mode[xgifb_mode_idx].mode_no != 0) &&
	       (XGIbios_mode[xgifb_mode_idx].xres <= var->xres) ) {
		if( (XGIbios_mode[xgifb_mode_idx].xres == var->xres) &&
		    (XGIbios_mode[xgifb_mode_idx].yres == var->yres) &&
		    (XGIbios_mode[xgifb_mode_idx].bpp == var->bits_per_pixel)) {
			XGIfb_mode_no = XGIbios_mode[xgifb_mode_idx].mode_no;
			found_mode = 1;
			break;
		}
		xgifb_mode_idx++;
	}

	if(found_mode)
		xgifb_mode_idx = XGIfb_validate_mode(xgifb_mode_idx);
	else
		xgifb_mode_idx = -1;

       	if(xgifb_mode_idx < 0) {
		printk(KERN_ERR "XGIfb: Mode %dx%dx%d not supported\n", var->xres,
		       var->yres, var->bits_per_pixel);
		xgifb_mode_idx = old_mode;
		return -EINVAL;
	}

	if(XGIfb_search_refresh_rate(xgi_video_info.refresh_rate) == 0) {
		XGIfb_rate_idx = XGIbios_mode[xgifb_mode_idx].rate_idx;
		xgi_video_info.refresh_rate = 60;
	}

	if(isactive) {

		
		XGIfb_pre_setmode();
		if(XGISetModeNew( &XGIhw_ext, XGIfb_mode_no) == 0) {
			printk(KERN_ERR "XGIfb: Setting mode[0x%x] failed\n", XGIfb_mode_no);
			return -EINVAL;
		}
	    info->fix.line_length = ((info->var.xres_virtual * info->var.bits_per_pixel)>>6);
 
	    outXGIIDXREG(XGISR,IND_XGI_PASSWORD,XGI_PASSWORD);

	    outXGIIDXREG(XGICR,0x13,(info->fix.line_length & 0x00ff));
	    outXGIIDXREG(XGISR,0x0E,(info->fix.line_length & 0xff00)>>8);

	    XGIfb_post_setmode();

		DPRINTK("XGIfb: Set new mode: %dx%dx%d-%d \n",
			XGIbios_mode[xgifb_mode_idx].xres,
			XGIbios_mode[xgifb_mode_idx].yres,
			XGIbios_mode[xgifb_mode_idx].bpp,
			xgi_video_info.refresh_rate);

		xgi_video_info.video_bpp = XGIbios_mode[xgifb_mode_idx].bpp;
		xgi_video_info.video_vwidth = info->var.xres_virtual;
		xgi_video_info.video_width = XGIbios_mode[xgifb_mode_idx].xres;
		xgi_video_info.video_vheight = info->var.yres_virtual;
		xgi_video_info.video_height = XGIbios_mode[xgifb_mode_idx].yres;
		xgi_video_info.org_x = xgi_video_info.org_y = 0;
		xgi_video_info.video_linelength = info->var.xres_virtual * (xgi_video_info.video_bpp >> 3);
		xgi_video_info.accel = 0;
		if(XGIfb_accel) {
		   xgi_video_info.accel = (var->accel_flags & FB_ACCELF_TEXT) ? -1 : 0;
		}
		switch(xgi_video_info.video_bpp) 
		{
        	case 8:
           		xgi_video_info.DstColor = 0x0000;
	    		xgi_video_info.XGI310_AccelDepth = 0x00000000;
 			    xgi_video_info.video_cmap_len = 256;
#if defined(__powerpc__)
                inXGIIDXREG (XGICR, 0x4D, cr_data);
                outXGIIDXREG(XGICR, 0x4D, (cr_data & 0xE0));
#endif
                break;
        	case 16:
            	xgi_video_info.DstColor = 0x8000;
            	xgi_video_info.XGI310_AccelDepth = 0x00010000;
#if defined(__powerpc__)
                inXGIIDXREG (XGICR, 0x4D, cr_data);
                outXGIIDXREG(XGICR, 0x4D, ((cr_data & 0xE0) | 0x0B));
#endif
			    xgi_video_info.video_cmap_len = 16;
            	break;
        	case 32:
            	xgi_video_info.DstColor = 0xC000;
	    		xgi_video_info.XGI310_AccelDepth = 0x00020000;
			    xgi_video_info.video_cmap_len = 16;
#if defined(__powerpc__)
                inXGIIDXREG (XGICR, 0x4D, cr_data);
                outXGIIDXREG(XGICR, 0x4D, ((cr_data & 0xE0) | 0x15));
#endif
            	break;
		    default:
			    xgi_video_info.video_cmap_len = 16;
		        printk(KERN_ERR "XGIfb: Unsupported depth %d", xgi_video_info.video_bpp);
			    xgi_video_info.accel = 0;
			    break;
    	}
	}
	XGIfb_bpp_to_var(var); /*update ARGB info*/
    orXGIIDXREG(XGISR, IND_XGI_MODULE_ENABLE, XGI_ENABLE_2D);
    orXGIIDXREG(XGISR, IND_XGI_PCI_ADDRESS_SET, (XGI_PCI_ADDR_ENABLE | XGI_MEM_MAP_IO_ENABLE));
    outXGIIDXREG(XGISR, 0x26, 0x01);
    outXGIIDXREG(XGISR, 0x26, 0x22);

	DEBUGPRN("End of do_set_var");

	return 0;
}

void XGI_dispinfo(struct ap_data *rec)
{
	rec->minfo.bpp    = xgi_video_info.video_bpp;
	rec->minfo.xres   = xgi_video_info.video_width;
	rec->minfo.yres   = xgi_video_info.video_height;
	rec->minfo.v_xres = xgi_video_info.video_vwidth;
	rec->minfo.v_yres = xgi_video_info.video_vheight;
	rec->minfo.org_x  = xgi_video_info.org_x;
	rec->minfo.org_y  = xgi_video_info.org_y;
	rec->minfo.vrate  = xgi_video_info.refresh_rate;
	rec->iobase       = xgi_video_info.vga_base - 0x30;
	rec->mem_size     = xgi_video_info.video_size;
	rec->disp_state   = xgi_video_info.disp_state; 
	rec->version      = (VER_MAJOR << 24) | (VER_MINOR << 16) | VER_LEVEL; 
	rec->hasVB        = xgi_video_info.hasVB; 
	rec->TV_type      = xgi_video_info.TV_type; 
	rec->TV_plug      = xgi_video_info.TV_plug; 
	rec->chip         = xgi_video_info.chip;
}




static int XGIfb_open(struct fb_info *info, int user)
{
    return 0;
}

static int XGIfb_release(struct fb_info *info, int user)
{
    return 0;
}

static int XGIfb_get_cmap_len(const struct fb_var_screeninfo *var)
{
	int rc = 16;		

	switch(var->bits_per_pixel) {
	case 8:
		rc = 256;	
		break;
	case 16:
		rc = 16;	
		break;		
	case 32:
		rc = 16;
		break;	
	}
	return rc;
}

static int XGIfb_setcolreg(unsigned regno, unsigned red, unsigned green, unsigned blue,
                           unsigned transp, struct fb_info *info)
{
	if (regno >= XGIfb_get_cmap_len(&info->var))
		return 1;

	switch (info->var.bits_per_pixel) {
	case 8:
	        outXGIREG(XGIDACA, regno);
		outXGIREG(XGIDACD, (red >> 10));
		outXGIREG(XGIDACD, (green >> 10));
		outXGIREG(XGIDACD, (blue >> 10));
		if (xgi_video_info.disp_state & DISPTYPE_DISP2) {
		        outXGIREG(XGIDAC2A, regno);
			outXGIREG(XGIDAC2D, (red >> 8));
			outXGIREG(XGIDAC2D, (green >> 8));
			outXGIREG(XGIDAC2D, (blue >> 8));
		}
		break;
	case 16:
		((u32 *)(info->pseudo_palette))[regno] =
		    ((red & 0xf800)) | ((green & 0xfc00) >> 5) | ((blue & 0xf800) >> 11);
		break;
	case 32:
		red >>= 8;
		green >>= 8;
		blue >>= 8;
		((u32 *) (info->pseudo_palette))[regno] = 
			(red << 16) | (green << 8) | (blue);
		break;
	}
	return 0;
}

static int XGIfb_set_par(struct fb_info *info)
{
	int err;

        if((err = XGIfb_do_set_var(&info->var, 1, info)))
		return err;
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,10)
	XGIfb_get_fix(&info->fix, info->currcon, info);
#else
	XGIfb_get_fix(&info->fix, -1, info);
#endif
	return 0;
}

static int XGIfb_check_var(struct fb_var_screeninfo *var,
                           struct fb_info *info)
{
	unsigned int htotal =
		var->left_margin + var->xres + var->right_margin +
		var->hsync_len;
	unsigned int vtotal = 0;
	unsigned int drate = 0, hrate = 0;
	int found_mode = 0;
	int refresh_rate, search_idx;

	/* save previous info->var  */
	check_var = info->var;
	memset(&check_var.reserved, 0, sizeof(check_var.reserved));
	check_var_yoffset = check_var.yoffset;
	check_var.yoffset = 0;	/* yeah, sucks */
	check_var.activate = 0;

	if((var->vmode & FB_VMODE_MASK) == FB_VMODE_NONINTERLACED) {
		vtotal = var->upper_margin + var->yres + var->lower_margin +
		         var->vsync_len;   
		vtotal <<= 1;
	} else if((var->vmode & FB_VMODE_MASK) == FB_VMODE_DOUBLE) {
		vtotal = var->upper_margin + var->yres + var->lower_margin +
		         var->vsync_len;   
		vtotal <<= 2;
	} else if((var->vmode & FB_VMODE_MASK) == FB_VMODE_INTERLACED) {
		vtotal = var->upper_margin + (var->yres/2) + var->lower_margin +
		         var->vsync_len;   
	} else 	vtotal = var->upper_margin + var->yres + var->lower_margin +
		         var->vsync_len;

	if(!(htotal) || !(vtotal)) {
		XGIFAIL("XGIfb: no valid timing data");
	}


        if(var->pixclock && htotal && vtotal) {
                drate = 1000000000 / var->pixclock;
                hrate = (drate * 1000) / htotal;
                xgi_video_info.refresh_rate = (unsigned int) (hrate * 2 / vtotal);
        } else {
                xgi_video_info.refresh_rate = 60;
        }

/*	
	if((var->pixclock) && (htotal)) {
	   drate = 1E12 / var->pixclock;
	   hrate = drate / htotal;
	   refresh_rate = (unsigned int) (hrate / vtotal * 2 + 0.5);
	} else refresh_rate = 60;

 TW: Calculation wrong for 1024x600 - force it to 60Hz */
	if((var->xres == 1024) && (var->yres == 600)) refresh_rate = 60;

	search_idx = 0;
	while((XGIbios_mode[search_idx].mode_no != 0) && 
	       (XGIbios_mode[search_idx].xres <= var->xres) ) {
	    if((XGIbios_mode[search_idx].xres == var->xres) &&
	       (XGIbios_mode[search_idx].yres == var->yres) &&
		    (XGIbios_mode[search_idx].bpp == var->bits_per_pixel)) {
		        if(XGIfb_validate_mode(search_idx) > 0) {
		    found_mode = 1;
		    break;
	        }
	    }
		search_idx++;
	}

	if(!found_mode) {
	
		printk(KERN_ERR "XGIfb: %dx%dx%d is no valid mode\n", 
			var->xres, var->yres, var->bits_per_pixel);
			
                search_idx = 0;
		while(XGIbios_mode[search_idx].mode_no != 0) {
		       
		   if( (var->xres <= XGIbios_mode[search_idx].xres) &&
		       (var->yres <= XGIbios_mode[search_idx].yres) && 
		       (var->bits_per_pixel == XGIbios_mode[search_idx].bpp) ) {
		          if(XGIfb_validate_mode(search_idx) > 0) {
			     found_mode = 1;
			     break;
			  }
		   }
		   search_idx++;
	        }			
		if(found_mode) {
			var->xres = XGIbios_mode[search_idx].xres;
		      	var->yres = XGIbios_mode[search_idx].yres;
		      	printk(KERN_DEBUG "XGIfb: Adapted to mode %dx%dx%d\n",
		   		var->xres, var->yres, var->bits_per_pixel);
		   
		} else {
		   	printk(KERN_ERR "XGIfb: Failed to find similar mode to %dx%dx%d\n", 
				var->xres, var->yres, var->bits_per_pixel);
		   	return -EINVAL;
		}
	}

	/* TW: TODO: Check the refresh rate */		
	
	/* Adapt RGB settings */
	XGIfb_bpp_to_var(var);	
	
	/* Sanity check for offsets */
	if (var->xoffset < 0)
		var->xoffset = 0;
	if (var->yoffset < 0)
		var->yoffset = 0;

	/* panto */
//	   var->yres_virtual = xgi_video_info.heapstart / (var->xres * (var->bits_per_pixel >> 3));
	if(var->yres_virtual < var->yres)
	        var->yres_virtual = var->yres;

//	if( (var->yres * 2) < var->yres_virtual ) //yilin to make the virtual = 2 * yres;
//	        var->yres_virtual = var->yres * 2;

	/* Truncate offsets to maximum if too high */
	if (var->xoffset > var->xres_virtual - var->xres)
		var->xoffset = var->xres_virtual - var->xres - 1;

	if (var->yoffset > var->yres_virtual - var->yres)
		var->yoffset = var->yres_virtual - var->yres - 1;
	
	/* Set everything else to 0 */
	var->red.msb_right = 
	    var->green.msb_right =
	    var->blue.msb_right =
	    var->transp.offset = var->transp.length = var->transp.msb_right = 0;		
		
	/* save this to aid in panning */
	check_var_valid = 1;

	return 0;
}

#ifdef XGIFB_PAN
#error
static int XGIfb_pan_display( struct fb_var_screeninfo *var, 
				 struct fb_info* info)
{
	int err;
	
	printk("\nInside pan_display: var->yoffset=%d",var->yoffset);
	
	if (var->xoffset > (var->xres_virtual - var->xres))
		return -EINVAL;
	if (var->yoffset > (var->yres_virtual - var->yres))
		return -EINVAL;

	if (var->vmode & FB_VMODE_YWRAP) {
		if (var->yoffset < 0
		    || var->yoffset >= info->var.yres_virtual
		    || var->xoffset) return -EINVAL;
	} else {
		if (var->xoffset + info->var.xres > info->var.xres_virtual ||
		    var->yoffset + info->var.yres > info->var.yres_virtual)
			return -EINVAL;
	}
    
	if((err = XGIfb_pan_var(var)) < 0) return err;
	info->var.xoffset = var->xoffset;
	info->var.yoffset = var->yoffset;

	if (var->vmode & FB_VMODE_YWRAP)
		info->var.vmode |= FB_VMODE_YWRAP;
	else
		info->var.vmode &= ~FB_VMODE_YWRAP;

//	printk(" End of pan_display");
	return 0;
}
#endif

static int XGIfb_blank(int blank, struct fb_info *info)
{
	u8 reg;
    return 0;
	inXGIIDXREG(XGICR, 0x17, reg);

	if(blank > 0)
		reg &= 0x7f;
	else
		reg |= 0x80;

    outXGIIDXREG(XGICR, 0x17, reg);		
	outXGIIDXREG(XGISR, 0x00, 0x01);    /* Synchronous Reset */
	outXGIIDXREG(XGISR, 0x00, 0x03);    /* End Reset */
    printk("\nXGIfb_blank: blank=%d",blank);
        return(0);
}


#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,15)
static int XGIfb_ioctl(struct fb_info *info, unsigned int cmd,
			    unsigned long arg)
#else
static int XGIfb_ioctl(struct inode *inode, struct file *file,
		       unsigned int cmd, unsigned long arg, 
		       struct fb_info *info)
#endif

{
	DEBUGPRN("inside ioctl");
	switch (cmd) {
	   case FBIO_ALLOC:
		if (!capable(CAP_SYS_RAWIO))
			return -EPERM;
		XGI_malloc((struct XGI_memreq *) arg);
		break;
	   case FBIO_FREE:
		if (!capable(CAP_SYS_RAWIO))
			return -EPERM;
		XGI_free(*(unsigned long *) arg);
		break;
	   case FBIOGET_HWCINFO:
		{
			unsigned long *hwc_offset = (unsigned long *) arg;

			if (XGIfb_caps & HW_CURSOR_CAP)
				*hwc_offset = XGIfb_hwcursor_vbase -
				    (unsigned long) xgi_video_info.video_vbase;
			else
				*hwc_offset = 0;

			break;
		}
	   case FBIOPUT_MODEINFO:
		{
			struct mode_info *x = (struct mode_info *)arg;

			xgi_video_info.video_bpp        = x->bpp;
			xgi_video_info.video_width      = x->xres;
			xgi_video_info.video_height     = x->yres;
			xgi_video_info.video_vwidth     = x->v_xres;
			xgi_video_info.video_vheight    = x->v_yres;
			xgi_video_info.org_x            = x->org_x;
			xgi_video_info.org_y            = x->org_y;
			xgi_video_info.refresh_rate     = x->vrate;
			xgi_video_info.video_linelength = xgi_video_info.video_vwidth * (xgi_video_info.video_bpp >> 3);
			switch(xgi_video_info.video_bpp) {
        		case 8:
            			xgi_video_info.DstColor = 0x0000;
	    			xgi_video_info.XGI310_AccelDepth = 0x00000000;
				xgi_video_info.video_cmap_len = 256;
            			break;
        		case 16:
            			xgi_video_info.DstColor = 0x8000;
            			xgi_video_info.XGI310_AccelDepth = 0x00010000;
				xgi_video_info.video_cmap_len = 16;
            			break;
        		case 32:
            			xgi_video_info.DstColor = 0xC000;
	    			xgi_video_info.XGI310_AccelDepth = 0x00020000;
				xgi_video_info.video_cmap_len = 16;
            			break;
			default:
				xgi_video_info.video_cmap_len = 16;
		       	 	printk(KERN_ERR "XGIfb: Unsupported accel depth %d", xgi_video_info.video_bpp);
				xgi_video_info.accel = 0;
				break;
    			}

			break;
		}
	   case FBIOGET_DISPINFO:
		XGI_dispinfo((struct ap_data *)arg);
		break;
	   case XGIFB_GET_INFO:  /* TW: New for communication with X driver */
	        {
			XGIfb_info *x = (XGIfb_info *)arg;

			//x->XGIfb_id = XGIFB_ID;
			x->XGIfb_version = VER_MAJOR;
			x->XGIfb_revision = VER_MINOR;
			x->XGIfb_patchlevel = VER_LEVEL;
			x->chip_id = xgi_video_info.chip_id;
			x->memory = xgi_video_info.video_size / 1024;
			x->heapstart = xgi_video_info.heapstart / 1024;
			x->fbvidmode = XGIfb_mode_no;
			x->XGIfb_caps = XGIfb_caps;
			x->XGIfb_tqlen = 512; /* yet unused */
			x->XGIfb_pcibus = xgi_video_info.pcibus;
			x->XGIfb_pcislot = xgi_video_info.pcislot;
			x->XGIfb_pcifunc = xgi_video_info.pcifunc;
			x->XGIfb_lcdpdc = XGIfb_detectedpdc;
			x->XGIfb_lcda = XGIfb_detectedlcda;
	                break;
		}
	   case XGIFB_GET_VBRSTATUS:
	        {
			unsigned long *vbrstatus = (unsigned long *) arg;
			if(XGIfb_CheckVBRetrace()) *vbrstatus = 1;
			else		           *vbrstatus = 0;
		}
                break;
#ifdef FBIO
          case 0x18ca:
                {
                    unsigned long reg = arg;  
                    unsigned char index, value;
                    unsigned int base;

                    base  = (reg & 0xfff0000) >> 16;
                    index = (reg & 0xff00)>>8;
                    value = (reg & 0xff);
                    printk("\nioctl 18ca: base:%x index:%x value:%x  ", base, index, value );
                    outXGIIDXREG(XGISR,IND_XGI_PASSWORD,XGI_PASSWORD);
                    if(reg & 0xf0000000)
                    {
                        if(base == 0x3c4)
                        {
                            outXGIIDXREG(XGISR,index,value);
                            inXGIIDXREG(XGISR,index,value);
                            printk("out 0x3c4 %x =%x",index, value);
                        }
                        else
                        if(base == 0x3d4)
                        {
                            outXGIIDXREG(XGICR,index,value);
                            inXGIIDXREG(XGICR,index,value);
                            printk("out 0x3d4 %x =%x",index, value);
                        }
                        else
                        {
                            printk("base error");
                            
                        }
 
                    }
                    else
                    {
                        if(base == 0x3c4)
                        {
                            inXGIIDXREG(XGISR,index,value);
                            printk("out 0x3c4 %x =%x",index, value);
                        }
                        else
                        if(base == 0x3d4)
                        {
                            inXGIIDXREG(XGICR,index,value);
                            printk("out 0x3d4 %x =%x",index, value);
                        }
                        else
                        {
                            printk("base error");
                        }
                            
    	            }

                }
                break;
#endif
	   default:
                printk("ioctl  default\n");
                
		return -EINVAL;
	}
	DEBUGPRN("end of ioctl");
	return 0;

}



/* ----------- FBDev related routines for all series ---------- */

static int XGIfb_get_fix(struct fb_fix_screeninfo *fix, int con,
			 struct fb_info *info)
{
	memset(fix, 0, sizeof(struct fb_fix_screeninfo));

	strcpy(fix->id, myid);

	fix->smem_start = xgi_video_info.video_base;

	fix->smem_len = xgi_video_info.video_size;
	

/*        if((!XGIfb_mem) || (XGIfb_mem > (xgi_video_info.video_size/1024))) {
	    if (xgi_video_info.video_size > 0x1000000) {
	        fix->smem_len = 0xD00000;
	    } else if (xgi_video_info.video_size > 0x800000)
		fix->smem_len = 0x800000;
	    else
		fix->smem_len = 0x400000;
        } else
		fix->smem_len = XGIfb_mem * 1024;
*/
	fix->type        = video_type;
	fix->type_aux    = 0;
	if(xgi_video_info.video_bpp == 8)
		fix->visual = FB_VISUAL_PSEUDOCOLOR;
	else
		fix->visual = FB_VISUAL_DIRECTCOLOR;
	fix->xpanstep    = 0;
#ifdef XGIFB_PAN
        if(XGIfb_ypan) 	 fix->ypanstep = 1;
#endif
	fix->ywrapstep   = 0;
	fix->line_length = xgi_video_info.video_linelength;
	fix->mmio_start  = xgi_video_info.mmio_base;
	fix->mmio_len    = XGIfb_mmio_size;

        if(xgi_video_info.chip > XG40)
           fix->accel    = FB_ACCEL_XGI_VOLARI_Z;
        else
           fix->accel    = FB_ACCEL_XGI_VOLARI_V;

	return 0;
}

#ifdef CONFIG_PHYS_ADDR_T_64BIT

static int XGIfb_mmap(struct fb_info *info, struct vm_area_struct *vma) {

        resource_size_t off, start, len;

        if (vma->vm_pgoff > (~0UL >> PAGE_SHIFT))
                return -EINVAL;
        off = vma->vm_pgoff << PAGE_SHIFT;

        /* frame buffer memory */
        start = xgi_video_info.video_base;
        len = PAGE_ALIGN((start & ~PAGE_MASK) + xgi_video_info.video_size);
        if (off >= len) {
                /* mmap wants memory mapped io? */
                off -= len;
                start = xgi_video_info.mmio_base;
                len = PAGE_ALIGN((start & ~PAGE_MASK) + XGIfb_mmio_size);
        }

        start &= PAGE_MASK;
        if ((vma->vm_end - vma->vm_start + off) > len)
                return -EINVAL;
        off += start;
        vma->vm_pgoff = off >> PAGE_SHIFT;
        /* Tell the swapper to stay away */
        vma->vm_flags |= VM_IO | VM_RESERVED;
        vma->vm_page_prot = pgprot_noncached_wc(vma->vm_page_prot);
        if (io_remap_pfn_range(vma, vma->vm_start,
                (unsigned long)(off >> PAGE_SHIFT),
                vma->vm_end - vma->vm_start, vma->vm_page_prot))
                        return -EAGAIN;

        return 0;
}
#endif
static struct fb_ops XGIfb_ops = {
	.owner        =	THIS_MODULE,
	.fb_open      = XGIfb_open,
	.fb_release   = XGIfb_release,
	.fb_check_var = XGIfb_check_var,
	.fb_set_par   = XGIfb_set_par,
	.fb_setcolreg = XGIfb_setcolreg,
#ifdef XGIFB_PAN
        .fb_pan_display = XGIfb_pan_display,
#endif	
        .fb_blank     = XGIfb_blank,
	.fb_fillrect  = fbcon_XGI_fillrect,
	.fb_copyarea  = fbcon_XGI_copyarea,
	.fb_imageblit = cfb_imageblit,
#ifdef CONFIG_PHYS_ADDR_T_64BIT
        .fb_mmap      = XGIfb_mmap,
#endif
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,15)
	.fb_cursor    = soft_cursor,	
#endif
	.fb_sync      = fbcon_XGI_sync,
	.fb_ioctl     =	XGIfb_ioctl,
};

/* ---------------- Chip generation dependent routines ---------------- */


/* for XGI 315/550/650/740/330 */

static int XGIfb_get_dram_size(void)
{
	
	u8  ChannelNum,tmp;
	u8  reg = 0;


        inXGIIDXREG(XGISR, IND_XGI_DRAM_SIZE, reg);
		switch ((reg & XGI_DRAM_SIZE_MASK) >> 4) {
		   case XGI_DRAM_SIZE_1MB:
			xgi_video_info.video_size = 0x100000;
			break;
		   case XGI_DRAM_SIZE_2MB:
			xgi_video_info.video_size = 0x200000;
			break;
		   case XGI_DRAM_SIZE_4MB:
			xgi_video_info.video_size = 0x400000;
			break;
		   case XGI_DRAM_SIZE_8MB:
			xgi_video_info.video_size = 0x800000;
			break;
		   case XGI_DRAM_SIZE_16MB:
			xgi_video_info.video_size = 0x1000000;
			break;
		   case XGI_DRAM_SIZE_32MB:
			xgi_video_info.video_size = 0x2000000;
			break;
		   case XGI_DRAM_SIZE_64MB:
			xgi_video_info.video_size = 0x4000000;
			break;
		   case XGI_DRAM_SIZE_128MB:
			xgi_video_info.video_size = 0x8000000;
			break;
		   case XGI_DRAM_SIZE_256MB:
			xgi_video_info.video_size = 0x10000000;
			break;
		   default:
			return -1;
		}
		
		tmp = (reg & 0x0c) >> 2;
		switch(xgi_video_info.chip)
		{
		    case XG20:
        case XG21:
		        ChannelNum = 1;
		        break;
		        
		    case XG42:
		        if(reg & 0x04)
		            ChannelNum = 2;
		        else
		            ChannelNum = 1;
		        break;
		        
		    case XG45:
		        if(tmp == 1)
                    ChannelNum = 2;		    
                else 
                if(tmp == 2)
                    ChannelNum = 3;
                else
                if(tmp == 3)
                    ChannelNum = 4;
                else
                    ChannelNum = 1;
		        break;

		    case XG40:
		    default:
                if(tmp == 2)
                    ChannelNum = 2;		    
                else 
                if(tmp == 3)
                    ChannelNum = 3;
                else
                    ChannelNum = 1;
		        break;
		}
		
		
		xgi_video_info.video_size = xgi_video_info.video_size * ChannelNum;

//      printk("XGIfb: SR14=%x DramSzie %x ChannelNum %x\n",reg,xgi_video_info.video_size ,ChannelNum );
		return 0;

}

static void XGIfb_detect_VB(void)
{
	u8 cr32, temp=0;

	xgi_video_info.TV_plug = xgi_video_info.TV_type = 0;

        switch(xgi_video_info.hasVB) {
	  case HASVB_LVDS_CHRONTEL:
	  case HASVB_CHRONTEL:
	     break;
	  case HASVB_301:
	  case HASVB_302:
//	     XGI_Sense30x(); //Yi-Lin TV Sense?
	     break;
	}

	inXGIIDXREG(XGICR, IND_XGI_SCRATCH_REG_CR32, cr32);

	if ((cr32 & XGI_CRT1) && !XGIfb_crt1off)
		XGIfb_crt1off = 0;
	else {
		if (cr32 & 0x5F)   
			XGIfb_crt1off = 1;
		else
			XGIfb_crt1off = 0;
	}

	if (XGIfb_crt2type != -1)
		/* TW: Override with option */
		xgi_video_info.disp_state = XGIfb_crt2type;
	else if (cr32 & XGI_VB_TV)
		xgi_video_info.disp_state = DISPTYPE_TV;		
	else if (cr32 & XGI_VB_LCD)
		xgi_video_info.disp_state = DISPTYPE_LCD;		
	else if (cr32 & XGI_VB_CRT2)
		xgi_video_info.disp_state = DISPTYPE_CRT2;
	else
		xgi_video_info.disp_state = 0;

	if(XGIfb_tvplug != -1)
		/* PR/TW: Override with option */
	        xgi_video_info.TV_plug = XGIfb_tvplug;
	else if (cr32 & XGI_VB_HIVISION) {
		xgi_video_info.TV_type = TVMODE_HIVISION;
		xgi_video_info.TV_plug = TVPLUG_SVIDEO;
	}
	else if (cr32 & XGI_VB_SVIDEO)
		xgi_video_info.TV_plug = TVPLUG_SVIDEO;
	else if (cr32 & XGI_VB_COMPOSITE)
		xgi_video_info.TV_plug = TVPLUG_COMPOSITE;
	else if (cr32 & XGI_VB_SCART)
		xgi_video_info.TV_plug = TVPLUG_SCART;

	if(xgi_video_info.TV_type == 0) {
	    /* TW: PAL/NTSC changed for 650 */
	    if((xgi_video_info.chip <= XGI_315PRO) || (xgi_video_info.chip >= XGI_330)) {

                inXGIIDXREG(XGICR, 0x38, temp);
		if(temp & 0x10)
			xgi_video_info.TV_type = TVMODE_PAL;
		else
			xgi_video_info.TV_type = TVMODE_NTSC;

	    } else {

	        inXGIIDXREG(XGICR, 0x79, temp);
		if(temp & 0x20)
			xgi_video_info.TV_type = TVMODE_PAL;
		else
			xgi_video_info.TV_type = TVMODE_NTSC;
	    }
	}

	/* TW: Copy forceCRT1 option to CRT1off if option is given */
    	if (XGIfb_forcecrt1 != -1) {
    		if (XGIfb_forcecrt1) XGIfb_crt1off = 0;
		else   	             XGIfb_crt1off = 1;
    	}
}

static void XGIfb_get_VB_type(void)
{
	u8 reg;

	if (!XGIfb_has_VB()) {
	        inXGIIDXREG(XGICR, IND_XGI_SCRATCH_REG_CR37, reg);
		switch ((reg & XGI_EXTERNAL_CHIP_MASK) >> 1) {
	 	   case XGI310_EXTERNAL_CHIP_LVDS:
			xgi_video_info.hasVB = HASVB_LVDS;
			break;
		   case XGI310_EXTERNAL_CHIP_LVDS_CHRONTEL:
			xgi_video_info.hasVB = HASVB_LVDS_CHRONTEL;
			break;
		   default:
			break;
		}
	}
}


static int XGIfb_has_VB(void)
{
	u8 vb_chipid;

	inXGIIDXREG(XGIPART4, 0x00, vb_chipid);
	switch (vb_chipid) {
	   case 0x01:
		xgi_video_info.hasVB = HASVB_301;
		break;
	   case 0x02:
		xgi_video_info.hasVB = HASVB_302;
		break;
	   default:
		xgi_video_info.hasVB = HASVB_NONE;
		return FALSE;
	}
	return TRUE;
}



/* ------------------ Sensing routines ------------------ */

/* TW: Determine and detect attached devices on XGI30x */
int
XGIDoSense(int tempbl, int tempbh, int tempcl, int tempch)
{
    int temp,i;

    outXGIIDXREG(XGIPART4,0x11,tempbl);
    temp = tempbh | tempcl;
    setXGIIDXREG(XGIPART4,0x10,0xe0,temp);
    for(i=0; i<10; i++) XGI_LongWait(&XGI_Pr);
    tempch &= 0x7f;
    inXGIIDXREG(XGIPART4,0x03,temp);
    temp ^= 0x0e;
    temp &= tempch;
    return(temp);
}

void
XGI_Sense30x(void)
{
  u8 backupP4_0d;
  u8 testsvhs_tempbl, testsvhs_tempbh;
  u8 testsvhs_tempcl, testsvhs_tempch;
  u8 testcvbs_tempbl, testcvbs_tempbh;
  u8 testcvbs_tempcl, testcvbs_tempch;
  u8 testvga2_tempbl, testvga2_tempbh;
  u8 testvga2_tempcl, testvga2_tempch;
  int myflag, result;

  inXGIIDXREG(XGIPART4,0x0d,backupP4_0d);
  outXGIIDXREG(XGIPART4,0x0d,(backupP4_0d | 0x04));



	testvga2_tempbh = 0x00; testvga2_tempbl = 0xd1;
        testsvhs_tempbh = 0x00; testsvhs_tempbl = 0xb9;
	testcvbs_tempbh = 0x00; testcvbs_tempbl = 0xb3;
	if((XGIhw_ext.ujVBChipID != VB_CHIP_301) &&
	   (XGIhw_ext.ujVBChipID != VB_CHIP_302)) {
	      testvga2_tempbh = 0x01; testvga2_tempbl = 0x90;
	      testsvhs_tempbh = 0x01; testsvhs_tempbl = 0x6b;
	      testcvbs_tempbh = 0x01; testcvbs_tempbl = 0x74;
	      if(XGIhw_ext.ujVBChipID == VB_CHIP_301LV ||
	         XGIhw_ext.ujVBChipID == VB_CHIP_302LV) {
	         testvga2_tempbh = 0x00; testvga2_tempbl = 0x00;
	         testsvhs_tempbh = 0x02; testsvhs_tempbl = 0x00;
	         testcvbs_tempbh = 0x01; testcvbs_tempbl = 0x00;
	      }
	}
	if(XGIhw_ext.ujVBChipID != VB_CHIP_301LV &&
	   XGIhw_ext.ujVBChipID != VB_CHIP_302LV) {
	   inXGIIDXREG(XGIPART4,0x01,myflag);
	   if(myflag & 0x04) {
	      testvga2_tempbh = 0x00; testvga2_tempbl = 0xfd;
	      testsvhs_tempbh = 0x00; testsvhs_tempbl = 0xdd;
	      testcvbs_tempbh = 0x00; testcvbs_tempbl = 0xee;
	   }
	}
	if((XGIhw_ext.ujVBChipID == VB_CHIP_301LV) ||
	   (XGIhw_ext.ujVBChipID == VB_CHIP_302LV) ) {
	   testvga2_tempbh = 0x00; testvga2_tempbl = 0x00;
	   testvga2_tempch = 0x00; testvga2_tempcl = 0x00;
	   testsvhs_tempch = 0x04; testsvhs_tempcl = 0x08;
	   testcvbs_tempch = 0x08; testcvbs_tempcl = 0x08;
	} else {
	   testvga2_tempch = 0x0e; testvga2_tempcl = 0x08;
	   testsvhs_tempch = 0x06; testsvhs_tempcl = 0x04;
	   testcvbs_tempch = 0x08; testcvbs_tempcl = 0x04;
	}


    if(testvga2_tempch || testvga2_tempcl || testvga2_tempbh || testvga2_tempbl) {
        result = XGIDoSense(testvga2_tempbl, testvga2_tempbh,
                            testvga2_tempcl, testvga2_tempch);
 	if(result) {
        	printk(KERN_INFO "XGIfb: Detected secondary VGA connection\n");
		orXGIIDXREG(XGICR, 0x32, 0x10);
	}
    }
    
    result = XGIDoSense(testsvhs_tempbl, testsvhs_tempbh,
                        testsvhs_tempcl, testsvhs_tempch);
    if(result) {
        printk(KERN_INFO "XGIfb: Detected TV connected to SVHS output\n");
        /* TW: So we can be sure that there IS a SVHS output */
	xgi_video_info.TV_plug = TVPLUG_SVIDEO;
	orXGIIDXREG(XGICR, 0x32, 0x02);
    }

    if(!result) {
        result = XGIDoSense(testcvbs_tempbl, testcvbs_tempbh,
	                    testcvbs_tempcl, testcvbs_tempch);
	if(result) {
	    printk(KERN_INFO "XGIfb: Detected TV connected to CVBS output\n");
	    /* TW: So we can be sure that there IS a CVBS output */
	    xgi_video_info.TV_plug = TVPLUG_COMPOSITE;
	    orXGIIDXREG(XGICR, 0x32, 0x01);
	}
    }
    XGIDoSense(0, 0, 0, 0);

    outXGIIDXREG(XGIPART4,0x0d,backupP4_0d);
}



/* ------------------------ Heap routines -------------------------- */

static int XGIfb_heap_init(void)
{
	XGI_OH *poh;
	u8 temp=0;
	unsigned long *cmdq_baseport = 0;
	unsigned long *read_port = 0;
	unsigned long *write_port = 0;
	XGI_CMDTYPE    cmd_type;
#ifndef AGPOFF
	
	struct agp_memory     *agp;
	u32            agp_phys;
#endif

/* TW: The heap start is either set manually using the "mem" parameter, or
 *     defaults as follows:
 *     -) If more than 16MB videoRAM available, let our heap start at 12MB.
 *     -) If more than  8MB videoRAM available, let our heap start at  8MB.
 *     -) If 4MB or less is available, let it start at 4MB.
 *     This is for avoiding a clash with X driver which uses the beginning
 *     of the videoRAM. To limit size of X framebuffer, use Option MaxXFBMem
 *     in XF86Config-4.
 *     The heap start can also be specified by parameter "mem" when starting the XGIfb
 *     driver. XGIfb mem=1024 lets heap starts at 1MB, etc.
 */
     if ((!XGIfb_mem) || (XGIfb_mem > (xgi_video_info.video_size/1024))) {
        if (xgi_video_info.video_size > 0x1000000) {
	        xgi_video_info.heapstart = 0xD00000;
	} else if (xgi_video_info.video_size > 0x800000) {
	        xgi_video_info.heapstart = 0x800000;
	} else {
		xgi_video_info.heapstart = 0x400000;
	}
     } else {
           xgi_video_info.heapstart = XGIfb_mem * 1024;
     }
     XGIfb_heap_start =
	       (unsigned long) (xgi_video_info.video_vbase + xgi_video_info.heapstart);
     printk(KERN_INFO "XGIfb: Memory heap starting at %dK\n",
     					(int)(xgi_video_info.heapstart / 1024));

     XGIfb_heap_end = (unsigned long) xgi_video_info.video_vbase + xgi_video_info.video_size;
     XGIfb_heap_size = XGIfb_heap_end - XGIfb_heap_start;


 
        /* TW: Now initialize the 310 series' command queue mode.
	 * On 310/325, there are three queue modes available which
	 * are chosen by setting bits 7:5 in SR26:
	 * 1. MMIO queue mode (bit 5, 0x20). The hardware will keep
	 *    track of the queue, the FIFO, command parsing and so
	 *    on. This is the one comparable to the 300 series.
	 * 2. VRAM queue mode (bit 6, 0x40). In this case, one will
	 *    have to do queue management himself. Register 0x85c4 will
	 *    hold the location of the next free queue slot, 0x85c8
	 *    is the "queue read pointer" whose way of working is
	 *    unknown to me. Anyway, this mode would require a
	 *    translation of the MMIO commands to some kind of
	 *    accelerator assembly and writing these commands
	 *    to the memory location pointed to by 0x85c4.
	 *    We will not use this, as nobody knows how this
	 *    "assembly" works, and as it would require a complete
	 *    re-write of the accelerator code.
	 * 3. AGP queue mode (bit 7, 0x80). Works as 2., but keeps the
	 *    queue in AGP memory space.
	 *
	 * SR26 bit 4 is called "Bypass H/W queue".
	 * SR26 bit 1 is called "Enable Command Queue Auto Correction"
	 * SR26 bit 0 resets the queue
	 * Size of queue memory is encoded in bits 3:2 like this:
	 * The queue location is to be written to 0x85C0.
	 *
         */
	cmdq_baseport = (unsigned long *)(xgi_video_info.mmio_vbase + MMIO_QUEUE_PHYBASE);
	write_port    = (unsigned long *)(xgi_video_info.mmio_vbase + MMIO_QUEUE_WRITEPORT);
	read_port     = (unsigned long *)(xgi_video_info.mmio_vbase + MMIO_QUEUE_READPORT);

	
		cmd_type = MMIO_CMD;

        XGIfb_heap_end -= COMMAND_QUEUE_AREA_SIZE;
        XGIfb_heap_size -= COMMAND_QUEUE_AREA_SIZE;

		outXGIIDXREG(XGISR, IND_XGI_CMDQUEUE_THRESHOLD, COMMAND_QUEUE_THRESHOLD);
		outXGIIDXREG(XGISR, IND_XGI_CMDQUEUE_SET, XGI_CMD_QUEUE_RESET);
 
 		*write_port = *read_port;

		/* TW: Set Auto_Correction bit */
 		temp |= (XGI_MMIO_CMD_ENABLE | XGI_CMD_AUTO_CORR);
 	        outXGIIDXREG(XGISR, IND_XGI_CMDQUEUE_SET, 0x22); //yilin
 
 		*cmdq_baseport = xgi_video_info.video_size - COMMAND_QUEUE_AREA_SIZE;
		XGIfb_caps |= MMIO_CMD_QUEUE_CAP;
 
//         	printk("XGIfb: MMIO Cmd Queue offset = 0x%lx, size is %dK\n",
// 			*cmdq_baseport, COMMAND_QUEUE_AREA_SIZE/1024);





     /* TW: Now reserve memory for the HWCursor. It is always located at the very
            top of the videoRAM, right below the TB memory area (if used). */
     if (XGIfb_heap_size >= XGIfb_hwcursor_size) {
		XGIfb_heap_end -= XGIfb_hwcursor_size;
		XGIfb_heap_size -= XGIfb_hwcursor_size;
		XGIfb_hwcursor_vbase = XGIfb_heap_end;

		XGIfb_caps |= HW_CURSOR_CAP;

//		printk("XGIfb: Hardware Cursor start at 0x%lx, size is %dK\n",
//		XGIfb_heap_end, XGIfb_hwcursor_size/1024);
     }

     XGIfb_heap.poha_chain = NULL;
     XGIfb_heap.poh_freelist = NULL;

     poh = XGIfb_poh_new_node();

     if(poh == NULL)  return 1;
	
     poh->poh_next = &XGIfb_heap.oh_free;
     poh->poh_prev = &XGIfb_heap.oh_free;
     poh->size = XGIfb_heap_end - XGIfb_heap_start + 1;
     poh->offset = XGIfb_heap_start - (unsigned long) xgi_video_info.video_vbase;

     DPRINTK("XGIfb: Heap start:0x%p, end:0x%p, len=%dk\n",
		(char *) XGIfb_heap_start, (char *) XGIfb_heap_end,
		(unsigned int) poh->size / 1024);

     DPRINTK("XGIfb: First Node offset:0x%x, size:%dk\n",
		(unsigned int) poh->offset, (unsigned int) poh->size / 1024);

     XGIfb_heap.oh_free.poh_next = poh;
     XGIfb_heap.oh_free.poh_prev = poh;
     XGIfb_heap.oh_free.size = 0;
     XGIfb_heap.max_freesize = poh->size;

     XGIfb_heap.oh_used.poh_next = &XGIfb_heap.oh_used;
     XGIfb_heap.oh_used.poh_prev = &XGIfb_heap.oh_used;
     XGIfb_heap.oh_used.size = SENTINEL;

     return 0;
}

static XGI_OH *XGIfb_poh_new_node(void)
{
	int           i;
	unsigned long cOhs;
	XGI_OHALLOC   *poha;
	XGI_OH        *poh;

	if (XGIfb_heap.poh_freelist == NULL) {
		poha = kmalloc(OH_ALLOC_SIZE, GFP_KERNEL);
		if(!poha) return NULL;

		poha->poha_next = XGIfb_heap.poha_chain;
		XGIfb_heap.poha_chain = poha;

		cOhs = (OH_ALLOC_SIZE - sizeof(XGI_OHALLOC)) / sizeof(XGI_OH) + 1;

		poh = &poha->aoh[0];
		for (i = cOhs - 1; i != 0; i--) {
			poh->poh_next = poh + 1;
			poh = poh + 1;
		}

		poh->poh_next = NULL;
		XGIfb_heap.poh_freelist = &poha->aoh[0];
	}

	poh = XGIfb_heap.poh_freelist;
	XGIfb_heap.poh_freelist = poh->poh_next;

	return (poh);
}

static XGI_OH *XGIfb_poh_allocate(unsigned long size)
{
	XGI_OH *pohThis;
	XGI_OH *pohRoot;
	int     bAllocated = 0;

	if (size > XGIfb_heap.max_freesize) {
		DPRINTK("XGIfb: Can't allocate %dk size on offscreen\n",
			(unsigned int) size / 1024);
		return (NULL);
	}

	pohThis = XGIfb_heap.oh_free.poh_next;

	while (pohThis != &XGIfb_heap.oh_free) {
		if (size <= pohThis->size) {
			bAllocated = 1;
			break;
		}
		pohThis = pohThis->poh_next;
	}

	if (!bAllocated) {
		DPRINTK("XGIfb: Can't allocate %dk size on offscreen\n",
			(unsigned int) size / 1024);
		return (NULL);
	}

	if (size == pohThis->size) {
		pohRoot = pohThis;
		XGIfb_delete_node(pohThis);
	} else {
		pohRoot = XGIfb_poh_new_node();

		if (pohRoot == NULL) {
			return (NULL);
		}

		pohRoot->offset = pohThis->offset;
		pohRoot->size = size;

		pohThis->offset += size;
		pohThis->size -= size;
	}

	XGIfb_heap.max_freesize -= size;

	pohThis = &XGIfb_heap.oh_used;
	XGIfb_insert_node(pohThis, pohRoot);

	return (pohRoot);
}

static void XGIfb_delete_node(XGI_OH *poh)
{
	XGI_OH *poh_prev;
	XGI_OH *poh_next;

	poh_prev = poh->poh_prev;
	poh_next = poh->poh_next;

	poh_prev->poh_next = poh_next;
	poh_next->poh_prev = poh_prev;

}

static void XGIfb_insert_node(XGI_OH *pohList, XGI_OH *poh)
{
	XGI_OH *pohTemp;

	pohTemp = pohList->poh_next;

	pohList->poh_next = poh;
	pohTemp->poh_prev = poh;

	poh->poh_prev = pohList;
	poh->poh_next = pohTemp;
}

static XGI_OH *XGIfb_poh_free(unsigned long base)
{
	XGI_OH *pohThis;
	XGI_OH *poh_freed;
	XGI_OH *poh_prev;
	XGI_OH *poh_next;
	unsigned long ulUpper;
	unsigned long ulLower;
	int foundNode = 0;

	poh_freed = XGIfb_heap.oh_used.poh_next;

	while(poh_freed != &XGIfb_heap.oh_used) {
		if(poh_freed->offset == base) {
			foundNode = 1;
			break;
		}

		poh_freed = poh_freed->poh_next;
	}

	if (!foundNode)  return (NULL);

	XGIfb_heap.max_freesize += poh_freed->size;

	poh_prev = poh_next = NULL;
	ulUpper = poh_freed->offset + poh_freed->size;
	ulLower = poh_freed->offset;

	pohThis = XGIfb_heap.oh_free.poh_next;

	while (pohThis != &XGIfb_heap.oh_free) {
		if (pohThis->offset == ulUpper) {
			poh_next = pohThis;
		}
			else if ((pohThis->offset + pohThis->size) ==
				 ulLower) {
			poh_prev = pohThis;
		}
		pohThis = pohThis->poh_next;
	}

	XGIfb_delete_node(poh_freed);

	if (poh_prev && poh_next) {
		poh_prev->size += (poh_freed->size + poh_next->size);
		XGIfb_delete_node(poh_next);
		XGIfb_free_node(poh_freed);
		XGIfb_free_node(poh_next);
		return (poh_prev);
	}

	if (poh_prev) {
		poh_prev->size += poh_freed->size;
		XGIfb_free_node(poh_freed);
		return (poh_prev);
	}

	if (poh_next) {
		poh_next->size += poh_freed->size;
		poh_next->offset = poh_freed->offset;
		XGIfb_free_node(poh_freed);
		return (poh_next);
	}

	XGIfb_insert_node(&XGIfb_heap.oh_free, poh_freed);

	return (poh_freed);
}

static void XGIfb_free_node(XGI_OH *poh)
{
	if(poh == NULL) return;

	poh->poh_next = XGIfb_heap.poh_freelist;
	XGIfb_heap.poh_freelist = poh;

}

void XGI_malloc(struct XGI_memreq *req)
{
	XGI_OH *poh;

	poh = XGIfb_poh_allocate(req->size);

	if(poh == NULL) {
		req->offset = 0;
		req->size = 0;
		DPRINTK("XGIfb: Video RAM allocation failed\n");
	} else {
		DPRINTK("XGIfb: Video RAM allocation succeeded: 0x%p\n",
			(char *) (poh->offset + (unsigned long) xgi_video_info.video_vbase));

		req->offset = poh->offset;
		req->size = poh->size;
	}

}

void XGI_free(unsigned long base)
{
	XGI_OH *poh;

	poh = XGIfb_poh_free(base);

	if(poh == NULL) {
		DPRINTK("XGIfb: XGIfb_poh_free() failed at base 0x%x\n",
			(unsigned int) base);
	}
}

/* --------------------- SetMode routines ------------------------- */

static void XGIfb_pre_setmode(void)
{
	u8 cr30 = 0, cr31 = 0;

	inXGIIDXREG(XGICR, 0x31, cr31);
	cr31 &= ~0x60;

	switch (xgi_video_info.disp_state & DISPTYPE_DISP2) {
	   case DISPTYPE_CRT2:
		cr30 = (XGI_VB_OUTPUT_CRT2 | XGI_SIMULTANEOUS_VIEW_ENABLE);
		cr31 |= XGI_DRIVER_MODE;
		break;
	   case DISPTYPE_LCD:
		cr30  = (XGI_VB_OUTPUT_LCD | XGI_SIMULTANEOUS_VIEW_ENABLE);
		cr31 |= XGI_DRIVER_MODE;
		break;
	   case DISPTYPE_TV:
		if (xgi_video_info.TV_type == TVMODE_HIVISION)
			cr30 = (XGI_VB_OUTPUT_HIVISION | XGI_SIMULTANEOUS_VIEW_ENABLE);
		else if (xgi_video_info.TV_plug == TVPLUG_SVIDEO)
			cr30 = (XGI_VB_OUTPUT_SVIDEO | XGI_SIMULTANEOUS_VIEW_ENABLE);
		else if (xgi_video_info.TV_plug == TVPLUG_COMPOSITE)
			cr30 = (XGI_VB_OUTPUT_COMPOSITE | XGI_SIMULTANEOUS_VIEW_ENABLE);
		else if (xgi_video_info.TV_plug == TVPLUG_SCART)
			cr30 = (XGI_VB_OUTPUT_SCART | XGI_SIMULTANEOUS_VIEW_ENABLE);
		cr31 |= XGI_DRIVER_MODE;

	        if (XGIfb_tvmode == 1 || xgi_video_info.TV_type == TVMODE_PAL)
			cr31 |= 0x01;
                else
                        cr31 &= ~0x01;
		break;
	   default:	/* disable CRT2 */
		cr30 = 0x00;
		cr31 |= (XGI_DRIVER_MODE | XGI_VB_OUTPUT_DISABLE);
	}

    outXGIIDXREG(XGICR, IND_XGI_SCRATCH_REG_CR30, cr30);
    outXGIIDXREG(XGICR, IND_XGI_SCRATCH_REG_CR31, cr31);
    outXGIIDXREG(XGICR, IND_XGI_SCRATCH_REG_CR33, (XGIfb_rate_idx & 0x0F));

	if(xgi_video_info.accel) XGIfb_syncaccel();

	
}

static void XGIfb_post_setmode(void)
{
	u8 reg;
	BOOLEAN doit = TRUE;
#if 0	/* TW: Wrong: Is not in MMIO space, but in RAM */
	/* Backup mode number to MMIO space */
	if(xgi_video_info.mmio_vbase) {
	  *(volatile u8 *)(((u8*)xgi_video_info.mmio_vbase) + 0x449) = (unsigned char)XGIfb_mode_no;
	}
#endif	
/*	outXGIIDXREG(XGISR,IND_XGI_PASSWORD,XGI_PASSWORD);
	outXGIIDXREG(XGICR,0x13,0x00);
	setXGIIDXREG(XGISR,0x0E,0xF0,0x01);
*test**/
	if (xgi_video_info.video_bpp == 8) {
		/* TW: We can't switch off CRT1 on LVDS/Chrontel in 8bpp Modes */
		if ((xgi_video_info.hasVB == HASVB_LVDS) || (xgi_video_info.hasVB == HASVB_LVDS_CHRONTEL)) {
			doit = FALSE;
		}
		/* TW: We can't switch off CRT1 on 301B-DH in 8bpp Modes if using LCD */
		if  (xgi_video_info.disp_state & DISPTYPE_LCD)  {
	        	doit = FALSE;
	        }
	}

	/* TW: We can't switch off CRT1 if bridge is in slave mode */
	if(xgi_video_info.hasVB != HASVB_NONE) {
		inXGIIDXREG(XGIPART1, 0x00, reg);


		if((reg & 0x50) == 0x10) {
			doit = FALSE;
		}

	} else XGIfb_crt1off = 0;

	inXGIIDXREG(XGICR, 0x17, reg);
	if((XGIfb_crt1off) && (doit))
		reg &= ~0x80;
	else 	      
		reg |= 0x80;
    outXGIIDXREG(XGICR, 0x17, reg);

    andXGIIDXREG(XGISR, IND_XGI_RAMDAC_CONTROL, ~0x04);

	if((xgi_video_info.disp_state & DISPTYPE_TV) && (xgi_video_info.hasVB == HASVB_301)) {

	   inXGIIDXREG(XGIPART4, 0x01, reg);

	   if(reg < 0xB0) {        	/* Set filter for XGI301 */

		switch (xgi_video_info.video_width) {
		   case 320:
			filter_tb = (xgi_video_info.TV_type == TVMODE_NTSC) ? 4 : 12;
			break;
		   case 640:
			filter_tb = (xgi_video_info.TV_type == TVMODE_NTSC) ? 5 : 13;
			break;
		   case 720:
			filter_tb = (xgi_video_info.TV_type == TVMODE_NTSC) ? 6 : 14;
			break;
		   case 800:
			filter_tb = (xgi_video_info.TV_type == TVMODE_NTSC) ? 7 : 15;
			break;
		   default:
			filter = -1;
			break;
		}

		orXGIIDXREG(XGIPART1, XGIfb_CRT2_write_enable, 0x01);

		if(xgi_video_info.TV_type == TVMODE_NTSC) {

		        andXGIIDXREG(XGIPART2, 0x3a, 0x1f);

			if (xgi_video_info.TV_plug == TVPLUG_SVIDEO) {

			        andXGIIDXREG(XGIPART2, 0x30, 0xdf);

			} else if (xgi_video_info.TV_plug == TVPLUG_COMPOSITE) {

			        orXGIIDXREG(XGIPART2, 0x30, 0x20);

				switch (xgi_video_info.video_width) {
				case 640:
				        outXGIIDXREG(XGIPART2, 0x35, 0xEB);
					outXGIIDXREG(XGIPART2, 0x36, 0x04);
					outXGIIDXREG(XGIPART2, 0x37, 0x25);
					outXGIIDXREG(XGIPART2, 0x38, 0x18);
					break;
				case 720:
					outXGIIDXREG(XGIPART2, 0x35, 0xEE);
					outXGIIDXREG(XGIPART2, 0x36, 0x0C);
					outXGIIDXREG(XGIPART2, 0x37, 0x22);
					outXGIIDXREG(XGIPART2, 0x38, 0x08);
					break;
				case 800:
					outXGIIDXREG(XGIPART2, 0x35, 0xEB);
					outXGIIDXREG(XGIPART2, 0x36, 0x15);
					outXGIIDXREG(XGIPART2, 0x37, 0x25);
					outXGIIDXREG(XGIPART2, 0x38, 0xF6);
					break;
				}
			}

		} else if(xgi_video_info.TV_type == TVMODE_PAL) {

			andXGIIDXREG(XGIPART2, 0x3A, 0x1F);

			if (xgi_video_info.TV_plug == TVPLUG_SVIDEO) {

			        andXGIIDXREG(XGIPART2, 0x30, 0xDF);

			} else if (xgi_video_info.TV_plug == TVPLUG_COMPOSITE) {

			        orXGIIDXREG(XGIPART2, 0x30, 0x20);

				switch (xgi_video_info.video_width) {
				case 640:
					outXGIIDXREG(XGIPART2, 0x35, 0xF1);
					outXGIIDXREG(XGIPART2, 0x36, 0xF7);
					outXGIIDXREG(XGIPART2, 0x37, 0x1F);
					outXGIIDXREG(XGIPART2, 0x38, 0x32);
					break;
				case 720:
					outXGIIDXREG(XGIPART2, 0x35, 0xF3);
					outXGIIDXREG(XGIPART2, 0x36, 0x00);
					outXGIIDXREG(XGIPART2, 0x37, 0x1D);
					outXGIIDXREG(XGIPART2, 0x38, 0x20);
					break;
				case 800:
					outXGIIDXREG(XGIPART2, 0x35, 0xFC);
					outXGIIDXREG(XGIPART2, 0x36, 0xFB);
					outXGIIDXREG(XGIPART2, 0x37, 0x14);
					outXGIIDXREG(XGIPART2, 0x38, 0x2A);
					break;
				}
			}
		}

		if ((filter >= 0) && (filter <=7)) {
			DPRINTK("FilterTable[%d]-%d: %02x %02x %02x %02x\n", filter_tb, filter, 
				XGI_TV_filter[filter_tb].filter[filter][0],
				XGI_TV_filter[filter_tb].filter[filter][1],
				XGI_TV_filter[filter_tb].filter[filter][2],
				XGI_TV_filter[filter_tb].filter[filter][3]
			);
			outXGIIDXREG(XGIPART2, 0x35, (XGI_TV_filter[filter_tb].filter[filter][0]));
			outXGIIDXREG(XGIPART2, 0x36, (XGI_TV_filter[filter_tb].filter[filter][1]));
			outXGIIDXREG(XGIPART2, 0x37, (XGI_TV_filter[filter_tb].filter[filter][2]));
			outXGIIDXREG(XGIPART2, 0x38, (XGI_TV_filter[filter_tb].filter[filter][3]));
		}

	     }
	  
	}

}

#ifndef MODULE
XGIINITSTATIC int __init XGIfb_setup(char *options)
{
	char *this_opt;
	


	xgi_video_info.refresh_rate = 0;

        printk(KERN_INFO "XGIfb: Options %s\n", options);

	if (!options || !*options)
		return 0;

	while((this_opt = strsep(&options, ",")) != NULL) {

		if (!*this_opt)	continue;

		if (!strncmp(this_opt, "mode:", 5)) {
			XGIfb_search_mode(this_opt + 5);
		} else if (!strncmp(this_opt, "vesa:", 5)) {
			XGIfb_search_vesamode(simple_strtoul(this_opt + 5, NULL, 0));
		} else if (!strncmp(this_opt, "mode:", 5)) {
			XGIfb_search_mode(this_opt + 5);
		} else if (!strncmp(this_opt, "vesa:", 5)) {
			XGIfb_search_vesamode(simple_strtoul(this_opt + 5, NULL, 0));
		} else if (!strncmp(this_opt, "vrate:", 6)) {
			xgi_video_info.refresh_rate = simple_strtoul(this_opt + 6, NULL, 0);
		} else if (!strncmp(this_opt, "rate:", 5)) {
			xgi_video_info.refresh_rate = simple_strtoul(this_opt + 5, NULL, 0);
		} else if (!strncmp(this_opt, "off", 3)) {
			XGIfb_off = 1;
		} else if (!strncmp(this_opt, "crt1off", 7)) {
			XGIfb_crt1off = 1;
		} else if (!strncmp(this_opt, "filter:", 7)) {
			filter = (int)simple_strtoul(this_opt + 7, NULL, 0);
		} else if (!strncmp(this_opt, "forcecrt2type:", 14)) {
			XGIfb_search_crt2type(this_opt + 14);
		} else if (!strncmp(this_opt, "forcecrt1:", 10)) {
			XGIfb_forcecrt1 = (int)simple_strtoul(this_opt + 10, NULL, 0);
                } else if (!strncmp(this_opt, "tvmode:",7)) {
		        XGIfb_search_tvstd(this_opt + 7);
                } else if (!strncmp(this_opt, "tvstandard:",11)) {
			XGIfb_search_tvstd(this_opt + 7);
                } else if (!strncmp(this_opt, "mem:",4)) {
		        XGIfb_mem = simple_strtoul(this_opt + 4, NULL, 0);
                } else if (!strncmp(this_opt, "dstn", 4)) {
			enable_dstn = 1;
			/* TW: DSTN overrules forcecrt2type */
			XGIfb_crt2type = DISPTYPE_LCD;
		} else if (!strncmp(this_opt, "pdc:", 4)) {
		        XGIfb_pdc = simple_strtoul(this_opt + 4, NULL, 0);
		        if(XGIfb_pdc & ~0x3c) {
			   printk(KERN_INFO "XGIfb: Illegal pdc parameter\n");
			   XGIfb_pdc = 0;
		        }
		} else if (!strncmp(this_opt, "noaccel", 7)) {
			XGIfb_accel = 0;
		} else if (!strncmp(this_opt, "noypan", 6)) {
		        XGIfb_ypan = 0;
		} else if (!strncmp(this_opt, "userom:", 7)) {
			XGIfb_userom = (int)simple_strtoul(this_opt + 7, NULL, 0);
//		} else if (!strncmp(this_opt, "useoem:", 7)) {
//			XGIfb_useoem = (int)simple_strtoul(this_opt + 7, NULL, 0);
		} else {
			printk(KERN_INFO "XGIfb: Invalid option %s\n", this_opt);
		}

		/* TW: Acceleration only with MMIO mode */
		if((XGIfb_queuemode != -1) && (XGIfb_queuemode != MMIO_CMD)) {
			XGIfb_ypan = 0;
			XGIfb_accel = 0;
		}
		/* TW: Panning only with acceleration */
		if(XGIfb_accel == 0) XGIfb_ypan = 0;

	}
	printk("\nxgifb: outa xgifb_setup 3450");
	return 0;
}
#endif

static unsigned char VBIOS_BUF[65535];

unsigned char* attempt_map_rom(struct pci_dev *dev,void *copy_address)
{
	  u32 rom_size      = 0;
    int j;
    u32 rom_address   = 0;
/*    
    u32 bar_size      = 0;
    u32 bar_backup    = 0;
    void *image       = 0;
    u32 image_size    = 0;
    int did_correct   = 0;
    u32 prefetch_addr = 0;
    u32 prefetch_size = 0;
    u32 prefetch_idx  = 0;
*/
    /*  Get the size of the expansion rom */
    pci_write_config_dword(dev, PCI_ROM_ADDRESS, 0xFFFFFFFF);
    pci_read_config_dword(dev, PCI_ROM_ADDRESS, &rom_size);
    if ((rom_size & 0x01) == 0)
    {
		printk("No ROM\n");
		return NULL;
    }

    rom_size &= 0xFFFFF800;
    rom_size = (~rom_size)+1;

    rom_address = pci_resource_start(dev, 0);
    if (rom_address == 0 || rom_address == 0xFFFFFFF0)
    {
		printk("No suitable rom address found\n"); return NULL;
    }

    printk("ROM Size is %dK, Address is %x\n", rom_size/1024, rom_address);

    /*  Map ROM */
    pci_write_config_dword(dev, PCI_ROM_ADDRESS, rom_address | PCI_ROM_ADDRESS_ENABLE);

    /* memcpy(copy_address, rom_address, rom_size); */
    {
		unsigned char *virt_addr = ioremap(rom_address, 0x8000000);

		unsigned char *from = (unsigned char *)virt_addr;
		unsigned char *to = (unsigned char *)copy_address;
		for (j=0; j<65536 /*rom_size*/; j++) *to++ = *from++;
// DEL 		
// DEL 		iounmap(virt_addr);
	}

	pci_write_config_dword(dev, PCI_ROM_ADDRESS, 0);
	
    printk("Copy is done\n");

	return copy_address;
}
#if defined(__i386__)
static char *XGI_find_rom(void)
{
        u32  segstart;
        unsigned char *rom_base;
        unsigned char *rom;
        int  stage;
        int  i;
        char XGI_rom_sig[] = "XGI Technology, Inc.";
        
        char *XGI_sig[5] = {
          "Volari GPU", "XG41","XG42", "XG45", "Volari Z7"
        };
	
	unsigned short XGI_nums[5] = {
	    XG40, XG41, XG42, XG45, XG20
	};

        for(segstart=0x000c0000; segstart<0x000f0000; segstart+=0x00001000) {

                stage = 1;

                rom_base = (char *)ioremap(segstart, 0x1000);

                if ((*rom_base == 0x55) && (((*(rom_base + 1)) & 0xff) == 0xaa))
                   stage = 2;

                if (stage != 2) {
                   iounmap(rom_base);
                   continue;
                }


		rom = rom_base + (unsigned short)(*(rom_base + 0x12) | (*(rom_base + 0x13) << 8));
                if(strncmp(XGI_rom_sig, rom, strlen(XGI_rom_sig)) == 0) {
                    stage = 3;
		}
                if(stage != 3) {
                    iounmap(rom_base);
                    continue;
                }

		rom = rom_base + (unsigned short)(*(rom_base + 0x14) | (*(rom_base + 0x15) << 8));
        for(i = 0;(i < 5) && (stage != 4); i++) {
            if(strncmp(XGI_sig[i], rom, strlen(XGI_sig[i])) == 0) {
		        if(XGI_nums[i] == xgi_video_info.chip) {
                    stage = 4;
                    break;
			    }
            }
        }
		

        if(stage != 4) {
            iounmap(rom_base);
            continue;
        }

        return rom_base;
        }

        return NULL;
}
#endif

/* +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ */

int __devinit xgifb_probe(struct pci_dev *pdev, const struct pci_device_id *ent)
{
	u16 reg16;
	u8  reg, reg1;

	if (XGIfb_off)
		return -ENXIO;

	XGIfb_registered = 0;

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,3))
	  fb_info = framebuffer_alloc(sizeof(struct fb_info), &pdev->dev);
	  if(!fb_info) return -ENOMEM;
#else
	  XGI_fb_info = kmalloc( sizeof(struct fb_info), GFP_KERNEL);
	  if(!XGI_fb_info) return -ENOMEM;
	  memset(XGI_fb_info, 0,  sizeof(struct fb_info));
#endif

  	xgi_video_info.chip_id = pdev->device;
	  pci_read_config_byte(pdev, PCI_REVISION_ID,&xgi_video_info.revision_id);
	  pci_read_config_word(pdev, PCI_COMMAND, &reg16);
	  XGIhw_ext.jChipRevision = xgi_video_info.revision_id;
	  XGIvga_enabled = reg16 & 0x01;
	
	  xgi_video_info.pcibus = pdev->bus->number;
	  xgi_video_info.pcislot = PCI_SLOT(pdev->devfn);
	  xgi_video_info.pcifunc = PCI_FUNC(pdev->devfn);
	  xgi_video_info.subsysvendor = pdev->subsystem_vendor;
	  xgi_video_info.subsysdevice = pdev->subsystem_device;
	
	
      switch (xgi_video_info.chip_id) {
        case PCI_DEVICE_ID_XG_21:
                XGIhw_ext.b20A1ECOIDIs21 = true;
                xgi_video_info.chip = XG21  ;
                XGIfb_hwcursor_size = HW_CURSOR_AREA_SIZE_315 * 2;
                XGIfb_CRT2_write_enable = IND_XGI_CRT2_WRITE_ENABLE_315;
	    break;

	    case PCI_DEVICE_ID_XG_20:
		xgi_video_info.chip = XG20;
		XGIfb_hwcursor_size = HW_CURSOR_AREA_SIZE_315 * 2;
		XGIfb_CRT2_write_enable = IND_XGI_CRT2_WRITE_ENABLE_315;
		break;
	    case PCI_DEVICE_ID_XG_40:
		xgi_video_info.chip = XG40;
		XGIfb_hwcursor_size = HW_CURSOR_AREA_SIZE_315 * 2;
		XGIfb_CRT2_write_enable = IND_XGI_CRT2_WRITE_ENABLE_315;
		break;
	    case PCI_DEVICE_ID_XG_41:
		xgi_video_info.chip = XG41;
		XGIfb_hwcursor_size = HW_CURSOR_AREA_SIZE_315 * 2;
		XGIfb_CRT2_write_enable = IND_XGI_CRT2_WRITE_ENABLE_315;
		break;
	    case PCI_DEVICE_ID_XG_42:
		xgi_video_info.chip = XG42;
		XGIfb_hwcursor_size = HW_CURSOR_AREA_SIZE_315 * 2;
		XGIfb_CRT2_write_enable = IND_XGI_CRT2_WRITE_ENABLE_315;
		break;
	    case PCI_DEVICE_ID_XG_27:
		xgi_video_info.chip = XG27;
		XGIfb_hwcursor_size = HW_CURSOR_AREA_SIZE_315 * 2;
		XGIfb_CRT2_write_enable = IND_XGI_CRT2_WRITE_ENABLE_315;
		break;


        default:
	    return -ENODEV;
	}
        //	  printk("XGIfb:chipid = %x\n",xgi_video_info.chip);
        XGIhw_ext.jChipType = xgi_video_info.chip;

        xgi_video_info.video_base = pci_resource_start(pdev, 0);
        xgi_video_info.mmio_base = pci_resource_start(pdev, 1);
        XGI_Pr.RelIO  =   pci_resource_start(pdev, 2) + 0x30;
        //    XGI_Pr.RelIO  = ioremap(pci_resource_start(pdev, 2), 128) + 0x30;
        printk("<%s> - line %d: xgi_video_info.video_base = 0x%llx\n", __func__, __LINE__,xgi_video_info.video_base);
        printk("<%s> - line %d: xgi_video_info.mmio_base = 0x%llx\n", __func__, __LINE__,xgi_video_info.mmio_base);
        //XGI_Pr.RelIO  = (unsigned long)ioremap(pci_resource_start(pdev, 2), 128) + 0x30;
        xgi_video_info.vga_base = XGI_Pr.RelIO;
        XGIhw_ext.pjIOAddress = (PUCHAR) XGI_Pr.RelIO;
        //printk("XGIfb: Relocate IO address: %lx [%08lx] \n", (long unsigned int)pci_resource_start(pdev, 2), XGI_Pr.RelIO);
        printk("XGIfb: Relocate IO address: 0x%llx [%lx] \n", pci_resource_start(pdev, 2), XGI_Pr.RelIO);
        XGIfb_mmio_size =  pci_resource_len(pdev, 1);

//	  if(!XGIvga_enabled) {
	    printk("XGIfb: Enable PCI device\n");
	      
	    if(pci_enable_device(pdev))   
	        return -EIO;
//	  }

	
    XGIRegInit(&XGI_Pr, (ULONG)XGIhw_ext.pjIOAddress);
    outXGIREG(XGI_Pr.P3c2,0x67 );	
    outXGIIDXREG(XGISR, IND_XGI_PASSWORD, XGI_PASSWORD);
    inXGIIDXREG(XGISR, IND_XGI_PASSWORD, reg1);
    
    if(reg1 != 0xa1) /*I/O error */
    {
         printk("\nXGIfb: I/O error!!!");
         return  -EIO;
    }

//determine XG21 or not
    if(xgi_video_info.chip == XG20)
    {
/*        orXGIIDXREG(XGICR, Index_CR_GPIO_Reg3, GPIOG_EN) ;
        inXGIIDXREG(XGICR, Index_CR_GPIO_Reg1, reg1) ;
        if(reg1 & GPIOG_READ)
        {
*/
        xgi_video_info.chip = XG21;
        XGIhw_ext.jChipType = XG21;
//        }
   
    }


	
      switch (xgi_video_info.chip) {
        case XG40:
        case XG41:
        case XG42:
        case XG45:
        case XG20:
        case XG21:
        case XG27:
            XGIhw_ext.bIntegratedMMEnabled = TRUE;
			break;

        default:
			break;
      }
	

	  XGIhw_ext.pDevice = NULL;
	  if(XGIfb_userom) 
	  {
	      //XGIhw_ext.pjVirtualRomBase = XGI_find_rom();
	      XGIhw_ext.pjVirtualRomBase = attempt_map_rom(pdev, VBIOS_BUF);
		  
	      if(XGIhw_ext.pjVirtualRomBase) 
	      {
            printk(KERN_INFO "XGIfb: Video ROM found and mapped to %p\n",XGIhw_ext.pjVirtualRomBase);

	      } else  
	      {

//	          printk(KERN_INFO "XGIfb: Video ROM not found\n");
	      }
    } else 
    {
	      XGIhw_ext.pjVirtualRomBase = NULL;
//        printk(KERN_INFO "XGIfb: Video ROM usage disabled\n");
	  }
	  XGIhw_ext.pjCustomizedROMImage = NULL;
	  XGIhw_ext.bSkipDramSizing = 0;
	  XGIhw_ext.pQueryVGAConfigSpace = &XGIfb_query_VGA_config_space;

	  strcpy(XGIhw_ext.szVBIOSVer, "0.85");


      XGIhw_ext.pSR = vmalloc(sizeof(XGI_DSReg) * SR_BUFFER_SIZE);
	  if (XGIhw_ext.pSR == NULL) 
	  {
		    printk(KERN_ERR "XGIfb: Fatal error: Allocating SRReg space failed.\n");
		    return -ENODEV;
	  }
	  XGIhw_ext.pSR[0].jIdx = XGIhw_ext.pSR[0].jVal = 0xFF;

	  XGIhw_ext.pCR = vmalloc(sizeof(XGI_DSReg) * CR_BUFFER_SIZE);
	  if (XGIhw_ext.pCR == NULL) 
	  {
	      vfree(XGIhw_ext.pSR);
		    printk(KERN_ERR "XGIfb: Fatal error: Allocating CRReg space failed.\n");
		    return -ENODEV;
	  }
	  XGIhw_ext.pCR[0].jIdx = XGIhw_ext.pCR[0].jVal = 0xFF;



	
	if (!XGIvga_enabled) 
	{
			/* Mapping Max FB Size for 315 Init */
	    XGIhw_ext.pjVideoMemoryAddress = ioremap(xgi_video_info.video_base, 0x10000000);
	    if((xgifb_mode_idx < 0) || ((XGIbios_mode[xgifb_mode_idx].mode_no) != 0xFF)) 
	    { 
#ifdef NOBIOS
		printk("XGIfb: XGIInit() ...");
		
		if(XGIInitNew(&XGIhw_ext))
		{
			printk("OK\n");
		}	
		else
		{
		    printk("Fail\n");
		}
#endif

		outXGIIDXREG(XGISR, IND_XGI_PASSWORD, XGI_PASSWORD);
		iounmap(XGIhw_ext.pjVideoMemoryAddress);	
	
	    }
	}
#ifdef NOBIOS
	else 
	{
	    if((xgifb_mode_idx < 0) || ((XGIbios_mode[xgifb_mode_idx].mode_no) != 0xFF)) 
	    { 
				  
		outXGIIDXREG(XGISR, IND_XGI_PASSWORD, XGI_PASSWORD);
		
		// yilin Because no VBIOS DRAM Sizing, Dram size will error.
		// Set SR13 ,14 temporarily.
		outXGIIDXREG(XGISR, 0x13, 0x45);
		outXGIIDXREG(XGISR, 0x14, 0x31);
            

	    }
	}
#endif
    outXGIIDXREG(XGISR, IND_XGI_PASSWORD, XGI_PASSWORD);
 
    if (xgi_video_info.chip == XG27)
    {
        inXGIIDXREG(XGISR, 0x3B, reg);
        reg = reg & 0x88;
        if ( (reg ==0x80) || (reg ==0x08) )  /* SR3B[7][3]MAA15 MAA11 (Power on Trapping) */
       	{	/*DDR*/
       	    outXGIIDXREG(XGISR, 0x13, 0x31);
       	    outXGIIDXREG(XGISR, 0x14, 0x31);
        }        
        else
       	{  	/*DDRII*/
       	    outXGIIDXREG(XGISR, 0x13, 0x45);
       	    outXGIIDXREG(XGISR, 0x14, 0x51);
       	}
    }
    else if (xgi_video_info.chip == XG21)
    {
        andXGIIDXREG(XGICR, 0xB4, (~0x02));     		/* Independent GPIO control */
     	udelay(800);
     	orXGIIDXREG(XGICR, 0x4A, 0x80);        	/* Enable GPIOH read */
        inXGIIDXREG(XGICR, 0x48, reg);
// HOTPLUG_SUPPORT    
// for current XG20 & XG21, GPIOH is floating, driver will fix DDR temporarily 	
     	if ( reg & 0x01 )						/* DVI read GPIOH */
       	{/*DDRII*/
       	    outXGIIDXREG(XGISR, 0x13, 0x45);
       	    outXGIIDXREG(XGISR, 0x14, 0x51);
       	}
        else
       	{/*DDR*/      	
       	    outXGIIDXREG(XGISR, 0x13, 0x31);
       	    outXGIIDXREG(XGISR, 0x14, 0x31);
       	}
       	orXGIIDXREG(XGICR, 0xB4, 0x02);
    }
	if (XGIfb_get_dram_size()) 
	{
	    vfree(XGIhw_ext.pSR);
	    vfree(XGIhw_ext.pCR);
	    printk(KERN_INFO "XGIfb: Fatal error: Unable to determine RAM size.\n");
	    return -ENODEV;
	}
	


       /* Enable PCI_LINEAR_ADDRESSING and MMIO_ENABLE  */
      orXGIIDXREG(XGISR, IND_XGI_PCI_ADDRESS_SET, (XGI_PCI_ADDR_ENABLE | XGI_MEM_MAP_IO_ENABLE));
        /* Enable 2D accelerator engine */
      orXGIIDXREG(XGISR, IND_XGI_MODULE_ENABLE, XGI_ENABLE_2D);

      while(!request_mem_region(xgi_video_info.video_base, xgi_video_info.video_size, "XGIfb FB"))
      {
          printk("unable request memory size %x",xgi_video_info.video_size);
   
          if(xgi_video_info.video_size < 0x400000)
          {
              printk(KERN_ERR "XGIfb: Fatal error: Unable to reserve frame buffer memory\n"); 
              vfree(XGIhw_ext.pSR);
              vfree(XGIhw_ext.pCR);
              return -ENODEV;
          }
          printk("try again! half size \n ");
          xgi_video_info.video_size = xgi_video_info.video_size >> 1;
      }

	  if (!request_mem_region(xgi_video_info.mmio_base, XGIfb_mmio_size, "XGIfb MMIO")) 
	  {
		    printk(KERN_ERR "XGIfb: Fatal error: Unable to reserve MMIO region\n");
		    release_mem_region(xgi_video_info.video_base, xgi_video_info.video_size);
		    vfree(XGIhw_ext.pSR);
		    vfree(XGIhw_ext.pCR);
		    return -ENODEV;
	  }


    xgi_video_info.video_vbase= ioremap(xgi_video_info.video_base, xgi_video_info.video_size);
    while(!xgi_video_info.video_vbase) /*can't remapping memory*/
    {
        xgi_video_info.video_size = xgi_video_info.video_size >>1;
        xgi_video_info.video_vbase= ioremap(xgi_video_info.video_base, xgi_video_info.video_size);
        if(xgi_video_info.video_size < 0x400000)
        {
            printk("Can't allcation enough memory!!\n");
            return -ENODEV;
        }
    }
    XGIhw_ext.pjVideoMemoryAddress = xgi_video_info.video_vbase;
    XGIhw_ext.ulVideoMemorySize = xgi_video_info.video_size;

	  xgi_video_info.mmio_vbase = ioremap(xgi_video_info.mmio_base, XGIfb_mmio_size);

	  printk(KERN_INFO "XGIfb: Framebuffer at 0x%llx, mapped to 0x%p, size %dk\n",
	      xgi_video_info.video_base, xgi_video_info.video_vbase,xgi_video_info.video_size / 1024);

	  printk(KERN_INFO "XGIfb: MMIO at 0x%llx, mapped to 0x%p, size %ldk\n",
	      xgi_video_info.mmio_base, xgi_video_info.mmio_vbase,XGIfb_mmio_size / 1024);

          printk("XGIfb: XGIInitNew() ...");		  
	  if(XGIInitNew(&XGIhw_ext))
	  {
		  printk("OK\n");
	  }	
	  else
	  {
		printk("Fail\n");
	  }

	  if(XGIfb_heap_init()) 
	  {
		    printk(KERN_WARNING "XGIfb: Failed to initialize offscreen memory heap\n");
	  }


	  xgi_video_info.mtrr = (unsigned int) 0;

	  if((xgifb_mode_idx < 0) || ((XGIbios_mode[xgifb_mode_idx].mode_no) != 0xFF)) 
	  { 
	      xgi_video_info.hasVB = HASVB_NONE;            
        if((xgi_video_info.chip == XG20)||(xgi_video_info.chip == XG21)||(xgi_video_info.chip == XG27))
        {
            xgi_video_info.hasVB = HASVB_NONE;            
        }else
		    XGIfb_get_VB_type();

		    XGIhw_ext.ujVBChipID = VB_CHIP_UNKNOWN;

		    XGIhw_ext.ulExternalChip = 0;

		    switch (xgi_video_info.hasVB) {
		    case HASVB_301:
		        inXGIIDXREG(XGIPART4, 0x01, reg);
			    if (reg >= 0xE0) {
				    XGIhw_ext.ujVBChipID = VB_CHIP_302LV;
				    printk(KERN_INFO "XGIfb: XGI302LV bridge detected (revision 0x%02x)\n",reg);
	  		    } else if (reg >= 0xD0) 
	  		    {
				    XGIhw_ext.ujVBChipID = VB_CHIP_301LV;
				    printk(KERN_INFO "XGIfb: XGI301LV bridge detected (revision 0x%02x)\n",reg);
	  		    } 
			    else {
 				    XGIhw_ext.ujVBChipID = VB_CHIP_301;
				    printk("XGIfb: XGI301 bridge detected\n");
			    }
			    break;
		    case HASVB_302:
		        inXGIIDXREG(XGIPART4, 0x01, reg);
			    if (reg >= 0xE0) {
				    XGIhw_ext.ujVBChipID = VB_CHIP_302LV;
				    printk(KERN_INFO "XGIfb: XGI302LV bridge detected (revision 0x%02x)\n",reg);
	  		    } else if (reg >= 0xD0) {
				    XGIhw_ext.ujVBChipID = VB_CHIP_301LV;
				    printk(KERN_INFO "XGIfb: XGI302LV bridge detected (revision 0x%02x)\n",reg);
	  		    } else if (reg >= 0xB0) {
				    inXGIIDXREG(XGIPART4,0x23,reg1);
			        XGIhw_ext.ujVBChipID = VB_CHIP_302B;
			    } else {
			        XGIhw_ext.ujVBChipID = VB_CHIP_302;
				printk(KERN_INFO "XGIfb: XGI302 bridge detected\n");
			    }
			    break;
		    case HASVB_LVDS:
			    XGIhw_ext.ulExternalChip = 0x1;
			    printk(KERN_INFO "XGIfb: LVDS transmitter detected\n");
 			    break;
 		    default:
			//printk(KERN_INFO "XGIfb: No or unknown bridge type detected\n");
			    break;
		}

		if (xgi_video_info.hasVB != HASVB_NONE) {
		    XGIfb_detect_VB();
        }

		if (xgi_video_info.disp_state & DISPTYPE_DISP2) {
			if (XGIfb_crt1off)
				xgi_video_info.disp_state |= DISPMODE_SINGLE;
			else
				xgi_video_info.disp_state |= (DISPMODE_MIRROR | DISPTYPE_CRT1);
		} else {
			xgi_video_info.disp_state = DISPMODE_SINGLE | DISPTYPE_CRT1;
		}

		if (xgi_video_info.disp_state & DISPTYPE_LCD) {
		    if (!enable_dstn) {
		        inXGIIDXREG(XGICR, IND_XGI_LCD_PANEL, reg);
			    reg &= 0x0f;
			    XGIhw_ext.ulCRT2LCDType = XGI310paneltype[reg];
			
		    } else {
		        // TW: FSTN/DSTN 
			XGIhw_ext.ulCRT2LCDType = LCD_320x480;
		    }
		}
		
		XGIfb_detectedpdc = 0;
	    XGIfb_detectedlcda = 0xff;
#ifndef NOBIOS

                /* TW: Try to find about LCDA */
		
        if((XGIhw_ext.ujVBChipID == VB_CHIP_302B) ||
	       (XGIhw_ext.ujVBChipID == VB_CHIP_301LV) ||
	       (XGIhw_ext.ujVBChipID == VB_CHIP_302LV)) 
	    {
	       int tmp;
	       inXGIIDXREG(XGICR,0x34,tmp);
	       if(tmp <= 0x13) 
	       {	
	          // Currently on LCDA? (Some BIOSes leave CR38) 
	          inXGIIDXREG(XGICR,0x38,tmp);
		      if((tmp & 0x03) == 0x03) 
		      {
//		          XGI_Pr.XGI_UseLCDA = TRUE;
		      }else
		      {
		     //  Currently on LCDA? (Some newer BIOSes set D0 in CR35) 
		         inXGIIDXREG(XGICR,0x35,tmp);
		         if(tmp & 0x01) 
		         {
//		              XGI_Pr.XGI_UseLCDA = TRUE;
		           }else
		           {
		               inXGIIDXREG(XGICR,0x30,tmp);
		               if(tmp & 0x20) 
		               {
		                   inXGIIDXREG(XGIPART1,0x13,tmp);
			               if(tmp & 0x04) 
			               {
//			                XGI_Pr.XGI_UseLCDA = TRUE;
			               }
		               }
		           }
		        }
	         } 

        }
		

#endif

		if (xgifb_mode_idx >= 0)
			xgifb_mode_idx = XGIfb_validate_mode(xgifb_mode_idx);

		if (xgifb_mode_idx < 0) {
			switch (xgi_video_info.disp_state & DISPTYPE_DISP2) {
			   case DISPTYPE_LCD:
				xgifb_mode_idx = DEFAULT_LCDMODE;
				break;
			   case DISPTYPE_TV:
				xgifb_mode_idx = DEFAULT_TVMODE;
				break;
			   default:
				xgifb_mode_idx = DEFAULT_MODE;
				break;
			}
		}

		XGIfb_mode_no = XGIbios_mode[xgifb_mode_idx].mode_no;


                if( xgi_video_info.refresh_rate == 0)
		    xgi_video_info.refresh_rate = 60; /*yilin set default refresh rate */
	        if(XGIfb_search_refresh_rate(xgi_video_info.refresh_rate) == 0)
                {
		    XGIfb_rate_idx = XGIbios_mode[xgifb_mode_idx].rate_idx;
		    xgi_video_info.refresh_rate = 60;
	        }

		xgi_video_info.video_bpp = XGIbios_mode[xgifb_mode_idx].bpp;
		xgi_video_info.video_vwidth = xgi_video_info.video_width = XGIbios_mode[xgifb_mode_idx].xres;
		xgi_video_info.video_vheight = xgi_video_info.video_height = XGIbios_mode[xgifb_mode_idx].yres;
		xgi_video_info.org_x = xgi_video_info.org_y = 0;
		xgi_video_info.video_linelength = xgi_video_info.video_width * (xgi_video_info.video_bpp >> 3);
		switch(xgi_video_info.video_bpp) {
        	case 8:
            		xgi_video_info.DstColor = 0x0000;
	    		xgi_video_info.XGI310_AccelDepth = 0x00000000;
			xgi_video_info.video_cmap_len = 256;
            		break;
        	case 16:
            		xgi_video_info.DstColor = 0x8000;
            		xgi_video_info.XGI310_AccelDepth = 0x00010000;
			xgi_video_info.video_cmap_len = 16;
            		break;
        	case 32:
            		xgi_video_info.DstColor = 0xC000;
	    		xgi_video_info.XGI310_AccelDepth = 0x00020000;
			xgi_video_info.video_cmap_len = 16;
            		break;
		default:
			xgi_video_info.video_cmap_len = 16;
		        printk(KERN_INFO "XGIfb: Unsupported depth %d", xgi_video_info.video_bpp);
			break;
    		}
		


		printk(KERN_INFO "XGIfb: Default mode is %dx%dx%d (%dHz)\n",
	       		xgi_video_info.video_width, xgi_video_info.video_height, xgi_video_info.video_bpp,
			xgi_video_info.refresh_rate);
			
		default_var.xres = default_var.xres_virtual = xgi_video_info.video_width;
		default_var.yres = default_var.yres_virtual = xgi_video_info.video_height;
		default_var.bits_per_pixel = xgi_video_info.video_bpp;
		
		XGIfb_bpp_to_var(&default_var);
		
		default_var.pixclock = (u32) (1000000000 /
				XGIfb_mode_rate_to_dclock(&XGI_Pr, &XGIhw_ext,
						XGIfb_mode_no, XGIfb_rate_idx));

		if(XGIfb_mode_rate_to_ddata(&XGI_Pr, &XGIhw_ext,
			 XGIfb_mode_no, XGIfb_rate_idx,
			 (ULONG*) &default_var.left_margin, (ULONG*)&default_var.right_margin, 
			 (ULONG*) &default_var.upper_margin,(ULONG*) &default_var.lower_margin,
			 (ULONG*) &default_var.hsync_len, (ULONG*) &default_var.vsync_len,
			 (ULONG*) &default_var.sync, (ULONG*) &default_var.vmode)) {

		   if((default_var.vmode & FB_VMODE_MASK) == FB_VMODE_INTERLACED) {
		      default_var.yres <<= 1;
		      default_var.yres_virtual <<= 1;
		   } else if((default_var.vmode	& FB_VMODE_MASK) == FB_VMODE_DOUBLE) {
		      default_var.pixclock >>= 1;
		      default_var.yres >>= 1;
		      default_var.yres_virtual >>= 1;
		   }
		   
	        }


		/* panto */
//		default_var.yres_virtual = xgi_video_info.heapstart / (default_var.xres * (default_var.bits_per_pixel >> 3));
		if (default_var.yres_virtual <= default_var.yres)
	        		default_var.yres_virtual = default_var.yres;

//        if( (default_var.yres * 2) < default_var.yres_virtual ) //yilin to make the yres_virtual =  yres* 2;
//	        default_var.yres_virtual = default_var.yres * 2;	  
		xgi_video_info.accel = 0;
		if(XGIfb_accel) {
		   xgi_video_info.accel = -1;
		   default_var.accel_flags |= FB_ACCELF_TEXT;
		   XGIfb_initaccel();
		}

		fb_info->flags = FBINFO_FLAG_DEFAULT;
		fb_info->var = default_var;
		fb_info->fix = XGIfb_fix;
		fb_info->par = &xgi_video_info;
		fb_info->screen_base = xgi_video_info.video_vbase;
		fb_info->fbops = &XGIfb_ops;
		XGIfb_get_fix(&fb_info->fix, -1, fb_info);
		fb_info->pseudo_palette = pseudo_palette;
		
		fb_alloc_cmap(&fb_info->cmap, 256 , 0);


#ifdef CONFIG_MTRR
		xgi_video_info.mtrr = mtrr_add((unsigned int) xgi_video_info.video_base,
				(unsigned int) xgi_video_info.video_size,
				MTRR_TYPE_WRCOMB, 1);
		if(xgi_video_info.mtrr) {
			printk(KERN_INFO "XGIfb: Added MTRRs\n");
		}
#endif

		if(register_framebuffer(fb_info) < 0)
        {
			return -EINVAL;
        }
			
		XGIfb_registered = 1;			

		printk(KERN_INFO "XGIfb: Installed XGIFB_GET_INFO ioctl (%x)\n", XGIFB_GET_INFO);
		
/*		printk(KERN_INFO "XGIfb: 2D acceleration is %s, scrolling mode %s\n",
		     XGIfb_accel ? "enabled" : "disabled",
		     XGIfb_ypan  ? "ypan" : "redraw");
*/
		printk(KERN_INFO "fb%d: %s frame buffer device, Version %d.%d.%02d\n",
	       		fb_info->node, myid, VER_MAJOR, VER_MINOR, VER_LEVEL);			     


	}

//	dumpVGAReg();

	return 0;
}


/*****************************************************/
/*                PCI DEVICE HANDLING                */
/*****************************************************/

static void __devexit xgifb_remove(struct pci_dev *pdev)
{
	/* Unregister the framebuffer */
//	if(xgi_video_info.registered) {
		unregister_framebuffer(fb_info);
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,3))
		framebuffer_release(fb_info);
#else
		kfree(fb_info);
#endif
//	}

	pci_set_drvdata(pdev, NULL);

};

static struct pci_driver xgifb_driver = {
	.name		= "xgifb",
	.id_table 	= xgifb_pci_table,
	.probe 		= xgifb_probe,
	.remove 	= __devexit_p(xgifb_remove)
};

XGIINITSTATIC int __init xgifb_init(void)
{
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,8)
#ifndef MODULE
	char *option = NULL;

	if (fb_get_options("Xgifb", &option))
		return -ENODEV;
	XGIfb_setup(option);
#endif
#endif
	return(pci_register_driver(&xgifb_driver));
}

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,8)
#ifndef MODULE
module_init(xgifb_init);
#endif
#endif

/*****************************************************/
/*                      MODULE                       */
/*****************************************************/

#ifdef MODULE

static char         *mode = NULL;
static int          vesa = 0;
static unsigned int rate = 0;
static unsigned int crt1off = 1;
static unsigned int mem = 0;
static char         *forcecrt2type = NULL;
static int          forcecrt1 = -1;
static int          pdc = -1;
static int          pdc1 = -1;
static int          noaccel = -1;
static int          noypan  = -1;
static int	    nomax = -1;
static int          userom = 1;
static int          useoem = -1;
static char         *tvstandard = NULL;
static int	    nocrt2rate = 0;
static int          scalelcd = -1;
static char	    *specialtiming = NULL;
static int	    lvdshl = -1;
static int	    tvxposoffset = 0, tvyposoffset = 0;
#if !defined(__i386__) && !defined(__x86_64__)
static int	    resetcard = 0;
static int	    videoram = 0;
#endif

MODULE_DESCRIPTION("Z7 Z9 Z9S Z11 framebuffer device driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("XGITECH , Others");



module_param(mem, int, 0);
module_param(noaccel, int, 0);
module_param(noypan, int, 0);
module_param(nomax, int, 0);
module_param(userom, int, 0);
module_param(useoem, int, 0);
module_param(mode, charp, 0);
module_param(vesa, int, 0);
module_param(rate, int, 0);
module_param(forcecrt1, int, 0);
module_param(forcecrt2type, charp, 0);
module_param(scalelcd, int, 0);
module_param(pdc, int, 0);
module_param(pdc1, int, 0);
module_param(specialtiming, charp, 0);
module_param(lvdshl, int, 0);
module_param(tvstandard, charp, 0);
module_param(tvxposoffset, int, 0);
module_param(tvyposoffset, int, 0);
module_param(filter, int, 0);
module_param(nocrt2rate, int, 0);
#if !defined(__i386__) && !defined(__x86_64__)
module_param(resetcard, int, 0);
module_param(videoram, int, 0);
#endif


MODULE_PARM_DESC(mem,
	"\nDetermines the beginning of the video memory heap in KB. This heap is used\n"
	  "for video RAM management for eg. DRM/DRI. On 300 series, the default depends\n"
	  "on the amount of video RAM available. If 8MB of video RAM or less is available,\n"
	  "the heap starts at 4096KB, if between 8 and 16MB are available at 8192KB,\n"
	  "otherwise at 12288KB. On 315 and Xabre series, the heap size is 32KB by default.\n"
	  "The value is to be specified without 'KB' and must match the MaxXFBMem setting\n"
	  "for XFree86 4.x/X.org 6.7 and later.\n");

MODULE_PARM_DESC(noaccel,
        "\nIf set to anything other than 0, 2D acceleration will be disabled.\n"
	  "(default: 0)\n");

MODULE_PARM_DESC(noypan,
        "\nIf set to anything other than 0, y-panning will be disabled and scrolling\n"
 	  "will be performed by redrawing the screen. (default: 0)\n");

MODULE_PARM_DESC(nomax,
        "\nIf y-panning is enabled, xgifb will by default use the entire available video\n"
	  "memory for the virtual screen in order to optimize scrolling performance. If\n"
	  "this is set to anything other than 0, xgifb will not do this and thereby \n"
	  "enable the user to positively specify a virtual Y size of the screen using\n"
	  "fbset. (default: 0)\n");



MODULE_PARM_DESC(mode,
       "\nSelects the desired default display mode in the format XxYxDepth,\n"
         "eg. 1024x768x16. Other formats supported include XxY-Depth and\n"
	 "XxY-Depth@Rate. If the parameter is only one (decimal or hexadecimal)\n"
	 "number, it will be interpreted as a VESA mode number. (default: 800x600x8)\n");

MODULE_PARM_DESC(vesa,
       "\nSelects the desired default display mode by VESA defined mode number, eg.\n"
         "0x117 (default: 0x0103)\n");


MODULE_PARM_DESC(rate,
	"\nSelects the desired vertical refresh rate for CRT1 (external VGA) in Hz.\n"
	  "If the mode is specified in the format XxY-Depth@Rate, this parameter\n"
	  "will be ignored (default: 60)\n");

MODULE_PARM_DESC(forcecrt1,
	"\nNormally, the driver autodetects whether or not CRT1 (external VGA) is \n"
	  "connected. With this option, the detection can be overridden (1=CRT1 ON,\n"
	  "0=CRT1 OFF) (default: [autodetected])\n");

MODULE_PARM_DESC(forcecrt2type,
	"\nIf this option is omitted, the driver autodetects CRT2 output devices, such as\n"
	  "LCD, TV or secondary VGA. With this option, this autodetection can be\n"
	  "overridden. Possible parameters are LCD, TV, VGA or NONE. NONE disables CRT2.\n"
	  "On systems with a SiS video bridge, parameters SVIDEO, COMPOSITE or SCART can\n"
	  "be used instead of TV to override the TV detection. Furthermore, on systems\n"
	  "with a SiS video bridge, SVIDEO+COMPOSITE, HIVISION, YPBPR480I, YPBPR480P,\n"
	  "YPBPR720P and YPBPR1080I are understood. However, whether or not these work\n"
	  "depends on the very hardware in use. (default: [autodetected])\n");

MODULE_PARM_DESC(scalelcd,
	"\nSetting this to 1 will force the driver to scale the LCD image to the panel's\n"
	  "native resolution. Setting it to 0 will disable scaling; LVDS panels will\n"
	  "show black bars around the image, TMDS panels will probably do the scaling\n"
	  "themselves. Default: 1 on LVDS panels, 0 on TMDS panels\n");

MODULE_PARM_DESC(pdc,
        "\nThis is for manually selecting the LCD panel delay compensation. The driver\n"
	  "should detect this correctly in most cases; however, sometimes this is not\n"
	  "possible. If you see 'small waves' on the LCD, try setting this to 4, 32 or 24\n"
	  "on a 300 series chipset; 6 on a 315 series chipset. If the problem persists,\n"
	  "try other values (on 300 series: between 4 and 60 in steps of 4; on 315 series:\n"
	  "any value from 0 to 31). (default: autodetected, if LCD is active during start)\n");

MODULE_PARM_DESC(pdc1,
        "\nThis is same as pdc, but for LCD-via CRT1. Hence, this is for the 315/330\n"
	  "series only. (default: autodetected if LCD is in LCD-via-CRT1 mode during\n"
	  "startup) - Note: currently, this has no effect because LCD-via-CRT1 is not\n"
	  "implemented yet.\n");

MODULE_PARM_DESC(specialtiming,
	"\nPlease refer to documentation for more information on this option.\n");

MODULE_PARM_DESC(lvdshl,
	"\nPlease refer to documentation for more information on this option.\n");

MODULE_PARM_DESC(tvstandard,
	"\nThis allows overriding the BIOS default for the TV standard. Valid choices are\n"
	  "pal, ntsc, palm and paln. (default: [auto; pal or ntsc only])\n");

MODULE_PARM_DESC(tvxposoffset,
	"\nRelocate TV output horizontally. Possible parameters: -32 through 32.\n"
	  "Default: 0\n");

MODULE_PARM_DESC(tvyposoffset,
	"\nRelocate TV output vertically. Possible parameters: -32 through 32.\n"
	  "Default: 0\n");

MODULE_PARM_DESC(filter,
	"\nSelects TV flicker filter type (only for systems with a SiS301 video bridge).\n"
	  "(Possible values 0-7, default: [no filter])\n");

MODULE_PARM_DESC(nocrt2rate,
	"\nSetting this to 1 will force the driver to use the default refresh rate for\n"
	  "CRT2 if CRT2 type is VGA. (default: 0, use same rate as CRT1)\n");




int __init xgifb_init_module(void)
{
    printk("\nXGIfb_init_module");
	if(mode)
		XGIfb_search_mode(mode);
	else if (vesa != -1)
		XGIfb_search_vesamode(vesa);

    return(xgifb_init());
}

static void __exit xgifb_remove_module(void)
{
	pci_unregister_driver(&xgifb_driver);
	printk(KERN_DEBUG "xgifb: Module unloaded\n");
}

module_init(xgifb_init_module);
module_exit(xgifb_remove_module);

#endif 	   /*  /MODULE  */

EXPORT_SYMBOL(XGI_malloc);
EXPORT_SYMBOL(XGI_free);
                                                      
