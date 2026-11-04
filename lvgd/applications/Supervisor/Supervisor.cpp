
static long Syscall3(long number, long arg1, long arg2, long arg3)
{
    long result = number;

    asm volatile(
        "int $0x80"
        : "+a"(result)
        : "b"(arg1), "c"(arg2), "d"(arg3)
        : "memory", "cc"
    );

    return result;
}

static void WriteText(const char* text)
{
    unsigned int length = 0;

    while (text[length])
        ++length;

    while (length)
    {
        long written = Syscall3(4, 2, (long)text, length);

        if (written <= 0)
            return;

        text += written;
        length -= (unsigned int)written;
    }
}

static void ExitProcess(int status)
{
    Syscall3(1, status, 0, 0);

    for (;;)
        ;
}

static char** g_Environment = 0;

static bool WaitForProcess(long process)
{
    int status;

    for (;;)
    {
        long result = Syscall3(7, process, (long)&status, 0);

        if (result == -4)
            continue;

        return result >= 0;
    }
}

static bool LaunchAndWait(const char* path, const char* workingDirectory)
{
    long process = Syscall3(2, 0, 0, 0);

    if (process < 0)
    {
        WriteText("Supervisor: fork failed\n");
        return false;
    }

    if (process == 0)
    {
        if (workingDirectory && Syscall3(12, (long)workingDirectory, 0, 0) < 0)
        {
            WriteText("Supervisor: chdir failed\n");
            ExitProcess(1);
        }

        char* const args[] = { (char*)path, 0 };

        Syscall3(11, (long)path, (long)args, (long)g_Environment);

        WriteText("Supervisor: exec failed\n");
        ExitProcess(127);
    }

    return WaitForProcess(process);
}

extern "C" int SupervisorMain(int argc, char** argv)
{
    if (argc != 2)
    {
        WriteText("Usage: Supervisor /path/to/game\n");
        return 1;
    }

    g_Environment = &argv[argc + 1];

    const char* gamePath = argv[1];
    char directory[4096];

    unsigned int length = 0;

    while (gamePath[length] && length < sizeof(directory) - 1)
    {
        directory[length] = gamePath[length];
        ++length;
    }

    if (gamePath[length])
    {
        WriteText("Supervisor: game path too long\n");
        return 1;
    }

    directory[length] = '\0';

    char* separator = 0;

    for (unsigned int i = 0; i < length; ++i)
    {
        if (directory[i] == '/')
            separator = &directory[i];
    }

    if (!separator)
    {
        WriteText("Supervisor: game path must be absolute\n");
        return 1;
    }

    if (separator == directory)
        separator[1] = '\0';
    else
        *separator = '\0';

    LaunchAndWait(gamePath, directory);
    LaunchAndWait("/games/lvgd/GameLauncher", 0);

    return 0;
}

asm(
    ".global _start\n"
    ".type _start,@function\n"
    "_start:\n"
    "xorl %ebp, %ebp\n"
    "movl (%esp), %eax\n"
    "leal 4(%esp), %edx\n"
    "andl $-16, %esp\n"
    "subl $8, %esp\n"
    "pushl %edx\n"
    "pushl %eax\n"
    "call SupervisorMain\n"
    "movl %eax, %ebx\n"
    "movl $1, %eax\n"
    "int $0x80\n"
    "hlt\n"
);
