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

#include <stdlib.h>
#include <string.h>

extern long xtoi(char *);

extern int  _fcpSockRecv(hFCP *hfcp, char *resp, int len);
extern int  _fcpSockRecvln(hFCP *hfcp, char *resp, int len);


/* suppress a compiler warning */
char *strdup(const char *s);

static int  getrespHello(hFCP *);
static int  getrespSuccess(hFCP *);

static int  getrespDataFound(hFCP *);
static int  getrespDataChunk(hFCP *);
static int  getrespDataNotFound(hFCP *);
static int  getrespRouteNotFound(hFCP *);

static int  getrespUriError(hFCP *);
static int  getrespRestarted(hFCP *);
static int  getrespKeycollision(hFCP *);
static int  getrespPending(hFCP *);

static int  getrespFailed(hFCP *);
static int  getrespFormatError(hFCP *);

/* FEC specific responses */
static int  getrespSegmentHeaders(hFCP *);
static int  getrespBlocksEncoded(hFCP *);
static int  getrespMadeMetadata(hFCP *);

/* Utility functions.. not directly part of the protocol */
static int  getrespblock(hFCP *, char *respblock, int bytesreqd);


int _fcpRecvResponse(hFCP *hfcp)
{
	char resp[1025];
	int  rc;

	while (1) {

		rc = _fcpSockRecvln(hfcp, resp, 1024);

		/* return -1 on error, except if it's a TIMEOUT */
		if (rc <= 0)
			return (rc == FCP_ERR_TIMEOUT ? FCP_ERR_TIMEOUT : -1);
	
		if (!strncmp(resp, "NodeHello", 9)) {
			hfcp->response.type = FCPRESP_TYPE_NODEHELLO;
			return getrespHello(hfcp);
		}
		else if (!strcmp(resp, "Success")) {
			hfcp->response.type = FCPRESP_TYPE_SUCCESS;
			return getrespSuccess(hfcp);
		}
		
		else if (!strcmp(resp, "DataFound")) {
			hfcp->response.type = FCPRESP_TYPE_DATAFOUND;
			return getrespDataFound(hfcp);
		}
		else if (!strcmp(resp, "DataChunk")) {
			hfcp->response.type = FCPRESP_TYPE_DATACHUNK;
			return getrespDataChunk(hfcp);
		}
		else if (!strcmp(resp, "DataNotFound")) {
			hfcp->response.type = FCPRESP_TYPE_DATANOTFOUND;
			return getrespDataNotFound(hfcp);
		}
		else if (!strcmp(resp, "RouteNotFound")) {
			hfcp->response.type = FCPRESP_TYPE_ROUTENOTFOUND;
			return getrespRouteNotFound(hfcp);
		}
		
		else if (!strcmp(resp, "UriError")) {
			hfcp->response.type = FCPRESP_TYPE_URIERROR;
			return getrespUriError(hfcp);
		}
		else if (!strcmp(resp, "Restarted")) {
			hfcp->response.type = FCPRESP_TYPE_RESTARTED;
			return getrespRestarted(hfcp);
		}
		
		else if (!strcmp(resp, "KeyCollision")) {
			hfcp->response.type = FCPRESP_TYPE_KEYCOLLISION;
			return getrespKeycollision(hfcp);
		}
		else if (!strcmp(resp, "Pending")) {
			hfcp->response.type = FCPRESP_TYPE_PENDING;
			return getrespPending(hfcp);
		}
		
		else if (!strcmp(resp, "FormatError")) {
			hfcp->response.type = FCPRESP_TYPE_FORMATERROR;
			return getrespFormatError(hfcp);
		}
		else if (!strcmp(resp, "Failed")) {
			hfcp->response.type = FCPRESP_TYPE_FAILED;
			return getrespFailed(hfcp);
		}
		
		/* FEC specific */
		else if (!strcmp(resp, "SegmentHeader")) {
			hfcp->response.type = FCPRESP_TYPE_SEGMENTHEADER;
			return getrespSegmentHeaders(hfcp);
		}
		else if (!strcmp(resp, "BlocksEncoded")) {
			hfcp->response.type = FCPRESP_TYPE_BLOCKSENCODED;
			return getrespBlocksEncoded(hfcp);
		}
		else if (!strcmp(resp, "MadeMetadata")) {
			hfcp->response.type = FCPRESP_TYPE_MADEMETADATA;
			return getrespMadeMetadata(hfcp);
		}
		
		/* Else, send a warning; a little loose, but this is still in development */
		else {
			_fcpLog(FCP_LOG_DEBUG, "_fcpRecvResponse(): received unhandled field \"%s\"", resp);
		}
	}
	
	return 0;
}


