## 搭建韦东山二期驱动视频学习环境 --- Kernel 源码

本仓库是"[搭建韦东山二期驱动视频学习环境](http://www.maziot.com/2018/05/19/mini2440-build-100ask2drv-dev-env/)"博客中对应的 kernel 源码，适用于 mini2440 开发板

如何使用本仓库：

1. 克隆仓库

       user@vmware:~/mini2440$ git clone git@github.com:mini2440/100ask.linux-2.6.22.6.git

2. 修改交叉编译器

       user@vmware:~/mini2440$ cd 100ask.linux-2.6.22.6/
       user@vmware:~/mini2440/100ask.linux-2.6.22.6$ vim Makefile

3. 配置内核

       user@vmware:~/mini2440/100ask.linux-2.6.22.6$ cp config_ok .config

4. 编译

       user@vmware:~/mini2440/100ask.linux-2.6.22.6$ make uImage
