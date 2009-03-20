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
 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cache.h" 
#include "ax25.h"
 
 
 
struct cacheNode *firstCacheNode = NULL;
struct cacheNode *lastCacheNode = NULL;
short cacheCount = 0;

short checkCache(uidata_t *uidata) {
	if (firstCacheNode == NULL) {
		//no node yet, create the first one.
		struct cacheNode *node = createCacheNode(uidata);
		firstCacheNode = node;
		lastCacheNode = node;
		//since there was no data, there could not be any doubles.
		return 1;
	} 


	//run through cache.
	short foundDuplicate = 0;
	struct cacheNode *current = firstCacheNode;
	struct cacheNode *liveData = createCacheNode(uidata);
	do {
		if (cacheCompareNodes(liveData, current)) {
			foundDuplicate = 1;
			break;
		}
	} while((current = current->next) != 0);

	if (foundDuplicate) {
		return 0;
	} else {
		//increase the cacheCount.
		cacheCount++;
		if (cacheCount >= CACHE_SIZE) {
			//pop off the first node before adding a new one.
			struct cacheNode *old = firstCacheNode;
			firstCacheNode = old->next;
			cacheFreeNode(old);
			cacheCount--;
		}
		//add current node to the list.
		struct cacheNode *tmp = lastCacheNode;
		lastCacheNode = liveData;
		tmp->next = lastCacheNode;
		//printf("%i\n",cacheCount);
	}

	return 1;
}

short cacheCompareNodes(struct cacheNode *node1, struct cacheNode *node2) {
	struct cacheItem *item1 = node1->item;
	struct cacheItem *item2 = node2->item;
	//printf("%s ~ %s | %s ~ %s | %s ~ %s\n",item1->src, item2->src,item1->dst, item2->dst,item1->data, item2->data);
	return ((!strcmp(item1->src, item2->src)) && (!strcmp(item1->dst, item2->dst)) && (!strcmp(item1->data, item2->data))); 
}

struct cacheNode* createCacheNode(uidata_t *uidata) {
	struct cacheNode *node = NULL;
	//alloc memory for node
	node = (struct cacheNode*)malloc(sizeof(struct cacheNode));
	//no next node yet.
	node->next = NULL;
	node->item = createCacheItem(uidata);
	
	return node;
}

struct cacheItem* createCacheItem(uidata_t *uidata) {
	struct cacheItem* item = NULL;
	//alloc mem for item
	item = (struct cacheItem*)malloc(sizeof(struct cacheItem));
	//fill item.
	strcpy(item->src,uidata->originator);
	strcpy(item->dst,uidata->destination);
	strcpy(item->data,uidata->data);
	
	return item;
}

void cacheFreeNode(struct cacheNode *node) {
	free(node->item);
	free(node);
}