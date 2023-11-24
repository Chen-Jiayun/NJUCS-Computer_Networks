| 姓名 | 学号  | 邮箱  | 完成时间  |
|------|---|---|---|
|   陈嘉昀   | 211220137  | jiayunchen@smail.nju.edu.cn | 2023年11月24日  |

*贴图请看pdf*

### 1 代码
#### 1.1代码设计
整个connection都不太需要设计到TCP协议中peer的各种状态，peer的状态都交给sender和receiver去维护就好，唯一需要关注的只有“何时关闭”即可

#### 1.2 bug修复
lab3中提到的设计是有问题的，size_t和uint16_t类型的数比大小不能先对size_t进行static_cast。
![[截屏2023-11-24 14.41.14.png]]


### 2 运行结果

![[Pasted image 20231124144150.png]]

