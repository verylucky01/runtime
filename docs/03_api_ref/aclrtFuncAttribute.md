# aclrtFuncAttribute

```
typedef enum {
    ACL_FUNC_ATTR_KERNEL_TYPE = 1,     // Kernel类型
    ACL_FUNC_ATTR_KERNEL_RATIO = 2,     // Kernel执行时需要的Cube Core与Vector Core的比例
} aclrtFuncAttribute;
```

当属性设置为ACL\_FUNC\_ATTR\_KERNEL\_TYPE时，属性值请参见[aclrtKernelType](aclrtKernelType.md)。
当属性设置为ACL\_FUNC\_ATTR\_KERNEL\_RATIO时，属性值需要使用以下方式获取：

  ```
  uint16_t* ratioArr = reinterpret_cast<uint16_t>(&attrValue);
  uint16_t aicratio = ratioArr[1];  // 表示Cube Core的比例
  uint16_t aivratio = ratioArr[0];  // 表示Vector Core的比例
  ```