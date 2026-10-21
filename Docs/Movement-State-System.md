# 脚本移动状态框架

本实现对应本地鸣潮导出中可以验证的分工：`RoleGaitComponent` 决策，
`CharacterUnifiedStateComponent` 校验和提交，`CharacterMoveComponent` 收到状态事件后更新运动参数。
这是适配本项目 C#、GAS 和 CMC 的实现；不声称复现未导出的库洛原生 RootMotion 或网络预测实现。

## 权威状态放在哪里

| 内容 | 所有者 | 使用方 |
| --- | --- | --- |
| 语义输入、连续按住时间 | InputRouter / Character.PlayerInputState | GA、通知、RoleGait |
| Walk/Run 偏好、冲刺窗口、保留需求、步态禁用来源 | C# WuwaRoleGaitComponent | UnifiedState |
| 位置／移动／朝向的合法组合、动作占用句柄 | C# WuwaUnifiedStateComponent | 原生只读 StateData |
| 已提交状态与变更事件 | 原生 UnifiedStateBridge（仅快照桥） | CMC、AnimBP |
| 速度、加速度、摩擦、制动和实际物理移动 | CMC | Character、动画采样 |
| 当前播放片段、混合、左右脚相位 | AnimBP | Mesh |

原生桥的 `BlueprintNativeEvent` 默认实现为空／失败，没有另一份 C++ 状态决策兜底。
Character 在游戏世界装配真正的托管派生类。缺少托管程序集会报错，不能只编译 C++ 就开始测试。

## 当前每帧与事件链

1. 输入处理器先更新 Character 的移动意图，再调用 RoleGait 刷新。
2. CMC 在 `Super::TickComponent` 前再调度一次 `RefreshPolicy`，处理计时与物理模式变化。
3. RoleGait 从 CMC 读取物理位置、速度等事实，先同步 PositionState，再决定普通 MoveState。
4. UnifiedState 拒绝非法组合，例如 `Air + Sprint`；动作占用中拒绝普通步态覆盖。
5. 提交内容发生变化才广播 `OnStateChanged`。CMC 按接受的 Gait 应用配置，不清零 Velocity。
6. AnimInstance 在游戏线程采样成 `LocomotionData`；动画线程只读快照。

这里没有额外 C# Tick，也不让 Character Tick 和 CMC Tick 相互依赖。
GAS 决定能力激活和结束；移动状态占用不代替 GAS 的打断权限、消耗或冷却判断。

## 地面规则

- 有方向输入：有效冲刺需求且允许 Sprint → Sprint；否则回到 DesiredGait 的 Walk/Run。
- 没有方向输入：之前为 Walk/Run/Sprint 且仍有移动速度 → 对应 Stop；停稳 → Stand。
- Dodge 结束且无输入 → Stand，不因 Dash 残余速度制造 RunStop。
- `StopGait` 保留最近实际移动的步态，Stop 动画完成前不会因物理速度先归零而换片段。
- DesiredGait 只保留 Walk/Run。兼容的 `SetDesiredGait(Sprint)` 表示一次长期冲刺请求。

`TemporarySprintDuration` 在 CMC 上配置，默认 1 秒，从窗口结束时开始计时。
Sustained 在窗口内由超过 0.2 秒的连续语义冲刺输入产生，提交后松开 Shift 不清除，
松开方向才清除；退出地面、清空输入、ResetSprintDesire 也清除。

## 蓝图和 GA 接口

- 切换走跑：沿用原输入指令和 `ToggleWalkRun`。
- 重置冲刺：AnimBP Event Graph 的 `Reset Sprint Desire` 或 CMC 的 `Clear Sprint Desire`。
- 单个系统限制步态：`RoleGaitComponent.SetGaitBlocked(Source, Gait, true/false)`。
  每个系统用自己的 Source；只能解除自己的限制。无效 Source 会被清理。
- 带动作占用的 GA：开启 `Overrides Move State`，设 `Action Move State` 和优先级。
  GA 基类在激活条件检查时预检、进入激活流程时申请，结束后释放自己的句柄；旧动作迟到的结束不能清除新动作。
