# DB Homework 3 note

只要不使用range query，hash table好像还好？

**PMDK: Persistent Memory Development Kit**

**Storage Performance Development Kit**

上面这俩好像是类似的东西

​	

我应该看什么书

* log怎么写，怎么用，怎么持久化，怎么做roll-back
  * redo log
  * undo log
* 增加删除功能应该对我原来的数据库做什么修改
  * 或者说如何把原来写的数据库的事务支持和事务隔离增加到persistent KV-engine中
* 上面提到的持久化工具怎么用



### Coping with system failures

必须保证transation的原子性

* all or nothing

transaction manager:

* issue information(signals) to log manager
* scheduling different transactions，support concurrency

<img src="C:\Users\HarryM\AppData\Roaming\Typora\typora-user-images\image-20200611154742314.png" alt="image-20200611154742314" style="zoom:67%;" />

buffer manager的作用是什么啊？

disk访问总是通过buffer manager完成的

**3 address spaces**

* the space of disk
* the virtual or main-memory address space(managed by the buffer manager)
* the local space of the transaction
  * 看来每一个transaction关于所涉及的变量应该单独做一份拷贝？

consistent state

log records?

undo log足以从system failure中恢复，但是txn和buffer manager需要满足以下两个条件：

* `<T, X, v>` 形式的log record必须在X的new value被写到disk**之前**被写到disk
* `COMMIT` 形式的log record必须在该txn中所有变量修改持久化到disk**之后**，尽快被写到disk中

第一条的先后顺序只需要单独作用于每个变量即可，不需要作用于所有变量的group和所有update record group之间

log manager需要一个flush-log command

* 这个command 应该有返回值表明是否flush成功

A和B的new values在txn的最后被统一写到disk中

以block的形式读写disk

**多线程中log可能出现的问题**

* 如果两个线程中的A和B变量在同一个block中，可能导致过早地写其中一个线程汇总变量的问题

**Abort需要特殊处理吗？**

* 19.9.1里面似乎有说？

recovery by undo log is idempotent, so no worry for crash during recovery 

####  **Recovery manager using undo log**

**Checkpointing**

* 需要执行checkpoint之前，需要停止接收新的txn，把当前正在活跃的txn执行到COMMIT或者ABORT，把相应的LOG flush到disk，然后把CKPT flush到disk
* checkpoint之前的txn肯定是已经完成了的，它们不需要undo
  * CKPT之前的log可以丢弃

**Nonquiescent checkpointing**

* 制作checkpointing的时候仍然允许新的txn发射

log格式

```
<START CKPT(T1, T2, ...)>
<END CKPT>
```

crash以后回溯log的时候可能有两种情况：

* 首先遇到的是`END CKPT` ，那么再往前走的第一个`<START CKPT>` 之前的log都可以被丢掉
* 首先遇到的是`START SKPT`，那么说明这个CKPT没有制作完成(两个CKPT的制作过程可以交叉吗？感觉应该不可以)，那么直到上一个`START CKPT`之间发射的txn log都需要被check
  * 上一个`START CKPT`之前的log可以被删除掉

**循环利用磁盘空间来存log的情况下如何找到最后一条log**

* 首先需要一个文件来循环存放有哪些文件(FILE pointer?)用于循环存放log
  * 根据这个文件找到最后一条log所在的文件（我感觉我多半不会存这么多文件）
* 其次log在多个文件构成的大文件中是循环存放的，找到最后一条log所在的文件之后可以根据上下两条log 的 index相对关系来锁定最后一条log（最后一条log的index比他右边的一条大）



#### Recovery using redo logging

把数据改变暂存在main memory里可以减少IO

redo logging重做已经commit 的txn，并且忽视没有commit的txn

writing-head logging rule

`<T, X, v>` 里存的是新值

写disk的顺序是：

* update line
* COMMIT log
* change the elements value

需要在CKPT吧dirty的buffer刷出去

* 这里的dirty指的是里面有已经commit的txn的数据但是还没有更新到disk
* 每个txn需要记录对应的buffer是什么（或者双向记录？）

最好用linked list组织一个txn的所有log？



#### Recovery using Redo/Undo logging

单独使用undo或者redo logging都会在是否刷出buffer这件事上造成冲突

* 不同的txn可能选择刷出或者不刷出buffer
* 那我就直接不同的线程用不同的buffer，只使用redo log可以吗？

多线程的问题

* 一个committed txn A可能读了一个uncommitted txn B的dirty value，这种情况下先redo和先undo得到的结果是不同的
* QAQ



## PMDK 

* 这个东西似乎很有用

transactional？



如果我倒着回溯log，哪些是有价值的？

* 首先要收集路上遇到的所有txn id
  * 如果这个txn id首先遇到的命令不是commit，那么这个txn应该直接被忽略
  * 建立一个txn id到结构体txn_info的映射
    * txn_info中应该有以下信息
      * valid，就是是否被commit的，是否需要记录其中命令的
      * command vector

* 试图遇到一个KPT END，那么进入一个分支
  * 找到对应的KPT BEGIN，那么获取到其中活跃的txn



算了，还是顺序遍历吧

* 第一遍顺序遍历，做两件事
  * 找出其中所有commit语句，记录其中的txn id
  * 试图找到最后一个有END的KPT BEGIN(不妨用一个vector来存所有的KPT index)
    * 如果找到了满足条件的KPT BEGIN，用这个集合筛选掉committed txn id set
* 第二遍遍历
  * 重做所有在committed txn id set 中的txn命令
    * 这不用进入线程中来做



fopen读写模式有一个问题是

* 比如我读到了文件的某个位置，这个时候我写，是写到什么位置？





## Fresh words

resilient 可迅速恢复的，有适应力的

archiving 存档

parity check 奇偶校验

remedy 处理方法，改进方法

metricate 错综复杂

ad-hoc 点对点的(这个单词我老是忘了QAQ)

extent 程度，限度

prematurely 早熟地

nonquiescent 不安静的

idempotent 幂等的

truncate 截短，缩短

pertaining 存在

untangle 解开，松开





