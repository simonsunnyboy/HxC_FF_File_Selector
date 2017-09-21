/*
//
// Copyright (C) 2009-2017 Jean-Fran�ois DEL NERO
//
// This file is part of the HxCFloppyEmulator file selector.
//
// HxCFloppyEmulator file selector may be used and distributed without restriction
// provided that this copyright statement is not removed from the file and that any
// derivative work contains the original copyright notice and the associated
// disclaimer.
//
// HxCFloppyEmulator file selector is free software; you can redistribute it
// and/or modify  it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// HxCFloppyEmulator file selector is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
//   See the GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with HxCFloppyEmulator file selector; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
//
*/

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "keysfunc_defs.h"

#include "cfg_file.h"

#include "hxcfeda.h"

#include "fat_opts.h"
#include "fat_misc.h"
#include "fat_defs.h"
#include "fat_filelib.h"

#include "msg_txt.h"

#include "version.h"

#include "conf.h"
#include "ui_context.h"

#include "gui_utils.h"

#include "fectrl.h"
#include "config_file.h"

#include "media_access.h"

#include "menu.h"

#include "menu_settings.h"
#include "menu_selectdrive.h"
#include "menu_commands.h"

#include "hardware.h"


// Slots buffer
disk_in_drive_v2 disks_slots[MAX_NUMBER_OF_SLOT];

static disk_in_drive_v2_long DirectoryEntry_tab[40];

// File listing pages directory offset buffer
FL_DIR file_list_status;
FL_DIR file_list_status_tab[MAX_PAGES_PER_DIRECTORY];

static struct fs_dir_ent dir_entry;

ui_context g_ui_ctx;

int check_version(direct_access_status_sector * dass)
{
	int i,count;

	i = 0;
	count = 0;

	if(dass->FIRMWAREVERSION[0] != 'v' && dass->FIRMWAREVERSION[0] != 'V')
	{
		return 0;
	}

	while(dass->FIRMWAREVERSION[i] && i < sizeof(dass->FIRMWAREVERSION))
	{
		if(dass->FIRMWAREVERSION[i] == '.')
			count++;

		i++;
	}

	if(count == 3)
		return 1;

	return 0;
}

void clear_list(ui_context * ctx,unsigned char add)
{
	unsigned char y_pos,i;

	y_pos = FILELIST_Y_POS;
	for(i=0;i<ctx->NUMBER_OF_FILE_ON_DISPLAY+add;i++)
	{
		clear_line(ctx,y_pos,0);
		y_pos=y_pos+8;
	}
}

void displayFolder(ui_context * ctx)
{
	int i;
	hxc_print(ctx,LEFT_ALIGNED,0,CURDIR_Y_POS,(char*)cur_folder_msg);

	for(i=15*8;i<ctx->SCREEN_XRESOL;i=i+8)
		hxc_print(ctx,LEFT_ALIGNED,(uint16_t)i,(uint16_t)CURDIR_Y_POS," ");

	if(strlen(ctx->currentPath)<32)
		hxc_printf(ctx,LEFT_ALIGNED,15*8,CURDIR_Y_POS,"%s",ctx->currentPath);
	else
		hxc_printf(ctx,LEFT_ALIGNED,15*8,CURDIR_Y_POS,"...%s    ",&ctx->currentPath[strlen(ctx->currentPath)-32]);
}

