/*
 *  saxIgate, a small APRS I-Gate for Linux
 *  Copyright (C) 2009 Robbie De Lise (ON4SAX)
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
 */
 
#ifndef _CACHE_H_
#define _CACHE_H_
 
 #include "ax25.h"
 
 #define CACHE_SIZE 10
 
 struct cacheItem {
	char src[10];
	char dst[10];
	char data[1000];
};

struct cacheNode {
	struct cacheItem *item;
	struct cacheNode *next; 
};

short checkCache(uidata_t*);
struct cacheNode *createCacheNode(uidata_t *uidata);
struct cacheItem *createCacheItem(uidata_t *uidata);
short cacheCompareNodes(struct cacheNode *node1, struct cacheNode *node2);
void cacheFreeNode(struct cacheNode *node);

#endif


