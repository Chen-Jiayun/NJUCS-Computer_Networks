| 姓名 | 学号  | 邮箱  | 完成时间  |
|------|---|---|---|
|   陈嘉昀   | 211220137  | jiayunchen@smail.nju.edu.cn | 2023年11月9日  |

*贴图请查看pdf*


### 1 代码
#### 1.1代码设计
整个发送逻辑就是画好状态图然后对着做就好，有这么几个edge case需要注意：
* syn包一定在local的_syn还在是为false的时候就发送，并且把_syn设置为true
* fin包有可能跟随数据包发送（如果接收方的窗口可以同时容纳“数据”和"fin bit"），也有可能单独发送（发送最后一个数据包的时，接收方的窗口放不下"fin bit"）
* 新的ack到达以后，对与timer的"清零"处理，必须在新的ack对outstanding_seg有影响的情况下进行
* 当tick发生的时候，如果要对timer进行“倍增处理”，必须在当前的window_size不为0的情况下进行


#### 1.2 规约调整
在这次写的代码中，触发了ByteStream的一个defensive programming：
![[截屏2023-11-09 20.29.15.png]]
![[截屏2023-11-09 20.29.43.png]]

这实际上是我设计的byte stream的一个undefined behavior，现在需要增加相应的specification：在使用read函数时，必须先通过buffer_size的检验
![[截屏2023-11-09 20.31.39.png]]

### 2 运行结果

![[截屏2023-11-09 20.33.45.png]]
