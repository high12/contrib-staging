/*
  This code is part of FCPTools - an FCP-based client library for Freenet

  CopyLeft (c) 2001 by David McNab

  Developers:
  - David McNab <david@rebirthing.co.nz>
  - Jay Oliveri <ilnero@gmx.net>
  
  Currently maintained by Jay Oliveri <ilnero@gmx.net>
  
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.
  
  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
  
  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include "ezFCPlib.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "ez_sys.h"

static void _fcpDestroyResponse(hFCP *h);

/*
	This version requires certain variables to be specified as arguments.
*/
hFCP *fcpCreateHFCP(char *host, int port, int htl, int optmask)
{
  hFCP *h;

	h = malloc(sizeof (hFCP));
	memset(h, 0, sizeof (hFCP));

	if (!host) {
		h->host = malloc(sizeof(EZFCP_DEFAULT_HOST + 1));
		strcpy(h->host, EZFCP_DEFAULT_HOST);
	}
	else {
		h->host = malloc(strlen(host) + 1);
		strcpy(h->host, host);
	}

	h->port = (port == 0 ? EZFCP_DEFAULT_PORT : port );
	h->htl =  (htl  < 0 ? EZFCP_DEFAULT_HTL  : htl  );
	
	h->options = _fcpCreateHOptions();

	/* do the handle option mask */
	h->options->noredirect       = (optmask & FCP_MODE_RAW ? FCP_MODE_RAW : 0);
	h->options->delete_local  = (optmask & FCP_MODE_DELETE_LOCAL ? FCP_MODE_DELETE_LOCAL : 0);
	h->options->skip_local    = (optmask & FCP_MODE_SKIP_LOCAL ? FCP_MODE_SKIP_LOCAL : 0);
	h->options->dbr           = (optmask & FCP_MODE_DBR ? FCP_MODE_DBR : 0);
	h->options->meta_redirect = (optmask & FCP_MODE_REDIRECT_METADATA ? FCP_MODE_REDIRECT_METADATA : 0);

	_fcpLog(FCP_LOG_DEBUG, "noredirect: %u, delete_local: %u, skip_local: %u, dbr: %u, meta_redirect: %u",
					h->options->noredirect,
					h->options->delete_local,
					h->options->skip_local,
					h->options->dbr,
					h->options->meta_redirect);
	
	h->key = _fcpCreateHKey();
	
	return h;
}

/* like dup() for files; duplicates an hFCP struct */

hFCP *fcpInheritHFCP(hFCP *hfcp)
{
  hFCP *h;

	if (!hfcp) return 0;

	h = fcpCreateHFCP(hfcp->host, hfcp->port, hfcp->htl,
		                hfcp->options->noredirect |
										hfcp->options->delete_local |
										hfcp->options->skip_local |
		                hfcp->options->dbr |
		                hfcp->options->meta_redirect);

	/* copy over any other options */
	h->options->timeout = hfcp->options->timeout;
	h->options->retry = hfcp->options->retry;

	return h;
}

void fcpDestroyHFCP(hFCP *h)
{
	/*_fcpLog(FCP_LOG_DEBUG, "Entered fcpDestroyHFCP()");*/

	if (h) {

		if (h->socket != FCP_SOCKET_DISCONNECTED) _fcpSockDisconnect(h);

		if (h->host) free(h->host);
		if (h->description) free(h->description);

		if (h->key) {
			_fcpDestroyHKey(h->key);
			free(h->key);
		}

		if (h->_redirect) free(h->_redirect);

		_fcpDestroyHOptions(h->options);
		_fcpDestroyResponse(h);

		h->socket = FCP_SOCKET_DISCONNECTED;

		/* let caller free 'h' */
	}
}

