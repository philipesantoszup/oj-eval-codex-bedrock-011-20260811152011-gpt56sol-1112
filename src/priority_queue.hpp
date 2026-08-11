#ifndef SJTU_PRIORITY_QUEUE_HPP
#define SJTU_PRIORITY_QUEUE_HPP

#include <cstddef>
#include <functional>
#include "exceptions.hpp"

namespace sjtu {
/**
 * @brief a container like std::priority_queue which is a heap internal.
 * **Exception Safety**: The `Compare` operation might throw exceptions for certain data.
 * In such cases, any ongoing operation should be terminated, and the priority queue should be restored to its original state before the operation began.
 */
template<typename T, class Compare = std::less<T>>
class priority_queue {
private:
	struct node {
		T value;
		node *left;
		node *right;
		size_t null_path_length;

		explicit node(const T &value_) : value(value_), left(nullptr),
			right(nullptr), null_path_length(1) {}
	};

	struct copy_task {
		const node *source;
		node *destination;
		copy_task *next;

		copy_task(const node *source_, node *destination_, copy_task *next_)
			: source(source_), destination(destination_), next(next_) {}
	};

	node *root;
	size_t element_count;

	static size_t length(node *p) {
		return p == nullptr ? 0 : p->null_path_length;
	}

	/**
	 * Comparisons are all performed while descending the right spines.  Node
	 * links are changed only while unwinding, after the last comparison has
	 * succeeded.  Consequently an exception from Compare leaves both input
	 * trees completely untouched.
	 */
	static node *meld(node *first, node *second, Compare &compare) {
		if (first == nullptr) return second;
		if (second == nullptr) return first;

		if (compare(first->value, second->value)) {
			node *temporary = first;
			first = second;
			second = temporary;
		}

		node *merged_right = meld(first->right, second, compare);
		first->right = merged_right;
		if (length(first->left) < length(first->right)) {
			node *temporary = first->left;
			first->left = first->right;
			first->right = temporary;
		}
		first->null_path_length = length(first->right) + 1;
		return first;
	}

	/** Delete a tree iteratively so a long left spine cannot overflow stack. */
	static void clear(node *p) {
		while (p != nullptr) {
			if (p->left != nullptr) {
				node *left_child = p->left;
				p->left = left_child->right;
				left_child->right = p;
				p = left_child;
			} else {
				node *right_child = p->right;
				delete p;
				p = right_child;
			}
		}
	}

	/** Iterative deep copy, also safe for heaps with a linear left spine. */
	static node *clone(const node *source) {
		if (source == nullptr) return nullptr;

		node *result = new node(source->value);
		result->null_path_length = source->null_path_length;
		copy_task *tasks = nullptr;
		try {
			tasks = new copy_task(source, result, nullptr);
			while (tasks != nullptr) {
				copy_task *current = tasks;
				tasks = tasks->next;
				const node *from = current->source;
				node *to = current->destination;
				delete current;

				if (from->left != nullptr) {
					node *child = new node(from->left->value);
					child->null_path_length = from->left->null_path_length;
					try {
						tasks = new copy_task(from->left, child, tasks);
					} catch (...) {
						delete child;
						throw;
					}
					to->left = child;
				}
				if (from->right != nullptr) {
					node *child = new node(from->right->value);
					child->null_path_length = from->right->null_path_length;
					try {
						tasks = new copy_task(from->right, child, tasks);
					} catch (...) {
						delete child;
						throw;
					}
					to->right = child;
				}
			}
		} catch (...) {
			while (tasks != nullptr) {
				copy_task *next = tasks->next;
				delete tasks;
				tasks = next;
			}
			clear(result);
			throw;
		}
		return result;
	}

public:
	/**
	 * @brief default constructor
	 */
	priority_queue() : root(nullptr), element_count(0) {}

	/**
	 * @brief copy constructor
	 * @param other the priority_queue to be copied
	 */
	priority_queue(const priority_queue &other)
		: root(clone(other.root)), element_count(other.element_count) {}

	/**
	 * @brief deconstructor
	 */
	~priority_queue() {
		clear(root);
	}

	/**
	 * @brief Assignment operator
	 * @param other the priority_queue to be assigned from
	 * @return a reference to this priority_queue after assignment
	 */
	priority_queue &operator=(const priority_queue &other) {
		if (this == &other) return *this;
		priority_queue temporary(other);
		node *old_root = root;
		root = temporary.root;
		temporary.root = old_root;
		size_t old_count = element_count;
		element_count = temporary.element_count;
		temporary.element_count = old_count;
		return *this;
	}

	/**
	 * @brief get the top element of the priority queue.
	 * @return a reference of the top element.
	 * @throws container_is_empty if empty() returns true
	 */
	const T & top() const {
		if (root == nullptr) throw container_is_empty();
		return root->value;
	}

	/**
	 * @brief push new element to the priority queue.
	 * @param e the element to be pushed
	 */
	void push(const T &e) {
		node *inserted = new node(e);
		try {
			Compare compare;
			node *new_root = meld(root, inserted, compare);
			root = new_root;
			++element_count;
		} catch (...) {
			delete inserted;
			throw runtime_error();
		}
	}

	/**
	 * @brief delete the top element from the priority queue.
	 * @throws container_is_empty if empty() returns true
	 */
	void pop() {
		if (root == nullptr) throw container_is_empty();
		node *old_root = root;
		try {
			Compare compare;
			node *new_root = meld(old_root->left, old_root->right, compare);
			root = new_root;
			--element_count;
		} catch (...) {
			throw runtime_error();
		}
		delete old_root;
	}

	/**
	 * @brief return the number of elements in the priority queue.
	 * @return the number of elements.
	 */
	size_t size() const {
		return element_count;
	}

	/**
	 * @brief check if the container is empty.
	 * @return true if it is empty, false otherwise.
	 */
	bool empty() const {
		return element_count == 0;
	}

	/**
	 * @brief merge another priority_queue into this one.
	 * The other priority_queue will be cleared after merging.
	 * The complexity is at most O(logn).
	 * @param other the priority_queue to be merged.
	 */
	void merge(priority_queue &other) {
		if (this == &other || other.root == nullptr) return;
		try {
			Compare compare;
			node *new_root = meld(root, other.root, compare);
			root = new_root;
			element_count += other.element_count;
			other.root = nullptr;
			other.element_count = 0;
		} catch (...) {
			throw runtime_error();
		}
	}
};

}

#endif
