ANGRY_TEST(abortsWhenClientSendMessageReceivesNoArguments)
{
    pid_t pid = angry_fork();
    if (pid == 0) {
        close(2);
        char *args[] = { "yabai" };
        client_send_message(1, args);
        exit(0);
    }
    int status = 0;
    waitpid(pid, &status, 0);
    ANGRY_ASSERT_ALL("expected client_send_message with 1 arg to exit 1",
                     WIFEXITED(status),
                     WEXITSTATUS(status) == 1);
}

ANGRY_TEST(printsVersionAndExitsInParseArguments)
{
    pid_t pid = angry_fork();
    if (pid == 0) {
        close(1);
        char *args[] = { "yabai", "--version" };
        parse_arguments(2, args);
        exit(1);
    }
    int status = 0;
    waitpid(pid, &status, 0);
    ANGRY_ASSERT_ALL("expected --version to exit cleanly with 0",
                     WIFEXITED(status),
                     WEXITSTATUS(status) == 0);
}
