### Redis 常见数据类型
String, Hash, List, Set, Zset(有序集合)

### Redis 数据类型笔记

| 数据类型 | 结构存储的值 | 读写能力（核心操作） | 典型应用场景 |
| :--- | :--- | :--- | :--- |
| **String** | 字符串、整数、浮点数 | 对整个字符串进行设置/获取（SET/GET/MSET/MGET）<br>对字符串部分内容进行操作（GETRANGE/SETRANGE/APPEND/STRLEN）<br>对整数/浮点数进行自增自减（INCR/DECR/INCRBY/DECRBY） | 缓存对象、分布式锁、计数器、Session共享 |
| **List** | 有序可重复的字符串列表 | 从头部/尾部推入或弹出元素（LPUSH/RPUSH/LPOP/RPOP）<br>获取列表范围内的元素（LRANGE）<br>阻塞式弹出（BLPOP/BRPOP）<br>通过索引获取/设置元素（LINDEX/LSET）<br>获取列表长度（LLEN） | 消息队列、最新消息列表、历史记录 |
| **Hash** | 键值对集合（field → value） | 设置/获取单个字段（HSET/HGET）<br>批量设置/获取字段（HMSET/HMGET）<br>获取所有字段和值（HGETALL）<br>对字段值进行自增（HINCRBY）<br>删除字段（HDEL）<br>判断字段是否存在（HEXISTS） | 存储对象（用户/商品）、购物车（field为商品ID） |
| **Set** | 无序不重复的字符串集合 | 添加/删除元素（SADD/SREM）<br>判断元素是否存在（SISMEMBER）<br>获取集合中所有元素（SMEMBERS）<br>获取集合大小（SCARD）<br>交集/并集/差集运算（SINTER/SUNION/SDIFF） | 标签系统、抽奖去重、共同好友模型 |
| **Sorted Set (ZSet)** | 带分数（score）的字符串集合，按分数排序且唯一 | 添加/删除元素（ZADD/ZREM）<br>按分数范围获取元素（ZRANGEBYSCORE）<br>按排名获取元素（ZRANGE/ZREVRANGE）<br>获取元素的分数（ZSCORE）<br>对元素分数进行自增（ZINCRBY）<br>按分数或排名范围删除（ZREMRANGEBYRANK/ZREMRANGEBYSCORE） | 排行榜、延时队列、带权重任务队列 |
| **BitMap** | 二进制位数组（每位为0/1） | 设置/获取某个位的值（SETBIT/GETBIT）<br>统计位值为1的数量（BITCOUNT）<br>对多个BitMap进行位运算（AND/OR/XOR/NOT）（BITOP）<br>查找第一个为0或1的位（BITPOS） | 每日签到、用户在线状态、活跃用户统计、布隆过滤器底层 |
| **HyperLogLog** | 不存元素本身，只存基数估算的统计结构 | 添加元素（PFADD）<br>估算独立元素数量（PFCOUNT）<br>合并多个HyperLogLog（PFMERGE） | UV统计、独立IP数、搜索词不重复计数 |
| **GEO** | 地理位置（经度、纬度、成员名） | 添加地理位置（GEOADD）<br>获取位置坐标（GEOPOS）<br>计算两位置间距离（GEODIST）<br>查找附近范围内的位置（GEORADIUS/GEORADIUSBYMEMBER）<br>获取位置的Geohash编码（GEOHASH） | 附近的人/店铺、打车范围搜索、地图位置存储 |
| **Stream** | 消息实体流（消息ID + 键值对字段） | 追加消息（XADD）<br>读取消息（XREAD）<br>消费者组模式读取（XREADGROUP）<br>确认消息（XACK）<br>获取未确认消息（XPENDING）<br>转移未处理消息（XCLAIM）<br>按范围读取消息（XRANGE/XREVRANGE） | 持久化消息队列、日志采集、实时数据管道（消费者组模式） |

### Redis架构
1. 单个`Redis`示例
   - 最直接的Redis部署方式，允许用户设置和运行小型实例
   - 缺点：如果此实例失败或不可用，则所有客户端对 `Redis` 的调用都将失败，从而降低系统的整体性能和速度
   - 命令发送到`Redis`后，首先在内存中处理，如果这些实例上设置了持久性，会在某个时间间隔产生一个子进程（注意不是线程）来生成数据持久化RDB快照或AOF
2. `Redis`高可用性
   - `Redis`的主从部署方式：当数据写入主实例时，它会将这些命令的副本发送到从部署客户端输出缓冲区，从而达到数据同步的效果
   - 从部署可以有一个或多个实例，它们可以帮助扩展 `Redis` 的读取操作或提供故障转移，以防 main 丢失
3. `Redis`复制
   - `Redis`每个主实例都有一个复制 `ID` 和一个偏移量
   - 主`Redis`部署上发生的每个操作都会增加偏移量
   - 当`Redis`副本示例罗由于主实例几个偏移量时，他会从主实例接受剩余的命令，然后在其数据集上重放，直到同步完成
   - 如果两个实例无法就复制 `ID` 达成一致，或者主实例不知道偏移量，则副本将请求全量同步。这时主实例会创建一个新的 `RDB` 快照并将其发送到副本。传输期间，主实例会缓冲快照截止和当前偏移之间的所有中间更新指令，这样在快照同步完后，再将这些指令发送到副本实例
   - 具有相同的复制 `ID` 和偏移量的实例具有完全相同的数据。当 `Redis `实例被提升为主实例或作为主实例从头开始重新启动时，它会被赋予一个新的复制 `ID`
   - 可以通过`ID`推断新提升的副本实例由哪个主实例幅值而来，从而推断不同实例的共同祖先，方便执行同步
4. `Redis`哨兵
   - 确保当前的主实例和从实例正常运行并做出响应
   - 当一个新的客户端尝试向 `Redis` 写东西时，`Sentinel` 会告知客户端当前的主实例
   - 监控——确保主从实例按预期工作。
   - 通知——通知系统管理员 `Redis` 实例中的事件。
   - 故障转移管理——如果主实例不可用并且足够多的（法定数量）节点同意这是真的，`Sentinel` 节点可以启动故障转移。
   - 配置管理——`Sentinel` 节点还充当当前主 `Redis` 实例的发现服务。
5. `Redis`集群
   - 使用`Redis`集群说明我们需要将存储的数据分散到多台机器上（分片），集群中每个`Redis`实例都被认为是整个数据的一部分
   - 向集群推送一个`key`，通过哈希处理等环节使给定的key始终映射到同一个分片