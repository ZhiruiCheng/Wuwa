# 切人动画快照：实现与阅读顺序

本次实现的是本地地面移动中的换人交接，包含 `StartMove` 起步和 `Move` 循环移动。沿用库洛导出代码可确认的两层职责：先交接运动事实，再同步动画状态和表现参数。不是对原版 `SyncAnimStates` 内部实现的还原。

## 起步叠加拆分

公共模板 `ABP_RoleBase` 的 `SM_Ground` 现在按以下路径运行：

- `Idle → StartMove → Move`：原有一次性起步 additive 位于 `StartMove`，`Move` 只输出原来的 `CP_MoveGround` 缓存姿势。
- `StartMove → Stop / Idle`：沿用已有停止与动作打断条件；优先于起步完成规则。
- `StartMove → Move`：起步播放器剩余 0.05 秒时开始 0.1 秒混合。
- `Stop → StartMove`：当前资产在重新按下方向键时重新进入起步状态；腿部循环续接脚步相位，起步 additive 保留自己的入场行为。早期拆分脚本保留的是 `Stop → Move`，此处以当前资产及运行测试结果为准。

本轮保留原有走、跑、冲刺共用起步表现，只拆分已有起步叠加；没有另外增加“步行转跑”动作或空中/落地状态。原 additive 播放器、资源绑定及节点 GUID 保留，子角色使用自己的原有资源覆盖。

迁移入口为 `UWuwaRoleStopEditorLibrary::SplitTemplateMoveStart`，只修改内存、不自动保存。执行脚本、修改前后图导出和资产备份在 `Saved/Diagnostics/MoveStartSplit`。保存前会编译公共图和两个子角色，并核对 `ParentAssetOverrides` 未变化。

## 连续点按：Stop 与腿部循环接续（2026-10-15）

分开的状态可以使用同一同步组。原走跑 Stop 节点为 `DoNotSync`，移动循环完全淡出后 `Locomotion` 不再提供有效脚步相位，再次进入 StartMove 时腿部回到默认起点。对照测试 64 次恢复中，32 次出现同步标记丢失及固定相位重启。

- `ConfigureStopLocomotionSync` 只把 Walk/Run 的四个 Stop 播放器接入 `Locomotion`，设为 `AlwaysLeader` 并开启 `Override Position when Joining Sync Group as Leader`。保留已选好的首段落脚时间，避免循环动画反过来拖动或倒退停步。
- Stop 素材只有开头两枚接触标记，超过最后标记后 UE 会丢失有效 marker position。`WuwaAnimSync.cpp` 的 `StopTailContact` 在真实 Stop → StartMove/Move 的这一帧检查当前选中的 Stop 播放器；仅当它已越过最后标记且同步组无有效相位时，按最后接触脚初始化重新进入的腿部循环。复用已有 marker 时间映射，在图初始化后、引擎同步 tick 前执行一次，不按整个停步片段长度推算走跑相位。
- 有效标记区间完全由 UE Sync Group 接续。Idle 重新起步不使用尾段补偿；起步 additive 的 `LocomotionStart` 独立播放；没有标记的 Sprint Stop 保持原有设置。本轮未编辑动画素材或状态转场规则。

公共模板和两个子角色修改前备份、图导出、保存记录在 `Saved/Diagnostics/MoveResume`。保存前按导出路径核对 28 个原始状态/转场对象及子角色资源覆盖；不把编译器重新生成的临时图计入比较。

验证：正常 `WuwaEditor Win64 Development` 构建及脚本发布通过。`FinalRegression/index.json` 为 12 项成功、0 项失败，其中 `StartLeanPIE` 有输入配置和蓝图对象类型告警，其余 11 项无告警。`RapidTaps.csv` 包含两个角色 × Walk/Run × 短起步/持续移动 × 8 次 WASD 恢复，共 64 次；其中 14 次覆盖最后标记之后的接续。还覆盖完整停步回 Idle、真正重新起步、双向角色切换和起步前倾。测试使用无界面 UE 与规定的速度样本，未代替手动画面/操作手感验收。

## 阶段一：采集

`WuwaAnimSyncSnapshot.h` 定义纯值快照，`UWuwaAnimInstance::CaptureRoleAnimation()` 从完成的动画帧读取：

