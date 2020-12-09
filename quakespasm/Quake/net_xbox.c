/*
Copyright (C) 1996-1997 Id Software, Inc.
Copyright (C) 2010-2014 QuakeSpasm developers

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#include <stdlib.h>
#include <string.h>

#include <lwip/debug.h>
#include <lwip/dhcp.h>
#include <lwip/init.h>
#include <lwip/netif.h>
#include <lwip/sys.h>
#include <lwip/tcpip.h>
#include <lwip/timeouts.h>
#include <lwip/errno.h>
#include <lwip/netdb.h>
#include <netif/etharp.h>
#include <pktdrv.h>

#include <hal/debug.h>
#include <windows.h>

#include "net_xbox.h"

static BOOL xnet_inited = 0;

// interface settings
static ip4_addr_t xnet_ip;
static ip4_addr_t xnet_netmask;
static ip4_addr_t xnet_gateway;
static ip4_addr_t xnet_dns;

// string versions
static char net_xbox_ip[20] = "0.0.0.0";
static char net_xbox_netmask[20] = "0.0.0.0";
static char net_xbox_gateway[20] = "0.0.0.0";

// this is referenced in the LWIP driver
struct netif *g_pnetif;
struct netif nforce_netif;

// this is not defined in any headers
err_t nforceif_init(struct netif *netif);

static void tcpip_init_done(void *arg)
{
	sys_sem_t *init_complete = (sys_sem_t *)arg;
	sys_sem_signal(init_complete);
}

static void packet_timer(void *arg)
{
	#define PKT_TMR_INTERVAL 5 /* ms */
	LWIP_UNUSED_ARG(arg);
	Pktdrv_ReceivePackets();
	sys_timeout(PKT_TMR_INTERVAL, packet_timer, NULL);
}

void NET_Xbox_Init(void)
{
	sys_sem_t init_complete;
	const ip4_addr_t *ip;
	BOOL use_dhcp = !strcmp(net_xbox_ip, "0.0.0.0");

	if(use_dhcp)
	{
		IP4_ADDR(&xnet_gateway, 0, 0, 0, 0);
		IP4_ADDR(&xnet_ip, 0, 0, 0, 0);
		IP4_ADDR(&xnet_netmask, 0, 0, 0, 0);
	}
	else
	{
		ip4addr_aton(net_xbox_ip, &xnet_ip);
		ip4addr_aton(net_xbox_gateway, &xnet_gateway);
		ip4addr_aton(net_xbox_netmask, &xnet_netmask);
	}

	sys_sem_new(&init_complete, 0);
	tcpip_init(tcpip_init_done, &init_complete);
	sys_sem_wait(&init_complete);
	sys_sem_free(&init_complete);

	g_pnetif = netif_add(&nforce_netif, &xnet_ip, &xnet_netmask, &xnet_gateway,
			NULL, nforceif_init, ethernet_input);
	if (!g_pnetif)
	{
			debugPrint("xbox_InitNet: netif_add failed\n");
			NET_Xbox_Shutdown();
			return;
	}

	netif_set_default(g_pnetif);
	netif_set_up(g_pnetif);

	if (use_dhcp)
	{
		dhcp_start(g_pnetif);
	}

	packet_timer(NULL);

	if (use_dhcp)
	{
		DWORD timeout;
		debugPrint("NET_Xbox_Init: Waiting for DHCP...\n");
		timeout = GetTickCount() + 15000; // time out in 15 sec
		while (dhcp_supplied_address(g_pnetif) == 0)
		{
			if (GetTickCount() > timeout)
			{
				debugPrint("NET_Xbox_Init: DHCP timed out\n");
				NET_Xbox_Shutdown();
				return;
			}
			NtYieldExecution();
		}
		debugPrint("NET_Xbox_Init: DHCP bound!\n");
	}

	xnet_inited = TRUE;
}

void NET_Xbox_Shutdown(void)
{
	sys_untimeout(packet_timer, NULL);
	Pktdrv_Quit();
	xnet_inited = FALSE;
}

int NET_Xbox_GetHostname(char *name, int namelen)
{
	if (!xnet_inited || !name)
	{
		errno = EFAULT;
		return -1;
	}

	const ip4_addr_t *localaddr = netif_ip4_addr(g_pnetif);
	if (localaddr)
	{
		char *ipstr = ip4addr_ntoa(localaddr);
		if (ipstr && *ipstr)
		{
			strncpy(name, ipstr, namelen);
			name[namelen - 1] = 0;
			return 0;
		}
	}

	errno = EINVAL;
	return -1;
}