void enter_sub_dir(ui_context * ctx,disk_in_drive_v2_long *disk_ptr)
{
	int currentPathLength;
	char folder[128+1];
	unsigned char c;
	int i;
	int old_index;

	old_index = strlen( ctx->currentPath );

	if ( !strncmp(disk_ptr->name,"..", 2) )
	{
		currentPathLength = strlen( ctx->currentPath ) - 1;
		do
		{
			ctx->currentPath[ currentPathLength ] = 0;
			currentPathLength--;
		}while ( ctx->currentPath[ currentPathLength ] != (unsigned char)'/' );
	}
	else
	{
		if((disk_ptr->name[0] != (unsigned char)'.'))
		{
			for (i=0; i < 128; i++ )
			{
				c = disk_ptr->name[i];

				if ( ( c >= (32+0) ) && (c <= 127) )
				{
					folder[i] = c;
				}
				else
				{
					folder[i] = 0;
					i = 128;
				}
			}

			currentPathLength = strlen( ctx->currentPath );

			if( ctx->currentPath[ currentPathLength-1] != '/')
			{
				strcat( ctx->currentPath, "/" );
			}

			strcat( ctx->currentPath, folder );
		}
	}

	displayFolder(ctx);

	ctx->selectorpos=0;

	if(!fl_opendir(ctx->currentPath, &file_list_status))
	{
		ctx->currentPath[old_index]=0;
		fl_opendir(ctx->currentPath, &file_list_status);
		displayFolder(ctx);
	}

	for(i=0;i<MAX_PAGES_PER_DIRECTORY;i++)
	{
		memcpy(&file_list_status_tab[i],&file_list_status ,sizeof(FL_DIR));
	}

 	clear_list(ctx,0);
	ctx->read_entry=1;
}

void show_all_slots(ui_context * ctx,int drive)
{
	char tmp_str[81];
	disk_in_drive_v2 * drive_slots_ptr;
	unsigned short i,xoffset,slotnumber;

	#ifdef DEBUG
	dbg_printf("show_all_slots : %d (nb of drive : %d)\n",drive,ctx->number_of_drive);
	#endif

	clear_list(ctx,0);

	if( drive >= ctx->number_of_drive )
		return;

	hxc_printf(ctx,CENTER_ALIGNED,0,FILELIST_Y_POS,"--- Drive %c slots selection ---",'A'+drive);

	for ( i = 1; i < ctx->NUMBER_OF_FILE_ON_DISPLAY; i++ )
	{
		slotnumber = i + (ctx->slotselectorpage * (ctx->NUMBER_OF_FILE_ON_DISPLAY-1));

		if( slotnumber < ctx->config_file_number_max_of_slot)
		{
			memset(tmp_str,0,sizeof(tmp_str));

			drive_slots_ptr = &disks_slots[ (slotnumber*ctx->number_of_drive) + drive];
			if( drive_slots_ptr->size )
			{
				memcpy(tmp_str,&drive_slots_ptr->name,MAX_SHORT_NAME_LENGHT);
			}

			if( slotnumber > 99 )
				xoffset = 0;
			else
			{
				if( slotnumber > 9 )
				{
					xoffset = 8;
					hxc_printf(ctx,LEFT_ALIGNED,0,(uint16_t)(FILELIST_Y_POS + (i*8)),"0");
				}
				else
				{
					xoffset = 16;
					hxc_printf(ctx,LEFT_ALIGNED,0,(uint16_t)(FILELIST_Y_POS + (i*8)),"00");
				}
			}

			hxc_printf(ctx,LEFT_ALIGNED,xoffset,(uint16_t)(FILELIST_Y_POS + (i*8)),"%d:%s", slotnumber, tmp_str);
		}
	}
}

int getext(char * path,char * exttodest)
{
	int i;

	i = 0;
	while(path[i] && i<256)
	{
		i++;
	}

	while( i && path[i]!='.')
	{
		i--;
	}

	if(path[i]=='.')
	{
		i++;
		exttodest[0] = path[i];
		exttodest[1] = path[i+1];
		exttodest[2] = path[i+2];

		// Remove trailing space
		i = 2;
		while(i>=0 && exttodest[i]==' ')
		{
			exttodest[i] = 0;
			i--;
		}
	}

	return 0;
}

void print_help(ui_context * ctx)
{
	int i;
	int key;

	i = 0;

	key = FCT_NO_FUNCTION;

	while(help_pages[i].txt && key!=FCT_ESCAPE)
	{
		clear_list(ctx,0);

		hxc_print(ctx,help_pages[i].align,0,HELP_Y_POS, (char*)help_pages[i].txt);

		do
		{
			key = wait_function_key();
		}while(key!=FCT_SELECT_FILE_DRIVEA && key!=FCT_ESCAPE);

		i++;
	}
}

void ui_save(ui_context * ctx,int preselected_slot)
{
	hxc_printf_box(ctx,(char*)save_msg);
	save_cfg_file(ctx,cfgfile_header,preselected_slot);
}