- 共用动画图的所属类，用于拒绝不兼容的图。
- 记录 `SM_Ground` 当前处于 `StartMove` 还是 `Move`，采集 `Locomotion` 同步组中实际有权重的循环 Sequence / BlendSpace Player，按生成属性名称匹配。两个状态共用缓存姿势，真正的 BlendSpace 不直接列在这些状态的播放器列表里。
- 在 `StartMove` 中额外采集 `LocomotionStart` 组的非循环 Sequence Player。它记录自己的归一化时间，不使用跑步循环的脚步相位；结束值 1 保持为目标动画结尾，不取模回到开头。
- 每个播放器的归一化进度和同步组标记位置。
- 动画数据及明确允许复制的数值参数。不存在的蓝图变量会跳过，不创建原作同名的假参数。

采集通过游戏线程 Proxy 接口等待该实例并行工作完成。快照没有旧实例、旧播放器或动画资源指针。未初始化、非地面移动、正在播放蒙太奇时不产生可用的动画快照。

## 阶段二：运动交接

`UWuwaFormationComponent::SwitchTo()` 在清技能、取消控制之前调用旧角色的 `CaptureRoleSwitch()`，保存：移动轴、速度、最近输入、加速度、走跑偏好、冲刺需求、临时冲刺剩余时间。

新角色完成上场初始化和 Possess 后，`RestoreRoleSwitch()` 恢复运动数据，再让 RoleGait 按新角色的限制重新接受步态。临时冲刺只续剩余时间，打开的动画窗口按切人时结束处理，不复制窗口的对象引用；持续冲刺仍会在松开方向后清除。

离场角色的胶囊碰撞先关闭，防止新角色上场找地面时撞到它。旧角色技能、蒙太奇的清理规则保留。

## 阶段三：动画交接

`QueueRoleAnimation()` 将请求交给新实例自己的 `FWuwaAnimSyncProxy`。

下一次 `UpdateAnimationNode()`：

1. 在根图完成初始化后，按外到内的顺序选择 `Locmotion.Ground` 和快照中的 `SM_Ground.StartMove / Move`。
2. 应用可复制的数值参数。
3. 正常更新动画图，让新角色选择自己的动画资产并初始化播放器。
4. 在引擎推进同步组之前设置播放器累计时间。目标资产存在有效标记时使用标记位置，否则按归一化进度映射。BlendSpace 同时接过滤波后的采样坐标，其累计时间本身就是归一化值，按该坐标处权重最大的带标记样本动画映射标记。循环末尾到开头的标记区间也需要单独处理。
5. 清空请求，后续帧正常播放。重新初始化 Proxy 也会清空请求。

切人流程立即进行一次零时长动画更新和骨骼求值，在本帧渲染前得到新姿势。继续保留上场时的 `InitializeAnimation()`，用于清掉新角色上次离场的残留；交接发生在它之后。

## 当前边界

- 覆盖共用角色动画图的地面起步与循环移动（走、跑、冲刺）。状态名称和同步组是当前项目动画图的约定，不是任意 AnimBP 的通用复制器。
- 不复制攻击、技能通知、蒙太奇、旧角色资源、任意蓝图变量、完整过渡栈或骨骼姿势。
- 停步、空中、攀爬尚未实现相位继承。状态之间正在进行的完整混合过渡栈也不复制：记录当前目标状态和其播放器，切人边界可能失去旧状态剩余的混合贡献。
- 运动快照只接到当前权威端切人流程。网络端的 OnRep 顺序、远端相位与客户端预测尚未扩展；不宣称已经完成联机同步。
- 起步拆分修改公共状态机图，并重新编译保存两个子角色；没有修改动画序列、BlendSpace 资源或引擎源码。

## 验证与学习

自动化测试 `Wuwa.Animation.RoleSwitch.Ground` 使用真实 changli / jiabei 角色资产，分别执行双向 Walk / Run / Sprint 交接，检查首帧状态、速度、输入、步态、标记/归一化进度以及后续动画推进。

2026-10-14 初版循环同步验证结果：

