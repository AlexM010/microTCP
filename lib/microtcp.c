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
  // 000 0  0 0 0 0 0 0 0 0 ACK=0 0 SYN=1 FIN=0
 
microtcp_header_t create_header (uint32_t seq, uint16_t control, uint32_t data_len,  uint32_t ack, uint16_t window, uint32_t checksum) {
  microtcp_header_t msg;

  msg.seq_number=htonl(seq);
  msg.control=htons(control);
  msg.data_len=htonl(data_len);
  msg.ack_number=htonl(ack);
  msg.window=htons(window);
  msg.future_use0=0;
  msg.future_use1=0;
  msg.future_use2=0;
  msg.checksum=htonl(checksum);

  return msg;                              
}

microtcp_header_t reverse(microtcp_header_t msg) {
  msg.seq_number=ntohl(msg.seq_number);
  msg.control=ntohs(msg.control);
  msg.data_len=ntohl(msg.data_len);
  msg.ack_number=ntohl(msg.ack_number);
  msg.window=ntohs(msg.window);
  msg.future_use0=ntohl(msg.future_use0);
  msg.future_use1=ntohl(msg.future_use1);
  msg.future_use2=ntohl(msg.future_use2);
  msg.checksum=ntohl(msg.checksum);

  return msg;
}
 
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

  // 000 0  0 0 0 0 0 0 0 0 ACK=0 0 SYN=1 FIN=0  ==  2
  msg = create_header(socket->seq_number, (uint16_t)2, 0, 0, 0, 0);
  //copy the header to the buffer
  memcpy (buf,&msg, sizeof(microtcp_header_t));
  msg.checksum=htonl( crc32((const uint8_t *)&msg, sizeof(microtcp_header_t)));
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

  rec = reverse(rec);
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
  // 000 0  0 0 0 0 0 0 0 0 ACK=1 0 SYN=0 FIN=0  ==  8
  msg = create_header(socket->seq_number, (uint16_t)8, 0, socket->ack_number, socket->init_win_size, 0);
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

  rec = reverse(rec);

  if(rec.control!=(uint16_t)2){
    perror("Packet has not SYN=1");
    socket->state=INVALID;
    return -1;
  }
  //creating packet for 2nd handshake
  seq=rand()%10000;
  ack=rec.seq_number+1;

  // 000 0  0 0 0 0 0 0 0 0 ACK=1 0 SYN=1 FIN=0  ==  10
  msg = create_header(seq, (uint16_t)10, 0, ack, socket->curr_win_size, 0);
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
  
  rec = reverse(rec);
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
  
  socket->recvbuf=malloc(MICROTCP_RECVBUF_LEN);  
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
    // 000 0  0 0 0 0 0 0 0 0 ACK=1 0 SYN=0 FIN=1  ==  9
    msg = create_header(socket->seq_number, (uint16_t)9, 0, socket->ack_number, socket->curr_win_size, 0);
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
    rec = reverse(rec);
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
    rec = reverse(rec);
    if(rec.control!=(uint16_t)9){
      perror("Packet has not ACK=1 and FIN=1");
      socket->state=INVALID;
      return -1;
    }
    socket->ack_number=rec.seq_number;
    // 000 0  0 0 0 0 0 0 0 0 ACK=1 0 SYN=0 FIN=0  == 8
    msg = create_header(socket->seq_number, (uint16_t)8, 0, ++socket->ack_number, socket->curr_win_size, 0);
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
    rec = reverse(rec);
    if(rec.control!=(uint16_t)9){
      perror("Packet has not ACK=1 and FIN=1");
      socket->state=INVALID;
      return -1;
    }
    socket->ack_number=rec.seq_number;
    // 000 0  0 0 0 0 0 0 0 0 ACK=1 0 SYN=0 FIN=0  == 8
    msg = create_header(socket->seq_number, (uint16_t)8, 0, socket->ack_number, socket->curr_win_size, 0);
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
    // 000 0  0 0 0 0 0 0 0 0 ACK=1 0 SYN=0 FIN=1  ==  9
    msg = create_header(socket->seq_number, (uint16_t)9, 0, socket->ack_number, socket->curr_win_size, 0);
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
    rec = reverse(rec);
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
  size_t remaining=length;
  size_t data_sent=0;
  size_t bytes_to_send=0;
  size_t chunks=0;
  microtcp_header_t header;
  void *data=NULL;
  while(data_sent < length){
    bytes_to_send = min(socket->curr_win_size , socket->cwnd, remaining);
    chunks = bytes_to_send / MICROTCP_MSS;
    data=malloc(MICROTCP_MSS+sizeof(microtcp_header_t));
    assert(data != NULL);
    for(i = 0; i < chunks; i++){
      seq_number++;

      header = create_header(seq_number, (uint16_t)0, 0, 0, MICROTCP_MSS, 0);

      memset(data,0,MICROTCP_MSS+sizeof(microtcp_header_t));

      memcpy(data,&header,sizeof(microtcp_header_t));
      memcpy(data+sizeof(microtcp_header_t),buffer+data_sent,MICROTCP_MSS);

      header.checksum=htonl(crc32((const uint8_t *)data, MICROTCP_MSS+sizeof(microtcp_header_t)));

      memset(data,0,MICROTCP_MSS+sizeof(microtcp_header_t));

      memcpy(data,&header,sizeof(microtcp_header_t));
      memcpy(data+sizeof(microtcp_header_t),buffer+data_sent,MICROTCP_MSS);
      if(sendto(socket->sd,(void*)data,MICROTCP_MSS+sizeof(microtcp_header_t),0,(struct sockaddr*)socket->address,socket->address_len)<0){
        perror("Cant send data");
        socket->state=INVALID;
        return -1;
      }
      data_sent+=MICROTCP_MSS;
    }
    free(data);
  /* Check if there is a semi -filled chunk */
    if(bytes_to_send % MICROTCP_MSS){
      chunks++;
      seq_number++;
      header = create_header(seq_number, (uint16_t)0, 0, 0, bytes_to_send % MICROTCP_MSS, 0);
      data=malloc((bytes_to_send %MICROTCP_MSS)+sizeof(microtcp_header_t));
      assert(data != NULL);
      memcpy(data, &header, sizeof(microtcp_header_t));
      memcpy(data+sizeof(microtcp_header_t), buffer+data_sent, bytes_to_send % MICROTCP_MSS);
      header.checksum=htonl(crc32((const uint8_t *)data, MICROTCP_MSS+sizeof(microtcp_header_t)));
      memset(data,0,(bytes_to_send%MICROTCP_MSS)+sizeof(microtcp_header_t));
      memcpy(data, &header, sizeof(microtcp_header_t));
      memcpy(data+sizeof(microtcp_header_t), buffer+data_sent, bytes_to_send % MICROTCP_MSS);
      if(sendto(socket->sd,(void*)data,(bytes_to_send%MICROTCP_MSS)+sizeof(microtcp_header_t),0,(struct sockaddr*)socket->address,socket->address_len)<0){
          perror("Cant send data");
          socket->state=INVALID;
          return -1;
      }
      data_sent+=(bytes_to_send%MICROTCP_MSS);
      free(data);
    }
/* Get the ACKs */
  for(i = 0; i < chunks; i++){
    recvfrom(...);
  }
/* Retransmissions */
/* Update window */
/* Update congestion control */
  remaining -= bytes_to_send;

  data_sent += bytes_to_send;

  }
  return data_sent;
}

ssize_t
microtcp_recv (microtcp_sock_t *socket, void *buffer, size_t length, int flags)
{
  /* Your code here */
}
