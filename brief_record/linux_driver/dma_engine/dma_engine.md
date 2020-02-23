# dma 框架

从 dma_request_chan() 开始：

struct dma_chan *dma_request_chan(struct device *dev, const char *name) 返回一个 dma_chan 用作 dma 传输。  

直接从设备树获取：chan = of_dma_request_slave_channel(dev->of_node, name);
                        同时判断 np 和 name 有一个为空返回失败，通常一个设备中的 dma-names 属性会同时指定几个 dma 通道，所以找到 np 还要指定 name 以定位对应的 dma 通道。  
                        获取 "dma-names" 的数量
                        根据当前的 dmas 属性找到引用的 controller 节点，dmas 属性第一个字节是 controller 的 phandler，后面字节是参数。  
                        根据 np 即设备树节点的匹配从 of_dma_list 中找到对应的 of_dma。
                        然后调用 of_dma->of_dma_xlate 的解析函数，解析当前节点，返回 dma_chan.


不从设备树获取：轮询所有的 dma_device_list，这个 list 中链接了所有的 dma_device ，也就是                   dma controller，使用 dma_filter_match 和 find_candidate 来找到 dma_chan.


获取到 dma_chan 之后调用 dmaengine_slave_config 对 dma_chan 进行配置。  

config 部分主要是配置 

struct dma_slave_config {
	enum dma_transfer_direction direction;
	phys_addr_t src_addr;
	phys_addr_t dst_addr;
	enum dma_slave_buswidth src_addr_width;
	enum dma_slave_buswidth dst_addr_width;
	u32 src_maxburst;
	u32 dst_maxburst;
	u32 src_port_window_size;
	u32 dst_port_window_size;
	bool device_fc;
	unsigned int slave_id;
};

然后传入到 dmaengine_slave_config 对 dma_chan 进行配置。  

然后使用 dmaengine_prep_slave_sg 获取传输的描述符。dmaengine_prep_slave_sg 实际上调用的是 chan->device->device_prep_slave_sg，

通常可以设置传输描述符的 callback 成员，传输成功之后调用的回调函数。  

然后调用 dmaengine_submit(desc) 提交本次的 dma 请求，实际调用 desc->tx_submit

调用 dma_async_issue_pending 检测并开始 dma 的工作，实际调用 chan->device->device_issue_pending(chan);