- UnifiedState 的公开 `Acquire/ReleaseMoveState` 用于其他需要持有动作状态的系统。
  同优先级新请求替换旧请求，不使用会让旧动作复活的栈。
- 查询状态：Character → UnifiedStateComponent → StateData。
  动画蓝图只读取 LocomotionData，不逐帧写回普通移动状态。

## 动画图的衔接

已有层级保持为 `Locmotion → Ground → SM_Ground(Idle / Move / Stop)`。
`Move` 内部仍通过缓存姿势使用原 BS，并选择 Sprint_F。

| 连线 | 使用字段 |
| --- | --- |
| Idle → Move、Stop → Move | bGroundMoveActive |
| Move → Stop | bWantsToStop（统一状态为某个 Stop） |
| Move → Idle | !bGroundMoveActive && !bWantsToStop |
| Move 中的 Sprint 选择 | bStateGroundSprint（实际接受的 Sprint） |
| Stop 中的 Walk/Run/Sprint 片段 | StopGait |

Stop → Idle 保持片段剩余时间规则。动作占用不依赖 Slot 是否持续更新 Source Pose。
现有 RootMotion Everything、关闭 Always Update Source Pose、Slot/FootIK/KawaiiPhysics 顺序保持原值。
现有已连通 Stop 采用左脚片段；右脚选择草稿保持原样，没有假定其已完成。

`BP_Ability_Dash` 已配置为占用 `Dodge`、优先级 100。前闪和后撤的
`PlayMontageAndWait` 的 `OnInterrupted`、`OnCancelled` 已接到各自已有的 `EndAbility`。
这样外部停止或替换蒙太奇也会归还移动状态，不只依赖自然播放结束。

编辑器迁移入口为 `WuwaMovementStateEditorLibrary.MigrateGroundStateAssets(bSave)`，
只处理检查过的 ABP_Changli 与 BP_Ability_Dash，保留旧节点以便核对。
`false` 仍会修改内存图并编译，`true` 编译成功后保存；执行前备份资产。

## 参数与扩展

CMC 的可选 `MovementSettings` 数据资产按 Walk/Run/Sprint 配置 MaxSpeed、MaxAcceleration、
GroundFriction、BrakingDeceleration。未指定时沿用 CMC 的 Walk/Run/Sprint 速度和原制动参数。
改运行中的配置后调用 `RefreshMovementSettings` 应用；不用在输入函数中设置速度。
未指定资产的兼容速度规则是 Walk=min(WalkSpeed, 初始 MaxWalkSpeed)，
Run/Sprint=max(各自速度配置, 初始 MaxWalkSpeed)。MaxWalkSpeed 在运行中是执行输出。

当前角色已使用 `/Game/Characters/Role/changli/DataAsset/DA_ChangliMovement`。
初始速度按角色原有效参数迁移为 Walk=100、Run=500、Sprint=900（cm/s），
加速度、摩擦和制动也从原角色组件复制。以后直接在这个数据资产中调整三种步态。

位置维度已经覆盖 Ground、Air、Climb、Water，以及 None；脚本会根据当前 CMC 模式解析
Jump/Fall/Flying、NormalClimb、NormalSwim。它们是逻辑入口，本项目缺少对应的完整动画图和资源，
不能把枚举存在视作已完成飞行、攀爬、游泳系统。

新增状态依次修改：统一状态枚举和合法组合表 → RoleGait 或专属动作的进入／退出规则 →
必要的 CMC 配置消费 → 动画快照与 AnimBP 表现。不要把这些判断重新塞回输入入口。
本次框架针对当前本地玩家；网络复制和预测回滚没有实现。

## 验证与构建

已通过项目的 31 项 `Wuwa.*` 自动化测试，包含真实托管组件、语义输入路由、
冲刺保留／到期、步态限制、物理模式变化、CMC 配置和动画快照，以及实际 Dash 蓝图的
自然结束、主动取消、蒙太奇被替换和重复激活。测试使用无界面 UE，未进行画面手感验收。

