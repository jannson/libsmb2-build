/* -*-  mode:c; tab-width:8; c-basic-offset:8; indent-tabs-mode:nil;  -*- */
/*
   Copyright (C) 2018 by Ronnie Sahlberg <ronniesahlberg@gmail.com>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU Lesser General Public License as published by
   the Free Software Foundation; either version 2.1 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public License
   along with this program; if not, see <http://www.gnu.org/licenses/>.
*/
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#include <errno.h>
#ifdef ESP_PLATFORM
#include <sys/poll.h>
#else
#include <poll.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#ifdef STDC_HEADERS
#include <stddef.h>
#endif

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <errno.h>
#include <stdio.h>

#include "compat.h"

#include "smb2.h"
#include "libsmb2.h"
#include "libsmb2-dcerpc.h"
#include "libsmb2-dcerpc-srvsvc.h"
#include "libsmb2-raw.h"
#include "libsmb2-private.h"

struct smb2nse {
        smb2_command_cb cb;
        void *cb_data;
        union {
                struct srvsvc_netshareenumall_req se_req;
        };
};

static void
nse_free(struct smb2nse *nse)
{
        free(discard_const(nse->se_req.server));
        free(nse);
}

static void
srvsvc_ioctl_cb(struct dcerpc_context *dce, int status,
                void *command_data, void *cb_data)
{
        struct smb2nse *nse = cb_data;
        struct srvsvc_rep *rep = command_data;
        struct smb2_context *smb2 = dcerpc_get_smb2_context(dce);

        if (status != SMB2_STATUS_SUCCESS) {
                nse->cb(smb2, status, NULL, nse->cb_data);
                nse_free(nse);
                dcerpc_destroy_context(dce);
                return;
        }
        
        nse->cb(smb2, rep->status, rep, nse->cb_data);
        nse_free(nse);
        dcerpc_destroy_context(dce);
}

static void
share_enum_bind_cb(struct dcerpc_context *dce, int status,
                      void *command_data, void *cb_data)
{
        struct smb2nse *nse = cb_data;
        struct smb2_context *smb2 = dcerpc_get_smb2_context(dce);

        if (status != SMB2_STATUS_SUCCESS) {
                nse->cb(smb2, status, NULL, nse->cb_data);
                nse_free(nse);
                dcerpc_destroy_context(dce);
                return;
        }

        status = dcerpc_call_async(dce,
                                   SRVSVC_NETRSHAREENUM,
                                   srvsvc_NetrShareEnum_req_coder, &nse->se_req,
                                   srvsvc_NetrShareEnum_rep_coder,
                                   sizeof(struct srvsvc_netshareenumall_rep),
                                   srvsvc_ioctl_cb, nse);
        if (status) {
                nse->cb(smb2, status, NULL, nse->cb_data);
                nse_free(nse);
                dcerpc_destroy_context(dce);
                return;
        }
}

int
smb2_share_enum_async(struct smb2_context *smb2,
                      smb2_command_cb cb, void *cb_data)
{
        struct dcerpc_context *dce;
        struct smb2nse *nse;
        int rc;
        char *server;

        dce = dcerpc_create_context(smb2);
        if (dce == NULL) {
                return -ENOMEM;
        }
        
        nse = calloc(1, sizeof(struct smb2nse));
        if (nse == NULL) {
                smb2_set_error(smb2, "Failed to allocate nse");
                dcerpc_destroy_context(dce);
                return -ENOMEM;
        }
        nse->cb = cb;
        nse->cb_data = cb_data;

        server = malloc(strlen(smb2->server) + 3);
        if (server == NULL) {
                free(nse);
                smb2_set_error(smb2, "Failed to allocate server");
                dcerpc_destroy_context(dce);
                return -ENOMEM;
        }
        
        sprintf(server, "\\\\%s", smb2->server);
        nse->se_req.server = server;

        nse->se_req.level = 1;
        nse->se_req.ctr = NULL;
        nse->se_req.max_buffer = 0xffffffff;
        nse->se_req.resume_handle = 0;

        rc = dcerpc_connect_context_async(dce, "srvsvc", &srvsvc_interface,
                                          share_enum_bind_cb, nse);
        if (rc) {
                nse_free(nse);
                dcerpc_destroy_context(dce);
                return rc;
        }
        
        return 0;
}

