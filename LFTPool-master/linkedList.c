#include <stdlib.h>
#include <stdio.h>

typedef struct node node_t;

struct node {
	int value;
	node_t *prev;
	node_t *next;
};

void add_to_list(node_t* head, int value) {
	node_t *new_node;
	new_node = (node_t *)malloc(sizeof (node_t));
	new_node->value = value;
	new_node->next = head;
	new_node->prev = head->prev;
	head->prev->next = new_node;
	head->prev = new_node;
}

int main() {
	node_t *list;
	int i;
	list = (node_t *)malloc(sizeof (node_t));
	list->prev = list;
	list->next = list;
	for (i = 0; i < 10; i++) {
		add_to_list(list, i);
	}
	node_t *node;
	for (node = list->next; node != list; node = node->next) {
		printf("%d\n", node->value);
	}
}