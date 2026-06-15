/*
 * This software is dual-licensed under GPLv3 and a commercial
 * license. See the file LICENSE.md distributed with this software for
 * full license information.
 */

/** \file
 * \brief
 * EtherCAT BPF driver for Darwin/macOS.
 *
 * macOS does not provide Linux PF_PACKET sockets. This backend uses BPF
 * descriptors to send and receive complete Ethernet frames on a selected
 * interface. The higher SOEM frame-index logic is intentionally kept aligned
 * with the Linux backend so the Darwin port remains small and reviewable.
 */

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <net/bpf.h>
#include <net/ethernet.h>
#include <net/if.h>
#include <poll.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "osal.h"
#include "oshw.h"

#ifndef ETH_P_ECAT
#define ETH_P_ECAT 0x88A4
#endif

/** Redundancy modes */
enum
{
   /** No redundancy, single NIC mode */
   ECT_RED_NONE,
   /** Double redundant NIC connection */
   ECT_RED_DOUBLE
};

/** Primary source MAC address used for EtherCAT. */
const uint16 priMAC[3] = EC_PRIMARY_MAC_ARRAY;
/** Secondary source MAC address used for EtherCAT. */
const uint16 secMAC[3] = EC_SECONDARY_MAC_ARRAY;

/** second MAC word is used for identification */
#define RX_PRIM priMAC[1]
/** second MAC word is used for identification */
#define RX_SEC  secMAC[1]

static void ecx_clear_rxbufstat(int *rxbufstat)
{
   int i;
   for (i = 0; i < EC_MAXBUF; i++)
   {
      rxbufstat[i] = EC_BUF_EMPTY;
   }
}

static int ecx_open_bpf(void)
{
   int fd;
   int i;
   char devname[32];

   fd = open("/dev/bpf", O_RDWR);
   if (fd >= 0)
   {
      return fd;
   }

   for (i = 0; i < 256; i++)
   {
      snprintf(devname, sizeof(devname), "/dev/bpf%d", i);
      fd = open(devname, O_RDWR);
      if (fd >= 0)
      {
         return fd;
      }
      if (errno != EBUSY)
      {
         break;
      }
   }

   return -1;
}

static void ecx_init_bpf_state(uint8 *bpfbuf, int *bpfbuflen, int *bpfbufpos, int *bpfbufused)
{
   (void)bpfbuf;
   *bpfbuflen = EC_BUFSIZE * 4;
   *bpfbufpos = 0;
   *bpfbufused = 0;
}

static int ecx_configure_bpf(int fd, const char *ifname, uint8 *bpfbuf, int *bpfbuflen, int *bpfbufpos, int *bpfbufused)
{
   struct ifreq ifr;
   u_int buflen;
   u_int enable;
   u_int dlt;
   int flags;

   ecx_init_bpf_state(bpfbuf, bpfbuflen, bpfbufpos, bpfbufused);

   buflen = (u_int)*bpfbuflen;
   if (ioctl(fd, BIOCSBLEN, &buflen) < 0)
   {
      return 0;
   }
   if (ioctl(fd, BIOCGBLEN, &buflen) < 0)
   {
      return 0;
   }
   if ((int)buflen > *bpfbuflen)
   {
      return 0;
   }
   *bpfbuflen = (int)buflen;

   memset(&ifr, 0, sizeof(ifr));
   strncpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name) - 1);
   if (ioctl(fd, BIOCSETIF, &ifr) < 0)
   {
      return 0;
   }

   if (ioctl(fd, BIOCGDLT, &dlt) < 0)
   {
      return 0;
   }
   if (dlt != DLT_EN10MB)
   {
      return 0;
   }

   enable = 1;
   (void)ioctl(fd, BIOCIMMEDIATE, &enable);
   (void)ioctl(fd, BIOCSHDRCMPLT, &enable);
   (void)ioctl(fd, BIOCPROMISC, NULL);

#ifdef BIOCSDIRECTION
   {
      u_int direction = BPF_D_IN;
      (void)ioctl(fd, BIOCSDIRECTION, &direction);
   }
#endif

   flags = fcntl(fd, F_GETFL, 0);
   if (flags >= 0)
   {
      (void)fcntl(fd, F_SETFL, flags | O_NONBLOCK);
   }

   return 1;
}

/** Basic setup to connect NIC to socket.
 * @param[in] port        = port context struct
 * @param[in] ifname      = Name of NIC device, f.e. "en0"
 * @param[in] secondary   = if >0 then use secondary stack instead of primary
 * @return >0 if succeeded
 */
