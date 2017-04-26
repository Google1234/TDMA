#include "tdma.h"

#define LOGSIZE  5
#define HASHSIZE 101
unsigned int slot_offset = 0;
unsigned int slot_t = 5;
valuetype rf_t = 30;
unsigned short int log_pointer = 0;//0 to LOGSIZE-1

								   //dic hashtable 
struct nlist { /* table entry: */
	struct nlist *next; /* next entry in chain */
	char *name; /* defined name */
	valuetype *slot_begin; /* replacement text */
	valuetype *slot_stop;
};
//store slot log
struct log_entity { /* table entry: */
	struct log_entity *next; /* next entry in chain */
	char *name; /* defined name */
	valuetype *slot_begin;
	valuetype *slot_stop;
};
//

static struct nlist *hashtab[HASHSIZE]; /* pointer table */
static struct log_entity *assign_log[LOGSIZE]; /* pointer table */
static struct log_entity *use_log[LOGSIZE]; /* pointer table */


											/* hash: form hash value for string s */
unsigned hash(char *s)
{
	unsigned hashval;
	for (hashval = 0; *s != '\0'; s++)
		hashval = *s + 10 * hashval;
	return hashval % HASHSIZE;
}

/* lookup: look for s in hashtab */
struct nlist *lookup(char *s)
{
	struct nlist *np;
	for (np = hashtab[hash(s)]; np != NULL; np = np->next)
		if (strcmp(s, np->name) == 0)
			return np; /* found */
	return NULL; /* not found */
}


char *strdup(char *);
valuetype *strdupint(valuetype *);
/* install: put (name, defn) in hashtab */
struct nlist *install(char *name, valuetype *slot_begin, valuetype *slot_stop)
{
	struct nlist *np;
	unsigned hashval;
	if ((np = lookup(name)) == NULL) { /* not found */
		np = (struct nlist *) malloc(sizeof(*np));
		if (np == NULL || (np->name = strdup(name)) == NULL)
			return NULL;
		hashval = hash(name);
		np->next = hashtab[hashval];
		hashtab[hashval] = np;
	}
	else/* already there */
	{
		free((void *)np->slot_begin); /*free previous defn */
		free((void *)np->slot_stop); /*free previous defn */
	}
	if ((np->slot_begin = strdupint(slot_begin)) == NULL)
		return NULL;
	if ((np->slot_stop = strdupint(slot_stop)) == NULL)
		return NULL;
	return np;
}

char *strdup(char *s) /* make a duplicate of s */
{
	char *p;
	p = (char *)malloc(strlen(s) + 1); /*  strlen(s)+1 for ’\0’ */
	if (p != NULL)
		strcpy(p, s);
	return p;
}
valuetype *strdupint(valuetype *v) /* make a duplicate of s */
{
	valuetype *p;
	p = (valuetype *)malloc(sizeof(valuetype));
	if (p != NULL)
		*p = *v;
	return p;
}
/* Do not use this :hard to free memory
char* sensor_id_to_string(unsigned int sensor_id)
{
char *s = malloc(8);
sprintf(s, "%d", sensor_id);
return s + 4;//only use part of string: 20160301--->0301
}
*/

void remove_sensor_id(unsigned int sensor_id)
{
	struct nlist *np;
	struct nlist *head;
	struct nlist *p;
	char  s[9]; //8byte + 1 stop byte:/0
	unsigned hashval;
	sprintf(s, "%d", sensor_id);

	hashval = hash(s);
	np = hashtab[hashval];
	head = (struct nlist *) malloc(sizeof(*head));
	p = head;
	for (; np != NULL; np = np->next)
	{
		if (strcmp(s, np->name) == 0)/* found */
		{
			p->next = np->next;
			//free((void *) np->next);
			free((void *)np->name);
			free((void *)np->slot_begin);
			free((void *)np->slot_stop);
			free((void *)np);
			np = NULL;
			break;
		}
		else
		{
			p->next = np;
			p = p->next;
		}
		/* not found */
	}
	hashtab[hashval] = head->next;
	free((void *)head);
	head = NULL;
}