原生构建使用项目的正常 `WuwaEditor Win64 Development` 配置。
不要给 UBT 额外传入 `-NoLiveCoding`：当前预编译 UE 5.7 的 `UFunction` 使用
`WITH_LIVE_CODING=1` 布局，该构建开关会让项目／插件 ABI 与引擎不一致。
运行编辑器时禁用 Live Coding 与此编译开关不同。
原生接口发生修改后先构建 C++，再发布 `Script/ManagedWuwa/ManagedWuwa.csproj` 到
`Binaries/Managed/net10.0`，让脚本使用最新生成的绑定。

## 地面脚部 IK（2026-10-15）

两个角色的 Control Rig 现在执行 `Wuwa Terrain Foot IK` 原生单元，位置仍在原来的
Slot / Control Rig / KawaiiPhysics 动画链中。原 Idle 专用射线函数已从更新执行链旁路；
原来的 `EnableFootIK`、脚高和骨盆偏移输入不再参与求解。Start / Move / Stop 状态及同步组不改动。

开关和参数在角色 AnimBP 的 Class Defaults → Wuwa → Animation → Foot IK：

- `FootIKSettings.Enabled` 默认开启；`Force Disable Foot IK` 可由特殊动作临时关闭。
- 地面站立、起步、走跑、冲刺和停步均可参与。空中、动作覆盖和播放蒙太奇时关闭，回到地面移动后恢复。
  这是本项目的动作保护策略；原作原生 `IKMode` 的完整实现不可见，不能把它当作完全还原。
- `Max Step Height=30`、`Max Step Depth=35`、`Max Pelvis Drop=20` 单位是世界厘米；
  脚踝倾角限制 35°，默认检测 Visibility 通道。地面必须对该通道有碰撞响应。
- 游戏线程采集状态、胶囊脚底的地面基准、平台速度和坡度；Rig 根据本帧输入动画的双脚位置检测地面。
  检测方式沿用 UE Control Rig 碰撞节点的世界查询路径，求解阶段不修改角色或移动组件。
- 修正量叠加到本帧动画脚高，保留摆腿和抬脚。骨盆向较低的支撑脚下沉，双骨 IK 不拉长腿；
  脚踝倾斜叠加到动画旋转。世界坐标与 Rig 坐标显式换算，兼容两个角色的 100 倍导入根骨骼。
- `Foot_IK_l/r` 存在时直接使用，包括权重零；当前导入动画缺少这些曲线时按输入姿势的脚/脚趾抬升量回退，
  Walk 保持 1、Run 在 0.55～1、Sprint 在 0～1 平滑调节。这些数值和几何回退是项目校准，未伪造原作曲线。
- 平地无需修正时完全保留输入姿势。检测未命中时回到中性偏移，不把射线末端当作地面。
  瞬移超过 100cm、离地或关闭时清空历史修正。移动平台采用原导出中的 25/5cm/s 滞回和插值速度 3；
  移动时的陡坡减弱采用 40～50° 区间。原作坡度符号特殊分支、落地时间预测和锁脚求解器未复刻。

实现：`WuwaAnimFootIK.cpp` 负责状态，`Game/Animation/IK/WuwaFootIK.cpp` 负责贴地。
迁移前六个资产备份在 `Saved/Diagnostics/FootIK/Backup/20261015T210002Z`。
`Saved/Diagnostics/FootIK/Applied.json` 记录保存范围和保留的参考骨骼数量。

验证：`Wuwa.Animation.FootIK.TerrainAndGating` 对两个实际角色检查平地、20° 斜坡、左右高低台阶、
脚踝实际目标、腿长/缩放、曲线零权重、无支撑、离地/强制关闭、运行中的 RigVM 链路和移动平台恢复。
连同连续点按、StopPhases、双向角色切换共 11 项自动化测试通过，报告在
`Saved/Diagnostics/FootIK/Regression/index.json`。这些是无界面碰撞与骨骼数值验证，未代替实际画面验收。