- `WuwaEditor Win64 Development` 编译通过；ManagedWuwa Release 发布通过。
- 相关回归共 14 项通过：6 项双向地面切换、2 项角色姿势/停步、3 项运动状态、3 项切人输入。切换测试还覆盖不兼容图拒绝、临时冲刺到期和目标步态限制。
- 其中 `Wuwa.Input.Formation.Routing` 带输入绑定和蓝图 `Invalid object types` 警告，断言通过；本次没有修改这些资产配置。
- 结果：`Saved/Automation/RoleAnimationSwitchRegression/index.json`；日志：`Saved/Logs/RoleAnimSync-Regression.log`。

代码阅读顺序：`WuwaRoleSwitchSnapshot.h` → `WuwaCharacterSwitch.cpp` → `WuwaAnimSyncSnapshot.h` → `WuwaAnimSync.cpp` → `WuwaRoleGaitComponent.cs` 的 `RestoreSwitchGait_Implementation`。

手动体验时，先在地面持续按方向切人，再按 Alt 切换走跑，最后验证冲刺切人和松开方向后的停步。自动化的零时长首帧检查不代替实际画面的自然度验收。

### 起步拆分验证（2026-10-14）

- C++ 编译通过，公共模板及 changli / jiabei 两个子动画蓝图编译、保存通过。
- 最终 16 项回归通过：初次运行 14 项通过；新增的两项测试修正“停用播放器缓存权重不代表当前姿势贡献”和“移动策略会替换直接写入的动作状态”两处测试采样后，复测通过。运行时代码和蓝图在这次复测前未再更改。
- 新增双向起步测试验证：进入 StartMove、起步播放器独立进度续播、续播后继续推进、结束后进入 Move 且起步状态权重归零、真正重新起步仍会播放、松开输入进入 Stop、动作状态样本能打断到 Idle。
- 原六项循环同步测试新增了持续移动切人不激活 StartMove 的检查。角色姿势、停步相位、运动状态、切人输入回归均通过；输入 Routing 仍有原有绑定/Invalid object types 警告。
- 最终结果汇总：`Saved/Diagnostics/MoveStartSplit/Verification.json`，保留原始及复测报告路径。备份路径见同目录 `Applied.json`。
- 本轮验证为真实角色资产的自动化姿势求值，没有进行人工 PIE 画面验收，也不宣称同步了正在混合的完整过渡栈。

编辑器检查入口：`ABP_RoleBase → AnimGraph → Locmotion → Ground → SM_Ground`。展开 `StartMove` 可见原起步叠加，展开 `Move` 仅有地面移动缓存姿势。当前走、跑、冲刺继续共用原有起步表现，尚未独立制作 WalkToRun、落地等新阶段。

### 起步前倾反馈诊断（2026-10-14）

用户反馈完全静止起跑时看不到前倾。核对发现，两个子角色都没有单独覆盖起步播放器，而是继承父图的 `/Game/Characters/Role/changli/NewModel/Support/LocoDetail_Accel_F`。这不是资源为空；其长度约 0.968 秒，Local Space Additive，参考动画为 Support/Run_BasePose。

- 新增 `Wuwa.Animation.RoleSwitch.StartLean`：为每个角色创建两个临时实例，在相同速度、时间下仅关闭对照实例的 additive，比较最终头部到骨盆方向。两个角色最大前倾差分别为 16.229° / 16.207°，测试通过。数据为 `Saved/Diagnostics/MoveStartSplit/LeanComparison.csv`。
- 新增 `Wuwa.Animation.RoleSwitch.StartLeanPIE`：使用 `/Game/Map/Wuwa`、真实控制器输入路由和移动物理，从静止起跑并切到另一个角色再次起跑。两个角色都进入有权重的 StartMove，随后进入 Move，起步阶段约 1.05 秒。数据为同目录 `LeanPIE.csv`，报告为 `Saved/Automation/MoveStartLeanPIE/index.json`。
- PIE 诊断使用 NullRHI，因此在测试实例上开启 AlwaysTickPoseAndRefreshBones；验证的是最终骨骼姿势，不是屏幕像素。没有修改或保存动画资产、角色设置、地图。
- 当前新进程中没有复现用户所见的无前倾，不能据此宣称已修复该反馈，也没有证实是编辑器旧实例/缓存导致。下一步需对照用户编辑器里选中的实际运行实例。
