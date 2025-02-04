/*******************************************************************************
* @file  onebox_util.c
* @brief This file include onebox_util application which is used to parse the user
* command and call related function.
*******************************************************************************
* # License
* <b>Copyright 2020 Silicon Laboratories Inc. www.silabs.com</b>
*******************************************************************************
*
* The licensor of this software is Silicon Laboratories Inc. Your use of this
* software is governed by the terms of Silicon Labs Master Software License
* Agreement (MSLA) available at
* www.silabs.com/about-us/legal/master-software-license-agreement. This
* software is distributed to you in Source Code format and is governed by the
* sections of the MSLA applicable to Source Code.
*
******************************************************************************/

#include <stdio.h>
#include <sys/socket.h>
#include <string.h>
#include <linux/types.h>
#include <linux/if.h>
#include <stdlib.h>
#include <linux/wireless.h>
#include <unistd.h>
#include <fcntl.h>
#include <inttypes.h>
#include "per_util.h"
#include <linux/netlink.h>

/******************************************************************************
 * main()
 *****************************************************************************/
int main(int argc, char *argv[])
{
  int sfd;
  char ifName[32];
  int ret          = 0;
  int cmdNo        = -1;
  unsigned char jj = 0, ii = 0;
  unsigned int bb_addr = 0, bb_val = 0, len = 0;
  struct bb_rf_param_t bb_rf_params;
  struct bb_rf_param_t bb_rf_read;
  struct efuse_content_t *efuse_content;
  int length;
  int multiple_bb_read = 0;

  struct nlmsghdr *nlh;

  if (argc < 3) {
    usage();
    return ONEBOX_STATUS_FAILURE;
  } else if (argc <= 50) {
    /* Get interface name */
    if (strlen(argv[1]) < sizeof(ifName)) {
      strcpy(ifName, argv[1]);
    } else {
      ONEBOX_PRINT("length of given interface name is more than the buffer size\n");
      return ONEBOX_STATUS_FAILURE;
    }

    while (ifName[jj] != '\0') {
      if ((ifName[jj] == ';') || (ifName[jj] == '#')) {
        ONEBOX_PRINT("Error: Wrong interface name given, aborting.\n");
        return ONEBOX_STATUS_FAILURE;
      }
      jj++;
    }
    cmdNo = getcmdnumber(argv[2], ifName);
    if (cmdNo == ONEBOX_STATUS_FAILURE)
      return ONEBOX_STATUS_FAILURE;
    //printf("cmd is %d \n",cmdNo);
  }

  /* Open socket */
  sfd = socket_creation();
  if (sfd < 0) {
    ONEBOX_PRINT("Socket Creation Failed\n");
    return ONEBOX_STATUS_FAILURE;
  }

  switch (cmdNo) {
    case UPDATE_WLAN_GAIN_TABLE: {
      ret = update_wlan_gain_table(argc, argv, ifName, sfd);
    } break;
    case RSI_SET_BB_WRITE:
      if (argc > 3) {
        bb_addr = strtol(argv[3], NULL, 16);
        bb_val  = strtol(argv[4], NULL, 16);
        ONEBOX_PRINT("BB addr: 0x%x value 0x%x\n", bb_addr, bb_val);
        bb_rf_params.value        = 1;
        bb_rf_params.no_of_values = 2;
        bb_rf_params.soft_reset   = 3;
        bb_rf_params.Data[1]      = bb_addr;
        bb_rf_params.Data[2]      = bb_val;
      }
      if (argc > 5) {
        bb_rf_params.no_of_values = (argc - 3);
        for (ii = 3; ii < bb_rf_params.no_of_values - 2 + 3; ii += 2) {
          bb_addr                   = strtol(argv[ii + 2], NULL, 16);
          bb_val                    = strtol(argv[ii + 3], NULL, 16);
          bb_rf_params.Data[ii]     = bb_addr;
          bb_rf_params.Data[ii + 1] = bb_val;
          ONEBOX_PRINT("BB addr: 0x%x value 0x%x\n", bb_addr, bb_val);
        }
      }
      ret = bb_read_write_wrapper(bb_rf_params, sfd);
      if (ret < 0) {
        printf("ERROR writing to BB\n");
      } else {
        printf("SUCCESS writing to BB\n");
      }
      break;
    case EFUSE_MAP: {
      if (argc > 3) {
        printf("Usage \n");
        printf("./onebox_util rpine0  print_efuse_map\n");
        break;
      }
      efuse_content = malloc(sizeof(struct efuse_content_t));
      length        = sizeof(struct efuse_content_t);
      if (rsi_print_efuse_map(efuse_content, sfd) < 0) {
        printf("Error while reading EFUSE MAP from driver\n");
        ret = ONEBOX_STATUS_FAILURE;
        free(efuse_content);
        break;
      } else {
        nlh = common_recv_mesg_wrapper(sfd, length);
        memcpy(efuse_content, NLMSG_DATA(nlh), length);
        printf(" ***********************EFUSE MAP******************************\n");
        printf(" efuse_map_version %18x\n", efuse_content->efuse_map_version);
        printf(" module_version	%21x\n", efuse_content->module_version);
        printf(" module_part_no	%21x\n", efuse_content->module_part_no);
        printf(" mfg_sw_version	%21x\n", efuse_content->mfg_sw_version);
        printf(" module_type %24x\n", efuse_content->module_type);
        printf(" chip_version %23x\n", efuse_content->chip_version);
        printf(" hw_configuration %19s\n",
               efuse_content->m4sb_cert_en ? "1.4 silicon acting as 1.3" : "1.4 silicon only");
        printf(" mfg_sw_subversion %18x\n", efuse_content->mfg_sw_subversion);
        printf(" chip_id_no	%21x\n", efuse_content->chip_id_no);
        printf(" sdb_mode	%21x\n", efuse_content->sdb_mode);
        printf(" ***********************EFUSE MAP******************************\n");
      }
      free(efuse_content);
    } break;
    case RSI_MULTIPLE_BB_READ:
      if (argc > 9) {
        printf("Maximum 6 address allowed\n");
        break;
      }
      multiple_bb_read = 1;
    case RSI_SET_BB_READ:
      len = sizeof(bb_rf_params);
      if (argc > 3) {
        bb_rf_params.value        = 0; //BB_READ_TYPE
        bb_rf_params.no_of_values = argc - 3;
        bb_rf_params.soft_reset   = 0;
        bb_rf_params.Data[2]      = 1;

        for (ii = 3, jj = 1; ii < argc; ii++, jj++) {
          bb_addr                 = strtol(argv[ii], NULL, 16);
          bb_rf_params.Data[jj]   = bb_addr;
          bb_rf_read.Data[jj - 1] = bb_addr;
          if (!multiple_bb_read)
            break;
        }
        ret = bb_read_write_wrapper(bb_rf_params, sfd);
        if (ret < 0) {
          printf("Error Sending to bb\n");
        }
        nlh = common_recv_mesg_wrapper(sfd, len);
        if (nlh == NULL) {
          printf("Error receving from bb\n");
          break;
        } else {
          memcpy(&bb_rf_params, NLMSG_DATA(nlh), len);
          for (ii = 0; ii < bb_rf_params.no_of_values; ii++) {
            printf("BB addr : 0x%x BB_read value is 0x%x\n", bb_rf_read.Data[ii], bb_rf_params.Data[ii]);
            if (!multiple_bb_read)
              break;
          }
        }
        free(nlh);
      }
      break;
    default:
      return ONEBOX_STATUS_FAILURE;
  }
  close(sfd);
  return ret;
}