hOptions *_fcpCreateHOptions(void)
{
	hOptions *h;

	h = malloc(sizeof (hOptions));
	memset(h, 0, sizeof (hOptions));

	h->logstream = EZFCP_DEFAULT_LOGSTREAM;		
	h->delete_local = EZFCP_DEFAULT_DELETELOCAL;
	h->regress = EZFCP_DEFAULT_REGRESS;
	h->retry = EZFCP_DEFAULT_RETRY;
	h->skip_local = EZFCP_DEFAULT_SKIPLOCAL;
	h->splitblock = EZFCP_DEFAULT_BLOCKSIZE;
	h->timeout = EZFCP_DEFAULT_TIMEOUT;
	h->verbosity = EZFCP_DEFAULT_VERBOSITY;

	return h;
}

void _fcpDestroyHOptions(hOptions *h)
{
	if (h) {
		if (h->homedir) free(h->homedir);
		if (h->tempdir) free(h->tempdir);
		
		if (h->logstream)
			if (h->logstream != stdout) fclose(h->logstream);
	}
}

hKey *_fcpCreateHKey(void)
{
	hKey *h;

	h = malloc(sizeof (hKey));
	memset(h, 0, sizeof (hKey));

	h->uri        = fcpCreateHURI();
	h->target_uri = fcpCreateHURI();

	h->tmpblock = _fcpCreateHBlock();

	/* binary_mode defaults to 0/text */
	h->tmpblock->binary_mode = 1;

	h->metadata = _fcpCreateHMetadata();

	return h;
}

void _fcpDestroyHKey(hKey *h)
{
	if (h) {
		int i;

		if (h->uri) {
			fcpDestroyHURI(h->uri);
			free(h->uri);
		}

		if (h->target_uri) {
			fcpDestroyHURI(h->target_uri);
			free(h->target_uri);
		}

		if (h->mimetype) free(h->mimetype);

		if (h->tmpblock) {
			_fcpDestroyHBlock(h->tmpblock);
			free(h->tmpblock);
		}

		if (h->metadata) {
			_fcpDestroyHMetadata(h->metadata);
			free(h->metadata);
		}

		if (h->segment_count) {

			for (i=0; i < h->segment_count; i++) {
				_fcpDestroyHSegment(h->segments[i]);
				free(h->segments[i]);
			}
			
			free(h->segments);
		}
	}
}

static void _fcpDestroyResponse(hFCP *h)
{
	if (h) {
		if (h->response.success.uri) free(h->response.success.uri);
		if (h->response.datachunk.data) free(h->response.datachunk.data);
		if (h->response.keycollision.uri) free(h->response.keycollision.uri);
		if (h->response.pending.uri) free(h->response.pending.uri);
		if (h->response.failed.reason) free(h->response.failed.reason);
		if (h->response.urierror.reason) free(h->response.urierror.reason);
		if (h->response.routenotfound.reason) free(h->response.routenotfound.reason);
		if (h->response.formaterror.reason) free(h->response.formaterror.reason);

		/* don't forget the nodeinfo section */
		if (h->response.nodeinfo.architecture) free(h->response.nodeinfo.architecture);
		if (h->response.nodeinfo.operatingsystem) free(h->response.nodeinfo.operatingsystem);
		if (h->response.nodeinfo.operatingsystemversion) free(h->response.nodeinfo.operatingsystemversion);
		if (h->response.nodeinfo.nodeaddress) free(h->response.nodeinfo.nodeaddress);
		if (h->response.nodeinfo.javavendor) free(h->response.nodeinfo.javavendor);
		if (h->response.nodeinfo.javaname) free(h->response.nodeinfo.javaname);
		if (h->response.nodeinfo.javaversion) free(h->response.nodeinfo.javaversion);
		if (h->response.nodeinfo.istransient) free(h->response.nodeinfo.istransient);
	}
}

hBlock *_fcpCreateHBlock(void)
{
	hBlock *h;

	h = malloc(sizeof (hBlock)); /* 1st ! */
	memset(h, 0, sizeof (hBlock));

	h->uri = fcpCreateHURI();

	if (_fcpTmpfile(h->filename) != 0) {
		_fcpLog(FCP_LOG_DEBUG, "could not create temp file %s", h->filename);
		return 0;
	}		

	h->fd = -1;
	h->delete = 1;

	return h;
}

