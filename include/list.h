#ifndef H_LIST
#define H_LIST

/*			=======================[Includes]=========================		  */
/*			=======================[Defines/Enums]====================		  */
/*			=======================[Structures]=======================		  */

typedef struct node node;

struct node
{
	node *next;
    node *prev;
	void *data;
};


typedef struct list
{
	int size;
	node * head;
	node * tail;
} list;

/*			=======================[Prototypes]=======================		  */

typedef int (*compar)(void const *, void const *);

/* Retourne une nouvelle liste vide */
list *newList(void);

/* Ajoute l'element data en queue de la liste */
void add(list *l, void *data);

/* Retourne l'element en tete de liste, ou NULL en cas de liste vide */
void *getHead(list *l);

void listDestroyed(list ** list, int freeElement);

int listContains(list *l, void const * data, compar cmp);

void *listSearch(list *l, void const * data, compar cmp);

void * listRemove(list * l, void const *data, compar cmp);

#endif