int getcmdnumber(char *command, char *ifName)
{
  if (!strcmp(command, "update_wlan_gain_table") && !strncmp(ifName, "rpine", 5)) {
    return UPDATE_WLAN_GAIN_TABLE;
  }
  if (!strcmp(command, "print_efuse_map") && !strncmp(ifName, "rpine", 5)) {
    return EFUSE_MAP;
  }
  if (!strcmp(command, "bb_write") && !strncmp(ifName, "rpine", 5)) {
    return RSI_SET_BB_WRITE;
  }
  if (!strcmp(command, "bb_read_multiple") && !strncmp(ifName, "rpine", 5)) {
    return RSI_MULTIPLE_BB_READ;
  }
  if (!strcmp(command, "bb_read") && !strncmp(ifName, "rpine", 5)) {
    return RSI_SET_BB_READ;
  } else {
    ONEBOX_PRINT("Error: Wrong command , Please follow usage...\n");
    usage();
    return ONEBOX_STATUS_FAILURE;
  }
}

void usage()
{
  ONEBOX_PRINT("Usage:./onebox_util rpine0 update_wlan_gain_table\n");
  ONEBOX_PRINT("Usage:./onebox_util rpine0 addr data\n");
  ONEBOX_PRINT("Usage:./onebox_util rpine0 addr\n");
  ONEBOX_PRINT("Usage:./onebox_util rpine0 print_efuse_map\n");
  return;
}
