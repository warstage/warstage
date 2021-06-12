// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#ifndef WARSTAGE__GEOMETRY__QUAD_TREE_H
#define WARSTAGE__GEOMETRY__QUAD_TREE_H


const int QuadTreeNodeItems = 16;


template <class T> class QuadTree {
	struct Item {
		float x_, y_;
		T value_;
		Item() : x_(), y_(), value_() {}
		Item(float x, float y, T value) : x_(x), y_(y), value_(value) {}
	};

	struct Node {
		Node* parent_;
		Node* children_[4];
		float minX_, minY_;
		float maxX_, maxY_;
		float midX_, midY_;
		int minX100_, maxX100_, minY100_, maxY100_;
		Item items_[QuadTreeNodeItems];
		int count_;

		Node(Node* parent, float minX, float minY, float maxX, float maxY);
		~Node();

		int get_index() const;
		int get_child_index(float x, float y) const;
		void split();
		void reset();
	};

	Node root_;

public:
	class Iterator {
		float x_, y_;
		int x100_, y100_;
		int radius100_;
		float radiusSquared_;
		const Node* node_;
		int index_;

	public:
		Iterator(const Node* root, float x, float y, float radius);
		Iterator(const Iterator& i) :
		x_(i.x_), y_(i.y_),
		x100_(i.x100_), y100_(i.y100_),
		radius100_(i.radius100_),
		radiusSquared_(i.radiusSquared_),
		node_(i.node_),
		index_(i.index_) {}

		//~Iterator() {}

		const T* operator*() const;

		Iterator& operator++() {
			move_next();
			return *this;
		}

	private:
		bool is_within_radius(const Item* item) const;
		bool is_within_radius(const Node* node) const;
		bool is_within_radius(float x, float y) const;

		void move_next();
		const Node* get_next_node() const;
	};

public:
	QuadTree(float minX, float minY, float maxX, float maxY);
	//~quadtree();

	void insert(float x, float y, T value);
	void clear();

	Iterator find(float x, float y, float radius) const;

private:
	static int convert(float value) { return (int)(value * 100); }
};




template <class T> QuadTree<T>::QuadTree(float minX, float minY, float maxX, float maxY) :
    root_(0, minX, minY, maxX, maxY) {
}



/*template <class T> quadtree<T>::~quadtree()
{
}*/



template <class T> void QuadTree<T>::insert(float x, float y, T value) {
	Node* node = &root_;
	int level = 0;

	while (node->children_[0]) {
		node = node->children_[node->get_child_index(x, y)];
		if (++level > 12)
			break;
	}

	while (node->count_ == QuadTreeNodeItems) {
		node->split();
		if (++level > 12)
			break;
		node = node->children_[node->get_child_index(x, y)];
	}

	node->items_[node->count_++] = Item(x, y, value);
}



template <class T> void QuadTree<T>::clear() {
    root_.reset();
}



template <class T> typename QuadTree<T>::Iterator QuadTree<T>::find(float x, float y, float radius) const {
	return Iterator(&root_, x, y, radius);
}



template <class T> QuadTree<T>::Node::Node(Node* parent, float minX, float minY, float maxX, float maxY)
    : parent_(parent),
    minX_(minX), minY_(minY),
    maxX_(maxX), maxY_(maxY),
    midX_((minX + maxX) / 2), midY_((minY + maxY) / 2),
    minX100_(convert(minX)), maxX100_(convert(maxX)), minY100_(convert(minY)), maxY100_(convert(maxY)),
    count_(0) {
	children_[0] = 0;
	children_[1] = 0;
	children_[2] = 0;
	children_[3] = 0;
}



template <class T> QuadTree<T>::Node::~Node() {
	delete children_[0];
	delete children_[1];
	delete children_[2];
	delete children_[3];
}



template <class T> int QuadTree<T>::Node::get_index() const {
	if (this == parent_->children_[0]) return 0;
	if (this == parent_->children_[1]) return 1;
	if (this == parent_->children_[2]) return 2;
	if (this == parent_->children_[3]) return 3;
	return -1;
}



template <class T> int QuadTree<T>::Node::get_child_index(float x, float y) const {
	return (x > midX_ ? 1 : 0) + (y > midY_ ? 2 : 0);
}



template <class T> void QuadTree<T>::Node::split() {
	if (!children_[0]) {
		children_[0] = new Node(this, minX_, minY_, midX_, midY_);
		children_[1] = new Node(this, midX_, minY_, maxX_, midY_);
		children_[2] = new Node(this, minX_, midY_, midX_, maxY_);
		children_[3] = new Node(this, midX_, midY_, maxX_, maxY_);
	}

	for (int i = 0; i < count_; ++i) {
		Item& item = items_[i];
		int index = get_child_index(item.x_, item.y_);
		Node* child = children_[index];

		if (child->count_ == QuadTreeNodeItems)
			child->split();

		child->items_[child->count_++] = item;
	}

	count_ = 0;
}



template <class T> void QuadTree<T>::Node::reset() {
	count_ = 0;

	if (children_[0]) {
		for (int i = 0; i < 4; ++i) {
            children_[i]->reset();
        }
	}
}



template <class T> QuadTree<T>::Iterator::Iterator(const Node* root, float x, float y, float radius)
    : x_(x), y_(y),
    x100_(convert(x)), y100_(convert(y)),
    radius100_(convert(radius)),
    radiusSquared_(radius * radius),
    node_(root),
    index_(0) {
	if (node_) {
		--index_;
		move_next();
	}
}



template <class T> const T* QuadTree<T>::Iterator::operator*() const {
	return node_ ? &node_->items_[index_].value_ : 0;
}



template <class T> bool QuadTree<T>::Iterator::is_within_radius(const Item* item) const {
	return is_within_radius(item->x_, item->y_);
}



template <class T> bool QuadTree<T>::Iterator::is_within_radius(const Node* node) const {
	int minX = node->minX100_ - radius100_;
	if (x100_ < minX)
		return false;

    int maxX = node->maxX100_ + radius100_;
	if (x100_ > maxX)
		return false;

    int minY = node->minY100_ - radius100_;
	if (y100_ < minY)
		return false;

    int maxY = node->maxY100_ + radius100_;
    if (y100_ > maxY)
		return false;

	return true;
}



template <class T> bool QuadTree<T>::Iterator::is_within_radius(float x, float y) const {
	float dx = x - x_;
	float dy = y - y_;
	float distanceSquared = dx * dx + dy * dy;
	return distanceSquared <= radiusSquared_;
}



template <class T> void QuadTree<T>::Iterator::move_next() {
	while (node_) {
		if (++index_ == node_->count_) {
			index_ = 0;
			node_ = get_next_node();
			while (node_ && !node_->count_)
				node_ = get_next_node();
			if (!node_)
				return;
		}
		if (is_within_radius(&node_->items_[index_]))
			return;
	}
}



template <class T> const typename QuadTree<T>::Node* QuadTree<T>::Iterator::get_next_node() const {
	if (node_->children_[0]) {
		for (int index = 0; index < 4; ++index) {
            const Node* child = node_->children_[index];
			if (is_within_radius(child))
				return child;
        }
	}

	const Node* current = node_;
	while (current->parent_) {
		int index = current->get_index();
		while (++index != 4) {
            const Node* sibling = current->parent_->children_[index];
			if (is_within_radius(sibling))
				return sibling;
		}

		current = current->parent_;
	}

	return 0;
}


#endif
