/*
 *  Hans - IP over ICMP
 *  Copyright (C) 2009 Friedrich Schöller <friedrich.schoeller@gmail.com>
 *  
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *  
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *  
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *  
 */

#include "tun.h"
#include "exception.h"
#include "utility.h"

#include <arpa/inet.h>
#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <syslog.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>

typedef ip IpHeader;

using namespace std;

Tun::Tun(const char *device, int mtu)
{
	this->mtu = mtu;

	if (device != NULL)
	{
		strncpy(this->device, device, VTUN_DEV_LEN);
		this->device[VTUN_DEV_LEN] = 0;
	}
	else
		this->device[0] = 0;

	fd = tun_open(this->device);
	if (fd == -1)
		throw Exception("could not create tunnel device");

	char cmdline[512];
	snprintf(cmdline, sizeof(cmdline), "/sbin/ifconfig %s mtu %u", this->device, mtu);
	if (popen(cmdline, "r") != 0)
		syslog(LOG_ERR, "could not set tun device mtu");
}

Tun::~Tun()
{
	tun_close(fd, device);
}

void Tun::setIp(uint32_t ip, uint32_t destIp, bool includeSubnet)
{
	char cmdline[512];
	string ips = Utility::formatIp(ip);
	string destIps = Utility::formatIp(destIp);

#ifdef LINUX
	snprintf(cmdline, sizeof(cmdline), "/sbin/ifconfig %s %s netmask 255.255.255.0", device, ips.c_str());
#else
	snprintf(cmdline, sizeof(cmdline), "/sbin/ifconfig %s %s %s netmask 255.255.255.255", device, ips.c_str(), destIps.c_str());
#endif

	if (popen(cmdline, "r") != 0)
		syslog(LOG_ERR, "could not set tun device ip address");

#ifndef LINUX
	if (includeSubnet)
	{
		snprintf(cmdline, sizeof(cmdline), "/sbin/route add %s/24 %s", destIps.c_str(), destIps.c_str());
		if (popen(cmdline, "r") != 0)
			syslog(LOG_ERR, "could not add route");
	}
#endif
}

void Tun::write(const char *buffer, int length)
{
	if (tun_write(fd, (char *)buffer, length) == -1)
		syslog(LOG_ERR, "error writing %d bytes to tun: %s", length, strerror(errno));
}

int Tun::read(char *buffer)
{
	int length = tun_read(fd, buffer, mtu);
	if (length == -1)
		syslog(LOG_ERR, "error reading from tun: %s", strerror(errno));
	return length;
}

int Tun::read(char *buffer, uint32_t &sourceIp, uint32_t &destIp)
{
	int length = read(buffer);

	IpHeader *header = (IpHeader *)buffer;
	sourceIp = ntohl(header->ip_src.s_addr);
	destIp = ntohl(header->ip_dst.s_addr);

	return length;
}
