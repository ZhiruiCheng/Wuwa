using UnrealSharp;
using UnrealSharp.Attributes;
using UnrealSharp.CoreUObject;
using UnrealSharp.GameplayAbilities;
using UnrealSharp.GameplayTags;
using UnrealSharp.Wuwa;

namespace ManagedWuwa.Game.NewWorld.Character.Common.Component.Input;

[UClass]
public partial class UWuwaCombatInputRuntime : UWuwaCombatInputRuntimeBridge
{
    // 预输入缓存：技能播放中按下、但当时出不了招的键，等到接招时机再出。
    private readonly CombatInputBuffer _buffer = new();

    // 上次处理时的 ASC 和角色。任意一个变了（换了控制的角色、角色重生），之前的缓存全部作废。
    private TWeakObjectPtr<UWuwaAbilitySystemComponent> _asc;
    private TWeakObjectPtr<AWuwaCharacter> _avatar;

    // 上次看到的"主技能开始次数"（SkillComponent 的 SkillStartSerial）。
    // 变了说明开始了新的主技能，上一个技能期间存的预输入作废。
    private long _observedSkillStart;

    // 已经处理过的"接招机会编号"（SkillComponent 的 InputOpportunitySerial；接招窗口打开、进入 ReadyEnd、动画断点、技能结束时各加一）。
    // 和当前编号相同就跳过，保证同一个接招时机只处理一次缓存，不会每帧重复尝试。
    private long _observedOpportunity;

    // 上次处理时的输入时间（ASC.InputTimeSeconds）。时间倒退（比如世界重新开始）时整体重置。
    private double _lastTime;

    // 正在处理中。招式条件、GA 激活等扩展代码可能在处理过程中又触发输入，此时拒绝重入。
    private bool _processing;

    // 输入批次编号：重置或清除缓存时加一。
    // 处理开始时记下，判断完再比对；变了说明中途有人清过缓存或重置过，这次的判断结果作废。
    private long _inputGeneration;

    private readonly record struct Candidate(FGameplayAbilitySpecHandle Handle, UWuwaGameplayAbilityBase Definition);

    /// <summary>
    /// 处理一次按下 / 长按 / 松开（规则按 Phase 区分）：能立刻出招就提交；暂时不能就存入预输入（松开不存），等下一个接招时机。
    /// 招式条件和许可检查可能执行扩展代码，做出决定前用 IsContextCurrent 确认技能、输入批次和战斗状态都没变，变了这次按下作废。
    /// </summary>
    public override EWuwaCombatInputResult ProcessInput(UWuwaAbilitySystemComponent asc, FWuwaInputEvent input, float bufferLifetimeSeconds)
    {
        // 只处理按下 / 长按 / 松开（轴输入的 Triggered、Canceled 不选招）；扩展代码里又触发输入时拒绝重入。
        if (_processing || !IsCommandPhase(input.Phase) || !input.InputTag.IsValid) return EWuwaCombatInputResult.Ignored;
        _processing = true;
        try
        {
            if (!ReadContext(asc, out var skills, out var state, out double now)) return EWuwaCombatInputResult.Ignored;
            long generation = _inputGeneration;
            int fightHandle = asc.InputAvatar.FightStateComponent.StateData.Handle;

            var resolution = Resolve(asc, state, input.InputTag, input.Phase, out var candidate);
            bool canCreate = resolution == EWuwaCombatInputResult.ActivationRequested && CanCreateCommand(asc, skills!, candidate);
            if (!IsContextCurrent(asc, skills!, state, generation, fightHandle)) return EWuwaCombatInputResult.Ignored;

            if (canCreate) return Submit(asc, candidate);
            // 长按 / 松开没有配对应阶段的行：和以前一样当没看见（不预输入，也不报 NoCandidate）
            if (input.Phase != EWuwaInputPhase.Pressed && resolution == EWuwaCombatInputResult.NoCandidate) return EWuwaCombatInputResult.Ignored;
            // 松开是一瞬间的事，不预输入；长按也不存（原作输入缓存 长按 = 0 秒）：连续触发的键每帧都会再发一次，到可接的那一帧自然接上
            if (input.Phase is EWuwaInputPhase.Released or EWuwaInputPhase.Held)
                return resolution == EWuwaCombatInputResult.ActivationRequested ? EWuwaCombatInputResult.Rejected : resolution;
            // 有配置但条件暂未命中：也存下原始按下，到接招时机条件可能已满足。
            if (resolution == EWuwaCombatInputResult.NoMatchingRule)
                return _buffer.Store(input, now, bufferLifetimeSeconds) ? EWuwaCombatInputResult.Buffered : resolution;
            if (resolution != EWuwaCombatInputResult.ActivationRequested) return resolution;
            // 主技能暂时不能开始：存下原始按下，不存此刻解析出的 GA 或 Spec。
            return candidate.Definition.IsMainSkill && _buffer.Store(input, now, bufferLifetimeSeconds)
                ? EWuwaCombatInputResult.Buffered : EWuwaCombatInputResult.Rejected;
        }
        finally { _processing = false; }
    }

