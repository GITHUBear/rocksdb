# BlockBasedTable

## SSTable 格式

SSTable 的基本格式如下所示：

```
<beginning_of_file>
[data block 1]
[data block 2]
...
[data block N]
[meta block 1: filter block]                  (see section: "filter" Meta Block)
[meta block 2: stats block]                   (see section: "properties" Meta Block)
[meta block 3: compression dictionary block]  (see section: "compression dictionary" Meta Block)
...
[meta block K: future extended block]  (we may add more meta blocks in the future)
[metaindex block]
[index block]
[Footer]                               (fixed size; starts at file_size - sizeof(Footer))
<end_of_file>
```

## 类结构

## BlockBasedTableBuilder 解析

### Add 


