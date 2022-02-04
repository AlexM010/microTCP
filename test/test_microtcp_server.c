#include <stdint.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include "../lib/microtcp.h"
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h> // for close
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

int
main(int argc, char **argv)
{
   
    microtcp_sock_t sock=microtcp_socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP) ;
    struct sockaddr_in sin;
    struct sockaddr client;
    socklen_t len =  sizeof(struct sockaddr);
    memset(&sin, 0, sizeof(struct sockaddr_in));
    sin.sin_family = AF_INET;
    sin.sin_port = htons(atoi(argv[1]));
    sin.sin_addr.s_addr = htonl(INADDR_ANY);


    if(microtcp_bind(&sock,(struct sockaddr *) &sin,
    sizeof(struct sockaddr_in)) ==  -1) {
        perror("TCP bind");
        exit(EXIT_FAILURE);
    }
    if(microtcp_accept(&sock,&client,len ) == -1){
        printf("Cannot accept\n");
        return -1;
    }else
        printf("TCP Connection Established\n");
    if(microtcp_shutdown(&sock,SHUT_RDWR) ==-1){
        perror("shutdown");
        exit(EXIT_FAILURE);
    }
    printf("TCP connection closed\n");
    return 0;
}