void ui_reboot(ui_context * ctx)
{
	hxc_printf_box(ctx,(char*)reboot_msg);
	sleep(1);
	jumptotrack(0);
	reboot();
}

void ui_savereboot(ui_context * ctx,int preselected_slot)
{
	ui_save(ctx,preselected_slot);
	ui_reboot(ctx);
}

void ui_chgcolor(ui_context * ctx,int color)
{
	ctx->colormode = color;
	set_color_scheme(color);
	setcfg_backgroundcolor(color);
	waitms(100);
}
			
int process_extra_functions(ui_context * ctx, unsigned char key)
{
	int refresh;

	refresh = 0;

	switch(key)
	{
		case FCT_HELP:
			print_help(ctx);
			refresh = 1;
		break;

		case FCT_SAVEREBOOT:
			ui_savereboot(ctx,-1);
		break;

		case FCT_SAVE:
			ui_save(ctx,-1);
			refresh = 1;
		break;

		case FCT_REBOOT:
			ui_reboot(ctx);
		break;

		case FCT_EMUCFG:
			enter_menu(ctx,settings_menu);
			refresh = 1;
		break;

		case FCT_CHGCOLOR:
			ui_chgcolor(ctx,ctx->colormode+1);
		break;
	}

	return refresh;
}

int ui_slots_menu(ui_context * ctx)
{
	unsigned char key;
	int slot,i;

	#ifdef DEBUG

	dbg_printf("enter ui_slots_menu\n");
	#endif

	////////////////////
	// Slots list menu

	show_all_slots(ctx,ctx->page_mode_index);

	invert_line(ctx,0,FILELIST_Y_POS+(ctx->slotselectorpos*8));

	do
	{
		key=wait_function_key();

		switch(key)
		{
			case FCT_UP_KEY: // UP
				if(ctx->slotselectorpos)
				{
					ctx->slotselectorpos--;
					invert_line(ctx,0,FILELIST_Y_POS+((ctx->slotselectorpos+1)*8));
					invert_line(ctx,0,FILELIST_Y_POS+((ctx->slotselectorpos)*8));
				}
				else
				{
					if(ctx->slotselectorpage)
					{
						ctx->slotselectorpos = (ctx->NUMBER_OF_FILE_ON_DISPLAY-1);
						ctx->slotselectorpage--;
					}

					show_all_slots(ctx,ctx->page_mode_index);
					invert_line(ctx,0,FILELIST_Y_POS+(ctx->slotselectorpos*8));
				}
			break;
			case FCT_DOWN_KEY: // Down

				if(ctx->slotselectorpos + (ctx->slotselectorpage * (ctx->NUMBER_OF_FILE_ON_DISPLAY-1)) < ctx->config_file_number_max_of_slot )
				{
					ctx->slotselectorpos++;
					if(ctx->slotselectorpos>(ctx->NUMBER_OF_FILE_ON_DISPLAY-1))
					{
						ctx->slotselectorpos = 1;
						ctx->slotselectorpage++;

						show_all_slots(ctx,ctx->page_mode_index);
						invert_line(ctx,0,FILELIST_Y_POS+(ctx->slotselectorpos*8));
					}
					else
					{
						invert_line(ctx,0,FILELIST_Y_POS+((ctx->slotselectorpos-1)*8));
						invert_line(ctx,0,FILELIST_Y_POS+(ctx->slotselectorpos*8));
					}
				}
			break;

			case FCT_RIGHT_KEY: // Right
				if(ctx->slotselectorpos + ((ctx->slotselectorpage+1) * (ctx->NUMBER_OF_FILE_ON_DISPLAY-1)) < ctx->config_file_number_max_of_slot )
				{
					ctx->slotselectorpage++;
				}

				show_all_slots(ctx,ctx->page_mode_index);
				invert_line(ctx,0,FILELIST_Y_POS+(ctx->slotselectorpos*8));
			break;

			case FCT_LEFT_KEY:
				if(ctx->slotselectorpage)
					ctx->slotselectorpage--;

				show_all_slots(ctx,ctx->page_mode_index);
				invert_line(ctx,0,FILELIST_Y_POS+(ctx->slotselectorpos*8));
			break;

			case FCT_CLEARSLOT:
				slot = (ctx->slotselectorpos + (ctx->slotselectorpage * (ctx->NUMBER_OF_FILE_ON_DISPLAY-1)));

				memset((void*)&disks_slots[(slot*ctx->number_of_drive) + ctx->page_mode_index ],0,sizeof(disk_in_drive_v2));

				i = 0;
				while( i < ctx->number_of_drive && !disks_slots[(slot*ctx->number_of_drive)+i].type[0] )
				{
					i++;
				}

				ctx->change_map[slot>>3] |= (0x80 >> (slot&7));

				// All drive empty - clear the slot
				if( i == ctx->number_of_drive )
				{
					ctx->slot_map[slot>>3] &= ~(0x80 >> (slot&7));
				}

				show_all_slots(ctx,ctx->page_mode_index);
				invert_line(ctx,0,FILELIST_Y_POS+(ctx->slotselectorpos*8));
			break;

			case FCT_SELECTSAVEREBOOT:

				slot = (ctx->slotselectorpos + (ctx->slotselectorpage * (ctx->NUMBER_OF_FILE_ON_DISPLAY-1)));
				if( ctx->slot_map[slot>>3] & (0x80 >> (slot&7)) )
				{
					ui_savereboot(ctx,slot);
				}

			break;
			case FCT_SELECT_FILE_DRIVEA:
				if(!ctx->slotselectorpos)
				{
					ctx->page_mode_index++;

					if(ctx->page_mode_index >= 2 + ctx->number_of_drive)
						ctx->page_mode_index = 0;
				}
			break;
			default:
				if(process_extra_functions(ctx, key))
				{
					show_all_slots(ctx,ctx->page_mode_index);
					invert_line(ctx,0,FILELIST_Y_POS+(ctx->slotselectorpos*8));
				}
			break;
		}
	}while(key != FCT_SELECT_FILE_DRIVEA && key != FCT_ESCAPE );

	if( key == FCT_SELECT_FILE_DRIVEA && !ctx->slotselectorpos )
	{
		clear_list(ctx,0);

		#ifdef DEBUG
		dbg_printf("leave ui_slots_menu\n");
		#endif
		return 0;
	}

	if( ( key != FCT_ESCAPE ) && ( ctx->page_mode_index < ctx->number_of_drive ) )
	{
		slot = (ctx->slotselectorpos + (ctx->slotselectorpage * (ctx->NUMBER_OF_FILE_ON_DISPLAY-1)));

		#ifdef DEBUG
		dbg_printf("set slot %d (page %d, pos %d)\n",slot,ctx->slotselectorpage,ctx->slotselectorpos);
		#endif

		memcpy( (void*)&disks_slots[ (slot*ctx->number_of_drive) + ( ctx->page_mode_index ) ],
				(void*)&DirectoryEntry_tab[ ctx->selectorpos ],
				sizeof(disk_in_drive_v2)
				);

		ctx->slot_map[slot>>3] |= (0x80 >> (slot&7));
		ctx->change_map[slot>>3] |= (0x80 >> (slot&7));
	}

	clear_list(ctx,0);

	#ifdef DEBUG
	dbg_printf("leave ui_slots_menu\n");
	#endif
	return 1;
}