    public override EWuwaCombatInputResult ProcessPendingInput(UWuwaAbilitySystemComponent asc)
    {
        // 断点事件和帧末 Tick 共用此入口；正在处理时（同一帧多次触发）拒绝重入。
        if (_processing) return EWuwaCombatInputResult.Ignored;
        _processing = true;
        try
        {
            if (!ReadContext(asc, out var skills, out var state, out double now)) return EWuwaCombatInputResult.Ignored;
            // 帧末每帧都会走到这里：顺便刷新屏幕上的技能状态和预输入缓存。
            CombatInputDebug.Show(state, _buffer.Describe(now));
            // 同一个接招机会只处理一次，不轮询重试激活。
            if (_observedOpportunity == state.InputOpportunitySerial) return EWuwaCombatInputResult.Ignored;
            _observedOpportunity = state.InputOpportunitySerial;
            long generation = _inputGeneration;
            int fightHandle = asc.InputAvatar.FightStateComponent.StateData.Handle;

            Candidate? best = null;
            foreach (var input in _buffer.Snapshot(now))
            {
                var resolution = Resolve(asc, state, input.InputTag, input.Phase, out var candidate);
                bool canCreate = resolution == EWuwaCombatInputResult.ActivationRequested && CanCreateCommand(asc, skills!, candidate);
                // 条件和许可检查可能执行扩展代码；上下文变了，这一批作废。
                if (!IsContextCurrent(asc, skills!, state, generation, fightHandle)) return EWuwaCombatInputResult.Ignored;
                if (!canCreate) continue;

                // 严格大于：同优先级保留较早到达的输入。
                if (best is null || candidate.Definition.InterruptLevel > best.Value.Definition.InterruptLevel)
                    best = candidate;
            }
            return best is { } selected ? Submit(asc, selected) : EWuwaCombatInputResult.Ignored;
        }
        finally { _processing = false; }
    }

    protected override int GetBufferedInputCount_Implementation() => _buffer.Count;

    protected override int ClearBufferedInput_Implementation(FGameplayTag inputTag)
    {
        ++_inputGeneration;
        return _buffer.Remove(inputTag);
    }

    
    public override void ResetInput()
    {
        //在这之前的所有缓存和判断结果,全部作废 因为编号加一
        ++_inputGeneration;
        _buffer.Clear();
        _asc = default;
        _avatar = default;
        _observedSkillStart = 0;
        _observedOpportunity = 0;
        _lastTime = 0;
    }

    private bool ReadContext(UWuwaAbilitySystemComponent asc, out UWuwaSkillBridgeComponent? skills, out FWuwaSkillData state, out double now)
    {
        skills = null;
        state = default;
        now = 0;
        
        if (!asc.IsValid()) { ResetInput(); return false; }
        var avatar = asc.InputAvatar;
        if (!avatar.IsValid() || !avatar.SkillComponent.IsValid() || !avatar.FightStateComponent.IsValid()) { ResetInput(); return false; }
        skills = avatar.SkillComponent;
        state = skills.GetCurrentSkillData();
        now = asc.InputTimeSeconds;
        if (!double.IsFinite(now) || now < 0) { ResetInput(); return false; }

        if (_asc != new TWeakObjectPtr<UWuwaAbilitySystemComponent>(asc) || _avatar != new TWeakObjectPtr<AWuwaCharacter>(avatar) || now < _lastTime)
        {
            ResetInput();
            _asc = new(asc);
            _avatar = new(avatar);
            _observedOpportunity = state.InputOpportunitySerial;
        }

        // 新技能即使在两次采样之间已经开始并结束，也必须使旧批次失效。
        // 原技能正常结束不增加 StartSerial，仍允许在结束断点消费。
        if (state.SkillStartSerial != _observedSkillStart) _buffer.Clear();
        _observedSkillStart = state.SkillStartSerial;
        _lastTime = now;
        _buffer.Prune(now);

        // 外部受击等接管后，不继续执行上一技能留下的预输入。
        var fight = avatar.FightStateComponent;
        if (fight.IsValid() && fight.StateData.Handle != 0 && fight.StateData.Handle != state.FightStateHandle) _buffer.Clear();
        return true;
    }