void _fcpDestroyHBlock(hBlock *h)
{
	if (h) {
		
		/* close and delete the file */
		_fcpBlockUnlink(h);
		_fcpDeleteBlockFile(h);
		
		if (h->uri) {
			fcpDestroyHURI(h->uri);
			free(h->uri);
		}
	}
}

hMetadata *_fcpCreateHMetadata(void)
{
	hMetadata *h;

	h = malloc(sizeof (hMetadata));
	memset(h, 0, sizeof (hMetadata));

	h->tmpblock = _fcpCreateHBlock();

	return h;
}

void _fcpDestroyHMetadata(hMetadata *h)
{
	if (h) {

		if (h->tmpblock) {
			_fcpDestroyHBlock(h->tmpblock);
			free(h->tmpblock);
		}
		
		/*if (h->raw_metadata) free(h->raw_metadata); DEPRECATE */

		if (h->cdoc_count)
			_fcpDestroyHMetadata_cdocs(h);
	}
}

void _fcpDestroyHMetadata_cdocs(hMetadata *h)
{
	if (h->cdoc_count) {
		int i;
		int j;
		
		for (i=0; i < h->cdoc_count; i++) {
			
			if (h->cdocs[i]->name) free(h->cdocs[i]->name);
			if (h->cdocs[i]->field_count) {
				
				for (j=0; j < (h->cdocs[i]->field_count * 2); j += 2) {
					free(h->cdocs[i]->data[j]);
					free(h->cdocs[i]->data[j+1]);
				}
			}

			free(h->cdocs[i]);
		}

		/*free(h->cdocs);*/
		h->cdoc_count = 0;
	}
}

hURI *fcpCreateHURI(void)
{
	hURI *h;

	h = malloc(sizeof (hURI));
	memset(h, 0, sizeof (hURI));

	return h;
}

void fcpDestroyHURI(hURI *h)
{
	if (h) {
		if (h->uri_str) free(h->uri_str);
		if (h->keyid) free(h->keyid);
		if (h->filename) free(h->filename);
		if (h->docname) free(h->docname);
	}
}

