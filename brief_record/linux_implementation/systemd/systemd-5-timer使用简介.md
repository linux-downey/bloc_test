# systemd-5-timer 的使用
在 systemd 的简介篇中，我们就知道，systemd 并不仅仅是 Upstart 或者 sysvinit 的替代品，相对来说，它有更大的野心：试图统一整个系统上用户空间的服务，因此，对于传统 linux 系统上相对独立的服务比如：syslog、udev、crontab 等，或是将其移植到 systemd 中，或是开发出替代方案。  

在传统的 linux 机器上，crontab 通常都是使用 crontab 实现的，而现在在部署了 systemd 的系统上，可以使用 的 systemd timer 服务，同时，旧版的 crontab 也还是支持的。  

如果你的系统上确实使用了 systemd，建议使用 timer 而不是旧版的 crontab，一方面，针对具体服务而言，基于 systemd 实现的 timer 定时服务配置更加方便且不失灵活性，另一方面，针对使用者而言，由于依附于 systemd 系统，直接通过 systemd 的命令来操作，对用户来说使用更方面，且日志被统一记录，维护起来也更得心应手。  


## systemd timer 简介
每个定时服务都对应一个定时单元，定时单元提供基于时间的事件，当事件产生时，定时单元对应的服务就会执行相应的动作，至于定时事件如何产生、产生之后执行怎样的动作，这由管理员的配置决定。  

定时单元是一个以 .timer 为后缀的文件，该文件符合单元文件的格式(单元文件介绍可以参考前面的章节：TODO)，定时单元匹配的服务可以通过 Unit= 选项显式指定。 若未指定，则默认是与该单元名称相同的 .service 单元,例如 foo.timer 默认匹配 foo.service 单元。  

**需要特别注意的是：如果在启动时间点到来的时候，匹配的单元已经被启动， 那么将不执行任何动作，也不会启动任何新的服务实例。 因此，那些设置了 RemainAfterExit=yes(当该服务的所有进程全部退出之后，依然将此服务视为处于活动状态) 的服务单元一般不适合使用基于定时器的启动。 因为这样的单元仅会被启动一次，然后就永远处于活动(active)状态了，当然，也不要周期性地执行那些可能长时间执行的服务，重复的启动是无效的。**  


## 依赖
各种类型的单元文件有不同的隐含依赖和默认依赖，对应定时服务而言，它的依赖如下。  


### 隐含依赖
定时器单元自动获得对匹配服务的 Before= 依赖。也就是相当于 foo.service 中添加 Before=foo.timer. 

### 默认依赖
除非明确设置了 DefaultDependencies=no ，否则 timer 单元将会自动添加下列依赖关系：

* Requires=sysinit.target, After=sysinit.target, Before=timers.target 依赖； 以及 Conflicts=shutdown.target, Before=shutdown.target 依赖(确保在关机前干净的停止)。 只有那些在系统启动的早期就必须启动的定时器，以及那些必须在关机动作的结尾才能停止的定时器才需要设置 DefaultDependencies=no 。

* 如果在 timer 单元中设置了至少一个 OnCalendar= 选项， 那么将会自动获得 After=time-sync.target 依赖， 以避免在系统时间尚未正确设置的情况下启动定时器。