void assign_time_slot(unsigned int sensor_id, unsigned int *slot_begin, unsigned int *slot_stop)
{

	struct nlist *node;
	char s[9];//8byte + 1 stop byte:/0
	struct log_entity *entity;
	valuetype buff;
	sprintf(s, "%d", sensor_id);
	node = lookup(s);
	if (node == NULL)
	{
		//Modify the value according to my algorithm
		*slot_begin = slot_offset;
		*slot_stop = slot_offset + slot_t;
		slot_offset = slot_offset + slot_t;

		install(s, slot_begin, slot_stop);
		//store to use_log
		node = lookup(s);

		entity = (struct log_entity *) malloc(sizeof(*entity));
		entity->name = strdup(s);
		entity->next = use_log[log_pointer];
		entity->slot_begin = strdupint(&system_time);
		buff = system_time + slot_t;
		entity->slot_stop = strdupint(&buff);
		use_log[log_pointer] = entity;
	}
	else
	{
		*slot_begin = *node->slot_begin;
		*slot_stop = *node->slot_stop;
		//store to use_log
		entity = (struct log_entity *) malloc(sizeof(*entity));
		entity->name = strdup(s);
		entity->next = use_log[log_pointer];
		entity->slot_begin = strdupint(node->slot_begin);
		entity->slot_stop = strdupint(node->slot_stop);
		use_log[log_pointer] = entity;
	}

}

void clean_hastable(unsigned short int index)
{
	struct nlist *entity;
	if (index<0 || index >= HASHSIZE)
	{
		return;
	}
	while (hashtab[index] != NULL)
	{
		entity = hashtab[index];
		hashtab[index] = entity->next;

		//free((void *)entity->next);
		free((void *)entity->name);
		free((void *)entity->slot_begin);
		free((void *)entity->slot_stop);
		free((void *)entity);
	}
}

void update_slot_assign()
{
	struct log_entity *entity;
	struct log_entity *buff_entity;
	unsigned short int index;
	valuetype value;

	log_pointer = (log_pointer + 1) % LOGSIZE;
	//store to assign_log
	//step 1:clear hashtable
	//Violence removed
	for (index = 0; index<HASHSIZE; index++)
	{
		clean_hastable(index);
	}
	//step 2:assigh time slot according to slot log
	//free assign log memory
	while (assign_log[log_pointer] != NULL)
	{
		entity = assign_log[log_pointer];
		assign_log[log_pointer] = entity->next;

		//free((void *)entity->next);
		free((void *)entity->name);
		free((void *)entity->slot_begin);
		free((void *)entity->slot_stop);
		free((void *)entity);
	}
	//assign value and update hastable
	for (entity = use_log[(log_pointer + LOGSIZE - 1) % LOGSIZE]; entity != NULL; entity = entity->next)
	{
		buff_entity = (struct log_entity *) malloc(sizeof(*buff_entity));
		buff_entity->next = assign_log[log_pointer];
		buff_entity->name = strdup(entity->name);
		value = *(entity->slot_begin) + rf_t;
		buff_entity->slot_begin = strdupint(&value);
		value = *(entity->slot_stop) + rf_t;
		buff_entity->slot_stop = strdupint(&value);
		assign_log[log_pointer] = buff_entity;

		install(buff_entity->name, buff_entity->slot_begin, buff_entity->slot_stop);
		//set slot_offset
		if (*buff_entity->slot_stop>slot_offset)
		{
			slot_offset = *buff_entity->slot_stop;
		}
	}
	//update slot_offset
	while (slot_offset<system_time)
	{
		slot_offset = slot_offset + rf_t;
	}

	//free use log memory
	while (use_log[log_pointer] != NULL)
	{
		entity = use_log[log_pointer];
		use_log[log_pointer] = entity->next;
		//free((void *)entity->next);
		free((void *)entity->name);
		free((void *)entity->slot_begin);
		free((void *)entity->slot_stop);
		free((void *)entity);
	}
}