/*
	getrespHello()

	Note this function copies the info directly into hfcp structure
*/
static int getrespHello(hFCP *hfcp)
{
	char resp[1025];
	int  rc;

	_fcpLog(FCP_LOG_DEBUG, "received NodeHello response");

	while ((rc = _fcpSockRecvln(hfcp, resp, 1024)) > 0) {

		_fcpLog(FCP_LOG_DEBUG, "NodeHello: %s", resp);

		if (!strncmp(resp, "MaxFileSize=", 12)) {
			hfcp->max_filesize = xtoi(resp+12);
		}

		else if (!strncmp(resp, "HighestSeenBuild=", 17)) {
			hfcp->highest_build = atoi(resp+17);
		}

		else if (!strncmp(resp, "Node=", 5)) {
			if (hfcp->description) free(hfcp->description);
			hfcp->description = strdup(resp+5);
		}

		else if (!strncmp(resp, "Protocol=", 9)) {
			if (hfcp->protocol) free(hfcp->protocol);
			hfcp->protocol = strdup(resp+9);
		}

		else if (!strncmp(resp, "EndMessage", 10))
			return FCPRESP_TYPE_NODEHELLO;

		else
		_fcpLog(FCP_LOG_DEBUG, "getrespHello(): received unhandled field \"%s\"", resp);
	}

	if (rc < 0)
		return (rc == FCP_ERR_TIMEOUT ? FCP_ERR_TIMEOUT : -1);
	else
		return 0;
}


static int getrespSuccess(hFCP *hfcp)
{
	char resp[1025];
	int  rc;

	_fcpLog(FCP_LOG_DEBUG, "received Success response");

	while ((rc = _fcpSockRecvln(hfcp, resp, 1024)) > 0) {

		if (!strncmp(resp, "URI=", 4)) {
			if (hfcp->response.success.uri) free(hfcp->response.success.uri);

			hfcp->response.success.uri = strdup(resp+4);

			_fcpLog(FCP_LOG_DEBUG, "getrespSuccess: uri=%s", hfcp->response.success.uri);
		}
		
		else if (!strncmp(resp, "PublicKey=", 10)) {
			strncpy(hfcp->response.success.publickey, resp + 10, L_KEY);
		}
		
		else if (!strncmp(resp, "PrivateKey=", 11)) {
			strncpy(hfcp->response.success.privatekey, resp + 11, L_KEY);
		}
		
		else if (!strncmp(resp, "EndMessage", 10)) {
			return FCPRESP_TYPE_SUCCESS;
		}

		else
		_fcpLog(FCP_LOG_DEBUG, "getrespSuccess(): received unhandled field \"%s\"", resp);
  }
	
	if (rc < 0)
		return (rc == FCP_ERR_TIMEOUT ? FCP_ERR_TIMEOUT : -1);
	else
		return 0;
	
}