void ui_mainfileselector(ui_context * ctx)
{
	int ret;
	short i,j,y_pos;
	unsigned char displayentry;
	unsigned char entrytype_icon;
	unsigned char key,c;
	unsigned char last_file;
	disk_in_drive_v2_long * disk_ptr;

	y_pos=FILELIST_Y_POS;

	#ifdef DEBUG
	dbg_printf("enter ui_mainfileselector\n");
	#endif

	ctx->page_mode_index = ( 2 + ctx->number_of_drive ) - 1;

	#ifdef DEBUG
	dbg_printf("page_mode_index : %d\n",ctx->page_mode_index);
	#endif

	clear_list(ctx,0);
	for(;;)
	{
		#ifdef DEBUG
		dbg_printf("Page : %d Selector pos : %d\n",ctx->page_number,ctx->selectorpos);
		#endif

		i=0;
		do
		{
			memset(&DirectoryEntry_tab[i],0,sizeof(disk_in_drive_v2_long));
			i++;
		}while((i<ctx->NUMBER_OF_FILE_ON_DISPLAY));

		last_file=0x00;

		y_pos = FILELIST_Y_POS;
		hxc_printf(ctx,CENTER_ALIGNED,0,y_pos,"--- SD/USB Media files ---");
		y_pos += 8;
		i = 1;
		do
		{
			displayentry=0xFF;
			if(!fl_readdir(&file_list_status, &dir_entry))
			{
				if(ctx->filtermode)
				{
					strlwr(dir_entry.filename);

					if(!strstr(dir_entry.filename,ctx->filter))
					{
						displayentry=0x00;
					}
				}

				if(displayentry)
				{
					// Get the file name extension.
					getext(dir_entry.filename,(char*)&DirectoryEntry_tab[i].type);

					// Get the file name
					j = 0;
					while(j<MAX_LONG_NAME_LENGHT && dir_entry.filename[j])
					{
						DirectoryEntry_tab[i].name[j] = dir_entry.filename[j];
						j++;
					}
					DirectoryEntry_tab[i].name[j] = 0x00;

					entrytype_icon = 12;
					DirectoryEntry_tab[i].attributes=0x00;

					if(dir_entry.is_dir)
					{
						entrytype_icon = 10;
						DirectoryEntry_tab[i].attributes = FILE_ATTR_DIRECTORY;
					}

					if(dir_entry.is_readonly)
						DirectoryEntry_tab[i].attributes |= FILE_ATTR_READ_ONLY;

					if(dir_entry.is_system)
						DirectoryEntry_tab[i].attributes |= FILE_ATTR_SYSTEM;

					if(dir_entry.is_hidden)
						DirectoryEntry_tab[i].attributes |= FILE_ATTR_HIDDEN;

					hxc_printf(ctx,LEFT_ALIGNED | DONTPARSE,0,y_pos," %c%s",entrytype_icon,dir_entry.filename);

					y_pos=y_pos+8;

					DirectoryEntry_tab[i].firstCluster = ENDIAN_32BIT(dir_entry.cluster) ;
					DirectoryEntry_tab[i].size =  ENDIAN_32BIT(dir_entry.size);

					#ifdef DEBUG
					dbg_printf("Entry : %s Size:%d Cluster 0x%.8X Flags 0x%.2X\n",DirectoryEntry_tab[i].name,DirectoryEntry_tab[i].size,DirectoryEntry_tab[i].firstCluster,DirectoryEntry_tab[i].attributes);
					#endif

					i++;
				}
			}
			else
			{
				last_file=0xFF;
				i=ctx->NUMBER_OF_FILE_ON_DISPLAY;
			}

		}while( i < ctx->NUMBER_OF_FILE_ON_DISPLAY );

		ctx->filtermode=0;

		if(ctx->page_number < MAX_PAGES_PER_DIRECTORY - 1 )
			memcpy(&file_list_status_tab[ctx->page_number+1],&file_list_status ,sizeof(FL_DIR));

		hxc_print(ctx,LEFT_ALIGNED,0,FILELIST_Y_POS+(ctx->selectorpos*8),">");
		invert_line(ctx,0,FILELIST_Y_POS+(ctx->selectorpos*8));

		ctx->read_entry=0;

		do
		{
			key=wait_function_key();
			if(1)
			{
				switch(key)
				{
				case FCT_UP_KEY: // UP
					invert_line(ctx,0,FILELIST_Y_POS+(ctx->selectorpos*8));
					hxc_print(ctx,LEFT_ALIGNED,0,FILELIST_Y_POS+(ctx->selectorpos*8)," ");

					ctx->selectorpos--;
					if(ctx->selectorpos<0)
					{
						ctx->selectorpos = ctx->NUMBER_OF_FILE_ON_DISPLAY-1;
						if(ctx->page_number)
							ctx->page_number--;
						clear_list(ctx,0);
						ctx->read_entry=1;
						memcpy(&file_list_status ,&file_list_status_tab[ctx->page_number],sizeof(FL_DIR));

						#ifdef DEBUG
						dbg_printf("Page change : %d\n",ctx->page_number);
						#endif
					}
					else
					{
						hxc_print(ctx,LEFT_ALIGNED,0,FILELIST_Y_POS+(ctx->selectorpos*8),">");
						invert_line(ctx,0,FILELIST_Y_POS+(ctx->selectorpos*8));
					}
					break;

				case FCT_DOWN_KEY: // Down
					invert_line(ctx,0,FILELIST_Y_POS+(ctx->selectorpos*8));
					hxc_print(ctx,LEFT_ALIGNED,0,FILELIST_Y_POS+(ctx->selectorpos*8)," ");

					ctx->selectorpos++;
					if(ctx->selectorpos>=ctx->NUMBER_OF_FILE_ON_DISPLAY)
					{
						ctx->selectorpos = 1;
						clear_list(ctx,0);
						ctx->read_entry=1;
						if(!last_file && ctx->page_number < MAX_PAGES_PER_DIRECTORY)
							ctx->page_number++;
						memcpy(&file_list_status ,&file_list_status_tab[ctx->page_number],sizeof(FL_DIR));

						#ifdef DEBUG
						dbg_printf("Page change : %d\n",ctx->page_number);
						#endif
					}
					else
					{
						hxc_print(ctx,LEFT_ALIGNED,0,FILELIST_Y_POS+(ctx->selectorpos*8),">");
						invert_line(ctx,0,FILELIST_Y_POS+(ctx->selectorpos*8));
					}

					break;

				case FCT_RIGHT_KEY: // Right
					if(!last_file && ctx->page_number < MAX_PAGES_PER_DIRECTORY)
						ctx->page_number++;

					memcpy(&file_list_status ,&file_list_status_tab[ctx->page_number],sizeof(FL_DIR));

					clear_list(ctx,0);
					ctx->read_entry=1;

					#ifdef DEBUG
					dbg_printf("Page change : %d\n",ctx->page_number);
					#endif
					break;

				case FCT_LEFT_KEY:
					if(ctx->page_number)
						ctx->page_number--;

					memcpy(&file_list_status ,&file_list_status_tab[ctx->page_number],sizeof(FL_DIR));

					clear_list(ctx,0);
					ctx->read_entry=1;

					#ifdef DEBUG
					dbg_printf("Page change : %d\n",ctx->page_number);
					#endif
					break;

				case FCT_SELECT_FILE_DRIVEA:
				case FCT_SHOWSLOTS:
					disk_ptr=(disk_in_drive_v2_long * )&DirectoryEntry_tab[ctx->selectorpos];

					if( disk_ptr->attributes & FILE_ATTR_DIRECTORY )
					{
						enter_sub_dir(ctx,disk_ptr);
					}
					else
					{
						if(!ctx->selectorpos || ctx->page_mode_index>= 1 + ctx->number_of_drive)
							ctx->page_mode_index = 0;

						if(!ctx->selectorpos)
							ctx->slotselectorpos = 0;
						do
						{
							if(ctx->page_mode_index < ctx->number_of_drive )
							{
								ret = ui_slots_menu(ctx);
							}
							else
							{
								enter_menu(ctx,commands_menu);
								clear_list(ctx,0);
								ret = 1;
							}
						}while(!ret && ctx->page_mode_index <  ctx->number_of_drive + 1 );

						memcpy(&file_list_status ,&file_list_status_tab[ctx->page_number],sizeof(FL_DIR));
						ctx->read_entry = 1;
					}
					break;

				case FCT_SELECTSAVEREBOOT:
					disk_ptr=(disk_in_drive_v2_long * )&DirectoryEntry_tab[ctx->selectorpos];

					if( disk_ptr->attributes & FILE_ATTR_DIRECTORY )
					{
						enter_sub_dir(ctx,disk_ptr);
					}
					else
					{
						// Update the Slot 1, select it and reboot...
						memcpy((void*)&disks_slots[1*ctx->number_of_drive],(void*)&DirectoryEntry_tab[ctx->selectorpos],sizeof(disk_in_drive_v2));
						ctx->slot_map[1>>3] |= (0x80 >> (1&7));
						ctx->change_map[1>>3] |= (0x80 >> (1&7));
						ui_savereboot(ctx,1);
					}
					break;

				case FCT_TOP:
					ctx->page_number=0;
					memcpy(&file_list_status ,&file_list_status_tab[ctx->page_number],sizeof(FL_DIR));
					clear_list(ctx,0);
					ctx->read_entry=1;
					break;

				case FCT_SEARCH:
					ctx->filtermode=0xFF;
					hxc_print(ctx,LEFT_ALIGNED,(ctx->SCREEN_XRESOL/2)+8*8,CURDIR_Y_POS,"Search:                     ");
					flush_char();
					i=0;
					do
					{
						ctx->filter[i]=0;
						c=get_char();
						if(c!='\n')
						{
							ctx->filter[i]=c;
							hxc_printf(ctx,LEFT_ALIGNED,(ctx->SCREEN_XRESOL/2)+(8*8)+(8*8)+(8*i),CURDIR_Y_POS,"%c",c);
						}
						i++;
					}while(c!='\n' && i<16);
					ctx->filter[i]=0;

					strlwr(ctx->filter);
					hxc_printf(ctx,LEFT_ALIGNED,ctx->SCREEN_XRESOL/2+(8*8)+(8*8),CURDIR_Y_POS,"[%s]",ctx->filter);
					ctx->selectorpos=0;
					ctx->page_number=0;
					memcpy(&file_list_status ,&file_list_status_tab[0],sizeof(FL_DIR));

					clear_list(ctx,0);
					ctx->read_entry=1;
					break;

				default:
					if(process_extra_functions(ctx, key))
					{
						memcpy(&file_list_status ,&file_list_status_tab[ctx->page_number],sizeof(FL_DIR));
						clear_list(ctx,0);
						ctx->read_entry=1;
					}
					break;
				}
			}
		}while(!ctx->read_entry);
	}
}

