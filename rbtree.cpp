#include "mem.h"
#include "rbtree.h"

template <class T>
rbnode<T>::rbnode(T v)
	: value(v)
{
	child[0] = nullptr;
	child[1] = nullptr;
	parent = nullptr;
}

template <class T>
rbtree<T>::rbtree(void)
	: head(nullptr)
{
}

template <class T>
rbtree<T>::rbtree(T obj) {
	rbnode<T> *newnode = new rbnode<T>(obj);
	newnode->col = false;
	head = newnode;
}

template <class T>
rbnode<T>* rbtree<T>::insert(T obj){
	rbnode<T>* curr = head;
	if(curr == nullptr){
		curr = new rbnode<T> (obj);
		curr->col = false;
		head = curr;
		return curr;
	}

	rbnode<T> *grandparent;
	rbnode<T> *parent = nullptr;
	bool right;
	rbnode<T> **ptr_to_new = &head;
	do{
		grandparent = parent;
		parent = curr;
		right = curr->value.sortval() < obj.sortval();
		ptr_to_new = &curr->child[right?1:0];
		curr = *ptr_to_new;
	} while(curr != nullptr);
	curr = new rbnode<T>(obj);
	*ptr_to_new = curr;
	rbnode<T> *retval = curr;
	if(!parent->col){
		curr->col = true;
		parent->child[right?1:0] = curr;
		return retval;
	}
	bool parentright = (grandparent->child[1] == parent);
	rbnode<T> *uncle = grandparent->child[parentright?0:1];
	if(uncle != nullptr && uncle->col){
		curr->col = true;
		parent->child[right?1:0] = curr;
		do {
			parent->col = false;
			uncle->col = false;
			curr = grandparent;
			parent = curr->parent;
			if(parent == nullptr){
				return retval;
			}
			if(!parent->col){
				curr->col = true;
				return retval;
			}
			grandparent = parent->parent;
			parentright = (grandparent->child[1] == parent);
			uncle = grandparent->child[parentright?0:1];
		} while(uncle != nullptr && uncle->col);
	}
	right = (parent->child[1] == curr);
	rbnode<T>* greatgrandparent = grandparent->parent;
	bool grandparentright;
	if(greatgrandparent != nullptr){
		grandparentright = (greatgrandparent->child[1] == grandparent);
	}
	if(right != parentright){
		parent->child[right?1:0] = curr->child[right?0:1];
		grandparent->child[parentright?1:0] = curr->child[right?1:0];
		curr->child[parentright?1:0] = parent;
		curr->child[parentright?0:1] = grandparent;
		curr->col = false;
	} else {
		grandparent->child[parentright?1:0] = parent->child[right?0:1];
		parent->child[right?0:1] = grandparent;
		parent->col = false;
		curr = parent;
	}
	grandparent->col = true;
	if(greatgrandparent == nullptr){
		head = curr;
	} else {
		greatgrandparent->child[grandparentright?1:0] = curr;
	}
	return retval;
}

template <class T>
rbnode<T>* rbtree<T>::findBelow(int value){
	rbnode<T> *curr = head;
	rbnode<T> *best = nullptr;

	while(curr != nullptr){
		int comp = curr->value.sortval();
		if(value == comp){
			return curr;
		} else if(value > comp){
			best = curr;
			curr = curr->child[1];
		} else {
			curr = curr->child[0];
		}
	}

	return best;
}

template <class T>
rbnode<T>* rbtree<T>::findAbove(int value){
	rbnode<T> *curr = head;
	rbnode<T> *best = nullptr;

	while(curr != nullptr){
		int comp = curr->value.sortval();
		if(value == comp){
			return curr;
		} else if(value < comp){
			best = curr;
			curr = curr->child[0];
		} else {
			curr = curr->child[1];
		}
	}

	return best;
}

template <class T>
void rbtree<T>::remove( rbnode<T> *node){
	rbnode<T> *child = node->child[0];
	if(child != nullptr){
		rbnode<T> *ch2 = node->child[1];
		rbnode<T> *parent;
		if(ch2 != nullptr){
			do{
				parent = ch2;
				ch2 = parent->child[0];
			} while(ch2 != nullptr);
		}
		node->value = parent->value;
		node = parent;
		child = node->child[1];
	} else {
		child = node->child[1];
	}
	balance_blacks(node,child);
	delete node;
}

template <class T>
void rbtree<T>::balance_blacks( rbnode<T> *node, rbnode<T> *child){
	if(node->col) return;
	if(child->col){
		child->col = false;
		return;
	}
	rbnode<T> *parent = node->parent;
	if(parent == nullptr) return;
	rbnode<T> *sibling = parent->child[0];
	bool sright = (sibling == node);
	if(sright) sibling = parent->child[1];
	bool b = sibling->col;
	if(b){
		parent->col = true;
		sibling->col = false;
		rbnode<T> *newsib = sibling->child[sright?0:1];
		parent->child[sright?1:0] = newsib;
		sibling->child[sright?0:1] = parent;
		rbnode<T> *gp = parent->parent;
		if(gp == nullptr){
			head = sibling;
		} else if(gp->child[0] == parent){
			gp->child[0] = sibling;
		} else {
			gp->child[1] = sibling;
		}
		parent = gp;
		sibling = newsib;
	}
	// sibling is known black now

	// assume n on the left for naming purposes
	rbnode<T> *sl = sibling->child[sright?0:1];
	rbnode<T> *sr = sibling->child[sright?1:0];

	bool pcol = parent->col;
	bool lcol = (sl != nullptr) && sl->col;
	bool rcol = (sr != nullptr) && sr->col;

	if(!b && !pcol && !lcol && !rcol) {
		sibling->col = true;
		balance_blacks(parent, node);
		return;
	}

	if(pcol && !lcol && !rcol){
		sibling->col = true;
		parent->col = false;
		return;
	}

	if(!rcol){
		sibling->child[sright?0:1] = sl->child[sright?1:0];
		parent->child[sright?1:0] = sl;
		sl->child[sright?1:0] = sibling;
		sibling->col = true;
		sl->col = false;

		sr = sibling;
		sibling = sl;
		sl = sibling->child[sright?0:1];
	}

	sibling->col = pcol;
	parent->col = false;
	sr->col = false;
	parent->child[sright?1:0] = sl;
	rbnode<T>* gp = parent->parent;
	if(gp == nullptr){
		head = sibling;
	} else if(gp->child[0] == parent){
		gp->child[0] = sibling;
	} else {
		gp->child[1] = sibling;
	}
}

template class rbtree<mem_tag>;