int ecx_setupnic(ecx_portt *port, const char *ifname, int secondary)
{
   int i;
   int rval;
   int *psock;
   uint8 *bpfbuf;
   int *bpfbuflen;
   int *bpfbufpos;
   int *bpfbufused;

   rval = 0;
   if (secondary)
   {
      if (port->redport)
      {
         psock = &(port->redport->sockhandle);
         *psock = -1;
         port->redstate = ECT_RED_DOUBLE;
         port->redport->stack.sock = &(port->redport->sockhandle);
         port->redport->stack.txbuf = &(port->txbuf);
         port->redport->stack.txbuflength = &(port->txbuflength);
         port->redport->stack.tempbuf = &(port->redport->tempinbuf);
         port->redport->stack.rxbuf = &(port->redport->rxbuf);
         port->redport->stack.rxbufstat = &(port->redport->rxbufstat);
         port->redport->stack.rxsa = &(port->redport->rxsa);
         ecx_clear_rxbufstat(&(port->redport->rxbufstat[0]));
         bpfbuf = &(port->redport->bpfbuf[0]);
         bpfbuflen = &(port->redport->bpfbuflen);
         bpfbufpos = &(port->redport->bpfbufpos);
         bpfbufused = &(port->redport->bpfbufused);
      }
      else
      {
         return 0;
      }
   }
   else
   {
      pthread_mutex_init(&(port->getindex_mutex), NULL);
      pthread_mutex_init(&(port->tx_mutex), NULL);
      pthread_mutex_init(&(port->rx_mutex), NULL);
      port->sockhandle = -1;
      port->lastidx = 0;
      port->redstate = ECT_RED_NONE;
      port->stack.sock = &(port->sockhandle);
      port->stack.txbuf = &(port->txbuf);
      port->stack.txbuflength = &(port->txbuflength);
      port->stack.tempbuf = &(port->tempinbuf);
      port->stack.rxbuf = &(port->rxbuf);
      port->stack.rxbufstat = &(port->rxbufstat);
      port->stack.rxsa = &(port->rxsa);
      ecx_clear_rxbufstat(&(port->rxbufstat[0]));
      psock = &(port->sockhandle);
      bpfbuf = &(port->bpfbuf[0]);
      bpfbuflen = &(port->bpfbuflen);
      bpfbufpos = &(port->bpfbufpos);
      bpfbufused = &(port->bpfbufused);
   }

   *psock = ecx_open_bpf();
   if (*psock < 0)
   {
      return 0;
   }

   if (ecx_configure_bpf(*psock, ifname, bpfbuf, bpfbuflen, bpfbufpos, bpfbufused))
   {
      for (i = 0; i < EC_MAXBUF; i++)
      {
         ec_setupheader(&(port->txbuf[i]));
         port->rxbufstat[i] = EC_BUF_EMPTY;
      }
      ec_setupheader(&(port->txbuf2));
      rval = 1;
   }

   if (!rval)
   {
      close(*psock);
      *psock = -1;
   }

   return rval;
}

/** Close sockets used
 * @param[in] port        = port context struct
 * @return 0
 */
int ecx_closenic(ecx_portt *port)
{
   if (port->sockhandle >= 0)
      close(port->sockhandle);
   if ((port->redport) && (port->redport->sockhandle >= 0))
      close(port->redport->sockhandle);

   return 0;
}

/** Fill buffer with ethernet header structure.
 * Destination MAC is always broadcast.
 * Ethertype is always ETH_P_ECAT.
 * @param[out] p = buffer
 */
void ec_setupheader(void *p)
{
   ec_etherheadert *bp;
   bp = p;
   bp->da0 = htons(0xffff);
   bp->da1 = htons(0xffff);
   bp->da2 = htons(0xffff);
   bp->sa0 = htons(priMAC[0]);
   bp->sa1 = htons(priMAC[1]);
   bp->sa2 = htons(priMAC[2]);
   bp->etype = htons(ETH_P_ECAT);
}

/** Get new frame identifier index and allocate corresponding rx buffer.
 * @param[in] port        = port context struct
 * @return new index.
 */