static int getrespDataFound(hFCP *hfcp)
{
	char resp[1025];
	int  rc;

	_fcpLog(FCP_LOG_DEBUG, "received DataFound response");

	hfcp->response.datafound.datalength = 0;
	hfcp->response.datafound.metadatalength = 0;

	while ((rc = _fcpSockRecvln(hfcp, resp, 1024)) > 0) {

		if (!strncmp(resp, "DataLength=", 11))
			hfcp->response.datafound.datalength = xtoi(resp + 11);
		
		else if (strncmp(resp, "MetadataLength=", 15) == 0)
			hfcp->response.datafound.metadatalength = xtoi(resp + 15);
		
		else if (!strncmp(resp, "EndMessage", 10))
			return FCPRESP_TYPE_DATAFOUND;

		else
		_fcpLog(FCP_LOG_DEBUG, "getrespDataFound(): received unhandled field \"%s\"", resp);
	}
	
	if (rc < 0)
		return (rc == FCP_ERR_TIMEOUT ? FCP_ERR_TIMEOUT : -1);
	else
		return 0;
}


static int getrespDataChunk(hFCP *hfcp)
{
	char resp[1025];
	int  rc;

	while ((rc = _fcpSockRecvln(hfcp, resp, 1024)) > 0) {

		if (!strncmp(resp, "Length=", 7))
			hfcp->response.datachunk.length = xtoi(resp + 7);
		
		else if (!strncmp(resp, "Data", 4))	{
			int len;

			len = hfcp->response.datachunk.length;
			if (hfcp->response.datachunk.data) free(hfcp->response.datachunk.data);
			hfcp->response.datachunk.data = (char *)malloc(len);
			
			/* get len bytes of data */
			if (getrespblock(hfcp, hfcp->response.datachunk.data, len) != len)
				return -1;

			return FCPRESP_TYPE_DATACHUNK;
		}

		else
			_fcpLog(FCP_LOG_DEBUG, "getrespDataChunk(): received unhandled field \"%s\"", resp);
	}

	if (rc < 0)
		return (rc == FCP_ERR_TIMEOUT ? FCP_ERR_TIMEOUT : -1);
	else
		return 0;
}


static int getrespDataNotFound(hFCP *hfcp)
{
	char resp[1025];
	int  rc;

	_fcpLog(FCP_LOG_DEBUG, "received DataNotFound response");

	while ((rc = _fcpSockRecvln(hfcp, resp, 1024)) > 0) {

		if (!strncmp(resp, "EndMessage", 10))
			return FCPRESP_TYPE_DATANOTFOUND;

		else
			_fcpLog(FCP_LOG_DEBUG, "getrespDataNotFound(): received unhandled field \"%s\"", resp);
	}

	if (rc < 0)
		return (rc == FCP_ERR_TIMEOUT ? FCP_ERR_TIMEOUT : -1);
	else
		return 0;
}


static int getrespRouteNotFound(hFCP *hfcp)
{
	char resp[1025];
	int  rc;

	_fcpLog(FCP_LOG_DEBUG, "received RouteNotFound response");

	while ((rc = _fcpSockRecvln(hfcp, resp, 1024)) > 0) {

		if (!strncmp(resp, "Reason=", 7)) {
			if (hfcp->response.routenotfound.reason) free(hfcp->response.routenotfound.reason);
			hfcp->response.routenotfound.reason = strdup(resp + 7);
		}

		else if (!strncmp(resp, "Unreachable=", 12))
			hfcp->response.routenotfound.unreachable = xtoi(resp + 12);

		else if (!strncmp(resp, "Restarted=", 10))
			hfcp->response.routenotfound.restarted = xtoi(resp + 10);

		else if (!strncmp(resp, "Rejected=", 9))
			hfcp->response.routenotfound.rejected = xtoi(resp + 9);

		else if (!strncmp(resp, "EndMessage", 10))
			return FCPRESP_TYPE_ROUTENOTFOUND;

		else
			_fcpLog(FCP_LOG_DEBUG, "getrespRouteNotFound(): received unhandled field \"%s\"", resp);
	}
	
	if (rc < 0)
		return (rc == FCP_ERR_TIMEOUT ? FCP_ERR_TIMEOUT : -1);
	else
		return 0;
}


