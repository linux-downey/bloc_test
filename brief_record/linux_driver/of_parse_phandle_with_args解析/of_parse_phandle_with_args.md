
获取对应的 device node 节点，结果放在 struct of_phandle_args *out_args
int of_parse_phandle_with_args(const struct device_node *np, const char *list_name,
				const char *cells_name, int index,
				struct of_phandle_args *out_args)
{
	if (index < 0)
		return -EINVAL;
	return __of_parse_phandle_with_args(np, list_name, cells_name, 0,
					    index, out_args);
}


参数： 
np：当前的 device node
list_name：当前节点中指定节点的名称
cells_name：目标节点中描述 cell 的名称
index：当前节点中指定的 index
out_args：存放目标节点和参数的结构体



struct of_phandle_args {
	struct device_node \*np;    		//获取到的目标节点的 node
	int args_count;             		//参数的个数
	uint32_t args[MAX_PHANDLE_ARGS];	//参数
};









