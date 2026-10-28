namespace ManagedWuwa.Game.Camera;

/// <summary>
/// 按来源持有的请求容器：最高优先级获胜，同级取最新请求。
/// 来源有效性由使用方传入，容器本身不依赖 UE 对象或某个相机组件。
/// </summary>
internal sealed class ZrOwnedRequests<TOwner, TValue>(Func<TOwner, bool> isOwnerValid)
{
    //模板用法
    private readonly record struct Request(int Handle, TOwner Owner, TValue Value, int Priority);

    private readonly List<Request> _requests = new();
    private int _lastIssuedHandle;

    public int Add(TOwner owner, TValue value, int priority)
    {
        // 不循环使用句柄：迟到的退出请求不能误删后来创建的模式。
        if (!isOwnerValid(owner) || _lastIssuedHandle == int.MaxValue)
            return 0;

        int handle = ++_lastIssuedHandle;
        _requests.Add(new Request(handle, owner, value, priority));
        return handle;
    }

    public bool Remove(int handle)
    {
        int index = _requests.FindIndex(request => request.Handle == handle);
        if (index < 0)
            return false;

        _requests.RemoveAt(index);
        return true;
    }

    public bool TryGetHighest(out int handle, out TValue value)
    {
        Request? highest = null;
        for (int index = _requests.Count - 1; index >= 0; --index)
        {
            var request = _requests[index];
            if (!isOwnerValid(request.Owner))
            {
                _requests.RemoveAt(index);
                continue;
            }

            if (highest is null || request.Priority > highest.Value.Priority)
                highest = request;
        }

        handle = highest?.Handle ?? 0;
        value = highest is { } selected ? selected.Value : default!;
        return highest.HasValue;
    }

    public void Clear() => _requests.Clear();
}