static int getrespUriError(hFCP *hfcp)
{
	char resp[1025];
	int  rc;

	_fcpLog(FCP_LOG_DEBUG, "received UriError response");

	while ((rc = _fcpSockRecvln(hfcp, resp, 1024)) > 0) {

		if (!strncmp(resp, "EndMessage", 10))
			return FCPRESP_TYPE_URIERROR;

		else
			_fcpLog(FCP_LOG_DEBUG, "getrespUriError(): received unhandled field \"%s\"", resp);
	}
	
	if (rc < 0)
		return (rc == FCP_ERR_TIMEOUT ? FCP_ERR_TIMEOUT : -1);
	else
		return 0;
}


static int getrespRestarted(hFCP *hfcp)
{
	char resp[1025];
	int  rc;

	_fcpLog(FCP_LOG_DEBUG, "received Restarted response");

	while ((rc = _fcpSockRecvln(hfcp, resp, 1024)) > 0) {

		if (!strncmp(resp, "Timeout=", 8)) {
			hfcp->response.restarted.timeout = xtoi(resp + 8);
		}

		else if (!strncmp(resp, "EndMessage", 10))
			return FCPRESP_TYPE_RESTARTED;

		else
			_fcpLog(FCP_LOG_DEBUG, "getrespRestarted(): received unhandled field \"%s\"", resp);
	}
	
	if (rc < 0)
		return (rc == FCP_ERR_TIMEOUT ? FCP_ERR_TIMEOUT : -1);
	else
		return 0;
}


static int getrespKeycollision(hFCP *hfcp)
{
	char resp[1025];
	int  rc;

	_fcpLog(FCP_LOG_DEBUG, "received KeyCollision response");

	while ((rc = _fcpSockRecvln(hfcp, resp, 1024)) > 0) {

		if (!strncmp(resp, "URI=", 4)) {
			if (hfcp->response.keycollision.uri) free(hfcp->response.keycollision.uri);

			hfcp->response.keycollision.uri = strdup(resp+4);

			_fcpLog(FCP_LOG_DEBUG, "getrespKeycollision: uri=%s", hfcp->response.keycollision.uri);
		}
		
		else if (!strncmp(resp, "PublicKey=", 10)) {
			strncpy(hfcp->response.keycollision.publickey, resp + 10, L_KEY);
		}
		
		else if (!strncmp(resp, "PrivateKey=", 11)) {
			strncpy(hfcp->response.keycollision.privatekey, resp + 11, L_KEY);
		}
		
		else if (!strncmp(resp, "EndMessage", 10))
			return FCPRESP_TYPE_KEYCOLLISION;

		else
			_fcpLog(FCP_LOG_DEBUG, "getrespKeyCollision(): received unhandled field \"%s\"", resp);
  }
	
	if (rc < 0)
		return (rc == FCP_ERR_TIMEOUT ? FCP_ERR_TIMEOUT : -1);
	else
		return 0;
}


/*
  Function:    getrespPending()

  Arguments    hfcpconn - Freenet FCP handle
*/

static int getrespPending(hFCP *hfcp)
{
	char resp[1025];
	int  rc;

	_fcpLog(FCP_LOG_DEBUG, "received Pending response");

	while ((rc = _fcpSockRecvln(hfcp, resp, 1024)) > 0) {

		if (!strncmp(resp, "URI=", 4)) {
			if (hfcp->response.pending.uri) free(hfcp->response.pending.uri);

			hfcp->response.pending.uri = strdup(resp+4);

			_fcpLog(FCP_LOG_DEBUG, "getrespPending: uri=%s", hfcp->response.pending.uri);
		}
		
		else if (!strncmp(resp, "PublicKey=", 10)) {
			strncpy(hfcp->response.pending.publickey, resp + 10, L_KEY);
		}
		
		else if (!strncmp(resp, "PrivateKey=", 11)) {
			strncpy(hfcp->response.pending.privatekey, resp + 11, L_KEY);
		}

		else if (!strncmp(resp, "Timeout=", 8)) { /* milliseconds */
			hfcp->response.pending.timeout = xtoi(resp + 8);
			hfcp->timeout = hfcp->response.pending.timeout;
		}
		
		else if (!strncmp(resp, "EndMessage", 10))
			return FCPRESP_TYPE_PENDING;

		else
			_fcpLog(FCP_LOG_DEBUG, "getrespPending(): received unhandled field \"%s\"", resp);
  }
	
	if (rc < 0)
		return (rc == FCP_ERR_TIMEOUT ? FCP_ERR_TIMEOUT : -1);
	else
		return 0;
}


