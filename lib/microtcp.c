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

#include "microtcp.h"
#include "../utils/crc32.h"
#include <string.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>
#include <arpa/inet.h> 
#include <ctype.h>
#include <stdlib.h>
 

 
microtcp_sock_t
microtcp_socket (int domain, int type, int protocol)
{
  microtcp_sock_t sock;
  sock.state=UNKNOWN;
  sock.sd=socket (domain, type, protocol);
  if (sock.sd < 0){
     perror ("Socket");
     sock.state=INVALID;
  }
  sock.init_win_size=MICROTCP_WIN_SIZE;
  sock.curr_win_size=MICROTCP_WIN_SIZE;
  sock.cwnd=MICROTCP_INIT_CWND;
  sock.ssthresh=MICROTCP_INIT_SSTHRESH;
  sock.buf_fill_level=0;
  sock.shuts=TRUE;
  return sock;
}

int
microtcp_bind (microtcp_sock_t *socket, const struct sockaddr *address,
               socklen_t address_len)
{
  int ret;
  ret=bind (socket->sd, address, address_len);
  if (ret < 0){
     perror ("Bind");
     socket->state=INVALID;
  }
  socket->shuts=FALSE;
  return ret;
}

int
microtcp_connect (microtcp_sock_t *socket, const struct sockaddr *address,
                  socklen_t address_len)
{
  srand(time(NULL));
  int tmp;
  ssize_t tmp_len;
  microtcp_header_t msg;
  microtcp_header_t rec;
  uint8_t buf[MICROTCP_RECVBUF_LEN];

  for(int i=0; i<MICROTCP_RECVBUF_LEN; i++)
    buf[i]=0;
  socket->address=(struct sockaddr*)address;
  socket->address_len=address_len;
 
  socket->seq_number=rand()%10000;
  msg.seq_number=htonl(socket->seq_number);
  // 000 0  0 0 0 0 0 0 0 0 ACK=0 0 SYN=1 FIN=0
  msg.control=htons((uint16_t)2);
  msg.data_len=0;
  msg.ack_number=0;
  msg.window=0;
  msg.future_use0=0;
  msg.future_use1=0;
  msg.future_use2=0;
  msg.checksum=0;
  //copy the header to the buffer
  memcpy (buf,&msg, sizeof(microtcp_header_t));
  msg.checksum=htonl(crc32((const uint8_t *)&msg, sizeof(microtcp_header_t)));
  tmp=sendto(socket->sd,(void*)&msg,sizeof(microtcp_header_t),0,address,address_len);
  if (tmp<0) {
    perror("Cant Establish socket");
    socket->state=INVALID;
    return -1;
  }
  memset(buf,0,MICROTCP_RECVBUF_LEN);
  if(tmp_len=recvfrom(socket->sd,buf,MICROTCP_RECVBUF_LEN,0,(struct sockaddr*) address,&address_len)<0){
    socket->state=INVALID;
    perror("Server did not accept connection");
    return -1;
  }


  memcpy(&rec,buf,sizeof(microtcp_header_t));
  int check=rec.checksum;
  rec.checksum=0;

   if((uint32_t)check!=ntohl(crc32((const uint8_t *)&rec,sizeof(microtcp_header_t)))){
    perror("Received packet destroyed1");
    socket->state=INVALID;
    return -1;
  }
  rec.seq_number=ntohl(rec.seq_number);
  rec.control=ntohs(rec.control);
  rec.data_len=ntohl(rec.data_len);
  rec.ack_number=ntohl(rec.ack_number);
  rec.window=ntohs(rec.window);
  rec.future_use0=ntohl(rec.future_use0);
  rec.future_use1=ntohl(rec.future_use1);
  rec.future_use2=ntohl(rec.future_use2);
  rec.checksum=ntohl(rec.checksum);

  if(rec.control!=(uint16_t)10){
    perror("Server did not accept connection");
    socket->state=INVALID;
    return -1;
  }
  if(socket->seq_number!=rec.ack_number-1){
    perror("Received packet destroyed2");
    socket->state=INVALID;
    return -1;
  }

  socket->ack_number=rec.seq_number+1;
  socket->seq_number++;
  socket->curr_win_size=rec.window;
  msg.seq_number=htonl(socket->seq_number);
  // 000 0  0 0 0 0 0 0 0 0 ACK=1 0 SYN=0 FIN=0
  msg.control=htons((uint16_t)8); //possible error
  msg.data_len=0;
  msg.ack_number=htonl(socket->ack_number);
  msg.window=htonl(socket->init_win_size);
  msg.future_use0=0;
  msg.future_use1=0;
  msg.future_use2=0;
  msg.checksum=0;

  memset(buf,0,MICROTCP_RECVBUF_LEN);
  memcpy(buf,&msg,sizeof(microtcp_header_t));
  msg.checksum=htonl(crc32((const uint8_t *)buf, sizeof(microtcp_header_t)));
  tmp=sendto(socket->sd,(void*)&msg,sizeof(microtcp_header_t),0,(struct sockaddr*)address,address_len);
  if (tmp<0) {
    perror("Cant Establish socket");
    socket->state=INVALID;
    socket->ack_number=0;
    socket->seq_number--;
    return -1;
  }
  socket->state=ESTABLISHED;
  return 0;
}

