using UnrealSharp.CoreUObject;
using UnrealSharp.Engine;

namespace ManagedWuwa.Game.ZRCamera.FightCamera;

//相机检测方法从pivot->desire发送半径为radius的小球根据enable判断是否需要检测,如果开启碰撞检测就返回当前第一个碰到的位置
internal sealed class ZRCCameraCollision
{
    private readonly List<AActor> _ignoredActors = new();
    
    public FVector CheckCollision(APawn pawn, FVector pivot, FVector desired, float radius, bool enabled)
    {
        //没开启就不检测了
        if (!enabled) { return desired; }
        
        _ignoredActors.Clear();
        _ignoredActors.Add(pawn);
        FHitResult outHit;
        //射线检测结果
        bool hit = SystemLibrary.SphereTraceByChannel(
            pivot, desired, Math.Max(1,radius), 
            ETraceTypeQuery.TraceTypeQuery2,     // ETraceTypeQuery：用哪个检测通道，见第 2 点
            false,     // bool：false = 用简单碰撞体检测（快），true = 用模型网格（慢）
            _ignoredActors,   // IList<AActor>：要忽略的 Actor 列表，这里放角色
            EDrawDebugTrace.None,    // EDrawDebugTrace：要不要把检测画出来，见第 3 点
            out outHit,  // 撞到的结果
            true
            );  
        //没有撞到就用原始值
        if (!hit) { return desired; }
        return outHit.StartPenetrating ? pivot : outHit.Location;
    }
}