static void shared_se_cb(struct smb2_context *smb2, int status,
                void *command_data, void *private_data)
{
        struct srvsvc_netshareenumall_rep *rep = command_data;
        int i = 0;
        struct smb2_shares* shares = (struct smb2_shares*)private_data;

        if (status) {
          shares->err = -10;
          shares->finish = 1;
          return;
        }
        shares->paths = calloc(rep->ctr->ctr1.count, sizeof(char*));
        for (i = 0; i < rep->ctr->ctr1.count; i++) {
            if (((rep->ctr->ctr1.array[i].type & 3) == SHARE_TYPE_DISKTREE) &&
            (0 == (rep->ctr->ctr1.array[i].type & SHARE_TYPE_HIDDEN))) { 
              shares->paths[shares->path_count] = strdup(rep->ctr->ctr1.array[i].name);
              shares->path_count++;
            }
        }
        smb2_free_data(smb2, rep);
        shares->finish = 1;
}

struct smb2_shares* smb2_shares_find(char *smb_url, char *password)
{
        struct smb2_context *smb2 = NULL;
        struct smb2_url *url = NULL;
      	struct pollfd pfd;
        struct smb2_shares* shares = calloc(1, sizeof(struct smb2_shares));
        if(NULL == shares) {
          return NULL;
        }

	      smb2 = smb2_init_context();
        if (smb2 == NULL) {
          shares->err = -1;
          goto SMB2_SHARE_FIN;
        }

        url = smb2_parse_url(smb2, smb_url);
        if (url == NULL) {
          shares->err = -2;
          goto SMB2_SHARE_FIN;
        }
        if (url->user) {
          smb2_set_user(smb2, url->user);
        }
        if (password) {
          smb2_set_password(smb2, password);
        }
        smb2_set_timeout(smb2, 6);

        smb2_set_security_mode(smb2, SMB2_NEGOTIATE_SIGNING_ENABLED);

        if (smb2_connect_share(smb2, url->server, "IPC$", NULL) < 0) {
          shares->err = -3;
          shares->err_str = strdup(smb2_get_error(smb2));
          goto SMB2_SHARE_FIN;
        }

        if (smb2_share_enum_async(smb2, shared_se_cb, shares) != 0) {
          shares->err = -4;
          goto SMB2_SHARE_FIN;
        }

        while (!shares->finish) {
          pfd.fd = smb2_get_fd(smb2);
          pfd.events = smb2_which_events(smb2);

          if (poll(&pfd, 1, 1000) < 0) {
            shares->err = -5;
            goto SMB2_SHARE_FIN;
          }
          if (pfd.revents == 0) {
              continue;
          }
          if (smb2_service(smb2, pfd.revents) < 0) {
              break;
          }
	    }

SMB2_SHARE_FIN:
        if(NULL != smb2) {
          smb2_disconnect_share(smb2);
        }
        if(NULL != url) {
          smb2_destroy_url(url);
        }
        if(NULL != smb2) {
          smb2_destroy_context(smb2);
        }
        return shares;
}

int smb2_shares_err(struct smb2_shares* shares) {
  return shares->err;
}

int smb2_shares_length(struct smb2_shares* shares) {
  return shares->path_count;
}

const char* smb2_shares_cstr(struct smb2_shares *shares, int i) {
  if (i >= shares->path_count) {
    return NULL;
  }
  return shares->paths[i];
}

const char* smb2_shares_err_cstr(struct smb2_shares *shares) {
  return shares->err_str;
}

void smb2_shares_destroy(struct smb2_shares* shares) {
  int i;
  if(NULL == shares) {
    return;
  }
  for(i = 0; i < shares->path_count; i++) {
    free(shares->paths[i]);
  }
  if(NULL != shares->paths) {
    free(shares->paths);
  }
  if(NULL != shares->err_str) {
    free(shares->err_str);
  }
  free(shares);
}