    //把一次按键翻译成"要放哪个技能"
    private static bool IsCommandPhase(EWuwaInputPhase phase) =>
        phase is EWuwaInputPhase.Pressed or EWuwaInputPhase.Held or EWuwaInputPhase.Released;

    private static EWuwaCombatInputResult Resolve(UWuwaAbilitySystemComponent asc, FWuwaSkillData state, FGameplayTag inputTag,
        EWuwaInputPhase phase, out Candidate candidate)
    {
        candidate = default;
        var context = new FWuwaInputCommandContext { ASC = asc, Avatar = asc.InputAvatar, CurrentSkill = state, InputTag = inputTag, Phase = phase };
        var resolution = WuwaInputCommandResolver.SelectSkillId(context, out var skillId);
        if (resolution != EWuwaCombatInputResult.ActivationRequested) return resolution;
        // 技能 ID 在 ASC 上唯一（授予时保证），找不到说明角色没有这个技能
        var handles = asc.FindSkillHandles(skillId);
        if (handles.Count == 0) return EWuwaCombatInputResult.NoCandidate;
        var definition = asc.GetAbilityForInput(handles[0]);
        if (!definition.IsValid()) return EWuwaCombatInputResult.NoCandidate;
        candidate = new(handles[0], definition);
        return EWuwaCombatInputResult.ActivationRequested;
    }

    // 自定义条件应当是纯查询；若扩展代码意外结束技能/清输入/换角色，不提交旧快照。
    private bool IsContextCurrent(UWuwaAbilitySystemComponent asc, UWuwaSkillBridgeComponent skills,
        FWuwaSkillData state, long generation, int fightHandle)
    {
        if (generation != _inputGeneration || !asc.IsValid() || !skills.IsValid()) return false;
        var avatar = asc.InputAvatar;
        if (!avatar.IsValid() || _avatar != new TWeakObjectPtr<AWuwaCharacter>(avatar)
            || avatar.SkillComponent != skills || !avatar.FightStateComponent.IsValid()) return false;
        var current = skills.GetCurrentSkillData();
        return current.SkillStartSerial == state.SkillStartSerial && current.FightStateHandle == state.FightStateHandle
            && current.InputOpportunitySerial == state.InputOpportunitySerial
            && avatar.FightStateComponent.StateData.Handle == fightHandle;
    }

    // 两道检查：GAS 是否允许激活（冷却、消耗、Tag）；主技能还要当前技能肯让位（SkillComponent.CanBeginSkill）。
    private static bool CanCreateCommand(UWuwaAbilitySystemComponent asc, UWuwaSkillBridgeComponent skills, Candidate candidate)
    {
        return asc.IsSpecAvailableForActivation(candidate.Handle)
            && (!candidate.Definition.IsMainSkill || skills.CanBeginSkill(candidate.Definition));
    }

    private EWuwaCombatInputResult Submit(UWuwaAbilitySystemComponent asc, Candidate candidate)
    {
        // 一批只提交一次：即便 GAS 因冷却/消耗拒绝，也不再尝试第二候选。
        // 原作提交后清批；这里提前清批，避免同步回调再次消费同一份输入。
        _buffer.Clear();
        return asc.RequestAbilityActivation(candidate.Handle) switch
        {
            EWuwaAbilityRequestResult.ActivationRequested => EWuwaCombatInputResult.ActivationRequested,
            EWuwaAbilityRequestResult.AlreadyActive => EWuwaCombatInputResult.AlreadyActive,
            _ => EWuwaCombatInputResult.Rejected
        };
    }
}
