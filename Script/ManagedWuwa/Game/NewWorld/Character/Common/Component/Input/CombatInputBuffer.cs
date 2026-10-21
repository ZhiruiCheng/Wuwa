using UnrealSharp.GameplayTags;
using UnrealSharp.Wuwa;

namespace ManagedWuwa.Game.NewWorld.Character.Common.Component.Input;

internal readonly record struct WuwaBufferedCombatInput(
    FGameplayTag InputTag, EWuwaInputPhase Phase, double InputTimeSeconds, double ExpiresAtSeconds);

// 缓存原始指令。断点时重新解释，选一条提交后清整批，不是按队列依次执行。
internal sealed class CombatInputBuffer
{
    private readonly List<WuwaBufferedCombatInput> _items = new();
    public int Count => _items.Count;

    public bool Store(FWuwaInputEvent input, double nowSeconds, double lifetimeSeconds)
    {
        // 只有按下可以预输入（断点时重新选招）；松开、长按不存（原作长按缓存 0 秒，连续触发的长按每帧会再来）
        if (input.Phase != EWuwaInputPhase.Pressed || !input.InputTag.IsValid
            || !double.IsFinite(input.Timestamp) || !double.IsFinite(nowSeconds)
            || !double.IsFinite(lifetimeSeconds) || lifetimeSeconds <= 0
            || !double.IsFinite(nowSeconds + lifetimeSeconds)) return false;

        Prune(nowSeconds);
        _items.Add(new(input.InputTag, input.Phase, input.Timestamp, nowSeconds + lifetimeSeconds));
        return true;
    }

    public void Prune(double nowSeconds)
    {
        if (!double.IsFinite(nowSeconds)) { Clear(); return; }
        _items.RemoveAll(item => nowSeconds >= item.ExpiresAtSeconds);
    }

    // 只读描述，给屏幕调试显示用；不清理也不消费输入。
    public string Describe(double nowSeconds) => "[" + string.Join(", ", _items.Select(item =>
        FormattableString.Invariant($"{item.InputTag}@{item.InputTimeSeconds:F3} expires={item.ExpiresAtSeconds:F3} remaining={item.ExpiresAtSeconds - nowSeconds:F3}s"))) + "]";

    public WuwaBufferedCombatInput[] Snapshot(double nowSeconds)
    {
        Prune(nowSeconds);
        return _items.ToArray();
    }

    public void Clear() => _items.Clear();

    public int Remove(FGameplayTag inputTag)
    {
        if (inputTag.IsValid) return _items.RemoveAll(item => item.InputTag.Equals(inputTag));
        int count = _items.Count;
        Clear();
        return count;
    }
}
