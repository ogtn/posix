#include <stdio.h>
#include <stdlib.h>

#include "list.h"


list *newList(void)
{
	list *l = malloc(sizeof(list));
	
	if(l != NULL)
	{
		l->head = NULL;
		l->tail = NULL;
		l->size = 0;
	}
	else
	{
		perror("newList : malloc");
	}
	
	return l;
}

void listDestroyed(list ** list, int freeElement)
{
    if(list != NULL && *list != NULL)
    {
        node * iter = (*list)->head;
		
		while(iter != NULL)
		{
            node * tmp = iter;
            
			if(freeElement)
			{
                free(iter->data);
			}
			iter = iter->next;
            
            free(tmp);
		}
        
        free(*list);
        *list = NULL;
    }
}

void add(list *l, void *data)
{
	if(l != NULL)
	{
		node * n = malloc(sizeof(node));
		
		if(n != NULL)
		{
			n->data = data;
			n->next = NULL;
			l->size++;
						
			if(l->size == 1)
			{
				l->tail = l->head = n;
			}
			else
			{
				l->tail->next = n;
				l->tail = n;
			}
		}
		else
		{
			perror("add : malloc");
		}
	}
}


void *getHead(list *l)
{
	void *res = NULL;
	
	if(l != NULL && l->size != 0)
	{	
		node *n = l->head;
		res = n->data;
		l->size--;
		l->head = n->next;
		free(n);
	}
	
	return res;
}


int listContains(list *l, void const * data, compar cmp)
{
	if(l != NULL)
	{
		node * iter = l->head;
		
		while(iter != NULL)
		{
			if(cmp(iter->data, data) == 0)
			{
				return 1;
			}
			iter = iter->next;
		}
	}
	return 0;
}


void *listSearch(list *l, void const * data, compar cmp)
{
	if(l != NULL)
	{
		node * iter = l->head;
		
		while(iter != NULL)
		{
			if(cmp(iter->data, data) == 0)
			{
				return iter->data;
			}
			iter = iter->next;
		}
	}
    
	return NULL;
}


void * listRemove(list * l, void const *data, compar cmp)
{
	void * retVal = NULL;
	
	if(l != NULL)
	{
		/* Liste non vide */
		if(l->head != NULL)
		{
			node * iter = l->head;
			
			/* Suppression du 1er élément de la liste */
			if(cmp(l->head->data, data) == 0)
			{
				retVal = l->head->data;
				l->head = l->head->next;
				free(iter);
				iter = NULL;
				
				if(l->head == NULL)
				{
					l->tail = NULL;
				}
				l->size--;
			}
			else
			{
				/* Suppression d'un élément dans la liste */
				while(retVal == NULL && iter->next != NULL)
				{
					if(cmp(iter->next->data, data) == 0)
					{
						node * tmpNode = iter->next;
						retVal = iter->next->data;
						iter->next = iter->next->next;
						
						if(iter->next == NULL)
						{
							l->tail = iter;
						}
						
						free(tmpNode);
						tmpNode = NULL;
						l->size--;
					}
					else
					{
						iter = iter->next;
					}
				}
			}
		}
	}
	
	return retVal;
}
