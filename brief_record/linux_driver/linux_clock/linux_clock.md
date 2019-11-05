clock 的配置可以先查看:
	/sys/kernel/debug/clk/clk_summary

struct clk *clk_register(struct device *dev, struct clk_hw *hw)
被定义在：drivers/clk/clk.c



找到是怎么把所有节点添加进去的。 

搞清楚是怎么和 struct device 和 字符串 联系到一起的。  

也就是先需要搞清楚，struct device 的时钟配置是怎么来的。