int
microtcp_accept (microtcp_sock_t *socket, struct sockaddr *address,
                 socklen_t address_len)
{
  srand(time(NULL));
  int tmp,seq,ack;
  ssize_t tmp_len;
  microtcp_header_t msg;
  microtcp_header_t rec;
  uint8_t buf[MICROTCP_RECVBUF_LEN];
  socket->address=address;
  socket->address_len=address_len;
  //receiving packet
  memset(buf,0,MICROTCP_RECVBUF_LEN);
  if(tmp_len=recvfrom(socket->sd,buf,MICROTCP_RECVBUF_LEN,0,(struct sockaddr*)address,&address_len)<0){
    socket->state=INVALID;
    perror("No packet from client received");
    return -1;
  }
  memcpy(&rec,buf,sizeof(microtcp_header_t));
  int check=rec.checksum;
  rec.checksum=0;

   if((uint32_t)check!=ntohl(crc32((const uint8_t *)&rec,sizeof(microtcp_header_t)))){
    perror("Received packet destroyed3");
    socket->state=INVALID;
    return -1;
  }
  rec.seq_number=ntohl(rec.seq_number);
  rec.control=ntohs(rec.control);
  rec.data_len=ntohl(rec.data_len);
  rec.ack_number=ntohl(rec.ack_number);
  rec.window=ntohs(rec.window);
  rec.future_use0=ntohl(rec.future_use0);
  rec.future_use1=ntohl(rec.future_use1);
  rec.future_use2=ntohl(rec.future_use2);


  if(rec.control!=(uint16_t)2){
    perror("Packet has not SYN=1");
    socket->state=INVALID;
    return -1;
  }
  //creating packet for 2nd handshake
  seq=rand()%10000;
  ack=rec.seq_number+1;

  msg.seq_number=htonl(seq);
  // 000 0  0 0 0 0 0 0 0 0 ACK=1 0 SYN=1 FIN=0
  msg.control=htons((uint16_t)10); //possible error
  msg.data_len=0;
  msg.ack_number=htonl(ack);
  msg.window=htonl(socket->curr_win_size);
  msg.future_use0=0;
  msg.future_use1=0;
  msg.future_use2=0;
  msg.checksum=0;
  memset(buf,0,MICROTCP_RECVBUF_LEN);
  memcpy(buf,&msg,sizeof(microtcp_header_t));
  msg.checksum=htonl(crc32((const uint8_t *)buf, sizeof(microtcp_header_t)));
  tmp=sendto(socket->sd,(void*)&msg,sizeof(microtcp_header_t),0,(struct sockaddr*)address,address_len);
  if (tmp<0) {
    perror("Cant Establish socket");
    socket->state=INVALID;
    return -1;
  }
    //receiving 2nd packet
  memset(buf,0,MICROTCP_RECVBUF_LEN);
  if(tmp_len=recvfrom(socket->sd,buf,MICROTCP_RECVBUF_LEN,0,(struct sockaddr*)address,&address_len)<0){
    socket->state=INVALID;
    perror("No packet from client received");
    return -1;
  }

  memcpy(&rec,buf,sizeof(microtcp_header_t));
  check=rec.checksum;
  rec.checksum=0;

   if((uint32_t)check!=ntohl(crc32((const uint8_t *)&rec,sizeof(microtcp_header_t)))){
    perror("Received packet destroyed4");
    socket->state=INVALID;
    return -1;
  }
  rec.seq_number=ntohl(rec.seq_number);
  rec.control=ntohs(rec.control);
  rec.data_len=ntohl(rec.data_len);
  rec.ack_number=ntohl(rec.ack_number);
  rec.window=ntohs(rec.window);
  rec.future_use0=ntohl(rec.future_use0);
  rec.future_use1=ntohl(rec.future_use1);
  rec.future_use2=ntohl(rec.future_use2);
  rec.checksum=ntohl(rec.checksum);
 
  if(rec.control!=(uint16_t)8){
    perror("Packet has not ACK=1");
    socket->state=INVALID;
    return -1;
  }
  if(rec.seq_number!=(uint32_t)ack||rec.ack_number!=(uint32_t)seq+1){
    perror("Received packet not valid");
    socket->state=INVALID;
    return -1;
  }
  //adding data received to buffer
  socket->seq_number=seq+1;
  socket->ack_number=ack+1;
  //coping received data to servers buffer for later
  /*
  socket->recvbuf=malloc(MICROTCP_RECVBUF_LEN);
  memcpy(buf+sizeof(microtcp_header_t),socket->recvbuf,rec.data_len);
  socket->buf_fill_len=rec.data_len;
  socket->curr_win_size-=rec.data_len;
  */
  socket->state=ESTABLISHED;
  return 0;
}

