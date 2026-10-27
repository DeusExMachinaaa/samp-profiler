// Minimal gamemode example for samp-profiler.
// Compile this file as a gamemode after copying plugin/samp_profiler.inc
// to your Pawn include directory.

#include <a_samp>
#include <samp_profiler>

new g_profile_update;

public OnGameModeInit()
{
    SetGameModeText("Profiler test");

    // Register once during startup. The returned integer is used on the
    // hot path, so no section name is copied while profiling.
    g_profile_update = SP_Register("OnPlayerUpdate");
    SP_SetEnabled(true);

    print("[test.pwn] samp-profiler is enabled");
    return 1;
}

public OnPlayerUpdate(playerid)
{
    new scope = SP_Begin(g_profile_update);

    // Put the code you want to measure inside this scope.
    // This example intentionally does very little work.
    GetPlayerState(playerid);

    SP_End(scope);
    return 1;
}

public OnPlayerCommandText(playerid, cmdtext[])
{
    if (!strcmp(cmdtext, "/profiler", true))
    {
        if (!IsPlayerAdmin(playerid))
        {
            SendClientMessage(playerid, -1, "RCON admin access required.");
            return 1;
        }

        // SP_End is silent. SP_Dump prints the accumulated report to the
        // samp-server console/log. The argument is a microsecond threshold.
        SP_Dump(0);
        SendClientMessage(playerid, -1, "Profiler report written to the server log.");
        return 1;
    }

    return 0;
}
