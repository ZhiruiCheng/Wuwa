# UnrealSharp 本地绑定生成改动

本项目使用 UnrealSharp 子模块，基准提交为 `46e9c2c1f27acf75d703a470412a01ad0af7ce1c`。
`local-glue-changes.patch` 保存了 2026-10-27 工作区内的三个绑定生成文件修改。
主仓库保存补丁，不向 UnrealSharp 官方仓库推送本地修改。

新克隆项目并初始化子模块后，在项目根目录运行：

```powershell
git submodule update --init --recursive
git -C Plugins/UnrealSharp apply --check ../../Patches/UnrealSharp/local-glue-changes.patch
git -C Plugins/UnrealSharp apply ../../Patches/UnrealSharp/local-glue-changes.patch
```

已有这些改动的工作区无需重复应用。可通过以下命令检查补丁是否已应用：

```powershell
git -C Plugins/UnrealSharp apply --reverse --check ../../Patches/UnrealSharp/local-glue-changes.patch
```