/*
	fcpParseHURI()

	This function parses a string containing a fully-qualified Freenet URI
	into simpler components.  It is written to be re-entrant on the same
	hURI pointer (it can be called repeatedly without being re-created.
*/
int fcpParseHURI(hURI *uri, char *key)
{
	int len;

	char *p_key;
	char *string_end;
	char  uri_s[513];

	/* clear out the dynamic arrays before attempting to parse a new uri */
	if (uri->uri_str) free(uri->uri_str);
  if (uri->keyid) free(uri->keyid);
	if (uri->filename) free(uri->filename);
	if (uri->docname) free(uri->docname);
	
	/* save the key root */
	p_key = key;
	
	/* zero the block of memory */
	memset(uri, 0, sizeof (hURI));

  /* skip 'freenet:' */
  if (!strncmp(key, "freenet:", 8))
    key += 8;
	
  /* classify key header */
  if (!strncmp(key, "SSK@", 4)) {
		
    uri->type = KEY_TYPE_SSK;
		strcpy(uri_s, "freenet:SSK@");

		key += 4;
		string_end = strstr(key, "/");

		/* check for case where there's nothing after the key id */
		if (!string_end) {
			uri->keyid = strdup(key);
			strcat(uri_s, uri->keyid);
		}

		else {
			/* string_end points to the '/' character */

			uri->keyid = malloc(string_end - key + 1);
			strncpy(uri->keyid, key, string_end - key);

			strcat(uri_s, uri->keyid);
			strcat(uri_s, "/");
		
			/* point key to the first char after the '/' */
			key = (string_end + 1);
			
			string_end = strstr(key, "//");
			
			/* if there's nothing after the '/' */
			if (!string_end) {
				uri->filename = strdup(key);
				strcat(uri_s, uri->filename);
			}

			/* TODO: handle multiple '//' characters with docnames */
			
			/* if there's a '//' character */
			else {

				uri->filename = malloc(string_end - key + 1);
				strncpy(uri->filename, key, string_end - key);

				strcat(uri_s, uri->filename);
				strcat(uri_s, "//");
				
				/* point key to char after '//' */
				key = (string_end + 2);
				
				/* take the rest as the metadata control document name */
				uri->docname = strdup(key);
				strcat(uri_s, uri->docname);
			}
		}
	}
		
  else if (!strncmp(key, "CHK@", 4)) {
	
		uri->type = KEY_TYPE_CHK;

		strcpy(uri_s, "freenet:CHK@");
		key += 4;

		len = strlen(key);

		if (len) {

			string_end = strstr(key, "/");
			
			/* check for the case where there's nothing after the CHK@key part */
			if (!string_end) {
				uri->keyid = strdup(key);
				strcat(uri_s, uri->keyid);
			}
			
			else {
				/* string_end points to the '/' character */
				uri->keyid = malloc(string_end - key + 1);
				strncpy(uri->keyid, key, string_end - key);

				strcat(uri_s, "/");

				/* point key to the first char after the '/' */
				key = (string_end + 1);
				
				string_end = strstr(key, "//");
				
				/* check for the case where there's nothing after the CHK@key/filename part :) */
				if (!string_end) {
					uri->filename = strdup(key);
					strcat(uri_s, uri->filename);
				}
				
				else { /* this shouldn't happen for CHK's */
					_fcpLog(FCP_LOG_DEBUG, "there's a metastring after the filename hint in the CHK@.. not supported");
				}
			}
		}
	}
	
	/* freenet:KSK@freetext.html */
  else if (!strncmp(key, "KSK@", 4)) {

    uri->type = KEY_TYPE_KSK;

		strcpy(uri_s, "freenet:KSK@");
    key += 4;

		uri->keyid = strdup(key);
		strcat(uri_s, uri->keyid);
  }
  
  else {
		_fcpLog(FCP_LOG_CRITICAL, "Error attempting to parse invalid key: %s", p_key);
    return -1;
  }

	uri->uri_str = strdup(uri_s);
	_fcpLog(FCP_LOG_DEBUG, "uri_str: %s", uri->uri_str);

  return 0;
}


/*************************************************************************/
/* FEC specific */
/*************************************************************************/


hSegment *_fcpCreateHSegment(void)
{
  hSegment *h;

	h = malloc(sizeof (hSegment));
  memset(h, 0, sizeof (hSegment));

  return h;
}

void _fcpDestroyHSegment(hSegment *h)
{
	if (h) {
		int i;

		if (h->header_str) free(h->header_str);

		if (h->db_count) {
			for (i=0; i < (unsigned short)h->db_count; i++) {
				_fcpDestroyHBlock(h->data_blocks[i]);
				free(h->data_blocks[i]);
			}
			
			free(h->data_blocks);
		}			
		
		if (h->cb_count) {
			for (i=0; i < (unsigned short)h->cb_count; i++) {
				_fcpDestroyHBlock(h->check_blocks[i]);
				free(h->check_blocks[i]);
			}
			
			free(h->check_blocks);
		}
	}
}

#ifdef fcpCreationTEST

int main(int c, char *argv[])
{
	char  s[513];
	hURI *uri;

	uri = fcpCreateHURI();
	strcpy(s, "SSK@TDtkQZpHTMbhGoSxWfR-ly8BTa4PAgM,phJ~IgAADJkijtDO7iAeSw/toad/1//index.html");
	
	fcpParseHURI(uri, s);
		
	printf(":%s:\n", uri->uri_str);
	
	return 0;
}

#endif
