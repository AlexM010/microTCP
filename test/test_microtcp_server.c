/*
 * microtcp, a lightweight implementation of TCP for teaching,
 * and academic purposes.
 *
 * Copyright (C) 2015-2017  Manolis Surligas <surligas@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * You can use this file to write a test microTCP server.
 * This file is already inserted at the build system.
 */
#include "../lib/microtcp.h"
#include "../lib/microtcp.h"
#include "../utils/crc32.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <arpa/inet.h>
int
main(int argc, char **argv)
{
    microtcp_sock_t socket;
    struct sockaddr_in client;
    int domain=AF_INET;
    int type=SOCK_DGRAM;
    int protocol=0;
    socket=microtcp_socket(domain,type,protocol);
    struct sockaddr_in sin;
    memset(&sin,0,sizeof(struct sockaddr_in));
    sin.sin_family=AF_INET;
    sin.sin_port=htons(40000);
    sin.sin_addr.s_addr=htonl(INADDR_ANY);
    microtcp_bind(&socket,(struct sockaddr*)&sin,sizeof(struct sockaddr_in));
    if(microtcp_accept(&socket,&client,sizeof(struct sockaddr) ) == -1){
        printf("Cannot accept\n");
        return -1;
    }else
        printf("TCP Connection Established\n");
    return 0;
}
