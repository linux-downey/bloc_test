在现代的处理器中，一般都会存在一个 GIC ，狭义上的 GIC 是专用的中断控制器。 

但是像某些外设 block，比如 gpio ，也可以是一个 interrupt controller，通常 gpio 在 GIC 中只占到一根或者两根中断线。

需要 gpio 自己来控制中断的分发。  

hw_irq 和 irq_num：

在 CPU 的角度来说，IRQ num 应该是统一的不重复的，这是IRQ num 映射之后的结果。  

事实上，hw_irq 指硬件上的 irq 线，比如gpio bank 的中断线从0开始，gpio1 、gpio2...对应 hw_irq 0,1....

但是实际上，面对驱动工程师时，gpio1的irq num并不是 0,1等等。

因此，所有的 hw_irq 都需要将可能重复的hw_irq 号统一映射到系统中，而且能唯一标识一个irq。


这一部分映射就由 irq_domain 来处理.

irq_domain 就像是一个容器，它需要先被创建，然后再往其中添加映射。  
	
	在实际的 gpio-controller 的 driver 中，将会调用：
		// 先获取该 gpio bank 的 irq base num，也就是该 gpio irq num 在整个系统中的起始 irq 位置。
		irq_base = devm_irq_alloc_descs(&pdev->dev, -1, 0, 32, numa_node_id());
		irq_domain_add_legacy()


	