/*
  Function:    getrespFailed()

  Arguments    hfcpconn - Freenet FCP handle

  Returns      FCPRESP_TYPE_SUCCESS if successful, -1 otherwise

  Description: reads in and processes details of Failed response
*/

static int getrespFailed(hFCP *hfcp)
{
	char resp[1025];
	int  rc;

	_fcpLog(FCP_LOG_DEBUG, "received Failed response");

	while ((rc = _fcpSockRecvln(hfcp, resp, 1024)) > 0) {

		if (!strncmp(resp, "Reason=", 7)) {
			if (hfcp->response.failed.reason) free(hfcp->response.failed.reason);
			
			hfcp->response.failed.reason = strdup(resp + 7);
		}
		
		else if (!strncmp(resp, "EndMessage", 10))
			return FCPRESP_TYPE_FAILED;

		else
			_fcpLog(FCP_LOG_DEBUG, "getrespFailed(): received unhandled field \"%s\"", resp);
  }
  
	if (rc < 0)
		return (rc == FCP_ERR_TIMEOUT ? FCP_ERR_TIMEOUT : -1);
	else
		return 0;
}


/*
	Function:    getrespFormatError()
	
	Arguments:   fcpconn - connection block
	
	Returns:     0 if successful, -1 otherwise
	
	Description: Gets the details of a format error
*/

static int getrespFormatError(hFCP *hfcp)
{
	char resp[1025];
	int  rc;

	_fcpLog(FCP_LOG_DEBUG, "received FormatError response");

	while ((rc = _fcpSockRecvln(hfcp, resp, 1024)) > 0) {

		if (!strncmp(resp, "Reason=", 7)) {
			if (hfcp->response.formaterror.reason) free(hfcp->response.formaterror.reason);
			
			hfcp->response.formaterror.reason = strdup(resp + 7);
			_fcpLog(FCP_LOG_DEBUG, "formaterror.reason: %s", resp + 7);
		}
		
		else if (!strncmp(resp, "EndMessage", 10))
			return FCPRESP_TYPE_FORMATERROR;

		else
			_fcpLog(FCP_LOG_DEBUG, "getrespFormatError(): received unhandled field \"%s\"", resp);
  }
  
	if (rc < 0)
		return (rc == FCP_ERR_TIMEOUT ? FCP_ERR_TIMEOUT : -1);
	else
		return 0;
}


static int  getrespSegmentHeaders(hFCP *hfcp)
{
	char resp[1025];
	int  rc;

	_fcpLog(FCP_LOG_DEBUG, "received SegmentHeader response");

	while ((rc = _fcpSockRecvln(hfcp, resp, 1024)) > 0) {

		if (!strncmp(resp, "FECAlgorithm=", 13))
			strncpy(hfcp->response.segmentheader.fec_algorithm, resp + 13, L_KEY);

		else if (!strncmp(resp, "FileLength=", 11))
			hfcp->response.segmentheader.filelength = xtoi(resp + 11);

		else if (!strncmp(resp, "Offset=", 7))
			hfcp->response.segmentheader.offset = xtoi(resp + 7);

		else if (!strncmp(resp, "BlockCount=", 11))
			hfcp->response.segmentheader.block_count = xtoi(resp + 11);

		else if (!strncmp(resp, "BlockSize=", 10))
			hfcp->response.segmentheader.block_size = xtoi(resp + 10);

		else if (!strncmp(resp, "DataBlockOffset=", 16))
			hfcp->response.segmentheader.datablock_offset = xtoi(resp + 16);

		else if (!strncmp(resp, "CheckBlockCount=", 16))
			hfcp->response.segmentheader.checkblock_count = xtoi(resp + 16);

		else if (!strncmp(resp, "CheckBlockSize=", 15))
			hfcp->response.segmentheader.checkblock_size = xtoi(resp + 15);

		else if (!strncmp(resp, "CheckBlockOffset=", 17))
			hfcp->response.segmentheader.checkblock_offset = xtoi(resp + 17);

		else if (!strncmp(resp, "Segments=", 9))
			hfcp->response.segmentheader.segments = xtoi(resp + 9);

		else if (!strncmp(resp, "SegmentNum=", 11))
			hfcp->response.segmentheader.segment_num = xtoi(resp + 11);

		else if (!strncmp(resp, "BlocksRequired=", 15))
			hfcp->response.segmentheader.blocks_required = xtoi(resp + 15);

 		else if (!strncmp(resp, "EndMessage", 10))
			return FCPRESP_TYPE_SEGMENTHEADER;

		else
			_fcpLog(FCP_LOG_DEBUG, "getrespSegmentHeaders(): received unhandled field \"%s\"", resp);
  }

  /* oops.. there's been a socket error of sorts */
	if (rc < 0)
		return (rc == FCP_ERR_TIMEOUT ? FCP_ERR_TIMEOUT : -1);
	else
		return 0;
}


