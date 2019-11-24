int __radix_tree_insert(struct radix_tree_root *root, unsigned long index,
			unsigned order, void *item)
{
	struct radix_tree_node *node;
	void __rcu **slot;
	int error;

	BUG_ON(radix_tree_is_internal_node(item));

	error = __radix_tree_create(root, index, order, &node, &slot);
	if (error)
		return error;

	error = insert_entries(node, slot, item, order, false);
	if (error < 0)
		return error;

	if (node) {
		unsigned offset = get_slot_offset(node, slot);
		BUG_ON(tag_get(node, 0, offset));
		BUG_ON(tag_get(node, 1, offset));
		BUG_ON(tag_get(node, 2, offset));
	} else {
		BUG_ON(root_tags_get(root));
	}

	return 0;
}

判断该 item 是否是树中的节点：判断该指针的最后一位是否为1，如果是1就表明是在树中的节点。  

创建一个 node 并通过参数获取，获取 slot




