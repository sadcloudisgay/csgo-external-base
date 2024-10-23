#include "memory.h"
#include "vector.h"
#include <thread>
#include <functional>
#include <Windows.h>

namespace offset
{
    // all offsets are named as they are named in the hazedumper file included.
    constexpr ::std::ptrdiff_t dwLocalPlayer = 0xDEF97C;
    constexpr ::std::ptrdiff_t dwEntityList = 0x4E051DC;
    constexpr ::std::ptrdiff_t dwViewMatrix = 0x4DF6024;
    constexpr ::std::ptrdiff_t dwClientState = 0x59F19C;
    constexpr ::std::ptrdiff_t dwClientState_ViewAngles = 0x4D90;
    constexpr ::std::ptrdiff_t dwClientState_GetLocalPlayer = 0x180;
    constexpr ::std::ptrdiff_t m_dwBoneMatrix = 0x26A8;
    constexpr ::std::ptrdiff_t m_bDormant = 0xED;
    constexpr ::std::ptrdiff_t m_iTeamNum = 0xF4;
    constexpr ::std::ptrdiff_t m_lifeState = 0x25F;
    constexpr ::std::ptrdiff_t m_vecOrigin = 0x138;
    constexpr ::std::ptrdiff_t m_vecViewOffset = 0x108;
    constexpr ::std::ptrdiff_t m_aimPunchAngle = 0x303C;
    constexpr ::std::ptrdiff_t m_bSpottedByMask = 0x980;
    constexpr ::std::ptrdiff_t dwForceJump = 0x52C0F50;
    constexpr ::std::ptrdiff_t m_fFlags = 0x104;
    constexpr ::std::ptrdiff_t dwGlowObjectManager = 0x535FCB8;
    constexpr ::std::ptrdiff_t m_iGlowIndex = 0x10488;
    constexpr ::std::ptrdiff_t dwForceAttack = 0x3233024;
    constexpr ::std::ptrdiff_t m_iCrosshairId = 0x11838;
}

struct Color
{
    constexpr Color(float r, float g, float b, float a = 1.f) noexcept :
        r(r), g(g), b(b), a(a) {}

    float r, g, b, a;
};

Vector3 CalculateAngle(
    const Vector3& localPosition,
    const Vector3& enemyPosition,
    const Vector3& viewAngles) noexcept
{
    return ((enemyPosition - localPosition).ToAngle() - viewAngles);
}

void GlowLogic(Memory& memory, uintptr_t client, int localTeam)
{
    const auto color = Color(1.f, 1.f, 1.f);

    while (true)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(0));

        for (auto i = 1; i <= 64; ++i)
        {
            const auto player = memory.Read<std::uintptr_t>(client + offset::dwEntityList + i * 0x10);
            if (player == 0) continue;

            const auto teamNum = memory.Read<std::int32_t>(player + offset::m_iTeamNum);
            const auto lifeState = memory.Read<std::int32_t>(player + offset::m_lifeState);

            if (teamNum != localTeam && lifeState == 0)
            {
                const auto glowObjectManager = memory.Read<std::uintptr_t>(client + offset::dwGlowObjectManager);
                const auto glowIndex = memory.Read<std::int32_t>(player + offset::m_iGlowIndex);

                if (glowIndex >= 0)
                {
                    memory.Write<Color>(glowObjectManager + (glowIndex * 0x38) + 0x8, color); // Color
                    memory.Write<bool>(glowObjectManager + (glowIndex * 0x38) + 0x27, true); // Render
                    memory.Write<bool>(glowObjectManager + (glowIndex * 0x38) + 0x28, true); // Render
                }
            }
        }
    }
}

// Bunny hop logic
void BunnyHop(Memory& memory, uintptr_t client, uintptr_t localPlayer) {
    while (true) {
        const auto onground = memory.Read<std::int32_t>(localPlayer + offset::m_fFlags);

        if (GetAsyncKeyState(VK_SPACE) && (onground & (1 << 0))) {
            memory.Write<BYTE>(client + offset::dwForceJump, 6);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(0));
    }
}