uint8 ecx_getindex(ecx_portt *port)
{
   uint8 idx;
   uint8 cnt;

   pthread_mutex_lock(&(port->getindex_mutex));

   idx = port->lastidx + 1;
   if (idx >= EC_MAXBUF)
   {
      idx = 0;
   }
   cnt = 0;
   while ((port->rxbufstat[idx] != EC_BUF_EMPTY) && (cnt < EC_MAXBUF))
   {
      idx++;
      cnt++;
      if (idx >= EC_MAXBUF)
      {
         idx = 0;
      }
   }
   port->rxbufstat[idx] = EC_BUF_ALLOC;
   if (port->redstate != ECT_RED_NONE)
      port->redport->rxbufstat[idx] = EC_BUF_ALLOC;
   port->lastidx = idx;

   pthread_mutex_unlock(&(port->getindex_mutex));

   return idx;
}

/** Set rx buffer status.
 * @param[in] port        = port context struct
 * @param[in] idx      = index in buffer array
 * @param[in] bufstat  = status to set
 */
void ecx_setbufstat(ecx_portt *port, uint8 idx, int bufstat)
{
   port->rxbufstat[idx] = bufstat;
   if (port->redstate != ECT_RED_NONE)
      port->redport->rxbufstat[idx] = bufstat;
}

/** Transmit buffer over socket (non blocking).
 * @param[in] port        = port context struct
 * @param[in] idx         = index in tx buffer array
 * @param[in] stacknumber = 0=Primary 1=Secondary stack
 * @return socket send result
 */
int ecx_outframe(ecx_portt *port, uint8 idx, int stacknumber)
{
   int lp, rval;
   ec_stackT *stack;

   if (!stacknumber)
   {
      stack = &(port->stack);
   }
   else
   {
      stack = &(port->redport->stack);
   }
   lp = (*stack->txbuflength)[idx];
   (*stack->rxbufstat)[idx] = EC_BUF_TX;
   rval = (int)write(*stack->sock, (*stack->txbuf)[idx], (size_t)lp);
   if (rval == -1)
   {
      (*stack->rxbufstat)[idx] = EC_BUF_EMPTY;
   }

   return rval;
}

/** Transmit buffer over socket (non blocking).
 * @param[in] port        = port context struct
 * @param[in] idx = index in tx buffer array
 * @return socket send result
 */
int ecx_outframe_red(ecx_portt *port, uint8 idx)
{
   ec_comt *datagramP;
   ec_etherheadert *ehp;
   int rval;

   ehp = (ec_etherheadert *)&(port->txbuf[idx]);
   ehp->sa1 = htons(priMAC[1]);
   rval = ecx_outframe(port, idx, 0);
   if (port->redstate != ECT_RED_NONE)
   {
      pthread_mutex_lock(&(port->tx_mutex));
      ehp = (ec_etherheadert *)&(port->txbuf2);
      datagramP = (ec_comt *)&(port->txbuf2[ETH_HEADERSIZE]);
      datagramP->index = idx;
      ehp->sa1 = htons(secMAC[1]);
      port->redport->rxbufstat[idx] = EC_BUF_TX;
      if (write(port->redport->sockhandle, &(port->txbuf2), (size_t)port->txbuflength2) == -1)
      {
         port->redport->rxbufstat[idx] = EC_BUF_EMPTY;
      }
      pthread_mutex_unlock(&(port->tx_mutex));
   }

   return rval;
}

static int ecx_next_bpf_frame(uint8 *bpfbuf, int *bpfbufpos, int *bpfbufused, ec_bufT *tempbuf)
{
   while (*bpfbufpos < *bpfbufused)
   {
      struct bpf_hdr *header = (struct bpf_hdr *)&bpfbuf[*bpfbufpos];
      uint8 *frame = &bpfbuf[*bpfbufpos + header->bh_hdrlen];
      int framelen = (int)header->bh_caplen;
      int next = *bpfbufpos + BPF_WORDALIGN(header->bh_hdrlen + header->bh_caplen);

      *bpfbufpos = next;
      if ((framelen >= (int)ETH_HEADERSIZE) && (framelen <= (int)sizeof(ec_bufT)))
      {
         ec_etherheadert *ehp = (ec_etherheadert *)frame;
         if (ehp->etype == htons(ETH_P_ECAT))
         {
            memcpy(tempbuf, frame, (size_t)framelen);
            return framelen;
         }
      }
   }

   *bpfbufpos = 0;
   *bpfbufused = 0;
   return 0;
}

/** Non blocking read of socket. Put frame in temporary buffer.
 * @param[in] port        = port context struct
 * @param[in] stacknumber = 0=primary 1=secondary stack
 * @return >0 if frame is available and read
 */