int
microtcp_shutdown (microtcp_sock_t *socket, int how)
{
  int tmp,seq,ack,check;
  ssize_t tmp_len;
  microtcp_header_t msg;
  microtcp_header_t rec;
  uint8_t buf[MICROTCP_RECVBUF_LEN]={0};
  if(socket->shuts==TRUE){
    msg.seq_number=htonl(socket->seq_number);
    // 000 0  0 0 0 0 0 0 0 0 ACK=1 0 SYN=0 FIN=1
    msg.control=htons((uint16_t)9); //possible error
    msg.data_len=0;
    msg.ack_number=htonl(socket->ack_number);
    msg.window=htonl(socket->curr_win_size);
    msg.future_use0=0;
    msg.future_use1=0;
    msg.future_use2=0;
    msg.checksum=0;
    memset(buf,0,MICROTCP_RECVBUF_LEN);
    memcpy(buf,&msg,sizeof(microtcp_header_t));
    msg.checksum=htonl(crc32((const uint8_t *)buf, sizeof(microtcp_header_t)));
    tmp=sendto(socket->sd,(void*)&msg,sizeof(microtcp_header_t),0,socket->address,socket->address_len);
    if (tmp<0) {
      perror("Cant Establish socket");
      socket->state=INVALID;
      return -1;
    }

    memset(buf,0,MICROTCP_RECVBUF_LEN);
    if(tmp_len=recvfrom(socket->sd,buf,MICROTCP_RECVBUF_LEN,0,(struct sockaddr*)socket->address,&socket->address_len)<0){
      socket->state=INVALID;
      perror("No packet from client received");
      return -1;
    }
    memcpy(&rec,buf,sizeof(microtcp_header_t));
    check=rec.checksum;
    rec.checksum=0;
    if((uint32_t)check!=ntohl(crc32((const uint8_t *)&rec,sizeof(microtcp_header_t)))){
      perror("Received packet destroyed5");
      socket->state=INVALID;
      return -1;
    }
    rec.seq_number=ntohl(rec.seq_number);
    rec.control=ntohs(rec.control);
    rec.data_len=ntohl(rec.data_len);
    rec.ack_number=ntohl(rec.ack_number);
    rec.window=ntohs(rec.window);
    rec.future_use0=ntohl(rec.future_use0);
    rec.future_use1=ntohl(rec.future_use1);
    rec.future_use2=ntohl(rec.future_use2);
    rec.checksum=ntohl(rec.checksum);
    
    if(rec.control!=(uint16_t)8){
      perror("Packet has not ACK=1");
      socket->state=INVALID;
      return -1;
    }
    if(rec.ack_number!=(uint32_t)socket->seq_number){
      perror("Received packet not valid");
      socket->state=INVALID;
      return -1;
    }
    socket->seq_number++;
    socket->state=CLOSING_BY_HOST;
    memset(buf,0,MICROTCP_RECVBUF_LEN);
    if(tmp_len=recvfrom(socket->sd,buf,MICROTCP_RECVBUF_LEN,0,(struct sockaddr*)socket->address,&socket->address_len)<0){
      socket->state=INVALID;
      perror("No packet from client received");
      return -1;
    }

    memcpy(&rec,buf,sizeof(microtcp_header_t));
    check=rec.checksum;
    rec.checksum=0;
    if((uint32_t)check!=ntohl(crc32((const uint8_t *)&rec,sizeof(microtcp_header_t)))){
      perror("Received packet destroyed6");
      socket->state=INVALID;
      return -1;
    }
    rec.seq_number=ntohl(rec.seq_number);
    rec.control=ntohs(rec.control);
    rec.data_len=ntohl(rec.data_len);
    rec.ack_number=ntohl(rec.ack_number);
    rec.window=ntohs(rec.window);
    rec.future_use0=ntohl(rec.future_use0);
    rec.future_use1=ntohl(rec.future_use1);
    rec.future_use2=ntohl(rec.future_use2);
    rec.checksum=ntohl(rec.checksum);
    if(rec.control!=(uint16_t)9){
      perror("Packet has not ACK=1 and FIN=1");
      socket->state=INVALID;
      return -1;
    }
    socket->ack_number=rec.seq_number;
    msg.seq_number=htonl(socket->seq_number);
    // 000 0  0 0 0 0 0 0 0 0 ACK=1 0 SYN=0 FIN=0
    msg.control=htons((uint16_t)8); //possible error
    msg.data_len=0;
    msg.ack_number=htonl(++socket->ack_number);
    msg.window=htonl(socket->curr_win_size);
    msg.future_use0=0;
    msg.future_use1=0;
    msg.future_use2=0;
    msg.checksum=0;
    memset(buf,0,MICROTCP_RECVBUF_LEN);
    memcpy(buf,&msg,sizeof(microtcp_header_t));
    msg.checksum=htonl(crc32((const uint8_t *)buf, sizeof(microtcp_header_t)));
    tmp=sendto(socket->sd,(void*)&msg,sizeof(microtcp_header_t),0,(struct sockaddr*)socket->address,socket->address_len);
    if (tmp<0) {
      perror("Cant Establish socket");
      socket->state=INVALID;
      return -1;
    }
    socket->state=CLOSED;

  }else{
    memset(buf,0,MICROTCP_RECVBUF_LEN);
    if(tmp_len=recvfrom(socket->sd,buf,MICROTCP_RECVBUF_LEN,0,(struct sockaddr*)socket->address,&socket->address_len)<0){
      socket->state=INVALID;
      perror("No packet from client received");
      return -1;
    }
    memcpy(&rec,buf,sizeof(microtcp_header_t));
    check=rec.checksum;
    rec.checksum=0;
    if((uint32_t)check!=ntohl(crc32((const uint8_t *)&rec,sizeof(microtcp_header_t)))){
      perror("Received packet destroyed7");
      socket->state=INVALID;
      return -1;
    }
    rec.seq_number=ntohl(rec.seq_number);
    rec.control=ntohs(rec.control);
    rec.data_len=ntohl(rec.data_len);
    rec.ack_number=ntohl(rec.ack_number);
    rec.window=ntohs(rec.window);
    rec.future_use0=ntohl(rec.future_use0);
    rec.future_use1=ntohl(rec.future_use1);
    rec.future_use2=ntohl(rec.future_use2);
    rec.checksum=ntohl(rec.checksum);
    if(rec.control!=(uint16_t)9){
      perror("Packet has not ACK=1 and FIN=1");
      socket->state=INVALID;
      return -1;
    }
    socket->ack_number=rec.seq_number;
    msg.seq_number=htonl(socket->seq_number);
    // 000 0  0 0 0 0 0 0 0 0 ACK=1 0 SYN=0 FIN=0
    msg.control=htons((uint16_t)8); //possible error
    msg.data_len=0;
    msg.ack_number=htonl(socket->ack_number);
    msg.window=htonl(socket->curr_win_size);
    msg.future_use0=0;
    msg.future_use1=0;
    msg.future_use2=0;
    msg.checksum=0;
    memset(buf,0,MICROTCP_RECVBUF_LEN);
    memcpy(buf,&msg,sizeof(microtcp_header_t));
    msg.checksum=htonl(crc32((const uint8_t *)buf, sizeof(microtcp_header_t)));
    tmp=sendto(socket->sd,(void*)&msg,sizeof(microtcp_header_t),0,(struct sockaddr*)socket->address,socket->address_len);
    if (tmp<0) {
      perror("Cant Establish socket");
      socket->state=INVALID;
      return -1;
    }
    socket->state=CLOSING_BY_PEER;
    msg.seq_number=htonl(socket->seq_number);
    // 000 0  0 0 0 0 0 0 0 0 ACK=1 0 SYN=0 FIN=1
    msg.control=htons((uint16_t)9); //possible error
    msg.data_len=0;
    msg.ack_number=htonl(socket->ack_number);
    msg.window=htonl(socket->curr_win_size);
    msg.future_use0=0;
    msg.future_use1=0;
    msg.future_use2=0;
    msg.checksum=0;
    memset(buf,0,MICROTCP_RECVBUF_LEN);
    memcpy(buf,&msg,sizeof(microtcp_header_t));
    msg.checksum=htonl(crc32((const uint8_t *)buf, sizeof(microtcp_header_t)));
    tmp=sendto(socket->sd,(void*)&msg,sizeof(microtcp_header_t),0,(struct sockaddr*)socket->address,socket->address_len);
    if (tmp<0) {
      perror("Cant Establish socket");
      socket->state=INVALID;
      return -1;
    }
    memset(buf,0,MICROTCP_RECVBUF_LEN);
    if(tmp_len=recvfrom(socket->sd,buf,MICROTCP_RECVBUF_LEN,0,(struct sockaddr*)socket->address,&socket->address_len)<0){
      socket->state=INVALID;
      perror("No packet from client received");
      return -1;
    }
    memcpy(&rec,buf,sizeof(microtcp_header_t));
    check=rec.checksum;
    rec.checksum=0;
    if((uint32_t)check!=ntohl(crc32((const uint8_t *)&rec,sizeof(microtcp_header_t)))){
      perror("Received packet destroyed8");
      socket->state=INVALID;
      return -1;
    }
    rec.seq_number=ntohl(rec.seq_number);
    rec.control=ntohs(rec.control);
    rec.data_len=ntohl(rec.data_len);
    rec.ack_number=ntohl(rec.ack_number);
    rec.window=ntohs(rec.window);
    rec.future_use0=ntohl(rec.future_use0);
    rec.future_use1=ntohl(rec.future_use1);
    rec.future_use2=ntohl(rec.future_use2);
    rec.checksum=ntohl(rec.checksum);

    if(rec.control!=(uint16_t)8){
      perror("Packet has not ACK=1");
      socket->state=INVALID;
      return -1;
    }
    if(rec.seq_number!=(uint32_t)socket->ack_number+1||rec.ack_number!=(uint32_t)socket->seq_number+1){
      perror("Packet has not correct seq or ACK");
      socket->state=INVALID;
      return -1;
    }
    socket->state=CLOSED;
  }
  close(socket->sd);
  return 0;
}

ssize_t
microtcp_send (microtcp_sock_t *socket, const void *buffer, size_t length,
               int flags)
{
  /* Your code here */
}

ssize_t
microtcp_recv (microtcp_sock_t *socket, void *buffer, size_t length, int flags)
{
  /* Your code here */
}
