#pragma once

#include <stdbool.h>
#include <unordered_set>

template <class T> class rbnode {
public:
	T value;
	bool col;
	rbnode<T>* child[2];
	rbnode<T>* parent;

	rbnode(T v);
};

template <class T>
class rbtree {
public:
	rbtree(void);
	rbtree(T obj);

	rbnode<T>* insert(T obj);
	rbnode<T>* findBelow(int value);
	rbnode<T>* findAbove(int value);

	void remove(rbnode<T> *node);

private:
	void balance_blacks(rbnode<T> *node, rbnode<T> *child);
	rbnode<T> *head;
};
