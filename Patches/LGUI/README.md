# LGUI 本地 UE 5.7 适配改动

LGUI 通过 `Plugins/LGUI` 子模块引用，版本由主仓库的子模块提交指针固定。
`local-ue57-changes.patch` 保存了 2026-10-27 工作区中的七个文件修改。

新克隆项目并初始化子模块后，在项目根目录执行：

```powershell
git submodule update --init --recursive
git -C Plugins/LGUI apply --check ../../Patches/LGUI/local-ue57-changes.patch
git -C Plugins/LGUI apply ../../Patches/LGUI/local-ue57-changes.patch
```

已有这些修改的本机工作区不要重复应用。检查是否已经应用：

```powershell
git -C Plugins/LGUI apply --reverse --check ../../Patches/LGUI/local-ue57-changes.patch
```

UnrealSharp 也有一份本地补丁，参见 `../UnrealSharp/README.md`。
