#include "list.h"
#include <string.h>
#include <stdio.h>

int main(int agrc, char ** argv)
{
	testAdd();
    
    return 0;
}

static void printList(list * l)
{
    node * iter = l->head;
    printf("\n List :");
	while(iter != NULL)
    {
		printf("'%s' -", (char *)iter->data);
		iter = iter->next;
	}
	printf("\n");
}

void testAdd()
{
	int retVal = 0;
	list * l = newList();

	char * a = "a",  * b = "b" ,  * c = "c",  * d = "d" , * e = "e";

    add(l, a);
    add(l, b);
    add(l, c);
    add(l, d);
    add(l, e);
    
    printList(l);
    
    listRemove(l, "a", strcmp);
    listRemove(l, "e", strcmp);
    
    printList(l);
    
    add(l, e);
    
    printList(l);
    
    listRemove(l, c, strcmp);
    listRemove(l, d, strcmp);
    
    printList(l);
    
    listRemove(l, b, strcmp);
    listRemove(l, e, strcmp);
    
    printList(l);
    
    add(l, a);
    add(l, b);
    add(l, c);
    add(l, d);
    add(l, e);
    
    printList(l);
    
    printf("%d A \n", l->size);
}