// Triggerbot logic
void TriggerBot(Memory& memory, uintptr_t client, uintptr_t localPlayer) {
    while (true) {
        if (GetAsyncKeyState(VK_XBUTTON2)) {  // Mouse button for triggerbot
            const auto crosshairId = memory.Read<std::int32_t>(localPlayer + offset::m_iCrosshairId);
            if (crosshairId > 0 && crosshairId <= 64) {
                const auto target = memory.Read<uintptr_t>(client + offset::dwEntityList + (crosshairId - 1) * 0x10);
                const auto targetTeam = memory.Read<std::int32_t>(target + offset::m_iTeamNum);
                const auto localTeam = memory.Read<std::int32_t>(localPlayer + offset::m_iTeamNum);

                if (targetTeam != localTeam) {
                    memory.Write<int>(client + offset::dwForceAttack, 6);
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                    memory.Write<int>(client + offset::dwForceAttack, 4);
                }
            }
        }
    }
}

// Main Thread
int main()
{
    Memory memory{ "csgo.exe" };
    const auto client = memory.GetModuleAddress("client.dll");
    const auto engine = memory.GetModuleAddress("engine.dll");

    if (!engine || !client)
    {
        printf("Couldn't find client.dll or engine.dll");
        return -1;
    }
    else
    {
        printf("Hooked\n");
    }

    const auto localPlayer = memory.Read<std::uintptr_t>(client + offset::dwLocalPlayer);
    if (localPlayer == 0)
        return 1;

    const auto localTeam = memory.Read<std::int32_t>(localPlayer + offset::m_iTeamNum);

    std::thread glowThread([&memory, client, localTeam]() {
        printf("Glow thread initialized\n");
        GlowLogic(memory, client, localTeam);
        });

    std::thread bunnyHopThread([&memory, client, localPlayer]() {
        printf("BunnyHop thread initialized\n");
        BunnyHop(memory, client, localPlayer);
        });

    std::thread triggerBotThread([&memory, client, localPlayer]() {
        printf("Triggerbot thread initialized\n");
        TriggerBot(memory, client, localPlayer);
        });

    while (true)
    {
        const auto localPlayer = memory.Read<std::uintptr_t>(client + offset::dwLocalPlayer);
        if (localPlayer == 0)
            continue;

        const auto localTeam = memory.Read<std::int32_t>(localPlayer + offset::m_iTeamNum);

        const auto localEyePosition = memory.Read<Vector3>(localPlayer + offset::m_vecOrigin) +
            memory.Read<Vector3>(localPlayer + offset::m_vecViewOffset);

        const auto clientState = memory.Read<std::uintptr_t>(engine + offset::dwClientState);
        const auto localPlayerId = memory.Read<std::int32_t>(clientState + offset::dwClientState_GetLocalPlayer);
        const auto viewAngles = memory.Read<Vector3>(clientState + offset::dwClientState_ViewAngles);
        const auto aimPunch = memory.Read<Vector3>(localPlayer + offset::m_aimPunchAngle) * 2;

        auto bestFov = 360.f;
        auto bestAngle = Vector3{};
        bool foundValidTarget = false;

        if (GetAsyncKeyState(VK_XBUTTON2))
        {
            for (auto i = 1; i <= 64; ++i)
            {
                const auto player = memory.Read<std::uintptr_t>(client + offset::dwEntityList + i * 0x10);

                if (player == 0)
                    continue;

                const auto teamNum = memory.Read<std::int32_t>(player + offset::m_iTeamNum);
                const auto lifeState = memory.Read<std::int32_t>(player + offset::m_lifeState);

                if (teamNum == localTeam || lifeState != 0)
                    continue;

                const auto boneMatrix = memory.Read<std::uintptr_t>(player + offset::m_dwBoneMatrix);
                if (boneMatrix == 0)
                    continue;

                const auto playerHeadPosition = Vector3{
                    memory.Read<float>(boneMatrix + 0x30 * 8 + 0x0C),
                    memory.Read<float>(boneMatrix + 0x30 * 8 + 0x1C),
                    memory.Read<float>(boneMatrix + 0x30 * 8 + 0x2C)
                };

                if (playerHeadPosition == Vector3{})
                    continue;

                const auto angle = CalculateAngle(
                    localEyePosition,
                    playerHeadPosition,
                    viewAngles + aimPunch);

                const auto fov = std::hypot(angle.x, angle.y);

                if (fov < bestFov)
                {
                    bestFov = fov;
                    bestAngle = angle;
                    foundValidTarget = true;
                }
            }

            if (foundValidTarget)
            {
                memory.Write<Vector3>(clientState + offset::dwClientState_ViewAngles, viewAngles + bestAngle / 1);
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    glowThread.join();
    bunnyHopThread.join();
    triggerBotThread.join();

    return 0;
}
