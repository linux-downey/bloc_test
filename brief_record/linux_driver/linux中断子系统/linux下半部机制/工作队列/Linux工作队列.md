


## 结构体关系图
对于这核心的五个结构体，他们之间的对应关系是这样的：

* work_pool 存在多个，通常是对应 CPU 而存在的，系统默认会为每个 CPU 创建两个 worker_pool，一个普通的一个高优先级的 pool，这些 pool 是和对应 CPU 严格绑定的，同时还会创建两个 ubound 类型的 pool，也就是不与 CPU 绑定的 pool。

* worker 用于描述内核线程，由 worker pool 产生，一个 worker pool 可以产生多个 worker。

* workqueue_struct：属于上层对外的接口，它更像是一个协调管理者，将 work 分配给不同的 worker_pool.

* pool_workqueue:属于 worker_pool 和 workqueue 之间的中介，负责将 workqueue 和 worker_pool 联系起来。  

* work：由用户添加，可以是多个，一个 workqueue 对应多个 work


为了更方便地理解它们之间的关系，我们以一个简单的例子来讲解它们之间的联系：

workqueue_struct 相当于公司的销售人员，对外揽活儿，揽进来的活儿呢，就是 work，销售人员可以有多个，活也有很多，一个销售人员可以接多个活，而一个活只能由一个销售人员经手。所以 workqueue_struct  和 work 是一对多的关系。  

销售人员揽进来的活自己是干不了的，那这个分给谁干呢？当然是技术部门来做，交给哪个技术部门呢？自然是销售目前所对接的技术部门，技术部门负责分配技术员来做这个活，如果一个技术员搞不定，就需要分配多个技术员。   

这个技术部门就是 woker_pool，而这个技术员就是 woker，表示内核线程。当开发者调用 schedule_work 将 work 添加到工作队列时，实际上是添加到了当前执行这段代码的 CPU 的 worker_pool 的 work_list 成员中(通常情况下)，这时候 worker_pool 就派出 worker 开始干活了，如果发现一个 worker 干不过来，就会派出另一个 worker 进行协助。   

这个活干得好不好，进度怎么样，是否需要技术支持，这就需要一个项目管理来进行协调，向上连接销售人员，向下连接技术部门，这个项目管理起到一个联系作用，它就是 pool_workqueue。  


看了这个例子，不知道你对这几个核心结构体有没有更深的了解，如果没有，接着看下面的图片，因为了解核心结构体是了解整个 workqueue 实现原理的关键所在：


没有完全搞清楚的问题：
1、是否指定添加到当前 CPU worker_pool 中
2、如果同一个 workqueue 由不同的 CPU 执行，是不是就会把同一个 workqueue 中的 work 放到不同的 worker 中
3、一个 pool_workqueue 是对应 workqueue 数量的 还是对应 work 数量的？
 


```

```










