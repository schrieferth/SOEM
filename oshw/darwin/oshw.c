/*
 * This software is dual-licensed under GPLv3 and a commercial
 * license. See the file LICENSE.md distributed with this software for
 * full license information.
 */

#include <arpa/inet.h>
#include <net/if.h>
#include <stdlib.h>
#include <string.h>

#include "oshw.h"

/**
 * Host to Network byte order (i.e. to big endian).
 *
 * Note that Ethercat uses little endian byte order, except for the Ethernet
 * header which is big endian as usual.
 */
uint16 oshw_htons(uint16 host)
{
   uint16 network = htons(host);
   return network;
}

/**
 * Network (i.e. big endian) to Host byte order.
 *
 * Note that Ethercat uses little endian byte order, except for the Ethernet
 * header which is big endian as usual.
 */
uint16 oshw_ntohs(uint16 network)
{
   uint16 host = ntohs(network);
   return host;
}

/** Create list over available network adapters.
 * @return First element in linked list of adapters
 */
ec_adaptert *oshw_find_adapters(void)
{
   int i;
   struct if_nameindex *ids;
   ec_adaptert *adapter;
   ec_adaptert *prev_adapter = NULL;
   ec_adaptert *ret_adapter = NULL;

   ids = if_nameindex();
   if (!ids)
   {
      return NULL;
   }

   for (i = 0; ids[i].if_index != 0; i++)
   {
      adapter = (ec_adaptert *)malloc(sizeof(ec_adaptert));
      if (!adapter)
      {
         break;
      }

      if (prev_adapter)
      {
         prev_adapter->next = adapter;
      }
      else
      {
         ret_adapter = adapter;
      }

      adapter->next = NULL;
      if (ids[i].if_name)
      {
         strncpy(adapter->name, ids[i].if_name, EC_MAXLEN_ADAPTERNAME - 1);
         adapter->name[EC_MAXLEN_ADAPTERNAME - 1] = '\0';
         strncpy(adapter->desc, ids[i].if_name, EC_MAXLEN_ADAPTERNAME - 1);
         adapter->desc[EC_MAXLEN_ADAPTERNAME - 1] = '\0';
      }
      else
      {
         adapter->name[0] = '\0';
         adapter->desc[0] = '\0';
      }

      prev_adapter = adapter;
   }

   if_freenameindex(ids);

   return ret_adapter;
}

/** Free allocated memory used by adapter collection.
 * @param[in] adapter = First element in linked list of adapters
 * EC_NOFRAME.
 */
void oshw_free_adapters(ec_adaptert *adapter)
{
   ec_adaptert *next_adapter;

   if (adapter)
   {
      next_adapter = adapter->next;
      free(adapter);
      while (next_adapter)
      {
         adapter = next_adapter;
         next_adapter = adapter->next;
         free(adapter);
      }
   }
}