static int getrespBlocksEncoded(hFCP *hfcp)
{
	char resp[1025];
	int  rc;

	_fcpLog(FCP_LOG_DEBUG, "received BlocksEncoded response");

	while ((rc = _fcpSockRecvln(hfcp, resp, 1024)) > 0) {

		if (!strncmp(resp, "BlockCount=", 11))
			hfcp->response.blocksencoded.block_count = xtoi(resp + 11);

		else if (!strncmp(resp, "BlockSize=", 10))
			hfcp->response.blocksencoded.block_size = xtoi(resp + 10);

 		else if (!strncmp(resp, "EndMessage", 10))
			return FCPRESP_TYPE_BLOCKSENCODED;

		else
			_fcpLog(FCP_LOG_DEBUG, "getrespBlocksEncoded(): received unhandled field \"%s\"", resp);
	}
	
	if (rc < 0)
		return (rc == FCP_ERR_TIMEOUT ? FCP_ERR_TIMEOUT : -1);
	else
		return 0;
}

static int getrespMadeMetadata(hFCP *hfcp)
{
	char resp[1025];
	int  rc;

	_fcpLog(FCP_LOG_DEBUG, "received MadeMetadata response");

	while ((rc = _fcpSockRecvln(hfcp, resp, 1024)) > 0) {

		if (!strncmp(resp, "DataLength=", 11))
			hfcp->response.mademetadata.datalength = xtoi(resp + 11);

		else if (!strncmp(resp, "EndMessage", 10))
			return FCPRESP_TYPE_MADEMETADATA;

		else
			_fcpLog(FCP_LOG_DEBUG, "getrespMadeMetadata(): received unhandled field \"%s\"", resp);
	}
	
	if (rc < 0)
		return (rc == FCP_ERR_TIMEOUT ? FCP_ERR_TIMEOUT : -1);
	else
		return 0;
}


/**********************************************************************/

/*
	Function:    getrespblock()

	Arguments:   fcpconn - connection block

	Returns:     number of bytes read if successful, -1 otherwise

	Description: Reads an arbitrary number of bytes from connection
*/

static int getrespblock(hFCP *hfcp, char *respblock, int bytesreqd)
{
	int i = 0;
	char *cp = respblock;
	
	while (bytesreqd > 0) {
		/* now, try to get and return desired number of bytes */

		if ((i = recv(hfcp->socket, cp, bytesreqd, 0)) > 0) {
			/* Increment current pointer (cp), and decrement bytes required
				 to read.
			*/
			cp += i;
			bytesreqd -= i;
		}
		else if (i == 0)
			break;      /* connection closed - got all we're gonna get */
		else
			return -1;  /* connection failure */
	}

	/* Return the bytes read */
	return cp - respblock;
}