int mount_drive(ui_context * ctx, int drive)
{
	unsigned short i;

	strcpy( ctx->currentPath, "/" );

	strcpy(ctx->FIRMWAREVERSION,"-------");

	if(media_access_init(drive))
	{
		hxc_printf_box(ctx,"Reading HXCSDFE.CFG ...");

		read_cfg_file(ctx,cfgfile_header);

		#ifdef DEBUG
		dbg_printf("read_cfg_file done\n");
		#endif

		if( getcfg_backgroundcolor() != 0xFF )
			set_color_scheme( getcfg_backgroundcolor() );

		displayFolder(ctx);

		fl_opendir(ctx->currentPath, &file_list_status);

		for(i=0;i<MAX_PAGES_PER_DIRECTORY;i++)
		{
			memcpy(&file_list_status_tab[i],&file_list_status ,sizeof(FL_DIR));
		}

		ctx->selectorpos = 1;
		ctx->read_entry = 1;
		ctx->page_number = 0;

		ctx->page_mode_index = ( 2 + ctx->number_of_drive ) - 1;

		clear_list(ctx,0);

		return 1;
	}
	return 0;
}

int main(int argc, char* argv[])
{
	int bootdev;
	ui_context * ctx;

	ctx = &g_ui_ctx;

	memset( ctx,0,sizeof(ui_context));

	if(process_command_line(argc, argv))
	{
		return 0;
	}

	init_display(ctx);

	init_display_buffer(ctx);
	
	#ifdef DEBUG
	dbg_printf("Init display Done\n");
	#endif

	if(argv)
		bootdev = get_start_unit(argv[0]);
	else
		bootdev = get_start_unit(0);

	if( bootdev < 0 )
	{
		hxc_printf_box(ctx,"ERROR: HxC Drive not detected !");
		lockup();
	}

	if( mount_drive(ctx, bootdev) )
	{
		#ifdef DEBUG
		dbg_printf("mount_drive done\n");
		#endif

		ui_mainfileselector(ctx);
	}

	lockup();
	return 0;
}