static int ecx_recvpkt(ecx_portt *port, int stacknumber)
{
   int bytesrx;
   ec_stackT *stack;
   uint8 *bpfbuf;
   int *bpfbuflen;
   int *bpfbufpos;
   int *bpfbufused;
   int framelen;

   if (!stacknumber)
   {
      stack = &(port->stack);
      bpfbuf = &(port->bpfbuf[0]);
      bpfbuflen = &(port->bpfbuflen);
      bpfbufpos = &(port->bpfbufpos);
      bpfbufused = &(port->bpfbufused);
   }
   else
   {
      stack = &(port->redport->stack);
      bpfbuf = &(port->redport->bpfbuf[0]);
      bpfbuflen = &(port->redport->bpfbuflen);
      bpfbufpos = &(port->redport->bpfbufpos);
      bpfbufused = &(port->redport->bpfbufused);
   }

   framelen = ecx_next_bpf_frame(bpfbuf, bpfbufpos, bpfbufused, stack->tempbuf);
   if (framelen > 0)
   {
      port->tempinbufs = framelen;
      return 1;
   }

   bytesrx = (int)read(*stack->sock, bpfbuf, (size_t)*bpfbuflen);
   if (bytesrx <= 0)
   {
      port->tempinbufs = bytesrx;
      return 0;
   }

   *bpfbufpos = 0;
   *bpfbufused = bytesrx;
   framelen = ecx_next_bpf_frame(bpfbuf, bpfbufpos, bpfbufused, stack->tempbuf);
   port->tempinbufs = framelen;

   return (framelen > 0);
}

/** Non blocking receive frame function. Uses RX buffer and index to combine
 * read frame with transmitted frame.
 */
int ecx_inframe(ecx_portt *port, uint8 idx, int stacknumber)
{
   uint16 l;
   int rval;
   uint8 idxf;
   ec_etherheadert *ehp;
   ec_comt *ecp;
   ec_stackT *stack;
   ec_bufT *rxbuf;

   if (!stacknumber)
   {
      stack = &(port->stack);
   }
   else
   {
      stack = &(port->redport->stack);
   }
   rval = EC_NOFRAME;
   rxbuf = &(*stack->rxbuf)[idx];
   if ((idx < EC_MAXBUF) && ((*stack->rxbufstat)[idx] == EC_BUF_RCVD))
   {
      l = (*rxbuf)[0] + ((uint16)((*rxbuf)[1] & 0x0f) << 8);
      rval = ((*rxbuf)[l] + ((uint16)(*rxbuf)[l + 1] << 8));
      (*stack->rxbufstat)[idx] = EC_BUF_COMPLETE;
   }
   else
   {
      pthread_mutex_lock(&(port->rx_mutex));
      if ((idx < EC_MAXBUF) && ((*stack->rxbufstat)[idx] == EC_BUF_RCVD))
      {
         l = (*rxbuf)[0] + ((uint16)((*rxbuf)[1] & 0x0f) << 8);
         rval = ((*rxbuf)[l] + ((uint16)(*rxbuf)[l + 1] << 8));
         (*stack->rxbufstat)[idx] = EC_BUF_COMPLETE;
      }
      else if (ecx_recvpkt(port, stacknumber))
      {
         rval = EC_OTHERFRAME;
         ehp = (ec_etherheadert *)(stack->tempbuf);
         if (ehp->etype == htons(ETH_P_ECAT))
         {
            stack->rxcnt++;
            ecp = (ec_comt *)(&(*stack->tempbuf)[ETH_HEADERSIZE]);
            l = etohs(ecp->elength) & 0x0fff;
            idxf = ecp->index;
            if (idxf == idx)
            {
               memcpy(rxbuf, &(*stack->tempbuf)[ETH_HEADERSIZE], (*stack->txbuflength)[idx] - ETH_HEADERSIZE);
               rval = ((*rxbuf)[l] + ((uint16)((*rxbuf)[l + 1]) << 8));
               (*stack->rxbufstat)[idx] = EC_BUF_COMPLETE;
               (*stack->rxsa)[idx] = ntohs(ehp->sa1);
            }
            else
            {
               if (idxf < EC_MAXBUF && (*stack->rxbufstat)[idxf] == EC_BUF_TX)
               {
                  rxbuf = &(*stack->rxbuf)[idxf];
                  memcpy(rxbuf, &(*stack->tempbuf)[ETH_HEADERSIZE], (*stack->txbuflength)[idxf] - ETH_HEADERSIZE);
                  (*stack->rxbufstat)[idxf] = EC_BUF_RCVD;
                  (*stack->rxsa)[idxf] = ntohs(ehp->sa1);
               }
            }
         }
      }
      pthread_mutex_unlock(&(port->rx_mutex));
   }

   return rval;
}

