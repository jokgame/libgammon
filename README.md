libgammon
=========

`libgammon` 实现 `backgammon` 游戏状态接口。

c/c++
-----

#### 使用源代码

可直接拷贝源代码目录 `backgammon` 目录到项目中，然后在需要的地方包含头文件 `backgammon/backgammon.h` 即可。

#### 编译成库

通过 `make` 命令编译成库引入到项目中，然后在需要的地方包含头文件 `backgammon/backgammon.h` 即可。

python
------

#### 安装

```
pip3 install libgammon
```

#### 使用

```py
from libgammon import Color, Grid, Move, Action, Game, Env
# ...
```