static int ecx_waitinframe_red(ecx_portt *port, uint8 idx, osal_timert *timer)
{
   osal_timert timer2;
   int wkc = EC_NOFRAME;
   int wkc2 = EC_NOFRAME;
   int primrx, secrx;
   struct pollfd fds[2];
   ec_stackT *stack;
   int pollcnt = 1;

   if (port->redstate == ECT_RED_NONE)
      wkc2 = 0;

   stack = &(port->stack);
   fds[0].fd = *stack->sock;
   fds[0].events = POLLIN;
   fds[0].revents = 0;
   if (port->redstate != ECT_RED_NONE)
   {
      pollcnt = 2;
      stack = &(port->redport->stack);
      fds[1].fd = *stack->sock;
      fds[1].events = POLLIN;
      fds[1].revents = 0;
   }

   do
   {
      if (poll(fds, (nfds_t)pollcnt, 1) >= 0)
      {
         if (wkc <= EC_NOFRAME)
            wkc = ecx_inframe(port, idx, 0);
         if (port->redstate != ECT_RED_NONE)
         {
            if (wkc2 <= EC_NOFRAME)
               wkc2 = ecx_inframe(port, idx, 1);
         }
      }
   } while (((wkc <= EC_NOFRAME) || (wkc2 <= EC_NOFRAME)) && !osal_timer_is_expired(timer));

   if (port->redstate != ECT_RED_NONE)
   {
      primrx = 0;
      if (wkc > EC_NOFRAME) primrx = port->rxsa[idx];
      secrx = 0;
      if (wkc2 > EC_NOFRAME) secrx = port->redport->rxsa[idx];

      if (((primrx == RX_SEC) && (secrx == RX_PRIM)))
      {
         memcpy(&(port->rxbuf[idx]), &(port->redport->rxbuf[idx]), port->txbuflength[idx] - ETH_HEADERSIZE);
         wkc = wkc2;
      }
      if (((primrx == 0) && (secrx == RX_SEC)) ||
          ((primrx == RX_PRIM) && (secrx == RX_SEC)))
      {
         if ((primrx == RX_PRIM) && (secrx == RX_SEC))
         {
            memcpy(&(port->txbuf[idx][ETH_HEADERSIZE]), &(port->rxbuf[idx]), port->txbuflength[idx] - ETH_HEADERSIZE);
         }
         osal_timer_start(&timer2, EC_TIMEOUTRET);
         ecx_outframe(port, idx, 1);
         do
         {
            wkc2 = ecx_inframe(port, idx, 1);
         } while ((wkc2 <= EC_NOFRAME) && !osal_timer_is_expired(&timer2));
         if (wkc2 > EC_NOFRAME)
         {
            memcpy(&(port->rxbuf[idx]), &(port->redport->rxbuf[idx]), port->txbuflength[idx] - ETH_HEADERSIZE);
            wkc = wkc2;
         }
      }
   }

   return wkc;
}

/** Blocking receive frame function. Calls ec_waitinframe_red().
 * @param[in] port        = port context struct
 * @param[in] idx       = requested index of frame
 * @param[in] timeout   = timeout in us
 * @return Workcounter if a frame is found with corresponding index, otherwise
 * EC_NOFRAME.
 */
int ecx_waitinframe(ecx_portt *port, uint8 idx, int timeout)
{
   int wkc;
   osal_timert timer;

   osal_timer_start(&timer, timeout);
   wkc = ecx_waitinframe_red(port, idx, &timer);

   return wkc;
}

/** Blocking send and receive frame function. Used for non processdata frames.
 * @param[in] port        = port context struct
 * @param[in] idx      = index of frame
 * @param[in] timeout  = timeout in us
 * @return Workcounter or EC_NOFRAME
 */
int ecx_srconfirm(ecx_portt *port, uint8 idx, int timeout)
{
   int wkc = EC_NOFRAME;
   osal_timert timer1, timer2;

   osal_timer_start(&timer1, timeout);
   do
   {
      ecx_outframe_red(port, idx);
      osal_timer_start(&timer2, EC_TIMEOUTRET);
      do
      {
         wkc = ecx_waitinframe_red(port, idx, &timer2);
      } while ((wkc <= EC_NOFRAME) && !osal_timer_is_expired(&timer2));
   } while ((wkc <= EC_NOFRAME) && !osal_timer_is_expired(&timer1));

   return wkc;
}
